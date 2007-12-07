/** @file
 * VBox OpenGL windows helper functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBOXGLWIN_H
#define ___VBOXGLWIN_H

#include <iprt/types.h>
#include <iprt/mem.h>

#ifdef RT_OS_WINDOWS
#define VBOX_OGL_DEBUG_WINDOW_OUTPUT
#elif defined(RT_OS_LINUX)

#define GLX_GLXEXT_PROTOTYPES
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
 
#define VBOX_OGL_DEBUG_WINDOW_OUTPUT
#endif
  
#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
#if defined(RT_OS_WINDOWS)
#define VBOX_OGL_GUEST_TO_HOST_HDC(a)       pClient->hdc
#elif defined(RT_OS_LINUX)
#define VBOX_OGL_GUEST_TO_HOST_HDC(a)       pClient->glxContext
#endif
#endif


typedef struct
{
    uint64_t    lastretval;
    uint32_t    ulLastError;

    bool        fHasLastError;

    uint8_t    *pLastParam;
    uint32_t    cbLastParam;

    struct
    {
#ifdef RT_OS_WINDOWS
        HWND        hwnd;
        HDC         hdc;
        HGLRC       hglrc;
#elif defined RT_OS_LINUX
        Display     *dpy;
        Window      win;
        GLXContext  ctx;
        GLXFBConfig *fbConfig;
        XVisualInfo *visinfo;
#endif
    } enable;

#ifdef VBOX_OGL_DEBUG_WINDOW_OUTPUT
#ifdef RT_OS_WINDOWS
    HWND        hwnd;
    HDC         hdc;
#elif defined RT_OS_LINUX
    Display     *dpy;
    Window      xWindow;
    GLXFBConfig actFBConfig;
    int         winWidth, winHeight;
    GLXFBConfig *PixelFormatToFBConfigMapper;
    int         numFBConfigs;
    GLXContext  glxContext;
    PFNGLXCHOOSEFBCONFIGSGIXPROC glxChooseFBConfig;
    PFNGLXGETVISUALFROMFBCONFIGSGIXPROC glxGetVisualFromFBConfig;
    PFNGLXCREATECONTEXTWITHCONFIGSGIXPROC glxCreateNewContext;
#endif
#endif
} VBOXOGLCTX, *PVBOXOGLCTX;


#ifndef RT_OS_WINDOWS

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
typedef void *      LPVOID;
typedef int16_t     SHORT;
typedef uint16_t    USHORT;
typedef int64_t     INT64;
typedef int32_t     INT32;
typedef float       FLOAT; /* ??? */

#define DECLARE_HANDLE(a)  typedef HANDLE a
#define WINAPI

#define PFD_DOUBLEBUFFER   0x00000001
#define PFD_STEREO         0x00000002
#define PFD_DRAW_TO_WINDOW 0x00000004
#define PFD_SUPPORT_OPENGL 0x00000020

#define PFD_TYPE_RGBA       0
#define PFD_TYPE_COLORINDEX 1

#define PFD_MAIN_PLANE 0
#define PFD_OVERLAY_PLANE 1
#define PFD_UNDERLAY_PLANE (-1)

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

int vboxglEnableOpenGL(VBOXOGLCTX *pClient);
int vboxglDisableOpenGL(VBOXOGLCTX *pClient);

RTUINTPTR vboxDrvIsExtensionAvailable(char *pszExtFunctionName);

#endif /* !___VBOXGLWIN_H */
