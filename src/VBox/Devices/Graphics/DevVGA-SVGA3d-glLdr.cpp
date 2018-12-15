/* $Id$ */
/** @file
 * DevVGA - VMWare SVGA device - 3D part, dynamic loading of GL function.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define VMSVGA3D_GL_DEFINE_PFN
#include "DevVGA-SVGA3d-glLdr.h"

#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/err.h>
#include <iprt/ldr.h>
#include <iprt/log.h>

#ifdef RT_OS_WINDOWS
# define OGLGETPROCADDRESS MyWinGetProcAddress
DECLINLINE(PROC) MyWinGetProcAddress(const char *pszSymbol)
{
    int rc;

    static RTLDRMOD s_hOpenGL32 = NULL;
    if (s_hOpenGL32 == NULL)
    {
        rc = RTLdrLoadSystem("opengl32", /* fNoUnload = */ true, &s_hOpenGL32);
        if (RT_FAILURE(rc))
           s_hOpenGL32 = NULL;
    }

    typedef PROC (WINAPI *PFNWGLGETPROCADDRESS)(LPCSTR);
    static PFNWGLGETPROCADDRESS s_wglGetProcAddress = NULL;
    if (s_wglGetProcAddress == NULL)
    {
        if (s_hOpenGL32 != NULL)
        {
            rc = RTLdrGetSymbol(s_hOpenGL32, "wglGetProcAddress", (void **)&s_wglGetProcAddress);
            if (RT_FAILURE(rc))
               s_wglGetProcAddress = NULL;
        }
    }

    if (s_wglGetProcAddress)
    {
        /* Khronos: [on failure] "some implementations will return other values. 1, 2, and 3 are used, as well as -1". */
        PROC p = s_wglGetProcAddress(pszSymbol);
        if (RT_VALID_PTR(p))
            return p;

        /* Might be an exported symbol. */
        rc = RTLdrGetSymbol(s_hOpenGL32, pszSymbol, (void **)&p);
        if (RT_SUCCESS(rc))
            return p;
    }

    return 0;
}

#elif defined(RT_OS_DARWIN)
# include <dlfcn.h>
# define OGLGETPROCADDRESS      MyNSGLGetProcAddress
/** Resolves an OpenGL symbol.  */
static void *MyNSGLGetProcAddress(const char *pszSymbol)
{
    /* Another copy in shaderapi.c. */
    static void *s_pvImage = NULL;
    if (s_pvImage == NULL)
        s_pvImage = dlopen("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL", RTLD_LAZY);
    return s_pvImage ? dlsym(s_pvImage, pszSymbol) : NULL;
}

#else
// # define OGLGETPROCADDRESS(x)   glXGetProcAddress((const GLubyte *)x)
# define OGLGETPROCADDRESS MyGLXGetProcAddress
static void *MyGLXGetProcAddress(const char *pszSymbol)
{
    int rc;

    static RTLDRMOD s_hGL = NULL;
    if (s_hGL == NULL)
    {
        rc = RTLdrLoadSystem("libGL.so", /* fNoUnload = */ true, &s_hGL);
        if (RT_FAILURE(rc))
           s_hGL = NULL;
    }

    typedef void(*)() (* PFNGLXGETPROCADDRESS)(const GLubyte * procName);
    static PFNGLXGETPROCADDRESS s_glXGetProcAddress = NULL;
    if (s_glXGetProcAddress == NULL)
    {
        if (s_hGL != NULL)
        {
            rc = RTLdrGetSymbol(s_hGL, "glXGetProcAddress", (void **)&s_glXGetProcAddress);
            if (RT_FAILURE(rc))
               s_glXGetProcAddress = NULL;
        }
    }

    if (s_glXGetProcAddress)
    {
        return glXGetProcAddress((const GLubyte *)pszSymbol);
    }

    return 0;
}
#endif

