/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxDispD3DCmn.h"
#include "VBoxScreen.h"

#include <iprt/log.h>


typedef struct VBOXSCREENMONENUM
{
    PVBOXSCREENMON pMon;
    UINT iCur;
    HRESULT hr;
    BOOL bChanged;
} VBOXSCREENMONENUM, *PVBOXSCREENMONENUM;

static VBOXSCREENMON g_VBoxScreenMon;


#define VBOX_E_INSUFFICIENT_BUFFER HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
#define VBOX_E_NOT_SUPPORTED HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)

typedef DECLCALLBACK(BOOLEAN) FNVBOXSCREENMON_ADAPTEROP(PVBOXSCREENMON pMon, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext);
typedef FNVBOXSCREENMON_ADAPTEROP *PFNVBOXSCREENMON_ADAPTEROP;
static HRESULT vboxScreenMonWDDMAdapterOp(PVBOXSCREENMON pMon, LPCWSTR pDevName, PFNVBOXSCREENMON_ADAPTEROP pfnOp, PVOID pContext)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME OpenAdapterData = {0};
    wcsncpy(OpenAdapterData.DeviceName, pDevName, RT_ELEMENTS(OpenAdapterData.DeviceName) - 1 /* the last one is always \0 */);
    HRESULT hr = S_OK;
    NTSTATUS Status = pMon->pfnD3DKMTOpenAdapterFromGdiDisplayName(&OpenAdapterData);
    Assert(!Status);
    if (!Status)
    {
        BOOLEAN bCloseAdapter = pfnOp(pMon, OpenAdapterData.hAdapter, OpenAdapterData.DeviceName, pContext);

        if (bCloseAdapter)
        {
            D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
            ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
            Status = pMon->pfnD3DKMTCloseAdapter(&ClosaAdapterData);
            if (Status)
            {
                Log((__FUNCTION__": pfnD3DKMTCloseAdapter failed, Status (0x%x)\n", Status));
                /* ignore */
                Status = 0;
            }
        }
    }
    else
    {
        Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName failed, Status (0x%x)\n", Status));
        hr = E_FAIL;
    }

    return hr;
}

typedef struct
{
    NTSTATUS Status;
    PVBOXDISPIFESCAPE pEscape;
    int cbData;
} VBOXDISPIFWDDM_ESCAPEOP_CONTEXT, *PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxScreenMonEscapeOp(PVBOXSCREENMON pMon, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext)
{
    PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT pCtx = (PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT)pContext;

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = hAdapter;
    //EscapeData.hDevice = NULL;
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = pCtx->pEscape;
    EscapeData.PrivateDriverDataSize = VBOXDISPIFESCAPE_SIZE(pCtx->cbData);
    //EscapeData.hContext = NULL;

    pCtx->Status = pMon->pfnD3DKMTEscape(&EscapeData);

    return TRUE;
}

static HRESULT vboxScreenMonEscape(PVBOXSCREENMON pMon, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    VBOXDISPIFWDDM_ESCAPEOP_CONTEXT Ctx = {0};
    Ctx.pEscape = pEscape;
    Ctx.cbData = cbData;
    HRESULT hr = vboxScreenMonWDDMAdapterOp(pMon, L"\\\\.\\DISPLAY1", vboxScreenMonEscapeOp, &Ctx);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(!Ctx.Status);
        if (Ctx.Status)
        {
            if (Ctx.Status == 0xC00000BBL) /* not supported */
                hr = VBOX_E_NOT_SUPPORTED;
            else
                hr = E_FAIL;
            Log((__FUNCTION__": pfnD3DKMTEscape failed, Status (0x%x)\n", Ctx.Status));
        }
    }
    else
        Log((__FUNCTION__": vboxScreenMRunnerWDDMAdapterOp failed, hr (0x%x)\n", hr));

    return hr;
}

