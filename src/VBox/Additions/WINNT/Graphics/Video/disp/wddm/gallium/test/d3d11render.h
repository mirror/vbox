/* $Id$ */
/** @file
 * Gallium D3D testcase. Interface for D3D11 tests.
 */

/*
 * Copyright (C) 2017-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_test_d3d11render_h
#define GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_test_d3d11render_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <d3d11.h>

#include <iprt/asm.h>
#include <iprt/cdefs.h>
#include <iprt/string.h>

#define D3D_RELEASE(ptr) do { \
    if (ptr)                  \
    {                         \
        (ptr)->Release();     \
        (ptr) = 0;            \
    }                         \
} while (0)

inline void D3DTestShowError(HRESULT hr, const char *pszString)
{
    (void)hr;
    MessageBoxA(0, pszString, 0, 0);
}

/* Expand __LINE__ number to string. */
#define D3DTEST_S(n) #n
#define D3DTEST_N2S(n) D3DTEST_S(n)

#define D3DAssertHR(_hr) do { if (FAILED(_hr)) D3DTestShowError((_hr), __FILE__ "@" D3DTEST_N2S(__LINE__)); } while(0)

#define HTEST(a) do { \
    hr = a; \
    D3DAssertHR(hr); \
} while (0)

class D3D11DeviceProvider
{
public:
    virtual ~D3D11DeviceProvider() {}
    virtual ID3D11Device *Device() = 0;
    virtual ID3D11DeviceContext *ImmediateContext() = 0;
    virtual ID3D11RenderTargetView *RenderTargetView() = 0;
};

class D3D11Render
{
public:
    D3D11Render() {}
    virtual ~D3D11Render() {}
    virtual HRESULT InitRender(D3D11DeviceProvider *pDP) = 0;
    virtual HRESULT DoRender(D3D11DeviceProvider *pDP) = 0;
    virtual void TimeAdvance(float dt) { (void)dt; return; }
};

D3D11Render *CreateRender(int iRenderId);
void DeleteRender(D3D11Render *pRender);

#endif /* !GA_INCLUDED_SRC_WINNT_Graphics_Video_disp_wddm_gallium_test_d3d11render_h */
