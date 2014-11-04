/** @file
 * VirtualBox OpenGL Cocoa Window System Helper Implementation.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef __DevVGA_SVGA3d_cocoa_h
#define __DevVGA_SVGA3d_cocoa_h

#include <iprt/cdefs.h>
#include <VBox/VBoxCocoa.h>
#include <OpenGL/OpenGL.h>

RT_C_DECLS_BEGIN

ADD_COCOA_NATIVE_REF(NSView);
ADD_COCOA_NATIVE_REF(NSOpenGLContext);

__attribute__ ((visibility("default"))) void vmsvga3dCocoaCreateContext(NativeNSOpenGLContextRef *ppCtx, NativeNSOpenGLContextRef pShareCtx);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaDestroyContext(NativeNSOpenGLContextRef pCtrx);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaCreateView(NativeNSViewRef *ppView, NativeNSViewRef pParentView);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaDestroyView(NativeNSViewRef pView);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int w, int h);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);
__attribute__ ((visibility("default"))) void vmsvga3dCocoaSwapBuffers(NativeNSOpenGLContextRef pCtx);

RT_C_DECLS_END

#endif /* !__DevVGA_SVGA3d_cocoa_h */
