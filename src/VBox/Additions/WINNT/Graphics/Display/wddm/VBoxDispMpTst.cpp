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

static void vboxDispMpTstLogRect(const char * pPrefix, RECT *pRect, const char * pSuffix)
{
    vboxVDbgDoPrint("%s left(%d), top(%d), right(%d), bottom(%d) %s", pPrefix, pRect->left, pRect->top, pRect->right, pRect->bottom, pSuffix);
}

static DECLCALLBACK(int) vboxDispMpTstThreadProc(RTTHREAD ThreadSelf, void *pvUser)
{
    VBOXDISPMP_REGIONS Regions;

    HRESULT hr;
    do
    {
        hr = vboxDispMpGetRegions(&Regions, INFINITE);
        Assert(hr == S_OK);
        if (hr == S_OK)
        {
            vboxVDbgDoPrint("\n>>>\n");
            HWND hWnd = Regions.hWnd;
            if (Regions.pRegions->fFlags.bAddVisibleRects)
            {
                uint32_t iVisibleRects = 0;
                uint32_t cVidibleRects = Regions.pRegions->RectsInfo.cRects;
                if (Regions.pRegions->fFlags.bPositionRect)
                {
                    iVisibleRects = 1;
                    --cVidibleRects;

                    vboxVDbgDoPrint("hWnd (0x%p), position and/or size changed: ", hWnd);
                    vboxDispMpTstLogRect("", Regions.pRegions->RectsInfo.aRects, "\n");
                }

                vboxVDbgDoPrint("hWnd (0x%p), visibleRects: \n", hWnd);
                for (uint32_t i = iVisibleRects; i < cVidibleRects; ++i)
                {
                    vboxDispMpTstLogRect("", &Regions.pRegions->RectsInfo.aRects[i], "");
                }
            }
            else if (Regions.pRegions->fFlags.bAddHiddenRects)
            {
                vboxVDbgDoPrint("hWnd (0x%p), hiddenRects: \n", hWnd);
                for (uint32_t i = 0; i < Regions.pRegions->RectsInfo.cRects; ++i)
                {
                    vboxDispMpTstLogRect("", &Regions.pRegions->RectsInfo.aRects[i], "");
                }
            }

            vboxVDbgDoPrint("\n<<<\n");
        }
    } while (1);
    return VINF_SUCCESS;
}

HRESULT vboxDispMpTstStart()
{
    HRESULT hr = vboxDispMpEnable();
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        int rc = RTThreadCreate(&g_VBoxDispMpTstThread, vboxDispMpTstThreadProc, NULL, 0,
                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "VBoxDispMpTst");
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            return S_OK;

        hr = vboxDispMpDisable();
        Assert(hr == S_OK);
        hr = E_FAIL;
    }
    return hr;
}

HRESULT vboxDispMpTstStop()
{
    HRESULT hr = vboxDispMpDisable();
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        int rc = RTThreadWaitNoResume(g_VBoxDispMpTstThread, RT_INDEFINITE_WAIT, NULL);
        AssertRC(rc);
    }
    return hr;
}

