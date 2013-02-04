/** @file
 * VirtualBox OpenGL Cocoa Window System implementation
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <OpenGL/OpenGL.h>

#include "renderspu.h"
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/path.h>

GLboolean renderspu_SystemInitVisual(VisualInfo *pVisInfo)
{
    CRASSERT(pVisInfo);

/*    cocoaGLVisualCreate(&pCtxInfo->context);*/

    return GL_TRUE;
}

GLboolean renderspu_SystemCreateContext(VisualInfo *pVisInfo, ContextInfo *pCtxInfo, ContextInfo *pSharedCtxInfo)
{
    CRASSERT(pVisInfo);
    CRASSERT(pCtxInfo);

    pCtxInfo->currentWindow = NULL;

    cocoaGLCtxCreate(&pCtxInfo->context, pVisInfo->visAttribs, pSharedCtxInfo ? pSharedCtxInfo->context : NULL);

    return GL_TRUE;
}

void renderspu_SystemDestroyContext(ContextInfo *pCtxInfo)
{
    if(!pCtxInfo)
        return;

    if(pCtxInfo->context)
    {
        cocoaGLCtxDestroy(pCtxInfo->context);
        pCtxInfo->context = NULL;
    }
}

void renderspuFullscreen(WindowInfo *pWinInfo, GLboolean fFullscreen)
{
    /* Real fullscreen isn't supported by VirtualBox */
}

GLboolean renderspu_SystemVBoxCreateWindow(VisualInfo *pVisInfo, GLboolean fShowIt, WindowInfo *pWinInfo)
{
    CRASSERT(pVisInfo);
    CRASSERT(pWinInfo);

    /* VirtualBox is the only frontend which support 3D right now. */
    char pszName[256];
    if (RTProcGetExecutablePath(pszName, sizeof(pszName)))
        /* Check for VirtualBox and VirtualBoxVM */
        if (RTStrNICmp(RTPathFilename(pszName), "VirtualBox", 10) != 0)
            return GL_FALSE;

    pWinInfo->visual = pVisInfo;
    pWinInfo->window = NULL;
    pWinInfo->nativeWindow = NULL;
    pWinInfo->currentCtx = NULL;

#ifdef __LP64__
    NativeNSViewRef pParentWin = (NativeNSViewRef)render_spu_parent_window_id;
#else /* __LP64__ */
    NativeNSViewRef pParentWin = (NativeNSViewRef)(uint32_t)render_spu_parent_window_id;
#endif /* __LP64__ */

    cocoaViewCreate(&pWinInfo->window, pParentWin, pVisInfo->visAttribs);

    if (fShowIt)
        renderspu_SystemShowWindow(pWinInfo, fShowIt);

    return GL_TRUE;
}

void renderspu_SystemReparentWindow(WindowInfo *pWinInfo)
{
#ifdef __LP64__
    NativeNSViewRef pParentWin = (NativeNSViewRef)render_spu_parent_window_id;
#else /* __LP64__ */
    NativeNSViewRef pParentWin = (NativeNSViewRef)(uint32_t)render_spu_parent_window_id;
#endif /* __LP64__ */
    cocoaViewReparent(pWinInfo->window, pParentWin);
}

void renderspu_SystemDestroyWindow(WindowInfo *pWinInfo)
{
    CRASSERT(pWinInfo);

    cocoaViewDestroy(pWinInfo->window);
}

void renderspu_SystemWindowPosition(WindowInfo *pWinInfo, GLint x, GLint y)
{
    CRASSERT(pWinInfo);

#ifdef __LP64__
    NativeNSViewRef pParentWin = (NativeNSViewRef)render_spu_parent_window_id;
#else /* __LP64__ */
    NativeNSViewRef pParentWin = (NativeNSViewRef)(uint32_t)render_spu_parent_window_id;
#endif /* __LP64__ */

    /*pParentWin is unused in the call, otherwise it might hold incorrect value if for ex. last reparent call was for
      a different screen*/
    cocoaViewSetPosition(pWinInfo->window, pParentWin, x, y);
}

void renderspu_SystemWindowSize(WindowInfo *pWinInfo, GLint w, GLint h)
{
    CRASSERT(pWinInfo);

    cocoaViewSetSize(pWinInfo->window, w, h);
}

void renderspu_SystemGetWindowGeometry(WindowInfo *pWinInfo, GLint *pX, GLint *pY, GLint *pW, GLint *pH)
{
    CRASSERT(pWinInfo);

    cocoaViewGetGeometry(pWinInfo->window, pX, pY, pW, pH);
}

void renderspu_SystemGetMaxWindowSize(WindowInfo *pWinInfo, GLint *pW, GLint *pH)
{
    CRASSERT(pWinInfo);

    *pW = 10000;
    *pH = 10000;
}

void renderspu_SystemShowWindow(WindowInfo *pWinInfo, GLboolean fShowIt)
{
    CRASSERT(pWinInfo);

    cocoaViewShow(pWinInfo->window, fShowIt);
}

void renderspu_SystemMakeCurrent(WindowInfo *pWinInfo, GLint nativeWindow, ContextInfo *pCtxInfo)
{
    CRASSERT(pWinInfo);
    CRASSERT(pCtxInfo);

/*    if(pWinInfo->visual != pCtxInfo->visual)*/
/*        printf ("visual mismatch .....................\n");*/

    cocoaViewMakeCurrentContext(pWinInfo->window, pCtxInfo->context);
}

void renderspu_SystemSwapBuffers(WindowInfo *pWinInfo, GLint flags)
{
    CRASSERT(pWinInfo);

    cocoaViewDisplay(pWinInfo->window);
}

void renderspu_SystemWindowVisibleRegion(WindowInfo *pWinInfo, GLint cRects, GLint* paRects)
{
    CRASSERT(pWinInfo);

    cocoaViewSetVisibleRegion(pWinInfo->window, cRects, paRects);
}

void renderspu_SystemSetRootVisibleRegion(GLint cRects, GLint *paRects)
{
}

void renderspu_SystemWindowApplyVisibleRegion(WindowInfo *pWinInfo)
{
}

void renderspu_SystemFlush()
{
    cocoaFlush();
}

void renderspu_SystemFinish()
{
    cocoaFinish();
}

void renderspu_SystemBindFramebufferEXT(GLenum target, GLuint framebuffer)
{
    cocoaBindFramebufferEXT(target, framebuffer);
}

void renderspu_SystemCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type)
{
    cocoaCopyPixels(x, y, width, height, type);
}

void renderspu_SystemGetIntegerv(GLenum pname, GLint * params)
{
    cocoaGetIntegerv(pname, params);
}

void renderspu_SystemReadBuffer(GLenum mode)
{
    cocoaReadBuffer(mode);
}

void renderspu_SystemDrawBuffer(GLenum mode)
{
    cocoaDrawBuffer(mode);
}

