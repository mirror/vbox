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

                pIf->modeData.wddm.pfnD3DKMTPollDisplayChildren = (PFND3DKMT_POLLDISPLAYCHILDREN)GetProcAddress(hGdi32, "D3DKMTPollDisplayChildren");
                Log((__FUNCTION__": pfnD3DKMTPollDisplayChildren = %p\n", pIf->modeData.wddm.pfnD3DKMTPollDisplayChildren));
                bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTPollDisplayChildren);

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
typedef struct VBOXDISPIF_WDDM_INTERNAL
{
    PCVBOXDISPIF pIf;
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

static DWORD vboxDispIfWddmInit(PCVBOXDISPIF pIf)
{
    memset(&g_VBoxDispIfWddm, 0, sizeof (g_VBoxDispIfWddm));
    g_VBoxDispIfWddm.pIf = pIf;
    return ERROR_SUCCESS;
}

static void vboxDispIfWddmTerm(PCVBOXDISPIF pIf)
{
    memset(&g_VBoxDispIfWddm, 0, sizeof (g_VBoxDispIfWddm));
}

typedef struct VBOXDISPIF_REINITMODES_OP
{
    VBOXDISPIFESCAPE_REINITVIDEOMODESBYMASK EscData;
} VBOXDISPIF_REINITMODES_OP, *PVBOXDISPIF_REINITMODES_OP;

static DECLCALLBACK(BOOLEAN) vboxDispIfReninitModesWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, DISPLAY_DEVICE *pDev, PVOID pContext)
{
    PVBOXDISPIF_REINITMODES_OP pData = (PVBOXDISPIF_REINITMODES_OP)pContext;
    VBOXDISPIFWDDM_ESCAPEOP_CONTEXT Ctx = {0};
    Ctx.pEscape = &pData->EscData.EscapeHdr;
    Ctx.cbData = sizeof (pData->EscData) - sizeof (pData->EscData.EscapeHdr);
//    Ctx.EscapeFlags.HardwareAccess = 0;
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

    D3DKMT_POLLDISPLAYCHILDREN PollData = {0};
    PollData.hAdapter = hAdapter;
    PollData.NonDestructiveOnly = 1;
    NTSTATUS Status = pIf->modeData.wddm.pfnD3DKMTPollDisplayChildren(&PollData);
    if (Status != 0)
    {
        Log((__FUNCTION__": pfnD3DKMTPollDisplayChildren failed, Status (0x%x)\n", Status));
    }
    return TRUE;
}

static DWORD vboxDispIfReninitModesWDDM(PCVBOXDISPIF const pIf, uint8_t *pScreenIdMask, BOOL fReconnectDisplaysOnChange)
{
    VBOXDISPIF_REINITMODES_OP OpData = {0};
    OpData.EscData.EscapeHdr.escapeCode = VBOXESC_REINITVIDEOMODESBYMASK;
    if (fReconnectDisplaysOnChange)
        OpData.EscData.EscapeHdr.u32CmdSpecific = VBOXWDDM_REINITVIDEOMODESBYMASK_F_RECONNECT_DISPLAYS_ON_CHANGE;

    memcpy(OpData.EscData.ScreenMask, pScreenIdMask, sizeof (OpData.EscData.ScreenMask));

    DWORD err = vboxDispIfWDDMAdapterOp(pIf, -1 /* iDisplay, -1 means primary */, vboxDispIfReninitModesWDDMOp, &OpData);
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
        uint8_t ScreenMask[VBOXWDDM_SCREENMASK_SIZE] = {0};
        ASMBitSet(ScreenMask, iChangedMode);
        vboxDispIfReninitModesWDDM(pIf, ScreenMask, TRUE);

        winEr = vboxDispIfWddmValidateFixResize(pIf, paDisplayDevices, paDeviceModes, cDevModes);

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
