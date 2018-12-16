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

#include <VBox/vmm/pdmdev.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/cdefs.h>
#include <iprt/ldr.h>
#include <iprt/log.h>

#ifdef RT_OS_WINDOWS
# define OGLGETPROCADDRESS MyWinGetProcAddress
DECLINLINE(PFNRT) MyWinGetProcAddress(const char *pszSymbol)
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
        PFNRT p = (PFNRT)s_wglGetProcAddress(pszSymbol);
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
# define OGLGETPROCADDRESS MyGLXGetProcAddress
static PFNRT MyGLXGetProcAddress(const char *pszSymbol)
{
    int rc;

    static RTLDRMOD s_hGL = NULL;
    if (s_hGL == NULL)
    {
        static const char s_szLibGL[] = "libGL.so.1";
        rc = RTLdrLoadEx(s_szLibGL, &s_hGL, RTLDRLOAD_FLAGS_GLOBAL | RTLDRLOAD_FLAGS_NO_UNLOAD, NULL);
        if (RT_FAILURE(rc))
        {
            LogRel(("VMSVGA3d: failed to load %s: %Rrc\n", s_szLibGL, rc));
            s_hGL = NULL;
            return NULL;
        }
    }

    typedef PFNRT (* PFNGLXGETPROCADDRESS)(const GLubyte * procName);
    static PFNGLXGETPROCADDRESS s_glXGetProcAddress = NULL;
    if (s_glXGetProcAddress == NULL)
    {
        rc = RTLdrGetSymbol(s_hGL, "glXGetProcAddress", (void **)&s_glXGetProcAddress);
        if (RT_FAILURE(rc))
        {
            LogRel(("VMSVGA3d: failed to get glXGetProcAddress: %Rrc\n", rc));
            s_glXGetProcAddress = NULL;
            return NULL;
        }
    }

    PFNRT p = s_glXGetProcAddress((const GLubyte *)pszSymbol);
    if (RT_VALID_PTR(p))
        return p;

    /* Might be an exported symbol. */
    rc = RTLdrGetSymbol(s_hGL, pszSymbol, (void **)&p);
    if (RT_SUCCESS(rc))
        return p;

    return NULL;
}
#endif

#define GLGETPROC_(ProcName, NameSuffix) do { \
    *(PFNRT *)&pfn_##ProcName = pfnRet = OGLGETPROCADDRESS(#ProcName NameSuffix); \
    if (pfnRet) { /* likely */ } \
    else \
    { \
        AssertLogRelMsg(pfnRet, ("%s missing\n", #ProcName NameSuffix)); \
        return PDMDevHlpVMSetError(pDevIns, VERR_VGA_GL_SYMBOL_NOT_FOUND, RT_SRC_POS, \
                                   "Missing OpenGL symbol '%s'\n", #ProcName NameSuffix); \
    } \
} while(0)

int glLdrInit(PPDMDEVINS pDevIns)
{
    /** @todo r=bird: Perhaps make this template include file driven? See
     * include/VBox/dbus.h, include/VBox/dbus-calls.h and iprt/runtime-loader.h for
     * instance.  Regardless, it would be would be nice if we could move up the
     * RTLdrLoadSystem/dlopen bits and have separate error reporting for those,
     * making use of VERR_VGA_GL_LOAD_FAILURE.  I can look into that, but
     * probably only after the release is out... */

    pfn_glAlphaFunc = 0;
    pfn_glBindTexture = 0;
    pfn_glBlendColor = 0;
    pfn_glBlendEquation = 0;
    pfn_glBlendFunc = 0;
    pfn_glClear = 0;
    pfn_glClearColor = 0;
    pfn_glClearDepth = 0;
    pfn_glClearStencil = 0;
    pfn_glClientActiveTexture = 0;
    pfn_glClipPlane = 0;
    pfn_glColorMask = 0;
    pfn_glColorPointer = 0;
    pfn_glCullFace = 0;
    pfn_glDeleteTextures = 0;
    pfn_glDepthFunc = 0;
    pfn_glDepthMask = 0;
    pfn_glDepthRange = 0;
    pfn_glDisable = 0;
    pfn_glDisableClientState = 0;
    pfn_glDrawArrays = 0;
    pfn_glDrawElements = 0;
    pfn_glEnable = 0;
    pfn_glEnableClientState = 0;
    pfn_glFogf = 0;
    pfn_glFogfv = 0;
    pfn_glFogi = 0;
    pfn_glFrontFace = 0;
    pfn_glGenTextures = 0;
    pfn_glGetBooleanv = 0;
    pfn_glGetError = 0;
    pfn_glGetFloatv = 0;
    pfn_glGetIntegerv = 0;
    pfn_glGetString = 0;
    pfn_glGetTexImage = 0;
    pfn_glLightModelfv = 0;
    pfn_glLightf = 0;
    pfn_glLightfv = 0;
    pfn_glLineWidth = 0;
    pfn_glLoadIdentity = 0;
    pfn_glLoadMatrixf = 0;
    pfn_glMaterialfv = 0;
    pfn_glMatrixMode = 0;
    pfn_glMultMatrixf = 0;
    pfn_glNormalPointer = 0;
    pfn_glPixelStorei = 0;
    pfn_glPointSize = 0;
    pfn_glPolygonMode = 0;
    pfn_glPolygonOffset = 0;
    pfn_glPopAttrib = 0;
    pfn_glPopMatrix = 0;
    pfn_glPushAttrib = 0;
    pfn_glPushMatrix = 0;
    pfn_glScissor = 0;
    pfn_glShadeModel = 0;
    pfn_glStencilFunc = 0;
    pfn_glStencilMask = 0;
    pfn_glStencilOp = 0;
    pfn_glTexCoordPointer = 0;
    pfn_glTexImage2D = 0;
    pfn_glTexParameterf = 0;
    pfn_glTexParameterfv = 0;
    pfn_glTexParameteri = 0;
    pfn_glTexSubImage2D = 0;
    pfn_glVertexPointer = 0;
    pfn_glViewport = 0;
#ifdef RT_OS_WINDOWS
    pfn_wglCreateContext = 0;
    pfn_wglDeleteContext = 0;
    pfn_wglMakeCurrent = 0;
    pfn_wglShareLists = 0;
#elif defined(RT_OS_LINUX)
    pfn_glXQueryVersion = 0;
    pfn_glXChooseVisual = 0;
    pfn_glXCreateContext = 0;
    pfn_glXMakeCurrent = 0;
    pfn_glXDestroyContext = 0;
#endif

    PFNRT pfnRet;
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
#elif defined(RT_OS_LINUX)
    GLGETPROC_(glXQueryVersion, "");
    GLGETPROC_(glXChooseVisual, "");
    GLGETPROC_(glXCreateContext, "");
    GLGETPROC_(glXMakeCurrent, "");
    GLGETPROC_(glXDestroyContext, "");
#endif
    return VINF_SUCCESS;
}

PFNRT glLdrGetProcAddress(const char *pszSymbol)
{
    return OGLGETPROCADDRESS(pszSymbol);
}

int glLdrGetExtFunctions(PPDMDEVINS pDevIns)
{
    PFNRT pfnRet;
    GLGETPROC_(glBlendColor, "");
    GLGETPROC_(glBlendEquation, "");
    GLGETPROC_(glClientActiveTexture, "");
    return VINF_SUCCESS;
}