static BOOL CALLBACK vboxScreenMonEnumProc(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData)
{
    PVBOXSCREENMONENUM pEnum = (PVBOXSCREENMONENUM)dwData;
    PVBOXSCREENMON pMon = pEnum->pMon;
    UINT cbRequired = RT_OFFSETOF(VBOXSCREENLAYOUT, aScreens[pEnum->iCur]);
    if (pEnum->hr == S_OK)
    {
        Assert(pEnum->iCur < VBOX_VIDEO_MAX_SCREENS);
        if (pEnum->iCur < VBOX_VIDEO_MAX_SCREENS)
        {
            MONITORINFOEX minfo;
            minfo.cbSize = sizeof (minfo);
            if (GetMonitorInfo(hMonitor,  (LPMONITORINFO)&minfo))
            {
                D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME OpenAdapterData = {0};
                wcsncpy(OpenAdapterData.DeviceName, minfo.szDevice, RT_ELEMENTS(OpenAdapterData.DeviceName) - 1 /* the last one is always \0 */);
                NTSTATUS Status = pMon->pfnD3DKMTOpenAdapterFromGdiDisplayName(&OpenAdapterData);
                Assert(!Status);
                if (!Status)
                {
                    if (pMon->LoData.ScreenLayout.ScreenLayout.cScreens <= pEnum->iCur
                            || pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].VidPnSourceId != OpenAdapterData.VidPnSourceId
                            || pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].pos.x != lprcMonitor->left
                            || pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].pos.y != lprcMonitor->top)
                    {
                        pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].VidPnSourceId = OpenAdapterData.VidPnSourceId;
                        pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].pos.x = lprcMonitor->left;
                        pMon->LoData.ScreenLayout.ScreenLayout.aScreens[pEnum->iCur].pos.y = lprcMonitor->top;
                        pEnum->bChanged = true;
                    }

                    D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
                    ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
                    HRESULT tmpHr = pMon->pfnD3DKMTCloseAdapter(&ClosaAdapterData);
                    Assert(tmpHr == S_OK);
                }
                else
                {
                    pEnum->hr = E_FAIL;
                }
            }
            else
            {
                DWORD winEr = GetLastError();
                HRESULT hr = HRESULT_FROM_WIN32(winEr);
                Assert(0);
                Assert(hr != S_OK);
                Assert(hr != S_FALSE);
                if (hr == S_OK || hr == S_FALSE)
                    hr = E_FAIL;
                pEnum->hr = hr;
            }

//            D3DKMT_OPENADAPTERFROMHDC Oa;
//            memset(&Oa, 0, sizeof (D3DKMT_OPENADAPTERFROMHDC));
//            Oa.hDc = hdcMonitor;
//            HRESULT hr = pMon->pfnD3DKMTOpenAdapterFromHdc(&Oa);
//            Assert(hr == S_OK);
//            if (hr == S_OK)
//            {
//                if (pEnum->pScreenInfo->cScreens <= pEnum->iCur
//                        || pEnum->pScreenInfo->aScreens[pEnum->iCur].VidPnSourceId != Oa.VidPnSourceId
//                        || pEnum->pScreenInfo->aScreens[pEnum->iCur].pos.x != lprcMonitor->left
//                        || pEnum->pScreenInfo->aScreens[pEnum->iCur].pos.y == lprcMonitor->top)
//                {
//                    pEnum->pScreenInfo->aScreens[pEnum->iCur].VidPnSourceId = Oa.VidPnSourceId;
//                    pEnum->pScreenInfo->aScreens[pEnum->iCur].pos.x = lprcMonitor->left;
//                    pEnum->pScreenInfo->aScreens[pEnum->iCur].pos.y = lprcMonitor->top;
//                    pEnum->bChanged = true;
//                }
//
//                D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
//                ClosaAdapterData.hAdapter = Oa.hAdapter;
//                HRESULT tmpHr = pMon->pfnD3DKMTCloseAdapter(&ClosaAdapterData);
//                Assert(tmpHr == S_OK);
//                pEnum->hr = hr;
//            }
        }
        else
        {
            pEnum->hr = VBOX_E_INSUFFICIENT_BUFFER;
        }
    }

    ++pEnum->iCur;

    return TRUE;
}


