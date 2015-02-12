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

#include <iprt/types.h>
#include <VBox/VBoxCocoa.h>
#include <OpenGL/OpenGL.h>

RT_C_DECLS_BEGIN

#ifndef ___renderspu_cocoa_helper_h
ADD_COCOA_NATIVE_REF(NSView);
ADD_COCOA_NATIVE_REF(NSOpenGLContext);
#endif

#ifdef IN_VMSVGA3D
# define VMSVGA3D_DECL(type)  DECLEXPORT(type)
#else
# define VMSVGA3D_DECL(type)  DECLIMPORT(type)
#endif

VMSVGA3D_DECL(void) vmsvga3dCocoaCreateContext(NativeNSOpenGLContextRef *ppCtx, NativeNSOpenGLContextRef pSharedCtx,
                                               bool fOtherProfile);
VMSVGA3D_DECL(void) vmsvga3dCocoaDestroyContext(NativeNSOpenGLContextRef pCtx);
VMSVGA3D_DECL(void) vmsvga3dCocoaCreateView(NativeNSViewRef *ppView, NativeNSViewRef pParentView);
VMSVGA3D_DECL(void) vmsvga3dCocoaDestroyView(NativeNSViewRef pView);
VMSVGA3D_DECL(void) vmsvga3dCocoaViewSetPosition(NativeNSViewRef pView, NativeNSViewRef pParentView, int x, int y);
VMSVGA3D_DECL(void) vmsvga3dCocoaViewSetSize(NativeNSViewRef pView, int w, int h);
VMSVGA3D_DECL(void) vmsvga3dCocoaViewMakeCurrentContext(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);
VMSVGA3D_DECL(void) vmsvga3dCocoaSwapBuffers(NativeNSViewRef pView, NativeNSOpenGLContextRef pCtx);

RT_C_DECLS_END

#endif /* !__DevVGA_SVGA3d_cocoa_h */

