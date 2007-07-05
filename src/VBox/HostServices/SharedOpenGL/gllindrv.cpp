/** @file
 *
 * VBox OpenGL
 *
 * Simple buffered OpenGL functions
 *
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
 *
 */

#include "vboxgl.h"
#define LOG_GROUP LOG_GROUP_SHARED_OPENGL
#include <VBox/log.h>

/**
 * Global init of VBox OpenGL for windows
 *
 * @returns VBox error code
 */
int vboxglGlobalInit()
{
    vboxInitOpenGLExtensions();
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
    return VINF_SUCCESS;
}

/* Driver functions */
void vboxglDrvCreateContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvCreateContext, 4);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(uint32_t, cx);
    OGL_PARAM(uint32_t, cy);
    OGL_PARAM(BYTE, cColorBits);
    OGL_PARAM(BYTE, iPixelType);
    OGL_PARAM(BYTE, cDepthBits);

    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvDeleteContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvDeleteContext, 1);
    OGL_PARAM(HGLRC, hglrc);
    /** @todo */
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvSetContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSetContext, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(HGLRC, hglrc);

    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvCopyContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvDeleteContext, 3);
    OGL_PARAM(HGLRC, hglrcSrc);
    OGL_PARAM(HGLRC, hglrcDst);
    OGL_PARAM(UINT,  mask);
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvReleaseContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvReleaseContext, 1);
    OGL_PARAM(HGLRC, hglrc);
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvCreateLayerContext(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvCreateLayerContext, 5);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(uint32_t, cx);
    OGL_PARAM(uint32_t, cy);
    OGL_PARAM(BYTE, cColorBits);
    OGL_PARAM(BYTE, iPixelType);
    OGL_PARAM(BYTE, cDepthBits);
    AssertFailed();
    /** @todo create memory dc with the parameters above */
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvShareLists(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvShareLists, 3);
    OGL_PARAM(HGLRC, hglrc1);
    OGL_PARAM(HGLRC, hglrc2);
    pClient->lastretval = 0; /** @todo */
}


void vboxglDrvRealizeLayerPalette(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvRealizeLayerPalette, 3);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(BOOL, bRealize);
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvSwapLayerBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSwapLayerBuffers, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(UINT, fuPlanes);
    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvSetPixelFormat(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSetPixelFormat, 2);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iPixelFormat);

    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvSwapBuffers(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSwapBuffers, 1);
    OGL_PARAM(HDC, hdc);

    pClient->lastretval = 0; /** @todo */
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

    pClient->lastretval = 0; /** @todo */
}

void vboxglDrvSetLayerPaletteEntries(VBOXOGLCTX *pClient, uint8_t *pCmdBuffer)
{
    OGL_CMD(DrvSetLayerPaletteEntries, 5);
    OGL_PARAM(HDC, hdc);
    OGL_PARAM(int, iLayerPlane);
    OGL_PARAM(int, iStart);
    OGL_PARAM(int, cEntries);
    OGL_MEMPARAM(COLORREF, pcr);
    pClient->lastretval = 0; /** @todo */
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
    pClient->lastretval = 0; /** @todo */
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

    pClient->lastretval = 0; /** @todo */
}

RTUINTPTR vboxDrvIsExtensionAvailable(char *pszExtFunctionName)
{
    return 0;
}

