/* $Id$ */
/** @file
 *
 * VBox extension to Wine D3D
 *
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
#include "config.h"
#include "wined3d_private.h"
#include "vboxext.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d_vbox);

typedef DECLCALLBACK(void) FNVBOXEXTWORKERCB(void *pvUser);
typedef FNVBOXEXTWORKERCB *PFNVBOXEXTWORKERCB;

HRESULT VBoxExtDwSubmitProc(PFNVBOXEXTWORKERCB pfnCb, void *pvCb);

/*******************************/

typedef struct VBOXEXT_WORKER
{
    CRITICAL_SECTION CritSect;

    HANDLE hEvent;

    HANDLE hThread;
    DWORD  idThread;
} VBOXEXT_WORKER, *PVBOXEXT_WORKER;



HRESULT VBoxExtWorkerCreate(PVBOXEXT_WORKER pWorker);
HRESULT VBoxExtWorkerDestroy(PVBOXEXT_WORKER pWorker);
HRESULT VBoxExtWorkerSubmitProc(PVBOXEXT_WORKER pWorker, PFNVBOXEXTWORKERCB pfnCb, void *pvCb);


/*******************************/

typedef struct VBOXEXT_GLOBAL
{
    VBOXEXT_WORKER Worker;
} VBOXEXT_GLOBAL, *PVBOXEXT_GLOBAL;

static VBOXEXT_GLOBAL g_VBoxExtGlobal;

#define WM_VBOXEXT_CALLPROC (WM_APP+1)

typedef struct VBOXEXT_CALLPROC
{
    PFNVBOXEXTWORKERCB pfnCb;
    void *pvCb;
} VBOXEXT_CALLPROC, *PVBOXEXT_CALLPROC;

static DWORD WINAPI vboxExtWorkerThread(void *pvUser)
{
    PVBOXEXT_WORKER pWorker = (PVBOXEXT_WORKER)pvUser;
    MSG Msg;

    PeekMessage(&Msg,
        NULL /* HWND hWnd */,
        WM_USER /* UINT wMsgFilterMin */,
        WM_USER /* UINT wMsgFilterMax */,
        PM_NOREMOVE);
    SetEvent(pWorker->hEvent);

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
            case WM_VBOXEXT_CALLPROC:
            {
                VBOXEXT_CALLPROC* pData = (VBOXEXT_CALLPROC*)Msg.lParam;
                pData->pfnCb(pData->pvCb);
                SetEvent(pWorker->hEvent);
                break;
            }
            default:
                TranslateMessage(&Msg);
                DispatchMessage(&Msg);
        }
    } while (1);
    return 0;
}

HRESULT VBoxExtWorkerCreate(PVBOXEXT_WORKER pWorker)
{
    InitializeCriticalSection(&pWorker->CritSect);
    pWorker->hEvent = CreateEvent(NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes */
            FALSE, /* BOOL bManualReset */
            FALSE, /* BOOL bInitialState */
            NULL /* LPCTSTR lpName */
          );
    if (pWorker->hEvent)
    {
        pWorker->hThread = CreateThread(
                              NULL /* LPSECURITY_ATTRIBUTES lpThreadAttributes */,
                              0 /* SIZE_T dwStackSize */,
                              vboxExtWorkerThread,
                              pWorker,
                              0 /* DWORD dwCreationFlags */,
                              &pWorker->idThread);
        if (pWorker->hThread)
        {
            DWORD dwResult = WaitForSingleObject(pWorker->hEvent, INFINITE);
            if (WAIT_OBJECT_0 == dwResult)
                return S_OK;
            ERR("WaitForSingleObject returned %d\n", dwResult);
        }
        else
        {
            DWORD winErr = GetLastError();
            ERR("CreateThread failed, winErr = (%d)", winErr);
        }

        DeleteCriticalSection(&pWorker->CritSect);
    }
    else
    {
        DWORD winErr = GetLastError();
        ERR("CreateEvent failed, winErr = (%d)", winErr);
    }

    return E_FAIL;
}

HRESULT VBoxExtWorkerDestroy(PVBOXEXT_WORKER pWorker)
{
    BOOL bResult = PostThreadMessage(pWorker->idThread, WM_QUIT, 0, 0);
    DWORD dwErr;
    if (!bResult)
    {
        DWORD winErr = GetLastError();
        ERR("PostThreadMessage failed, winErr = (%d)", winErr);
        return E_FAIL;
    }

    dwErr = WaitForSingleObject(pWorker->hThread, INFINITE);
    if (dwErr != WAIT_OBJECT_0)
    {
        ERR("WaitForSingleObject returned (%d)", dwErr);
        return E_FAIL;
    }

    CloseHandle(pWorker->hEvent);
    DeleteCriticalSection(&pWorker->CritSect);

    return S_OK;
}

