/* $Id$ */
/** @file
 * OpenGL testcase. Interface for OpenGL tests.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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

inline void TestShowError(HRESULT hr, const char *pszString)
{
    (void)hr;
    MessageBoxA(0, pszString, 0, 0);
}

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

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_ogltest_oglrender_h */
