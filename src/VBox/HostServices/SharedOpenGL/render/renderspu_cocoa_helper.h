/* $Id$ */
/** @file
 * VirtualBox OpenGL Cocoa Window System Helper definition
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___renderspu_cocoa_helper_h
#define ___renderspu_cocoa_helper_h

#include <iprt/cdefs.h>
#include <VBox/VBoxCocoa.h>
#include <OpenGL/OpenGL.h>

RT_C_DECLS_BEGIN

ADD_COCOA_NATIVE_REF(NSView);
ADD_COCOA_NATIVE_REF(NSOpenGLContext);

/* OpenGL context management */
void cocoaGLCtxCreate(NativeNSOpenGLContextRef *ppCtx, GLbitfield fVisParams, NativeNSOpenGLContextRef pSharedCtx);
void cocoaGLCtxDestroy(NativeNSOpenGLContextRef pCtx);

/* View management */
void cocoaViewCreate(NativeNSViewRef *ppView, NativeNSViewRef pParentView, GLbitfield fVisParams);
void cocoaViewReparent(NativeNSViewRef pView, NativeNSViewRef pParentView);
void cocoaViewDestroy(NativeNSViewRef pView);
void cocoaViewDisplay(NativeNSViewRef pView);
void cocoaViewShow(NativeNSViewRef pView, GLboolean fShowIt);
void cocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y);
void cocoaViewSetSize(NativeNSViewRef pView, int w, int h);
void cocoaViewGetGeometry(NativeNSViewRef pView, int *pX, int *pY, int *pW, int *pH);

void cocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);
void cocoaViewSetVisibleRegion(NativeNSViewRef pView, GLint cRects, GLint* paRects);

/* OpenGL wrapper */
void cocoaFlush(void);
void cocoaFinish(void);
void cocoaBindFramebufferEXT(GLenum target, GLuint framebuffer);
void cocoaCopyPixels(GLint x, GLint y, GLsizei width, GLsizei height, GLenum type);
void cocoaGetIntegerv(GLenum pname, GLint *params);
void cocoaReadBuffer(GLenum mode);
void cocoaDrawBuffer(GLenum mode);

RT_C_DECLS_END

#endif /* !___renderspu_cocoa_helper_h */