//static HRESULT vboxScreenMonCheckUpdateChange(PVBOXSCREENMON pMon)
static HRESULT vboxScreenMonCheckUpdateChange()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    VBOXSCREENMONENUM monEnum = {0};
    monEnum.pMon = pMon;

    BOOL bResult = EnumDisplayMonitors(NULL, NULL, vboxScreenMonEnumProc, (LPARAM)&monEnum);
    if (bResult)
    {
        Assert(monEnum.hr == S_OK);
        if (monEnum.hr == S_OK && monEnum.bChanged)
        {
            Assert(monEnum.iCur);
            pMon->LoData.ScreenLayout.ScreenLayout.cScreens = monEnum.iCur;

            UINT cbSize = RT_OFFSETOF(VBOXSCREENLAYOUT, aScreens[pMon->LoData.ScreenLayout.ScreenLayout.cScreens]);
            HRESULT hr = vboxScreenMonEscape(pMon, &pMon->LoData.ScreenLayout.EscapeHdr, cbSize);
            Assert(hr == S_OK);
            return hr;
        }
        return monEnum.hr;
    }

    DWORD winEr = GetLastError();
    HRESULT hr = HRESULT_FROM_WIN32(winEr);
    Assert(0);
    Assert(hr != S_OK);
    Assert(hr != S_FALSE);
    if (hr == S_OK || hr == S_FALSE)
        hr = E_FAIL;
    return hr;
}

static LRESULT CALLBACK vboxScreenMonWndProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch(uMsg)
    {
        case WM_DISPLAYCHANGE:
        {
            HRESULT hr = vboxScreenMonCheckUpdateChange();
            Assert(hr == S_OK);
        }
        case WM_CLOSE:
            Log((__FUNCTION__": got WM_CLOSE for hwnd(0x%x)", hwnd));
            return 0;
        case WM_DESTROY:
            Log((__FUNCTION__": got WM_DESTROY for hwnd(0x%x)", hwnd));
            return 0;
        case WM_NCHITTEST:
            Log((__FUNCTION__": got WM_NCHITTEST for hwnd(0x%x)\n", hwnd));
            return HTNOWHERE;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define VBOXSCREENMONWND_NAME L"VboxScreenMonWnd"

HRESULT vboxScreenMonWndCreate(HWND *phWnd)
{
    HRESULT hr = S_OK;
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    /* Register the Window Class. */
    WNDCLASS wc;
    if (!GetClassInfo(hInstance, VBOXSCREENMONWND_NAME, &wc))
    {
        wc.style = 0;//CS_OWNDC;
        wc.lpfnWndProc = vboxScreenMonWndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = NULL;
        wc.hCursor = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = VBOXSCREENMONWND_NAME;
        if (!RegisterClass(&wc))
        {
            DWORD winErr = GetLastError();
            Log((__FUNCTION__": RegisterClass failed, winErr(%d)\n", winErr));
            hr = E_FAIL;
        }
    }

    if (hr == S_OK)
    {
        HWND hWnd = CreateWindowEx (WS_EX_TOOLWINDOW,
                                        VBOXSCREENMONWND_NAME, VBOXSCREENMONWND_NAME,
                                        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED,
                                        -100, -100,
                                        10, 10,
                                        NULL, //GetDesktopWindow() /* hWndParent */,
                                        NULL /* hMenu */,
                                        hInstance,
                                        NULL /* lpParam */);
        Assert(hWnd);
        if (hWnd)
        {
            *phWnd = hWnd;
        }
        else
        {
            DWORD winErr = GetLastError();
            Log((__FUNCTION__": CreateWindowEx failed, winErr(%d)\n", winErr));
            hr = E_FAIL;
        }
    }

    return hr;
}

static HRESULT vboxScreenMonWndDestroy(HWND hWnd)
{
    BOOL bResult = DestroyWindow(hWnd);
    if (bResult)
        return S_OK;

    DWORD winErr = GetLastError();
    Log((__FUNCTION__": DestroyWindow failed, winErr(%d) for hWnd(0x%x)\n", winErr, hWnd));
    Assert(0);

    return HRESULT_FROM_WIN32(winErr);
}

//HRESULT vboxScreenMonInit(PVBOXSCREENMON pMon)
HRESULT vboxScreenMonInit()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    HRESULT hr = S_OK;

    memset(pMon, 0, sizeof (VBOXSCREENMON));

    pMon->LoData.ScreenLayout.EscapeHdr.escapeCode = VBOXESC_SCREENLAYOUT;

    pMon->hGdi32 = LoadLibraryW(L"gdi32.dll");
    if (pMon->hGdi32 != NULL)
    {
        bool bSupported = true;
        pMon->pfnD3DKMTOpenAdapterFromHdc = (PFND3DKMT_OPENADAPTERFROMHDC)GetProcAddress(pMon->hGdi32, "D3DKMTOpenAdapterFromHdc");
        Log((__FUNCTION__"pfnD3DKMTOpenAdapterFromHdc = %p\n", pMon->pfnD3DKMTOpenAdapterFromHdc));
        bSupported &= !!(pMon->pfnD3DKMTOpenAdapterFromHdc);

        pMon->pfnD3DKMTOpenAdapterFromGdiDisplayName = (PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)GetProcAddress(pMon->hGdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
        Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName = %p\n", pMon->pfnD3DKMTOpenAdapterFromGdiDisplayName));
        bSupported &= !!(pMon->pfnD3DKMTOpenAdapterFromGdiDisplayName);

        pMon->pfnD3DKMTCloseAdapter = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(pMon->hGdi32, "D3DKMTCloseAdapter");
        Log((__FUNCTION__": pfnD3DKMTCloseAdapter = %p\n", pMon->pfnD3DKMTCloseAdapter));
        bSupported &= !!(pMon->pfnD3DKMTCloseAdapter);

        pMon->pfnD3DKMTEscape = (PFND3DKMT_ESCAPE)GetProcAddress(pMon->hGdi32, "D3DKMTEscape");
        Log((__FUNCTION__": pfnD3DKMTEscape = %p\n", pMon->pfnD3DKMTEscape));
        bSupported &= !!(pMon->pfnD3DKMTEscape);

        Assert(bSupported);
        if (bSupported)
        {
            hr = vboxScreenMonWndCreate(&pMon->hWnd);
            Assert(hr == S_OK);
            if (hr == S_OK)
                return S_OK;
        }
        else
        {
            Log((__FUNCTION__": one of pfnD3DKMT function pointers failed to initialize\n"));
            hr = VBOX_E_NOT_SUPPORTED;
        }

        FreeLibrary(pMon->hGdi32);
    }
    else
    {
        DWORD winEr = GetLastError();
        hr = HRESULT_FROM_WIN32(winEr);
        Assert(0);
        Assert(hr != S_OK);
        Assert(hr != S_FALSE);
        if (hr == S_OK || hr == S_FALSE)
            hr = E_FAIL;
    }

    return hr;
}

//HRESULT vboxScreenMonTerm(PVBOXSCREENMON pMon)
HRESULT vboxScreenMonTerm()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    HRESULT tmpHr = vboxScreenMonWndDestroy(pMon->hWnd);
    Assert(tmpHr == S_OK);

    FreeLibrary(pMon->hGdi32);

    pMon->bInited = FALSE;
    return S_OK;
}

