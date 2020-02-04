/* $Id$ */
/** @file
 * OpenGL testcase. Interface for OpenGL tests.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_ogltest_oglrender_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_ogltest_oglrender_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#ifdef VBOX
#include <iprt/win/windows.h>
#else
#include <windows.h>
#endif
#include <GL/gl.h>
#include <GL/glu.h>
#include <glext.h>

inline void TestShowError(HRESULT hr, const char *pszString)
{
    (void)hr;
    MessageBoxA(0, pszString, 0, 0);
}
/* Expand __LINE__ number to a string. */
#define OGLTEST_S(n) #n
#define OGLTEST_N2S(n) OGLTEST_S(n)

#define GL_CHECK_ERROR() do { if (glGetError() != GL_NO_ERROR) TestShowError((E_FAIL), __FILE__ "@" OGLTEST_N2S(__LINE__)); } while(0)

class OGLRender
{
public:
    OGLRender() {}
    virtual ~OGLRender() {}
    virtual HRESULT InitRender() = 0;
    virtual HRESULT DoRender() = 0;
    virtual void TimeAdvance(float dt) { (void)dt; return; }
};

OGLRender *CreateRender(int iRenderId);
void DeleteRender(OGLRender *pRender);

extern PFNGLBINDBUFFERPROC                             glBindBuffer;
extern PFNGLDELETEBUFFERSPROC                          glDeleteBuffers;
extern PFNGLGENBUFFERSPROC                             glGenBuffers;
extern PFNGLBUFFERDATAPROC                             glBufferData;
extern PFNGLMAPBUFFERPROC                              glMapBuffer;
extern PFNGLUNMAPBUFFERPROC                            glUnmapBuffer;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC                glEnableVertexAttribArray;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC               glDisableVertexAttribArray;
extern PFNGLVERTEXATTRIBPOINTERPROC                    glVertexAttribPointer;
extern PFNGLCREATESHADERPROC                           glCreateShader;
extern PFNGLATTACHSHADERPROC                           glAttachShader;
extern PFNGLCOMPILESHADERPROC                          glCompileShader;
extern PFNGLCREATEPROGRAMPROC                          glCreateProgram;
extern PFNGLDELETEPROGRAMPROC                          glDeleteProgram;
extern PFNGLDELETESHADERPROC                           glDeleteShader;
extern PFNGLDETACHSHADERPROC                           glDetachShader;
extern PFNGLLINKPROGRAMPROC                            glLinkProgram;
extern PFNGLSHADERSOURCEPROC                           glShaderSource;
extern PFNGLUSEPROGRAMPROC                             glUseProgram;
extern PFNGLGETPROGRAMIVPROC                           glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC                      glGetProgramInfoLog;
extern PFNGLGETSHADERIVPROC                            glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC                       glGetShaderInfoLog;
extern PFNGLVERTEXATTRIBDIVISORPROC                    glVertexAttribDivisor;
extern PFNGLDRAWARRAYSINSTANCEDPROC                    glDrawArraysInstanced;

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_ogltest_oglrender_h */
