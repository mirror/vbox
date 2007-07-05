/** @file
 *
 * VBox OpenGL windows helper functions
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

#ifndef __VBOXGLWIN__H
#define __VBOXGLWIN__H

#include <iprt/types.h>

#ifdef __WIN__
#define VBOX_OGL_DEBUG_WINDOW_OUTPUT
#endif

#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
#define VBOX_OGL_GUEST_TO_HOST_HDC(a)       pClient->hdc
#else
#define VBOX_OGL_GUEST_TO_HOST_HDC(a)       a
#endif

typedef struct
{
    uint64_t    lastretval;
    uint32_t    ulLastError;

    bool        fHasLastError;

    uint8_t    *pLastParam;
    uint32_t    cbLastParam;

#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
    HWND        hwnd;
    HDC         hdc;
#endif
} VBOXOGLCTX, *PVBOXOGLCTX;


#ifndef __WIN__

typedef uint32_t    DWORD;
typedef uint16_t    WORD;
typedef uint8_t     BYTE;
typedef BYTE        BOOL;
typedef RTGCUINTPTR HANDLE;
typedef HANDLE      HDC;
typedef HANDLE      HGLRC;
typedef uint32_t    UINT;
typedef uint32_t    COLORREF;
typedef void        VOID;

#define DECLARE_HANDLE(a)  typedef HANDLE a;
#define WINAPI

typedef struct
{
    WORD  nSize;
    WORD  nVersion;
    DWORD dwFlags;
    BYTE  iPixelType;
    BYTE  cColorBits;
    BYTE  cRedBits;
    BYTE  cRedShift;
    BYTE  cGreenBits;
    BYTE  cGreenShift;
    BYTE  cBlueBits;
    BYTE  cBlueShift;
    BYTE  cAlphaBits;
    BYTE  cAlphaShift;
    BYTE  cAccumBits;
    BYTE  cAccumRedBits;
    BYTE  cAccumGreenBits;
    BYTE  cAccumBlueBits;
    BYTE  cAccumAlphaBits;
    BYTE  cDepthBits;
    BYTE  cStencilBits;
    BYTE  cAuxBuffers;
    BYTE  iLayerType;
    BYTE  bReserved;
    DWORD dwLayerMask;
    DWORD dwVisibleMask;
    DWORD dwDamageMask;
} PIXELFORMATDESCRIPTOR, *PPIXELFORMATDESCRIPTOR, *LPPIXELFORMATDESCRIPTOR;

typedef struct
{
    WORD  nSize;
    WORD  nVersion;
    DWORD dwFlags;
    BYTE  iPixelType;
    BYTE  cColorBits;
    BYTE  cRedBits;
    BYTE  cRedShift;
    BYTE  cGreenBits;
    BYTE  cGreenShift;
    BYTE  cBlueBits;
    BYTE  cBlueShift;
    BYTE  cAlphaBits;
    BYTE  cAlphaShift;
    BYTE  cAccumBits;
    BYTE  cAccumRedBits;
    BYTE  cAccumGreenBits;
    BYTE  cAccumBlueBits;
    BYTE  cAccumAlphaBits;
    BYTE  cDepthBits;
    BYTE  cStencilBits;
    BYTE  cAuxBuffers;
    BYTE  iLayerPlane;
    BYTE  bReserved;
    COLORREF crTransparent;
} LAYERPLANEDESCRIPTOR, *PLAYERPLANEDESCRIPTOR, LPLAYERPLANEDESCRIPTOR;

#endif

void vboxglDrvReleaseContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvCreateContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvDeleteContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvCopyContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvSetContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvCreateLayerContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvShareLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvDescribeLayerPlane(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvSetLayerPaletteEntries(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvGetLayerPaletteEntries(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvRealizeLayerPalette(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvSwapLayerBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvDescribePixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvSetPixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);
void vboxglDrvSwapBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer);

#endif /* __VBOXGLWIN__H */
