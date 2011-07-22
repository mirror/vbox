/* $Id$ */

/** @file
 * VBoxVideo Display D3D User mode dll
 */

/*
 * Copyright (C) 2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxDispD3DIf.h"
#include "VBoxDispD3DCmn.h"

#include <iprt/assert.h>

void VBoxDispD3DClose(VBOXDISPD3D *pD3D)
{
    FreeLibrary(pD3D->hD3DLib);
    pD3D->hD3DLib = NULL;
}


HRESULT VBoxDispD3DOpen(VBOXDISPD3D *pD3D)
{
#ifdef VBOX_WDDM_WOW64
    pD3D->hD3DLib = LoadLibraryW(L"VBoxD3D9wddm-x86.dll");
#else
    pD3D->hD3DLib = LoadLibraryW(L"VBoxD3D9wddm.dll");
#endif
    Assert(pD3D->hD3DLib);
    if (pD3D->hD3DLib)
    {
        pD3D->pfnDirect3DCreate9Ex = (PFNVBOXDISPD3DCREATE9EX)GetProcAddress(pD3D->hD3DLib, "Direct3DCreate9Ex");
        Assert(pD3D->pfnDirect3DCreate9Ex);
        if (pD3D->pfnDirect3DCreate9Ex)
        {
            pD3D->pfnVBoxWineExD3DDev9CreateTexture = (PFNVBOXWINEEXD3DDEV9_CREATETEXTURE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9CreateTexture");
            Assert(pD3D->pfnVBoxWineExD3DDev9CreateTexture);
            if (pD3D->pfnVBoxWineExD3DDev9CreateTexture)
            {
                pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture = (PFNVBOXWINEEXD3DDEV9_CREATECUBETEXTURE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9CreateCubeTexture");
                Assert(pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture);
                if (pD3D->pfnVBoxWineExD3DDev9CreateCubeTexture)
                {
                    pD3D->pfnVBoxWineExD3DDev9Flush = (PFNVBOXWINEEXD3DDEV9_FLUSH)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9Flush");
                    Assert(pD3D->pfnVBoxWineExD3DDev9Flush);
                    if (pD3D->pfnVBoxWineExD3DDev9Flush)
                    {
                        pD3D->pfnVBoxWineExD3DDev9Update = (PFNVBOXWINEEXD3DDEV9_UPDATE)GetProcAddress(pD3D->hD3DLib, "VBoxWineExD3DDev9Update");
                        Assert(pD3D->pfnVBoxWineExD3DDev9Update);
                        if (pD3D->pfnVBoxWineExD3DDev9Update)
                        {
                            return S_OK;
                        }
                    }
                }
            }
        }
        else
        {
            DWORD winErr = GetLastError();
            vboxVDbgPrintR((__FUNCTION__": GetProcAddressW (for Direct3DCreate9Ex) failed, winErr = (%d)", winErr));
        }

        VBoxDispD3DClose(pD3D);
    }
    else
    {
        DWORD winErr = GetLastError();
        vboxVDbgPrintR((__FUNCTION__": LoadLibraryW failed, winErr = (%d)", winErr));
    }

    return E_FAIL;
}

#define WM_VBOXDISP_CALLPROC (WM_APP+1)

typedef struct VBOXDISP_CALLPROC
{
    PFNVBOXDISPWORKERCB pfnCb;
    void *pvCb;
} VBOXDISP_CALLPROC;

static DWORD WINAPI vboxDispWorkerThread(void *pvUser)
{
    VBOXDISPWORKER *pWorker = (VBOXDISPWORKER*)pvUser;
    MSG Msg;

    PeekMessage(&Msg,
        NULL /* HWND hWnd */,
        WM_USER /* UINT wMsgFilterMin */,
        WM_USER /* UINT wMsgFilterMax */,
        PM_NOREMOVE);
    RTSemEventSignal(pWorker->hEvent);

    do
    {
        BOOL bResult = GetMessage(&Msg,
            0 /*HWND hWnd*/,
            0 /*UINT wMsgFilterMin*/,
            0 /*UINT wMsgFilterMax*/
            );

        if(!bResult) /* WM_QUIT was posted */
            break;

        Assert(bResult != -1);
        if(bResult == -1) /* error occurred */
            break;

        switch (Msg.message)
        {
            case WM_VBOXDISP_CALLPROC:
            {
                VBOXDISP_CALLPROC* pData = (VBOXDISP_CALLPROC*)Msg.lParam;
                pData->pfnCb(pData->pvCb);
                RTSemEventSignal(pWorker->hEvent);
                break;
            }
            default:
                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
        }
    } while (1);
    return 0;
}