HRESULT vboxScreenMonRun()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    MSG Msg;

    HRESULT hr = S_FALSE;

    if (!pMon->bInited)
    {
        PeekMessage(&Msg,
            NULL /* HWND hWnd */,
            WM_USER /* UINT wMsgFilterMin */,
            WM_USER /* UINT wMsgFilterMax */,
            PM_NOREMOVE);

        pMon->bInited = TRUE;
    }

    BOOL bCheck = TRUE;
    do
    {
        if (bCheck)
        {
            hr = vboxScreenMonCheckUpdateChange();
            Assert(hr == S_OK);

            bCheck = FALSE;
        }

        BOOL bResult = GetMessage(&Msg,
            0 /*HWND hWnd*/,
            0 /*UINT wMsgFilterMin*/,
            0 /*UINT wMsgFilterMax*/
            );

        if(!bResult) /* WM_QUIT was posted */
        {
            hr = S_FALSE;
            break;
        }

        if(bResult == -1) /* error occurred */
        {
            DWORD winEr = GetLastError();
            hr = HRESULT_FROM_WIN32(winEr);
            Assert(0);
            /* just ensure we never return success in this case */
            Assert(hr != S_OK);
            Assert(hr != S_FALSE);
            if (hr == S_OK || hr == S_FALSE)
                hr = E_FAIL;
            break;
        }

        switch (Msg.message)
        {
            case WM_DISPLAYCHANGE:
                bCheck = TRUE;
            default:
                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
                break;
        }
    } while (1);
    return 0;
}