#define GLGETPROC_(ProcName, NameSuffix) do { \
    *(void **)&pfn_##ProcName = OGLGETPROCADDRESS(#ProcName NameSuffix); \
    AssertLogRelMsgReturn(pfn_##ProcName, (#ProcName NameSuffix " missing\n"), VERR_NOT_IMPLEMENTED); \
} while(0)

int glLdrInit(void)
{
    GLGETPROC_(glAlphaFunc, "");
    GLGETPROC_(glBindTexture, "");
    GLGETPROC_(glBlendFunc, "");
    GLGETPROC_(glClear, "");
    GLGETPROC_(glClearColor, "");
    GLGETPROC_(glClearDepth, "");
    GLGETPROC_(glClearStencil, "");
    GLGETPROC_(glClipPlane, "");
    GLGETPROC_(glColorMask, "");
    GLGETPROC_(glColorPointer, "");
    GLGETPROC_(glCullFace, "");
    GLGETPROC_(glDeleteTextures, "");
    GLGETPROC_(glDepthFunc, "");
    GLGETPROC_(glDepthMask, "");
    GLGETPROC_(glDepthRange, "");
    GLGETPROC_(glDisable, "");
    GLGETPROC_(glDisableClientState, "");
    GLGETPROC_(glDrawArrays, "");
    GLGETPROC_(glDrawElements, "");
    GLGETPROC_(glEnable, "");
    GLGETPROC_(glEnableClientState, "");
    GLGETPROC_(glFogf, "");
    GLGETPROC_(glFogfv, "");
    GLGETPROC_(glFogi, "");
    GLGETPROC_(glFrontFace, "");
    GLGETPROC_(glGenTextures, "");
    GLGETPROC_(glGetBooleanv, "");
    GLGETPROC_(glGetError, "");
    GLGETPROC_(glGetFloatv, "");
    GLGETPROC_(glGetIntegerv, "");
    GLGETPROC_(glGetString, "");
    GLGETPROC_(glGetTexImage, "");
    GLGETPROC_(glLightModelfv, "");
    GLGETPROC_(glLightf, "");
    GLGETPROC_(glLightfv, "");
    GLGETPROC_(glLineWidth, "");
    GLGETPROC_(glLoadIdentity, "");
    GLGETPROC_(glLoadMatrixf, "");
    GLGETPROC_(glMaterialfv, "");
    GLGETPROC_(glMatrixMode, "");
    GLGETPROC_(glMultMatrixf, "");
    GLGETPROC_(glNormalPointer, "");
    GLGETPROC_(glPixelStorei, "");
    GLGETPROC_(glPointSize, "");
    GLGETPROC_(glPolygonMode, "");
    GLGETPROC_(glPolygonOffset, "");
    GLGETPROC_(glPopAttrib, "");
    GLGETPROC_(glPopMatrix, "");
    GLGETPROC_(glPushAttrib, "");
    GLGETPROC_(glPushMatrix, "");
    GLGETPROC_(glScissor, "");
    GLGETPROC_(glShadeModel, "");
    GLGETPROC_(glStencilFunc, "");
    GLGETPROC_(glStencilMask, "");
    GLGETPROC_(glStencilOp, "");
    GLGETPROC_(glTexCoordPointer, "");
    GLGETPROC_(glTexImage2D, "");
    GLGETPROC_(glTexParameterf, "");
    GLGETPROC_(glTexParameterfv, "");
    GLGETPROC_(glTexParameteri, "");
    GLGETPROC_(glTexSubImage2D, "");
    GLGETPROC_(glVertexPointer, "");
    GLGETPROC_(glViewport, "");
#ifdef RT_OS_WINDOWS
    GLGETPROC_(wglCreateContext, "");
    GLGETPROC_(wglDeleteContext, "");
    GLGETPROC_(wglMakeCurrent, "");
    GLGETPROC_(wglShareLists, "");
#endif
    return VINF_SUCCESS;
}

void *glLdrGetProcAddress(const char *pszSymbol)
{
    return OGLGETPROCADDRESS(pszSymbol);
}
