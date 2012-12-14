/** @file
 * VBoxTray - Display Settings Interface abstraction for XPDM & WDDM
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxTray.h"

#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>

#include <malloc.h>

#ifdef VBOX_WITH_WDDM
#include <iprt/asm.h>
#endif

/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us */
DWORD VBoxDispIfInit(PVBOXDISPIF pIf)
{
    pIf->enmMode = VBOXDISPIF_MODE_XPDM;
    return NO_ERROR;
}

#ifdef VBOX_WITH_WDDM
static void vboxDispIfWddmTerm(PCVBOXDISPIF pIf);
static DWORD vboxDispIfWddmInit(PCVBOXDISPIF pIf);
#endif

DWORD VBoxDispIfTerm(PVBOXDISPIF pIf)
{
#ifdef VBOX_WITH_WDDM
    if (pIf->enmMode == VBOXDISPIF_MODE_WDDM)
    {
        vboxDispIfWddmTerm(pIf);
    }
#endif

    pIf->enmMode = VBOXDISPIF_MODE_UNKNOWN;
    return NO_ERROR;
}

static DWORD vboxDispIfEscapeXPDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData, int iDirection)
{
    HDC  hdc = GetDC(HWND_DESKTOP);
    VOID *pvData = cbData ? VBOXDISPIFESCAPE_DATA(pEscape, VOID) : NULL;
    int iRet = ExtEscape(hdc, pEscape->escapeCode,
            iDirection >= 0 ? cbData : 0,
            iDirection >= 0 ? (LPSTR)pvData : NULL,
            iDirection <= 0 ? cbData : 0,
            iDirection <= 0 ? (LPSTR)pvData : NULL);
    ReleaseDC(HWND_DESKTOP, hdc);
    if (iRet > 0)
        return VINF_SUCCESS;
    else if (iRet == 0)
        return ERROR_NOT_SUPPORTED;
    /* else */
    return ERROR_GEN_FAILURE;
}