static int vboxDispWorkerSubmit(VBOXDISPWORKER *pWorker, UINT Msg, LPARAM lParam)
{
    /* need to serialize since vboxDispWorkerThread is using one pWorker->hEvent
     * to signal job completion */
    int rc = RTCritSectEnter(&pWorker->CritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        BOOL bResult = PostThreadMessage(pWorker->idThread, Msg, 0, lParam);
        Assert(bResult);
        if (bResult)
        {
            rc = RTSemEventWait(pWorker->hEvent, RT_INDEFINITE_WAIT);
            AssertRC(rc);
        }
        else
            rc = VERR_GENERAL_FAILURE;

        int tmpRc = RTCritSectLeave(&pWorker->CritSect);
        AssertRC(tmpRc);
    }
    return rc;
}

HRESULT VBoxDispWorkerSubmitProc(VBOXDISPWORKER *pWorker, PFNVBOXDISPWORKERCB pfnCb, void *pvCb)
{
    VBOXDISP_CALLPROC Ctx;
    Ctx.pfnCb = pfnCb;
    Ctx.pvCb = pvCb;
    int rc =  vboxDispWorkerSubmit(pWorker, WM_VBOXDISP_CALLPROC, (LPARAM)&Ctx);
    AssertRC(rc);
    return RT_SUCCESS(rc) ? S_OK : E_FAIL;
}

HRESULT VBoxDispWorkerCreate(VBOXDISPWORKER *pWorker)
{
    int rc = RTCritSectInit(&pWorker->CritSect);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pWorker->hEvent);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            pWorker->hThread = CreateThread(
                                  NULL /* LPSECURITY_ATTRIBUTES lpThreadAttributes */,
                                  0 /* SIZE_T dwStackSize */,
                                  vboxDispWorkerThread,
                                  pWorker,
                                  0 /* DWORD dwCreationFlags */,
                                  &pWorker->idThread);
            Assert(pWorker->hThread);
            if (pWorker->hThread)
            {
                rc = RTSemEventWait(pWorker->hEvent, RT_INDEFINITE_WAIT);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                    return S_OK;
                /* destroy thread ? */
            }
            else
            {
                DWORD winErr = GetLastError();
                vboxVDbgPrintR((__FUNCTION__": CreateThread failed, winErr = (%d)", winErr));
                rc = VERR_GENERAL_FAILURE;
            }
            int tmpRc = RTSemEventDestroy(pWorker->hEvent);
            AssertRC(tmpRc);
        }
        int tmpRc = RTCritSectDelete(&pWorker->CritSect);
        AssertRC(tmpRc);
    }
    return E_FAIL;
}

HRESULT VBoxDispWorkerDestroy(VBOXDISPWORKER *pWorker)
{
    int rc = VINF_SUCCESS;
    BOOL bResult = PostThreadMessage(pWorker->idThread, WM_QUIT, 0, 0);
    Assert(bResult);
    if (bResult)
    {
        DWORD dwErr = WaitForSingleObject(pWorker->hThread, INFINITE);
        Assert(dwErr == WAIT_OBJECT_0);
        if (dwErr == WAIT_OBJECT_0)
        {
            rc = RTSemEventDestroy(pWorker->hEvent);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectDelete(&pWorker->CritSect);
                AssertRC(rc);
            }
        }
        else
            rc = VERR_GENERAL_FAILURE;
    }
    else
        rc = VERR_GENERAL_FAILURE;

    return RT_SUCCESS(rc) ? S_OK : E_FAIL;
}

