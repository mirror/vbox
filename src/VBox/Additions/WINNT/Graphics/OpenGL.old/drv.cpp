/** @file
 *
 * VirtualBox Windows NT/2000/XP guest OpenGL ICD
 *
 * ICD entry points.
 *
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

/*
 * Mesa 3-D graphics library
 * Version:  6.1
 *
 * Copyright (C) 1999-2004  Brian Paul   All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * BRIAN PAUL BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * File name: icd.c
 * Author:    Gregor Anich
 *
 * ICD (Installable Client Driver) interface.
 * Based on the windows GDI/WGL driver.
 */

#include "VBoxOGL.h"

#define GL_FUNC(func) gl##func

static ICDTABLE icdTable = { 336, {
#define ICD_ENTRY(func) (PROC)GL_FUNC(func),
#include "VBoxICDList.h"
#undef ICD_ENTRY
} };


BOOL APIENTRY DrvValidateVersion(DWORD version)
{
    DbgPrintf(("DrvValidateVersion %x -> always TRUE\n", version));
    return TRUE;
}


PICDTABLE APIENTRY DrvSetContext(HDC hdc, HGLRC hglrc, void *callback)
{
    NOREF(callback);

    /* Note: we ignore the callback parameter here */
    VBOX_OGL_GEN_SYNC_OP2(DrvSetContext, hdc, hglrc);
    return &icdTable;
}

