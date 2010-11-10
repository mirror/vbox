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

#include "VBoxDispIf.h"

#include <iprt/log.h>
#include <iprt/err.h>
#include <iprt/assert.h>

/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us */
DWORD VBoxDispIfInit(PVBOXDISPIF pIf)
{
    pIf->enmMode = VBOXDISPIF_MODE_XPDM;
    return NO_ERROR;
}

DWORD VBoxDispIfTerm(PVBOXDISPIF pIf)
{
    pIf->enmMode = VBOXDISPIF_MODE_UNKNOWN;
    return NO_ERROR;
}

static DWORD vboxDispIfEscapeXPDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    HDC  hdc = GetDC(HWND_DESKTOP);
    VOID *pvData = cbData ? VBOXDISPIFESCAPE_DATA(pEscape, VOID) : NULL;
    int iRet = ExtEscape(hdc, pEscape->escapeCode, cbData, (LPCSTR)pvData, 0, NULL);
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
    if (OSinfo.dwMajorVersion >= 6)
    {
        /* this is vista and up */
        Log((__FUNCTION__": this is vista and up\n"));
        HMODULE hGdi32 = GetModuleHandle("gdi32");
        if (hGdi32 != NULL)
        {
            bool bSupported = true;
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
        Log((__FUNCTION__": can not switch to VBOXDISPIF_MODE_WDDM, because os is not Vista or upper\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    return err;
}

typedef DECLCALLBACK(BOOLEAN) FNVBOXDISPIFWDDM_ADAPTEROP(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext);
typedef FNVBOXDISPIFWDDM_ADAPTEROP *PFNVBOXDISPIFWDDM_ADAPTEROP;
static DWORD vboxDispIfWDDMAdapterOp(PCVBOXDISPIF pIf, LPCWSTR pDevName, PFNVBOXDISPIFWDDM_ADAPTEROP pfnOp, PVOID pContext)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME OpenAdapterData = {0};
    wcsncpy(OpenAdapterData.DeviceName, pDevName, RT_ELEMENTS(OpenAdapterData.DeviceName) - 1 /* the last one is always \0 */);
    DWORD err = NO_ERROR;
    NTSTATUS Status = pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName(&OpenAdapterData);
    Assert(!Status);
    if (!Status)
    {
        BOOLEAN bCloseAdapter = pfnOp(pIf, OpenAdapterData.hAdapter, OpenAdapterData.DeviceName, pContext);

        if (bCloseAdapter)
        {
            D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
            ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
            Status = pIf->modeData.wddm.pfnD3DKMTCloseAdapter(&ClosaAdapterData);
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
        err = ERROR_GEN_FAILURE;
    }

    return err;
}

typedef struct
{
    NTSTATUS Status;
    PVBOXDISPIFESCAPE pEscape;
    int cbData;
} VBOXDISPIFWDDM_ESCAPEOP_CONTEXT, *PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxDispIfEscapeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext)
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

    pCtx->Status = pIf->modeData.wddm.pfnD3DKMTEscape(&EscapeData);

    return TRUE;
}

static DWORD vboxDispIfEscapeWDDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    VBOXDISPIFWDDM_ESCAPEOP_CONTEXT Ctx = {0};
    Ctx.pEscape = pEscape;
    Ctx.cbData = cbData;
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, L"\\\\.\\DISPLAY1", vboxDispIfEscapeWDDMOp, &Ctx);
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

DECLCALLBACK(BOOLEAN) vboxDispIfResizeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext)
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
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, L"\\\\.\\DISPLAY1", vboxDispIfResizeWDDMOp, &Ctx);
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
            return vboxDispIfEscapeXPDM(pIf, pEscape, cbData);
#ifdef VBOX_WITH_WDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfEscapeWDDM(pIf, pEscape, cbData);
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
        return VINF_ALREADY_INITIALIZED;

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
