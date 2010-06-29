/** @file
 *
 * VBoxVideo Display D3D User mode dll
 *
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "VBoxDispMp.h"

#include <iprt/thread.h>

#include "VBoxDispD3DCmn.h"

static RTTHREAD g_VBoxDispMpTstThread;
static VBOXDISPMP_CALLBACKS g_VBoxDispMpTstCallbacks;
static HMODULE g_hVBoxDispMpModule;
static PFNVBOXDISPMP_GETCALLBACKS g_pfnVBoxDispMpGetCallbacks;


static void vboxDispMpTstLogRect(const char * pPrefix, RECT *pRect, const char * pSuffix)
{
    vboxVDbgPrint(("%s left(%d), top(%d), right(%d), bottom(%d) %s", pPrefix, pRect->left, pRect->top, pRect->right, pRect->bottom, pSuffix));
}

static DECLCALLBACK(int) vboxDispMpTstThreadProc(RTTHREAD ThreadSelf, void *pvUser)
{
    VBOXDISPMP_REGIONS Regions;

    HRESULT hr = g_VBoxDispMpTstCallbacks.pfnEnableEvents();
    Assert(hr == S_OK);
    if (hr != S_OK)
        return VERR_GENERAL_FAILURE;

    do
    {
        hr = g_VBoxDispMpTstCallbacks.pfnGetRegions(&Regions, INFINITE);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            vboxVDbgPrint(("\n>>>\n"));
            HWND hWnd = Regions.hWnd;
            if (Regions.pRegions->fFlags.bSetVisibleRects)
            {
                uint32_t iVisibleRects = 0;
                uint32_t cVisibleRects = Regions.pRegions->RectsInfo.cRects;
                if (Regions.pRegions->fFlags.bSetViewRect)
                {
                    iVisibleRects = 1;

                    vboxVDbgPrint(("hWnd (0x%p), position and/or size changed: ", hWnd));
                    vboxDispMpTstLogRect("", Regions.pRegions->RectsInfo.aRects, "\n");
                }

                vboxVDbgPrint(("hWnd (0x%p), visibleRects: \n", hWnd));
                for (uint32_t i = iVisibleRects; i < cVisibleRects; ++i)
                {
                    vboxDispMpTstLogRect("", &Regions.pRegions->RectsInfo.aRects[i], "");
                }
            }
            else if (Regions.pRegions->fFlags.bAddHiddenRects)
            {
                vboxVDbgPrint(("hWnd (0x%p), hiddenRects: \n", hWnd));
                for (uint32_t i = 0; i < Regions.pRegions->RectsInfo.cRects; ++i)
                {
                    vboxDispMpTstLogRect("", &Regions.pRegions->RectsInfo.aRects[i], "");
                }
            }

            vboxVDbgPrint(("\n<<<\n"));
        }
    } while (1);

    hr = g_VBoxDispMpTstCallbacks.pfnDisableEvents();
    Assert(hr == S_OK);

    return VINF_SUCCESS;
}

HRESULT vboxDispMpTstStart()
{
    HRESULT hr = E_FAIL;
    g_hVBoxDispMpModule = GetModuleHandleW(L"VBoxDispD3D.dll");
    Assert(g_hVBoxDispMpModule);

    if (g_hVBoxDispMpModule)
    {
        g_pfnVBoxDispMpGetCallbacks = (PFNVBOXDISPMP_GETCALLBACKS)GetProcAddress(g_hVBoxDispMpModule, "VBoxDispMpGetCallbacks");
        Assert(g_pfnVBoxDispMpGetCallbacks);
        if (g_pfnVBoxDispMpGetCallbacks)
        {
            hr = g_pfnVBoxDispMpGetCallbacks(VBOXDISPMP_VERSION, &g_VBoxDispMpTstCallbacks);
            Assert(hr == S_OK);
            if (hr == S_OK)
            {
                int rc = RTThreadCreate(&g_VBoxDispMpTstThread, vboxDispMpTstThreadProc, NULL, 0,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VBoxDispMpTst");
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                    return S_OK;

                hr = E_FAIL;
            }
        }
        FreeLibrary(g_hVBoxDispMpModule);
    }

    return hr;
}

HRESULT vboxDispMpTstStop()
{
    HRESULT hr = g_VBoxDispMpTstCallbacks.pfnDisableEvents();
    Assert(hr == S_OK);
#if 0
    if (hr == S_OK)
    {
        int rc = RTThreadWaitNoResume(g_VBoxDispMpTstThread, RT_INDEFINITE_WAIT, NULL);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            BOOL bResult = FreeLibrary(g_hVBoxDispMpModule);
            Assert(bResult);
#ifdef DEBUG
            if (!bResult)
            {
                DWORD winEr = GetLastError();
                hr = HRESULT_FROM_WIN32(winEr);
            }
#endif
        }
        else
            hr = E_FAIL;
    }
#endif
    return hr;
}