static HRESULT vboxExtWorkerSubmit(VBOXEXT_WORKER *pWorker, UINT Msg, LPARAM lParam)
{
    HRESULT hr = E_FAIL;
    BOOL bResult;
    /* need to serialize since vboxExtWorkerThread is using one pWorker->hEvent
     * to signal job completion */
    EnterCriticalSection(&pWorker->CritSect);
    bResult = PostThreadMessage(pWorker->idThread, Msg, 0, lParam);
    if (bResult)
    {
        DWORD dwErr = WaitForSingleObject(pWorker->hEvent, INFINITE);
        if (dwErr == WAIT_OBJECT_0)
        {
            hr = S_OK;
        }
        else
        {
            ERR("WaitForSingleObject returned (%d)", dwErr);
        }
    }
    else
    {
        DWORD winErr = GetLastError();
        ERR("PostThreadMessage failed, winErr = (%d)", winErr);
        return E_FAIL;
    }
    LeaveCriticalSection(&pWorker->CritSect);
    return hr;
}

HRESULT VBoxExtWorkerSubmitProc(PVBOXEXT_WORKER pWorker, PFNVBOXEXTWORKERCB pfnCb, void *pvCb)
{
    VBOXEXT_CALLPROC Ctx;
    Ctx.pfnCb = pfnCb;
    Ctx.pvCb = pvCb;
    return vboxExtWorkerSubmit(pWorker, WM_VBOXEXT_CALLPROC, (LPARAM)&Ctx);
}

static HRESULT vboxExtInit()
{
    HRESULT hr = S_OK;
    memset(&g_VBoxExtGlobal, 0, sizeof (g_VBoxExtGlobal));
    hr = VBoxExtWorkerCreate(&g_VBoxExtGlobal.Worker);
    return hr;
}

static HRESULT vboxExtTerm()
{
    HRESULT hr = VBoxExtWorkerDestroy(&g_VBoxExtGlobal.Worker);
    return hr;
}

static DWORD g_cVBoxExtInits = 0;
HRESULT VBoxExtCheckInit()
{
    HRESULT hr = S_OK;
    if (!g_cVBoxExtInits)
    {
        hr = vboxExtInit();
        if (FAILED(hr))
        {
            ERR("vboxExtInit failed, hr (0x%x)", hr);
            return hr;
        }
    }
    ++g_cVBoxExtInits;
    return S_OK;
}

HRESULT VBoxExtCheckTerm()
{
    HRESULT hr = S_OK;
    if (g_cVBoxExtInits == 1)
    {
        hr = vboxExtTerm();
        if (FAILED(hr))
        {
            ERR("vboxExtTerm failed, hr (0x%x)", hr);
            return hr;
        }
    }
    --g_cVBoxExtInits;
    return S_OK;
}

HRESULT VBoxExtDwSubmitProc(PFNVBOXEXTWORKERCB pfnCb, void *pvCb)
{
    return VBoxExtWorkerSubmitProc(&g_VBoxExtGlobal.Worker, pfnCb, pvCb);
}

typedef struct VBOXEXT_GETDC_CB
{
    HWND hWnd;
    HDC hDC;
} VBOXEXT_GETDC_CB, *PVBOXEXT_GETDC_CB;

static DECLCALLBACK(void) vboxExtGetDCWorker(void *pvUser)
{
    PVBOXEXT_GETDC_CB pData = (PVBOXEXT_GETDC_CB)pvUser;
    pData->hDC = GetDC(pData->hWnd);
}

typedef struct VBOXEXT_RELEASEDC_CB
{
    HWND hWnd;
    HDC hDC;
    int ret;
} VBOXEXT_RELEASEDC_CB, *PVBOXEXT_RELEASEDC_CB;

static DECLCALLBACK(void) vboxExtReleaseDCWorker(void *pvUser)
{
    PVBOXEXT_RELEASEDC_CB pData = (PVBOXEXT_RELEASEDC_CB)pvUser;
    pData->ret = ReleaseDC(pData->hWnd, pData->hDC);
}

HDC VBoxExtGetDC(HWND hWnd)
{
#ifdef VBOX_WINE_WITH_SINGLE_CONTEXT
    HRESULT hr;
    VBOXEXT_GETDC_CB Data = {0};
    Data.hWnd = hWnd;
    Data.hDC = NULL;

    hr = VBoxExtDwSubmitProc(vboxExtGetDCWorker, &Data);
    if (FAILED(hr))
    {
        ERR("VBoxExtDwSubmitProc feiled, hr (0x%x)\n", hr);
        return NULL;
    }

    return Data.hDC;
#else
    return GetDC(hWnd);
#endif
}