#ifdef VBOX_WITH_WDDM
static DWORD vboxDispIfSwitchToWDDM(PVBOXDISPIF pIf)
{
    DWORD err = NO_ERROR;
    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);
    bool bSupported = true;

    if (OSinfo.dwMajorVersion >= 6)
    {
        Log((__FUNCTION__": this is vista and up\n"));
        HMODULE hUser = GetModuleHandle("USER32");
        if (hUser)
        {
            *(uintptr_t *)&pIf->modeData.wddm.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
            Log((__FUNCTION__": VBoxDisplayInit: pfnChangeDisplaySettingsEx = %p\n", pIf->modeData.wddm.pfnChangeDisplaySettingsEx));
            bSupported &= !!(pIf->modeData.wddm.pfnChangeDisplaySettingsEx);

            *(uintptr_t *)&pIf->modeData.wddm.pfnEnumDisplayDevices = (uintptr_t)GetProcAddress(hUser, "EnumDisplayDevicesA");
            Log((__FUNCTION__": VBoxDisplayInit: pfnEnumDisplayDevices = %p\n", pIf->modeData.wddm.pfnEnumDisplayDevices));
            bSupported &= !!(pIf->modeData.wddm.pfnEnumDisplayDevices);

            /* this is vista and up */
            HMODULE hGdi32 = GetModuleHandle("gdi32");
            if (hGdi32 != NULL)
            {
                pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc = (PFND3DKMT_OPENADAPTERFROMHDC)GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromHdc");
                Log((__FUNCTION__"pfnD3DKMTOpenAdapterFromHdc = %p\n", pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc);

                pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName = (PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
                Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName = %p\n", pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName);

                pIf->modeData.wddm.pfnD3DKMTCloseAdapter = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(hGdi32, "D3DKMTCloseAdapter");
                Log((__FUNCTION__": pfnD3DKMTCloseAdapter = %p\n", pIf->modeData.wddm.pfnD3DKMTCloseAdapter));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTCloseAdapter);

                pIf->modeData.wddm.pfnD3DKMTEscape = (PFND3DKMT_ESCAPE)GetProcAddress(hGdi32, "D3DKMTEscape");
                Log((__FUNCTION__": pfnD3DKMTEscape = %p\n", pIf->modeData.wddm.pfnD3DKMTEscape));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTCloseAdapter);

                pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn = (PFND3DKMT_INVALIDATEACTIVEVIDPN)GetProcAddress(hGdi32, "D3DKMTInvalidateActiveVidPn");
                Log((__FUNCTION__": pfnD3DKMTInvalidateActiveVidPn = %p\n", pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn);

                if (!bSupported)
                {
                    Log((__FUNCTION__": one of pfnD3DKMT function pointers failed to initialize\n"));
                    err = ERROR_NOT_SUPPORTED;
                }
            }
            else
            {
                Log((__FUNCTION__": GetModuleHandle(gdi32) failed, err(%d)\n", GetLastError()));
                err = ERROR_NOT_SUPPORTED;
            }

        }
        else
        {
            Log((__FUNCTION__": GetModuleHandle(USER32) failed, err(%d)\n", GetLastError()));
            err = ERROR_NOT_SUPPORTED;
        }
    }
    else
    {
        Log((__FUNCTION__": can not switch to VBOXDISPIF_MODE_WDDM, because os is not Vista or upper\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    if (err == ERROR_SUCCESS)
    {
        err = vboxDispIfWddmInit(pIf);
    }

    return err;
}

static DWORD vboxDispIfWDDMAdpHdcCreate(int iDisplay, HDC *phDc, DISPLAY_DEVICE *pDev)
{
    DWORD winEr = ERROR_INVALID_STATE;
    memset(pDev, 0, sizeof (*pDev));
    pDev->cb = sizeof (*pDev);

    for (int i = 0; ; ++i)
    {
        if (EnumDisplayDevices(NULL, /* LPCTSTR lpDevice */ i, /* DWORD iDevNum */
                pDev, 0 /* DWORD dwFlags*/))
        {
            if (i == iDisplay || (iDisplay < 0 && pDev->StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE))
            {
                HDC hDc = CreateDC(NULL, pDev->DeviceName, NULL, NULL);
                if (hDc)
                {
                    *phDc = hDc;
                    return NO_ERROR;
                }
                else
                {
                    winEr = GetLastError();
                    Log(("CreateDC failed %d", winEr));
                    break;
                }
            }
            Log(("display data no match display(%d): i(%d), flags(%d)", iDisplay, i, pDev->StateFlags));
        }
        else
        {
            winEr = GetLastError();
            Log(("EnumDisplayDevices failed %d", winEr));
            break;
        }
    }

    Log(("vboxDispIfWDDMAdpHdcCreate failure branch %d", winEr));
    return winEr;
}


typedef DECLCALLBACK(BOOLEAN) FNVBOXDISPIFWDDM_ADAPTEROP(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, DISPLAY_DEVICE *pDev, PVOID pContext);
typedef FNVBOXDISPIFWDDM_ADAPTEROP *PFNVBOXDISPIFWDDM_ADAPTEROP;
static DWORD vboxDispIfWDDMAdapterOp(PCVBOXDISPIF pIf, int iDisplay, PFNVBOXDISPIFWDDM_ADAPTEROP pfnOp, PVOID pContext)
{
    D3DKMT_OPENADAPTERFROMHDC OpenAdapterData = {0};
    DISPLAY_DEVICE DDev;
    DWORD err = vboxDispIfWDDMAdpHdcCreate(iDisplay, &OpenAdapterData.hDc, &DDev);
    Assert(err == NO_ERROR);
    if (err == NO_ERROR)
    {
        NTSTATUS Status = pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc(&OpenAdapterData);
        Assert(!Status);
        if (!Status)
        {
            BOOLEAN bCloseAdapter = pfnOp(pIf, OpenAdapterData.hAdapter, &DDev, pContext);

            if (bCloseAdapter)
            {
                D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
                ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
                Status = pIf->modeData.wddm.pfnD3DKMTCloseAdapter(&ClosaAdapterData);
                if (Status)
                {
                    Log((__FUNCTION__": pfnD3DKMTCloseAdapter failed, Status (0x%x)\n", Status));
                }
            }
        }
        else
        {
            Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName failed, Status (0x%x)\n", Status));
            err = ERROR_GEN_FAILURE;
        }

        DeleteDC(OpenAdapterData.hDc);
    }
    else
        Log((__FUNCTION__": vboxDispIfWDDMAdpHdcCreate failed, winEr (%d)\n", err));

    return err;
}

typedef struct
{
    NTSTATUS Status;
    PVBOXDISPIFESCAPE pEscape;
    int cbData;
    D3DDDI_ESCAPEFLAGS EscapeFlags;
} VBOXDISPIFWDDM_ESCAPEOP_CONTEXT, *PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxDispIfEscapeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, DISPLAY_DEVICE *pDev, PVOID pContext)
{
    PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT pCtx = (PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT)pContext;

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = hAdapter;
    //EscapeData.hDevice = NULL;
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags = pCtx->EscapeFlags;
    EscapeData.pPrivateDriverData = pCtx->pEscape;
    EscapeData.PrivateDriverDataSize = VBOXDISPIFESCAPE_SIZE(pCtx->cbData);
    //EscapeData.hContext = NULL;

    pCtx->Status = pIf->modeData.wddm.pfnD3DKMTEscape(&EscapeData);

    return TRUE;
}

static DWORD vboxDispIfEscapeWDDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData, BOOL fHwAccess)
{
    VBOXDISPIFWDDM_ESCAPEOP_CONTEXT Ctx = {0};
    Ctx.pEscape = pEscape;
    Ctx.cbData = cbData;
    if (fHwAccess)
        Ctx.EscapeFlags.HardwareAccess = 1;
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, -1 /* iDisplay, -1 means primary */, vboxDispIfEscapeWDDMOp, &Ctx);
    if (err == NO_ERROR)
    {
        if (!Ctx.Status)
            err = NO_ERROR;
        else
        {
            if (Ctx.Status == 0xC00000BBL) /* not supported */
                err = ERROR_NOT_SUPPORTED;
            else
                err = ERROR_GEN_FAILURE;
            Log((__FUNCTION__": pfnD3DKMTEscape failed, Status (0x%x)\n", Ctx.Status));
        }
    }
    else
        Log((__FUNCTION__": vboxDispIfWDDMAdapterOp failed, err (%d)\n", err));

    return err;
}