static DWORD WINAPI vboxScreenMRunnerThread(void *pvUser)
{
    PVBOXSCREENMONRUNNER pRunner = (PVBOXSCREENMONRUNNER)pvUser;
    Assert(0);

    BOOL bRc = SetEvent(pRunner->hEvent);
    if (!bRc)
    {
        DWORD winErr = GetLastError();
        Log((__FUNCTION__": SetEvent failed, winErr = (%d)", winErr));
        HRESULT tmpHr = HRESULT_FROM_WIN32(winErr);
        Assert(0);
        Assert(tmpHr != S_OK);
    }

    HRESULT hr = vboxScreenMonInit();
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vboxScreenMonRun();
        Assert(hr == S_OK);

        vboxScreenMonTerm();
    }
    return 0;
}

HRESULT VBoxScreenMRunnerStart(PVBOXSCREENMONRUNNER pMon)
{
    HRESULT hr = E_FAIL;
    memset(pMon, 0, sizeof (VBOXSCREENMONRUNNER));

    pMon->hEvent = CreateEvent(NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes*/
            FALSE, /* BOOL bManualReset*/
            FALSE, /* BOOL bInitialState */
            NULL /* LPCTSTR lpName */
          );
    if (pMon->hEvent)
    {
        pMon->hThread = CreateThread(NULL /* LPSECURITY_ATTRIBUTES lpThreadAttributes */,
                                              0 /* SIZE_T dwStackSize */,
                                              vboxScreenMRunnerThread,
                                              pMon,
                                              0 /* DWORD dwCreationFlags */,
                                              &pMon->idThread);
        if (pMon->hThread)
        {
            Assert(0);
            return S_OK;
        }
        else
        {
            DWORD winErr = GetLastError();
            Log((__FUNCTION__": CreateThread failed, winErr = (%d)", winErr));
            hr = HRESULT_FROM_WIN32(winErr);
            Assert(0);
            Assert(hr != S_OK);
        }
        CloseHandle(pMon->hEvent);
    }
    else
    {
        DWORD winErr = GetLastError();
        Log((__FUNCTION__": CreateEvent failed, winErr = (%d)", winErr));
        hr = HRESULT_FROM_WIN32(winErr);
        Assert(0);
        Assert(hr != S_OK);
    }

    return hr;
}

HRESULT VBoxScreenMRunnerStop(PVBOXSCREENMONRUNNER pMon)
{
    if (!pMon->hThread)
        return S_OK;

    Assert(0);

    HANDLE ahHandles[2];
    ahHandles[0] = pMon->hThread;
    ahHandles[1] = pMon->hEvent;
    DWORD dwResult = WaitForMultipleObjects(2, ahHandles,
      FALSE, /* BOOL bWaitAll */
      INFINITE /* DWORD dwMilliseconds */
    );
    HRESULT hr = E_FAIL;
    if (dwResult == WAIT_OBJECT_0 + 1) /* Event is signaled */
    {
        BOOL bResult = PostThreadMessage(pMon->idThread, WM_QUIT, 0, 0);
        if (bResult)
        {
            DWORD dwErr = WaitForSingleObject(pMon->hThread, INFINITE);
            if (dwErr == WAIT_OBJECT_0)
            {
                hr = S_OK;
            }
            else
            {
                DWORD winErr = GetLastError();
                hr = HRESULT_FROM_WIN32(winErr);
                Assert(0);
            }
        }
        else
        {
            DWORD winErr = GetLastError();
            Assert(winErr != ERROR_SUCCESS);
            if (winErr == ERROR_INVALID_THREAD_ID)
            {
                hr = S_OK;
            }
            else
            {
                hr = HRESULT_FROM_WIN32(winErr);
                Assert(0);
            }
        }
    }
    else if (dwResult == WAIT_OBJECT_0)
    {
        /* thread has terminated already */
        hr = S_OK;
    }
    else
    {
        Assert(0);
    }

    if (hr == S_OK)
    {
        CloseHandle(pMon->hThread);
        pMon->hThread = 0;
        CloseHandle(pMon->hEvent);
        pMon->hThread = 0;
    }

    return hr;
}