/* DrvSetPixelFormat can only be called once for each window (if hdc associated with one). */
BOOL APIENTRY DrvSetPixelFormat(HDC hdc, int iPixelFormat)
{
    uint32_t cx, cy;
    HWND hwnd;

    /** check dimensions and pixel format of hdc */
    hwnd = WindowFromDC(hdc);
    if (hwnd)
    {
        RECT rect;

        GetWindowRect(hwnd, &rect);
        cx = rect.right - rect.left;
        cy = rect.bottom - rect.top;
    }
    else
    {
        /** @todo get dimenions of memory dc; a bitmap should be selected in there */
        AssertFailed();
    }

    VBOX_OGL_GEN_SYNC_OP4_RET(BOOL, DrvSetPixelFormat, hdc, iPixelFormat, cx, cy);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

HGLRC APIENTRY DrvCreateContext(HDC hdc)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(HGLRC, DrvCreateContext, hdc);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

HGLRC APIENTRY DrvCreateLayerContext(HDC hdc, int iLayerPlane)
{
    VBOX_OGL_GEN_SYNC_OP2_RET(HGLRC, DrvCreateLayerContext, hdc, iLayerPlane);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

BOOL APIENTRY DrvDescribeLayerPlane(HDC hdc,int iPixelFormat,
                                    int iLayerPlane, UINT nBytes,
                                    LPLAYERPLANEDESCRIPTOR plpd)
{
    VBOX_OGL_GEN_SYNC_OP5_PASS_PTR_RET(BOOL, DrvDescribeLayerPlane, hdc, iPixelFormat, iLayerPlane, nBytes, nBytes, plpd);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

int APIENTRY DrvGetLayerPaletteEntries(HDC hdc, int iLayerPlane,
                                       int iStart, int cEntries,
                                       COLORREF *pcr)
{
    VBOX_OGL_GEN_SYNC_OP5_PASS_PTR_RET(int, DrvGetLayerPaletteEntries, hdc, iLayerPlane, iStart, cEntries, sizeof(COLORREF)*cEntries, pcr);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

int APIENTRY DrvDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
    /* if ppfd == NULL, then DrvDescribelayerPlane returns the maximum nr of supported pixel formats */
    VBOX_OGL_GEN_SYNC_OP4_PASS_PTR_RET(int, DrvDescribePixelFormat, hdc, iPixelFormat, nBytes, nBytes, ppfd);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}


BOOL APIENTRY DrvDeleteContext(HGLRC hglrc)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(BOOL, DrvDeleteContext, hglrc);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

BOOL APIENTRY DrvCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
    VBOX_OGL_GEN_SYNC_OP3_RET(BOOL, DrvCopyContext, hglrcSrc, hglrcDst, mask);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

void APIENTRY DrvReleaseContext(HGLRC hglrc)
{
    VBOX_OGL_GEN_SYNC_OP1(DrvReleaseContext, hglrc);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
}

BOOL APIENTRY DrvShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
    VBOX_OGL_GEN_SYNC_OP2_RET(BOOL, DrvShareLists, hglrc1, hglrc2);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

int APIENTRY DrvSetLayerPaletteEntries(HDC hdc, int iLayerPlane,
                                       int iStart, int cEntries,
                                       CONST COLORREF *pcr)
{
    VBOX_OGL_GEN_SYNC_OP5_PTR_RET(int, DrvSetLayerPaletteEntries, hdc, iLayerPlane, iStart, cEntries, sizeof(COLORREF)*cEntries, pcr);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}


BOOL APIENTRY DrvRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL bRealize)
{
    VBOX_OGL_GEN_SYNC_OP3_RET(BOOL, DrvRealizeLayerPalette, hdc, iLayerPlane, bRealize);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

BOOL APIENTRY DrvSwapLayerBuffers(HDC hdc, UINT fuPlanes)
{
    VBOX_OGL_GEN_SYNC_OP2_RET(BOOL, DrvSwapLayerBuffers, hdc, fuPlanes);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

BOOL APIENTRY DrvSwapBuffers(HDC hdc)
{
    VBOX_OGL_GEN_SYNC_OP1_RET(BOOL, DrvSwapBuffers, hdc);
    /* Propagate error through the right channel */
    SetLastError(glGetError());
    return retval;
}

#ifdef VBOX_WITH_WGL_EXPORTS
extern PROC APIENTRY DrvGetProcAddress(LPCSTR lpszProc);

/* Test export for directly linking with vboxogl.dll */
int WINAPI wglSetLayerPaletteEntries(HDC hdc, int iLayerPlane,
                                     int iStart, int cEntries,
                                     CONST COLORREF *pcr)
{
    return DrvSetLayerPaletteEntries(hdc, iLayerPlane, iStart, cEntries, pcr);
}

BOOL WINAPI wglSetPixelFormat(HDC hdc, int iPixelFormat)
{
    return DrvSetPixelFormat(hdc, iPixelFormat);
}

BOOL WINAPI wglRealizeLayerPalette(HDC hdc, int iLayerPlane, BOOL bRealize)
{
    return DrvRealizeLayerPalette(hdc, iLayerPlane, bRealize);
}

BOOL WINAPI wglDeleteContext(HGLRC hglrc)
{
    return DrvDeleteContext(hglrc);
}

BOOL WINAPI wglSwapLayerBuffers(HDC hdc, UINT fuPlanes)
{
    return DrvSwapLayerBuffers(hdc, fuPlanes);
}

BOOL WINAPI SwapBuffers(HDC hdc)
{
    return DrvSwapBuffers(hdc);
}

BOOL WINAPI wglCopyContext(HGLRC hglrcSrc, HGLRC hglrcDst, UINT mask)
{
    return DrvCopyContext(hglrcSrc, hglrcDst, mask);
}

BOOL WINAPI wglShareLists(HGLRC hglrc1, HGLRC hglrc2)
{
    return DrvShareLists(hglrc1, hglrc2);
}

BOOL WINAPI wglMakeCurrent(HDC hdc, HGLRC hglrc)
{
    DrvSetContext(hdc, hglrc, NULL);
    return TRUE;
}

PROC WINAPI wglGetProcAddress(LPCSTR lpszProc)
{
    return DrvGetProcAddress(lpszProc);
}

int WINAPI wglDescribePixelFormat(HDC hdc, int iPixelFormat, UINT nBytes, LPPIXELFORMATDESCRIPTOR ppfd)
{
    return DrvDescribePixelFormat(hdc, iPixelFormat, nBytes, ppfd);
}

BOOL WINAPI wglDescribeLayerPlane(HDC hdc,int iPixelFormat,
                                  int iLayerPlane, UINT nBytes,
                                  LPLAYERPLANEDESCRIPTOR plpd)
{
    return DrvDescribeLayerPlane(hdc, iPixelFormat, iLayerPlane, nBytes, plpd);
}

int WINAPI wglGetLayerPaletteEntries(HDC hdc, int iLayerPlane,
                                     int iStart, int cEntries,
                                     COLORREF *pcr)
{
    return DrvGetLayerPaletteEntries(hdc, iLayerPlane, iStart, cEntries, pcr);
}

HGLRC WINAPI wglCreateContext(HDC hdc)
{
    return DrvCreateContext(hdc);
}

#endif