typedef struct
{
    NTSTATUS Status;
    VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO Info;
} VBOXDISPIFWDDM_RESIZEOP_CONTEXT, *PVBOXDISPIFWDDM_RESIZEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxDispIfResizeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, DISPLAY_DEVICE *pDev, PVOID pContext)
{
    PVBOXDISPIFWDDM_RESIZEOP_CONTEXT pCtx = (PVBOXDISPIFWDDM_RESIZEOP_CONTEXT)pContext;
    D3DKMT_INVALIDATEACTIVEVIDPN IAVidPnData = {0};
    uint32_t cbData = VBOXWDDM_RECOMMENDVIDPN_SIZE(1);
    PVBOXWDDM_RECOMMENDVIDPN pData = (PVBOXWDDM_RECOMMENDVIDPN)malloc(cbData);
    if (pData)
    {
        memset(pData, 0, cbData);
        pData->cScreenInfos = 1;
        memcpy(&pData->aScreenInfos[0], &pCtx->Info, sizeof (VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO));

        IAVidPnData.hAdapter = hAdapter;
        IAVidPnData.pPrivateDriverData = pData;
        IAVidPnData.PrivateDriverDataSize = cbData;

        pCtx->Status = pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn(&IAVidPnData);
        Assert(!pCtx->Status);
        if (pCtx->Status)
            Log((__FUNCTION__": pfnD3DKMTInvalidateActiveVidPn failed, Status (0x%x)\n", pCtx->Status));

        free(pData);
    }
    else
    {
        Log((__FUNCTION__": malloc failed\n"));
        pCtx->Status = -1;
    }

    return TRUE;
}

static DWORD vboxDispIfResizeWDDM(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    VBOXDISPIFWDDM_RESIZEOP_CONTEXT Ctx = {0};
    Ctx.Info.Id = Id;
    Ctx.Info.Width = Width;
    Ctx.Info.Height = Height;
    Ctx.Info.BitsPerPixel = BitsPerPixel;
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, -1, /* (int)Id - always say -1 to use primary display since the display does not really matter here */
            vboxDispIfResizeWDDMOp, &Ctx);
    if (err == NO_ERROR)
    {
        if (!Ctx.Status)
            err = NO_ERROR;
        else
        {
            if (Ctx.Status == 0xC00000BBL) /* not supported */
                err = ERROR_NOT_SUPPORTED;
            else
                err = ERROR_GEN_FAILURE;
            Log((__FUNCTION__": vboxDispIfResizeWDDMOp failed, Status (0x%x)\n", Ctx.Status));
        }
    }
    else
        Log((__FUNCTION__": vboxDispIfWDDMAdapterOp failed, err (%d)\n", err));

    return err;
}
#endif

DWORD VBoxDispIfEscape(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
        case VBOXDISPIF_MODE_XPDM:
            return vboxDispIfEscapeXPDM(pIf, pEscape, cbData, 1);
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfEscapeWDDM(pIf, pEscape, cbData, TRUE /* BOOL fHwAccess */);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

DWORD VBoxDispIfEscapeInOut(PCVBOXDISPIF const pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
        case VBOXDISPIF_MODE_XPDM:
            return vboxDispIfEscapeXPDM(pIf, pEscape, cbData, 0);
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfEscapeWDDM(pIf, pEscape, cbData, TRUE /* BOOL fHwAccess */);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vboxDispIfResizeXPDM(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    return ERROR_NOT_SUPPORTED;
}

DWORD VBoxDispIfResize(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            return ERROR_NOT_SUPPORTED;
        case VBOXDISPIF_MODE_XPDM:
            return vboxDispIfResizeXPDM(pIf, Id, Width, Height, BitsPerPixel);
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfResizeWDDM(pIf, Id, Width, Height, BitsPerPixel);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}


#ifdef VBOX_WITH_WDDM
/**/
typedef DECLCALLBACK(VOID) FNVBOXSCREENMONRUNNER_CB(void *pvCb);
typedef FNVBOXSCREENMONRUNNER_CB *PFNVBOXSCREENMONRUNNER_CB;

typedef struct VBOXSCREENMON
{
    HANDLE hThread;
    DWORD idThread;
    HANDLE hEvent;
    HWND hWnd;
    PFNVBOXSCREENMONRUNNER_CB pfnCb;
    void *pvCb;
} VBOXSCREENMON, *PVBOXSCREENMON;


static VBOXSCREENMON g_VBoxScreenMon;


#define VBOX_E_INSUFFICIENT_BUFFER HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER)
#define VBOX_E_NOT_SUPPORTED HRESULT_FROM_WIN32(ERROR_NOT_SUPPORTED)


static void vboxScreenMonOnChange()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    pMon->pfnCb(pMon->pvCb);
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
            vboxScreenMonOnChange();
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

#define VBOXSCREENMONWND_NAME "VboxScreenMonWnd"

static HRESULT vboxScreenMonWndCreate(HWND *phWnd)
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

static HRESULT vboxScreenMonWndInit()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    return vboxScreenMonWndCreate(&pMon->hWnd);
}

HRESULT vboxScreenMonWndTerm()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    HRESULT tmpHr = vboxScreenMonWndDestroy(pMon->hWnd);
    Assert(tmpHr == S_OK);

    HINSTANCE hInstance = (HINSTANCE)GetModuleHandle(NULL);
    UnregisterClass(VBOXSCREENMONWND_NAME, hInstance);

    return S_OK;
}