int VBoxExtReleaseDC(HWND hWnd, HDC hDC)
{
#ifdef VBOX_WINE_WITH_SINGLE_CONTEXT
    HRESULT hr;
    VBOXEXT_RELEASEDC_CB Data = {0};
    Data.hWnd = hWnd;
    Data.hDC = hDC;
    Data.ret = 0;

    hr = VBoxExtDwSubmitProc(vboxExtReleaseDCWorker, &Data);
    if (FAILED(hr))
    {
        ERR("VBoxExtDwSubmitProc feiled, hr (0x%x)\n", hr);
        return -1;
    }

    return Data.ret;
#else
    return ReleaseDC(hWnd, hDC);
#endif
}

/* window creation API */
static LRESULT CALLBACK vboxExtWndProc(HWND hwnd,
    UINT uMsg,
    WPARAM wParam,
    LPARAM lParam
)
{
    switch(uMsg)
    {
        case WM_CLOSE:
            TRACE("got WM_CLOSE for hwnd(0x%x)", hwnd);
            return 0;
        case WM_DESTROY:
            TRACE("got WM_DESTROY for hwnd(0x%x)", hwnd);
            return 0;
        case WM_NCHITTEST:
            TRACE("got WM_NCHITTEST for hwnd(0x%x)\n", hwnd);
            return HTNOWHERE;
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

#define VBOXEXTWND_NAME "VboxDispD3DWineWnd"

HRESULT vboxExtWndDoCreate(DWORD w, DWORD h, HWND *phWnd)
{
    HRESULT hr = S_OK;
    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    /* Register the Window Class. */
    WNDCLASS wc;
    if (!GetClassInfo(hInstance, VBOXEXTWND_NAME, &wc))
    {
        wc.style = 0;//CS_OWNDC;
        wc.lpfnWndProc = vboxExtWndProc;
        wc.cbClsExtra = 0;
        wc.cbWndExtra = 0;
        wc.hInstance = hInstance;
        wc.hIcon = NULL;
        wc.hCursor = NULL;
        wc.hbrBackground = NULL;
        wc.lpszMenuName = NULL;
        wc.lpszClassName = VBOXEXTWND_NAME;
        if (!RegisterClass(&wc))
        {
            DWORD winErr = GetLastError();
            ERR("RegisterClass failed, winErr(%d)\n", winErr);
            hr = E_FAIL;
        }
    }

    if (hr == S_OK)
    {
        HWND hWnd = CreateWindowEx (WS_EX_TOOLWINDOW,
                                        VBOXEXTWND_NAME, VBOXEXTWND_NAME,
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
            ERR("CreateWindowEx failed, winErr(%d)\n", winErr);
            hr = E_FAIL;
        }
    }

    return hr;
}

static HRESULT vboxExtWndDoDestroy(HWND hWnd)
{
    BOOL bResult = DestroyWindow(hWnd);
    DWORD winErr;
    Assert(bResult);
    if (bResult)
        return S_OK;

    winErr = GetLastError();
    ERR("DestroyWindow failed, winErr(%d) for hWnd(0x%x)\n", winErr, hWnd);

    return E_FAIL;
}

typedef struct VBOXEXTWND_CREATE_INFO
{
    int hr;
    HWND hWnd;
    DWORD width;
    DWORD height;
} VBOXEXTWND_CREATE_INFO;

typedef struct VBOXEXTWND_DESTROY_INFO
{
    int hr;
    HWND hWnd;
} VBOXEXTWND_DESTROY_INFO;

DECLCALLBACK(void) vboxExtWndDestroyWorker(void *pvUser)
{
    VBOXEXTWND_DESTROY_INFO *pInfo = (VBOXEXTWND_DESTROY_INFO*)pvUser;
    pInfo->hr = vboxExtWndDoDestroy(pInfo->hWnd);
    Assert(pInfo->hr == S_OK);
}

DECLCALLBACK(void) vboxExtWndCreateWorker(void *pvUser)
{
    VBOXEXTWND_CREATE_INFO *pInfo = (VBOXEXTWND_CREATE_INFO*)pvUser;
    pInfo->hr = vboxExtWndDoCreate(pInfo->width, pInfo->height, &pInfo->hWnd);
    Assert(pInfo->hr == S_OK);
}

HRESULT VBoxExtWndDestroy(HWND hWnd)
{
    HRESULT hr;
    VBOXEXTWND_DESTROY_INFO Info;
    Info.hr = E_FAIL;
    Info.hWnd = hWnd;
    hr = VBoxExtDwSubmitProc(vboxExtWndDestroyWorker, &Info);
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        Assert(Info.hr == S_OK);
        return Info.hr;
    }
    return hr;
}

HRESULT VBoxExtWndCreate(DWORD width, DWORD height, HWND *phWnd)
{
    HRESULT hr;
    VBOXEXTWND_CREATE_INFO Info;
    Info.hr = E_FAIL;
    Info.width = width;
    Info.height = height;
    hr = VBoxExtDwSubmitProc(vboxExtWndCreateWorker, &Info);
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