static LRESULT CALLBACK WindowProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch(uMsg)
    {
        case WM_CLOSE:
            vboxVDbgPrint((__FUNCTION__": got WM_CLOSE for hwnd(0x%x)", hwnd));
            return 0;
        case WM_DESTROY:
            vboxVDbgPrint((__FUNCTION__": got WM_DESTROY for hwnd(0x%x)", hwnd));
            return 0;
        case WM_NCHITTEST:
            vboxVDbgPrint((__FUNCTION__": got WM_NCHITTEST for hwnd(0x%x)\n", hwnd));
            return HTNOWHERE;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define VBOXDISPWND_NAME L"VboxDispD3DWindow"

HRESULT vboxDispWndDoCreate(DWORD w, DWORD h, HWND *phWnd)
{
    HRESULT hr = S_OK;
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    /* Register the Window Class. */
    WNDCLASS wc;
    if (!GetClassInfo(hInstance, VBOXDISPWND_NAME, &wc))
    {
        wc.style = 0;//CS_OWNDC;
        wc.lpfnWndProc = WindowProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = NULL;
        wc.hCursor = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = VBOXDISPWND_NAME;
        if (!RegisterClass(&wc))
        {
            DWORD winErr = GetLastError();
            vboxVDbgPrint((__FUNCTION__": RegisterClass failed, winErr(%d)\n", winErr));
            hr = E_FAIL;
        }
    }

    if (hr == S_OK)
    {
        HWND hWnd = CreateWindowEx (WS_EX_TOOLWINDOW,
                                        VBOXDISPWND_NAME, VBOXDISPWND_NAME,
                                        WS_POPUP | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_DISABLED,
                                        0, 0,
                                        w, h,
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
            vboxVDbgPrint((__FUNCTION__": CreateWindowEx failed, winErr(%d)\n", winErr));
            hr = E_FAIL;
        }
    }

    return hr;
}

static HRESULT vboxDispWndDoDestroy(HWND hWnd)
{
    BOOL bResult = DestroyWindow(hWnd);
    Assert(bResult);
    if (bResult)
        return S_OK;

    DWORD winErr = GetLastError();
    vboxVDbgPrint((__FUNCTION__": DestroyWindow failed, winErr(%d) for hWnd(0x%x)\n", winErr, hWnd));

    return E_FAIL;
}

typedef struct VBOXDISPWND_CREATE_INFO
{
    int hr;
    HWND hWnd;
    DWORD width;
    DWORD height;
} VBOXDISPWND_CREATE_INFO;

typedef struct VBOXDISPWND_DESTROY_INFO
{
    int hr;
    HWND hWnd;
} VBOXDISPWND_DESTROY_INFO;

DECLCALLBACK(void) vboxDispWndDestroyWorker(void *pvUser)
{
    VBOXDISPWND_DESTROY_INFO *pInfo = (VBOXDISPWND_DESTROY_INFO*)pvUser;
    pInfo->hr = vboxDispWndDoDestroy(pInfo->hWnd);
    Assert(pInfo->hr == S_OK);
}

DECLCALLBACK(void) vboxDispWndCreateWorker(void *pvUser)
{
    VBOXDISPWND_CREATE_INFO *pInfo = (VBOXDISPWND_CREATE_INFO*)pvUser;
    pInfo->hr = vboxDispWndDoCreate(pInfo->width, pInfo->height, &pInfo->hWnd);
    Assert(pInfo->hr == S_OK);
}


HRESULT VBoxDispWndDestroy(PVBOXWDDMDISP_ADAPTER pAdapter, HWND hWnd)
{
    VBOXDISPWND_DESTROY_INFO Info;
    Info.hr = E_FAIL;
    Info.hWnd = hWnd;
    HRESULT hr = VBoxDispWorkerSubmitProc(&pAdapter->WndWorker, vboxDispWndDestroyWorker, &Info);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(Info.hr == S_OK);
        return Info.hr;
    }
    return hr;
}

HRESULT VBoxDispWndCreate(PVBOXWDDMDISP_ADAPTER pAdapter, DWORD width, DWORD height, HWND *phWnd)
{
    VBOXDISPWND_CREATE_INFO Info;
    Info.hr = E_FAIL;
    Info.width = width;
    Info.height = height;
    HRESULT hr = VBoxDispWorkerSubmitProc(&pAdapter->WndWorker, vboxDispWndCreateWorker, &Info);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(Info.hr == S_OK);
        if (Info.hr == S_OK)
            *phWnd = Info.hWnd;
        return Info.hr;
    }
    return hr;
}