#define WM_VBOXSCREENMON_INIT_QUIT (WM_APP+2)

HRESULT vboxScreenMonRun()
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    MSG Msg;

    HRESULT hr = S_FALSE;

    PeekMessage(&Msg,
            NULL /* HWND hWnd */,
            WM_USER /* UINT wMsgFilterMin */,
            WM_USER /* UINT wMsgFilterMax */,
            PM_NOREMOVE);

    BOOL bCheck = TRUE;

    do
    {
        if (bCheck)
        {
            vboxScreenMonOnChange();

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
            case WM_VBOXSCREENMON_INIT_QUIT:
            case WM_CLOSE:
            {
                PostQuitMessage(0);
                break;
            }
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

static DWORD WINAPI vboxScreenMonRunnerThread(void *pvUser)
{
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;

    BOOL bRc = SetEvent(pMon->hEvent);
    if (!bRc)
    {
        DWORD winErr = GetLastError();
        Log((__FUNCTION__": SetEvent failed, winErr = (%d)", winErr));
        HRESULT tmpHr = HRESULT_FROM_WIN32(winErr);
        Assert(0);
        Assert(tmpHr != S_OK);
    }

    HRESULT hr = vboxScreenMonWndInit();
    Assert(hr == S_OK);
    if (hr == S_OK)
    {
        hr = vboxScreenMonRun();
        Assert(hr == S_OK);

        vboxScreenMonWndTerm();
    }

    return 0;
}

HRESULT VBoxScreenMonInit(PFNVBOXSCREENMONRUNNER_CB pfnCb, void *pvCb)
{
    HRESULT hr = E_FAIL;
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    memset(pMon, 0, sizeof (*pMon));

    pMon->pfnCb = pfnCb;
    pMon->pvCb = pvCb;

    pMon->hEvent = CreateEvent(NULL, /* LPSECURITY_ATTRIBUTES lpEventAttributes*/
            TRUE, /* BOOL bManualReset*/
            FALSE, /* BOOL bInitialState */
            NULL /* LPCTSTR lpName */
          );
    if (pMon->hEvent)
    {
        pMon->hThread = CreateThread(NULL /* LPSECURITY_ATTRIBUTES lpThreadAttributes */,
                                              0 /* SIZE_T dwStackSize */,
                                              vboxScreenMonRunnerThread,
                                              pMon,
                                              0 /* DWORD dwCreationFlags */,
                                              &pMon->idThread);
        if (pMon->hThread)
        {
            DWORD dwResult = WaitForSingleObject(pMon->hEvent, INFINITE);
            if (dwResult == WAIT_OBJECT_0)
                return S_OK;
            else
            {
                Log(("WaitForSingleObject failed!"));
                hr = E_FAIL;
            }
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

VOID VBoxScreenMonTerm()
{
    HRESULT hr;
    PVBOXSCREENMON pMon = &g_VBoxScreenMon;
    if (!pMon->hThread)
        return;

    BOOL bResult = PostThreadMessage(pMon->idThread, WM_VBOXSCREENMON_INIT_QUIT, 0, 0);
    DWORD winErr;
    if (bResult
            || (winErr = GetLastError()) == ERROR_INVALID_THREAD_ID) /* <- could be that the thread is terminated */
    {
        DWORD dwErr = WaitForSingleObject(pMon->hThread, INFINITE);
        if (dwErr == WAIT_OBJECT_0)
        {
            hr = S_OK;
        }
        else
        {
            winErr = GetLastError();
            hr = HRESULT_FROM_WIN32(winErr);
            Assert(0);
        }
    }
    else
    {
        hr = HRESULT_FROM_WIN32(winErr);
        Assert(0);
    }

    CloseHandle(pMon->hThread);
    pMon->hThread = 0;
    CloseHandle(pMon->hEvent);
    pMon->hThread = 0;
}
/**/

typedef struct VBOXDISPIF_WDDM_INTERNAL
{
    PCVBOXDISPIF pIf;

    HANDLE hResizeEvent;
} VBOXDISPIF_WDDM_INTERNAL, *PVBOXDISPIF_WDDM_INTERNAL;

static VBOXDISPIF_WDDM_INTERNAL g_VBoxDispIfWddm;

static BOOL vboxDispIfWddmValidateResize(DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    DISPLAY_DEVICE DisplayDevice;
    int i = 0;
    UINT cMatched = 0;
    DEVMODE CurDevMode, RegDevMode;
    for (int i = 0; ; ++i)
    {
        ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
        DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

        if (!EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
            break;

        Log(("VBoxTray: vboxDispIfValidateResize: [%d(%d)] %s\n", i, cMatched, DisplayDevice.DeviceName));

        BOOL bFetchDevice = FALSE;

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("VBoxTray: vboxDispIfValidateResize: Found primary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("VBoxTray: vboxDispIfValidateResize: Found secondary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }

        if (bFetchDevice)
        {
            if (cMatched >= cDevModes)
            {
                Log(("VBoxTray: vboxDispIfValidateResize: %d >= %d\n", cDevModes, cMatched));
                return FALSE;
            }

            /* First try to get the video mode stored in registry (ENUM_REGISTRY_SETTINGS).
             * A secondary display could be not active at the moment and would not have
             * a current video mode (ENUM_CURRENT_SETTINGS).
             */
            ZeroMemory(&RegDevMode, sizeof(RegDevMode));
            RegDevMode.dmSize = sizeof(DEVMODE);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_REGISTRY_SETTINGS, &RegDevMode))
            {
                Log(("VBoxTray: vboxDispIfValidateResize: EnumDisplaySettings error %d\n", GetLastError ()));
                return FALSE;
            }

            /* with Win8 WDDM Display-only driver, it seems like sometimes we get an auto-resize setting being stored in registry, although current settings do not match */
            ZeroMemory(&CurDevMode, sizeof(CurDevMode));
            CurDevMode.dmSize = sizeof(CurDevMode);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_CURRENT_SETTINGS, &CurDevMode))
            {
                /* ENUM_CURRENT_SETTINGS returns FALSE when the display is not active:
                 * for example a disabled secondary display */
                Log(("VBoxTray: vboxDispIfValidateResize: EnumDisplaySettings(ENUM_CURRENT_SETTINGS) error %d\n", GetLastError ()));
                return FALSE;
            }

            /* No ENUM_REGISTRY_SETTINGS yet. Seen on Vista after installation.
             * Get the current video mode then.
             */
            if (   RegDevMode.dmPelsWidth != 0
                    && RegDevMode.dmPelsHeight == 0)
            {
                if (CurDevMode.dmBitsPerPel != RegDevMode.dmBitsPerPel
                        || CurDevMode.dmPelsWidth != RegDevMode.dmPelsWidth
                        || CurDevMode.dmPelsHeight != RegDevMode.dmPelsHeight
                        || CurDevMode.dmPosition.x != RegDevMode.dmPosition.x
                        || CurDevMode.dmPosition.y != RegDevMode.dmPosition.y)
                {
                    Log(("VBoxTray: vboxDispIfValidateResize: current settings do not match registry settings, trating as no-match"));
                    return FALSE;
                }
            }

            UINT j = 0;
            for (; j < cDevModes; ++j)
            {
                if (!strncmp(DisplayDevice.DeviceName, paDisplayDevices[j].DeviceName, RT_ELEMENTS(CurDevMode.dmDeviceName)))
                {
                    if (paDeviceModes[j].dmBitsPerPel != CurDevMode.dmBitsPerPel
                            || (paDeviceModes[j].dmPelsWidth & 0xfff8) != (CurDevMode.dmPelsWidth & 0xfff8)
                            || (paDeviceModes[j].dmPelsHeight & 0xfff8) != (CurDevMode.dmPelsHeight & 0xfff8)
                            || (paDeviceModes[j].dmPosition.x & 0xfff8) != (CurDevMode.dmPosition.x & 0xfff8)
                            || (paDeviceModes[j].dmPosition.y & 0xfff8) != (CurDevMode.dmPosition.y & 0xfff8)
                            || (paDisplayDevices[j].StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP) != (DisplayDevice.StateFlags & DISPLAY_DEVICE_ATTACHED_TO_DESKTOP))
                    {
                        return FALSE;
                    }
                    break;
                }
            }

            if (j == cDevModes)
                return FALSE;

            ++cMatched;
        }
    }

    return cMatched == cDevModes;
}

static DWORD vboxDispIfWddmValidateFixResize(PCVBOXDISPIF const pIf, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    if (vboxDispIfWddmValidateResize(paDisplayDevices, paDeviceModes, cDevModes))
        return NO_ERROR;

    DWORD winEr;
    LONG status = DISP_CHANGE_SUCCESSFUL;

    /* now try to resize in a "regular" way */
    /* Assign the new rectangles to displays. */
    for (UINT i = 0; i < cDevModes; i++)
    {
        /* On Vista one must specify DM_BITSPERPEL.
         * Note that the current mode dmBitsPerPel is already in the DEVMODE structure.
         */
        paDeviceModes[i].dmFields = DM_POSITION | DM_PELSHEIGHT | DM_PELSWIDTH | DM_BITSPERPEL;

        Log(("VBoxTray: ResizeDisplayDevice: pfnChangeDisplaySettingsEx %x: %dx%dx%d at %d,%d\n",
                pIf->modeData.wddm.pfnChangeDisplaySettingsEx,
              paDeviceModes[i].dmPelsWidth,
              paDeviceModes[i].dmPelsHeight,
              paDeviceModes[i].dmBitsPerPel,
              paDeviceModes[i].dmPosition.x,
              paDeviceModes[i].dmPosition.y));

        /* the miniport might have been adjusted the display mode stuff,
         * adjust the paDeviceModes[i] by picking the closest available one */
//        DEVMODE AdjustedMode = paDeviceModes[i];
//        vboxDispIfAdjustMode(&paDisplayDevices[i], &AdjustedMode);

        LONG tmpStatus = pIf->modeData.wddm.pfnChangeDisplaySettingsEx((LPSTR)paDisplayDevices[i].DeviceName,
                                        &paDeviceModes[i], NULL, CDS_NORESET | CDS_UPDATEREGISTRY, NULL);
        Log(("VBoxTray: ResizeDisplayDevice: ChangeDisplaySettingsEx position status %d, err %d\n", tmpStatus, GetLastError ()));

        if (tmpStatus != DISP_CHANGE_SUCCESSFUL)
        {
            status = tmpStatus;
        }
    }

    /* A second call to ChangeDisplaySettings updates the monitor. */
    LONG tmpStatus = pIf->modeData.wddm.pfnChangeDisplaySettingsEx(NULL, NULL, NULL, 0, NULL);
    Log(("VBoxTray: ResizeDisplayDevice: ChangeDisplaySettings update status %d\n", status));
    if (tmpStatus == DISP_CHANGE_SUCCESSFUL)
    {
        if (status == DISP_CHANGE_SUCCESSFUL)
        {
            return NO_ERROR;
        }
        tmpStatus = status;
    }

    winEr = ERROR_GEN_FAILURE;
    return winEr;
}

static DECLCALLBACK(VOID) vboxDispIfWddmScreenMonCb(void *pvCb)
{
    PVBOXDISPIF_WDDM_INTERNAL pData = (PVBOXDISPIF_WDDM_INTERNAL)pvCb;

    SetEvent(pData->hResizeEvent);
}

static DWORD vboxDispIfWddmInit(PCVBOXDISPIF pIf)
{
    memset(&g_VBoxDispIfWddm, 0, sizeof (g_VBoxDispIfWddm));
    g_VBoxDispIfWddm.pIf = pIf;
    g_VBoxDispIfWddm.hResizeEvent = CreateEvent(NULL,
                FALSE, /* BOOL bManualReset */
                FALSE, /* BOOL bInitialState */
                NULL /* LPCTSTR lpName */
          );
    if (g_VBoxDispIfWddm.hResizeEvent)
    {
        HRESULT hr = VBoxScreenMonInit(vboxDispIfWddmScreenMonCb, &g_VBoxDispIfWddm);
        if (SUCCEEDED(hr))
        {
            /* ensure event is reset */
            WaitForSingleObject(g_VBoxDispIfWddm.hResizeEvent, 0);
            return ERROR_SUCCESS;
        }
        CloseHandle(g_VBoxDispIfWddm.hResizeEvent);
    }
    return ERROR_GEN_FAILURE;
}

static void vboxDispIfWddmTerm(PCVBOXDISPIF pIf)
{
    VBoxScreenMonTerm();
    CloseHandle(g_VBoxDispIfWddm.hResizeEvent);
    memset(&g_VBoxDispIfWddm, 0, sizeof (g_VBoxDispIfWddm));
}

static DWORD vboxDispIfReninitModesWDDM(PCVBOXDISPIF const pIf, uint8_t *pScreenIdMask, BOOL fReconnectDisplaysOnChange)
{
    VBOXDISPIFESCAPE_REINITVIDEOMODESBYMASK Data = {0};
    Data.EscapeHdr.escapeCode = VBOXESC_REINITVIDEOMODESBYMASK;
    if (fReconnectDisplaysOnChange)
        Data.EscapeHdr.u32CmdSpecific = VBOXWDDM_REINITVIDEOMODESBYMASK_F_RECONNECT_DISPLAYS_ON_CHANGE;

    memcpy(Data.ScreenMask, pScreenIdMask, sizeof (Data.ScreenMask));

    DWORD err = vboxDispIfEscapeWDDM(pIf, &Data.EscapeHdr, sizeof (Data) - sizeof (Data.EscapeHdr), fReconnectDisplaysOnChange ? FALSE /* hw access must be false here,
                                                             * otherwise the miniport driver would fail
                                                             * request to prevent a deadlock */
                                                        : TRUE);
    if (err != NO_ERROR)
    {
        Log((__FUNCTION__": VBoxDispIfEscape failed with err (%d)\n", err));
    }
    return err;
}

static DWORD vboxDispIfAdjustMode(DISPLAY_DEVICE *pDisplayDevice, DEVMODE *pDeviceMode)
{
    DEVMODE CurMode;
    DEVMODE BestMatchMode;
    DWORD i = 0;
    int64_t diffWH = INT64_MAX;
    int diffBpp = INT32_MAX;
    for (; ; ++i)
    {
        CurMode.dmSize = sizeof (CurMode);
        CurMode.dmDriverExtra = 0;

        if (!EnumDisplaySettings(pDisplayDevice->DeviceName, i, &CurMode))
            break;

        if (CurMode.dmPelsWidth == pDeviceMode->dmPelsWidth
                && CurMode.dmPelsHeight == pDeviceMode->dmPelsHeight
                && CurMode.dmBitsPerPel == pDeviceMode->dmBitsPerPel)
        {
            Log(("Exact match found"));
            *pDeviceMode = CurMode;
            return NO_ERROR;
        }

        int diffCurrW = RT_ABS((int)(CurMode.dmPelsWidth - pDeviceMode->dmPelsWidth));
        int diffCurrH = RT_ABS((int)(CurMode.dmPelsHeight - pDeviceMode->dmPelsHeight));
        int diffCurrBpp = RT_ABS((int)(CurMode.dmBitsPerPel - pDeviceMode->dmBitsPerPel)
                                - 1 /* <- to make higher bpp take precedence over lower ones */
                                );

        int64_t diffCurrHW = (int64_t)diffCurrW*diffCurrW + (int64_t)diffCurrH*diffCurrH;

        if (i == 0
               || diffCurrHW < diffWH
               || (diffCurrHW == diffWH && diffCurrBpp < diffBpp))
        {
            /* first run */
            BestMatchMode = CurMode;
            diffWH = diffCurrHW;
            diffBpp = diffCurrBpp;
            continue;
        }
    }

    if (i == 0)
    {
        Log(("No modes found!"));
        return NO_ERROR;
    }

    *pDeviceMode = BestMatchMode;
    return NO_ERROR;
}

static DWORD vboxDispIfAdjustModeValues(PCVBOXDISPIF const pIf, DISPLAY_DEVICE *pDisplayDevice, DEVMODE *pDeviceMode)
{
    VBOXDISPIFESCAPE_ADJUSTVIDEOMODES Data = {0};
    Data.EscapeHdr.escapeCode = VBOXESC_REINITVIDEOMODESBYMASK;
    Data.EscapeHdr.u32CmdSpecific = 1;
    Data.aScreenInfos[0].Mode.Id =
    Data.aScreenInfos[0].Mode.Width = pDeviceMode->dmPelsWidth;
    Data.aScreenInfos[0].Mode.Height = pDeviceMode->dmPelsHeight;
    Data.aScreenInfos[0].Mode.BitsPerPixel = pDeviceMode->dmBitsPerPel;
    DWORD err = vboxDispIfEscapeWDDM(pIf, &Data.EscapeHdr, sizeof (Data) - sizeof (Data.EscapeHdr), TRUE);
    if (err != NO_ERROR)
    {
        Log((__FUNCTION__": VBoxDispIfEscape failed with err (%d)\n", err));
    }
    return err;
}

DWORD vboxDispIfResizeModesWDDM(PCVBOXDISPIF const pIf, UINT iChangedMode, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    UINT cbVidPnInfo = VBOXWDDM_RECOMMENDVIDPN_SIZE(cDevModes);
    PVBOXWDDM_RECOMMENDVIDPN pVidPnInfo = (PVBOXWDDM_RECOMMENDVIDPN)alloca(cbVidPnInfo);
    pVidPnInfo->cScreenInfos = cDevModes;
    D3DKMT_HANDLE hAdapter = NULL;
    NTSTATUS Status;
    DWORD winEr = NO_ERROR;
    UINT i = 0;

    for (; i < cDevModes; i++)
    {
        PVBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO pInfo = &pVidPnInfo->aScreenInfos[i];
        D3DKMT_OPENADAPTERFROMHDC OpenAdapterData = {0};
        OpenAdapterData.hDc = CreateDC(NULL, paDisplayDevices[i].DeviceName, NULL, NULL);
        if (!OpenAdapterData.hDc)
        {
            winEr = GetLastError();
            Log(("WARNING: Failed to get dc for display device %s, winEr %d\n", paDisplayDevices[i].DeviceName, winEr));
            break;
        }

        Status = pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc(&OpenAdapterData);
        Assert(!Status);
        if (Status)
        {
            winEr = ERROR_GEN_FAILURE;
            Log(("WARNING: Failed to open adapter from dc, Status 0x%x\n", Status));
            break;
        }

        pInfo->Id = OpenAdapterData.VidPnSourceId;
        pInfo->Width = paDeviceModes[i].dmPelsWidth;
        pInfo->Height = paDeviceModes[i].dmPelsHeight;
        pInfo->BitsPerPixel = paDeviceModes[i].dmBitsPerPel;

        if (!hAdapter)
        {
            hAdapter = OpenAdapterData.hAdapter;
        }
        else
        {
            D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
            ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
            Status = pIf->modeData.wddm.pfnD3DKMTCloseAdapter(&ClosaAdapterData);
            if (Status)
                Log(("WARNING: Failed to close adapter, Status 0x%x\n", Status));
        }
    }

    BOOL fAbleToInvalidateVidPn = FALSE;

    if (winEr == NO_ERROR)
    {
        Assert(hAdapter);

        D3DKMT_INVALIDATEACTIVEVIDPN IAVidPnData = {0};
        IAVidPnData.hAdapter = hAdapter;
        IAVidPnData.pPrivateDriverData = pVidPnInfo;
        IAVidPnData.PrivateDriverDataSize = cbVidPnInfo;

        DWORD winEr = NO_ERROR;
        Status = pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn(&IAVidPnData);
        Assert(!Status);
        if (Status)
        {
            Log((__FUNCTION__": pfnD3DKMTInvalidateActiveVidPn failed, Status (0x%x)\n", Status));
            winEr = ERROR_GEN_FAILURE;
        }
        else
        {
            fAbleToInvalidateVidPn = TRUE;
        }
    }

    if (hAdapter)
    {
        D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
        ClosaAdapterData.hAdapter = hAdapter;
        Status = pIf->modeData.wddm.pfnD3DKMTCloseAdapter(&ClosaAdapterData);
        if (Status)
            Log(("WARNING: Failed to close adapter[2], Status 0x%x\n", Status));
    }

//    for (i = 0; i < cDevModes; i++)
//    {
//        vboxDispIfAdjustMode(&paDisplayDevices[i], &paDeviceModes[i]);
//    }

    if (fAbleToInvalidateVidPn)
    {
        Log(("Invalidating VidPn Worked!\n"));
        winEr = vboxDispIfWddmValidateFixResize(pIf, paDisplayDevices, paDeviceModes, cDevModes);
    }
    else
    {
        Log(("Falling back to monitor mode reinit\n"));
        /* fallback impl needed for display-only driver
         * since D3DKMTInvalidateActiveVidPn is not available for WDDM > 1.0:
         * make the driver invalidate VidPn,
         * which is done by emulating a monitor re-plug currently */
        /* ensure event is reset */
        WaitForSingleObject(g_VBoxDispIfWddm.hResizeEvent, 0);

        uint8_t ScreenMask[VBOXWDDM_SCREENMASK_SIZE] = {0};
        ASMBitSet(ScreenMask, iChangedMode);
        vboxDispIfReninitModesWDDM(pIf, ScreenMask, TRUE);

        for (UINT i = 0; i < 4; ++i)
        {
            WaitForSingleObject(g_VBoxDispIfWddm.hResizeEvent, 500);
            winEr = vboxDispIfWddmValidateFixResize(pIf, paDisplayDevices, paDeviceModes, cDevModes);
            if (winEr == NO_ERROR)
                break;
        }

        Assert(winEr == NO_ERROR);
    }

    return winEr;
}
#endif /* VBOX_WITH_WDDM */

DWORD VBoxDispIfResizeModes(PCVBOXDISPIF const pIf, UINT iChangedMode, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            return ERROR_NOT_SUPPORTED;
        case VBOXDISPIF_MODE_XPDM:
            return ERROR_NOT_SUPPORTED;
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfResizeModesWDDM(pIf, iChangedMode, paDisplayDevices, paDeviceModes, cDevModes);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

DWORD VBoxDispIfReninitModes(PCVBOXDISPIF const pIf, uint8_t *pScreenIdMask, BOOL fReconnectDisplaysOnChange)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            return ERROR_NOT_SUPPORTED;
        case VBOXDISPIF_MODE_XPDM:
            return ERROR_NOT_SUPPORTED;
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfReninitModesWDDM(pIf, pScreenIdMask, fReconnectDisplaysOnChange);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vboxDispIfSwitchToXPDM_NT4(PVBOXDISPIF pIf)
{
    return NO_ERROR;
}

static DWORD vboxDispIfSwitchToXPDM(PVBOXDISPIF pIf)
{
    DWORD err = NO_ERROR;
    AssertBreakpoint();
    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);
    if (OSinfo.dwMajorVersion >= 5)
    {
        HMODULE hUser = GetModuleHandle("USER32");
        if (NULL != hUser)
        {
            bool bSupported = true;
            *(uintptr_t *)&pIf->modeData.xpdm.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
            Log((__FUNCTION__": pfnChangeDisplaySettingsEx = %p\n", pIf->modeData.xpdm.pfnChangeDisplaySettingsEx));
            bSupported &= !!(pIf->modeData.xpdm.pfnChangeDisplaySettingsEx);

            if (!bSupported)
            {
                Log((__FUNCTION__": pfnChangeDisplaySettingsEx function pointer failed to initialize\n"));
                err = ERROR_NOT_SUPPORTED;
            }
        }
        else
        {
            Log((__FUNCTION__": failed to get USER32 handle, err (%d)\n", GetLastError()));
            err = ERROR_NOT_SUPPORTED;
        }
    }
    else
    {
        Log((__FUNCTION__": can not switch to VBOXDISPIF_MODE_XPDM, because os is not >= w2k\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    return err;
}

DWORD VBoxDispIfSwitchMode(PVBOXDISPIF pIf, VBOXDISPIF_MODE enmMode, VBOXDISPIF_MODE *penmOldMode)
{
    /* @todo: may need to addd synchronization in case we want to change modes dynamically
     * i.e. currently the mode is supposed to be initialized once on service initialization */
    if (penmOldMode)
        *penmOldMode = pIf->enmMode;

    if (enmMode == pIf->enmMode)
        return NO_ERROR;

#ifdef VBOX_WITH_WDDM
    if (pIf->enmMode == VBOXDISPIF_MODE_WDDM)
    {
        vboxDispIfWddmTerm(pIf);
    }
#endif

    DWORD err = NO_ERROR;
    switch (enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_XPDM_NT4\n"));
            err = vboxDispIfSwitchToXPDM_NT4(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to XPDM_NT4 mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_XPDM_NT4;
            }
            else
                Log((__FUNCTION__": failed to switch to XPDM_NT4 mode, err (%d)\n", err));
            break;
        case VBOXDISPIF_MODE_XPDM:
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_XPDM\n"));
            err = vboxDispIfSwitchToXPDM(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to XPDM mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_XPDM;
            }
            else
                Log((__FUNCTION__": failed to switch to XPDM mode, err (%d)\n", err));
            break;
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
        {
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_WDDM\n"));
            err = vboxDispIfSwitchToWDDM(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to WDDM mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_WDDM;
            }
            else
                Log((__FUNCTION__": failed to switch to WDDM mode, err (%d)\n", err));
            break;
        }
#endif
        default:
            err = ERROR_INVALID_PARAMETER;
            break;
    }
    return err;
}
