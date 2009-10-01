/** @file
 *
 * VirtualBox OpenGL Cocoa Window System Helper definition
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef __renderspu_cocoa_helper_h
#define __renderspu_cocoa_helper_h

#ifdef __OBJC__
# import <Cocoa/Cocoa.h>
typedef NSView *NativeViewRef;
typedef NSOpenGLContext *NativeGLCtxRef;
#else /* __OBJC__ */
typedef void *NativeViewRef;
typedef void *NativeGLCtxRef;
#endif /* __OBJC__ */

#include <iprt/cdefs.h>
#include <OpenGL/OpenGL.h>

RT_C_DECLS_BEGIN

/* OpenGL context management */
void cocoaGLCtxCreate(NativeGLCtxRef *ppCtx, GLbitfield fVisParams);
void cocoaGLCtxDestroy(NativeGLCtxRef pCtx);

/* View management */
void cocoaViewCreate(NativeViewRef *ppView, NativeViewRef pParentView, GLbitfield fVisParams);
void cocoaViewDestroy(NativeViewRef pView);
void cocoaViewDisplay(NativeViewRef pView);
void cocoaViewShow(NativeViewRef pView, GLboolean fShowIt);
void cocoaViewSetPosition(NativeViewRef pView, NativeViewRef pParentView, int x, int y);
void cocoaViewSetSize(NativeViewRef pView, int w, int h);
void cocoaViewGetGeometry(NativeViewRef pView, int *pX, int *pY, int *pW, int *pH);

void cocoaViewMakeCurrentContext(NativeViewRef pView, NativeGLCtxRef pCtx);
void cocoaViewSetVisibleRegion(NativeViewRef pView, GLint cRects, GLint* paRects);

/* OpenGL wrapper */
void cocoaFlush();
void cocoaFinish();
void cocoaBindFramebufferEXT(GLenum target, GLuint framebuffer);

RT_C_DECLS_END

#endif /* __renderspu_cocoa_helper_h */

