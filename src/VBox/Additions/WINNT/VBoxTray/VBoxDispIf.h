/** @file
 * VBoxTray - Display Settings Interface abstraction for XPDM & WDDM
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include <iprt/cdefs.h>

#ifdef VBOX_WITH_WDDM
# define D3DKMDT_SPECIAL_MULTIPLATFORM_TOOL
# include <d3dkmthk.h>
#endif

#include <VBoxDisplay.h>

typedef enum
{
    VBOXDISPIF_MODE_UNKNOWN  = 0,
    VBOXDISPIF_MODE_XPDM_NT4 = 1,
    VBOXDISPIF_MODE_XPDM
#ifdef VBOX_WITH_WDDM
    , VBOXDISPIF_MODE_WDDM
#endif
} VBOXDISPIF_MODE;
/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us
 * NOTE: DispIf makes no checks whether the display driver is actually a VBox driver,
 * it just switches between using different backend OS API based on the VBoxDispIfSwitchMode call
 * It's caller's responsibility to initiate it to work in the correct mode */
typedef struct VBOXDISPIF
{
    VBOXDISPIF_MODE enmMode;
    /* with WDDM the approach is to call into WDDM miniport driver via PFND3DKMT API provided by the GDI,
     * The PFND3DKMT is supposed to be used by the OpenGL ICD according to MSDN, so this approach is a bit hacky */
    union
    {
        struct
        {
            LONG (WINAPI * pfnChangeDisplaySettingsEx)(LPCSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
        } xpdm;
#ifdef VBOX_WITH_WDDM
        struct
        {
            /* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
            LONG (WINAPI * pfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);

            /* EnumDisplayDevices does not exist in NT. isVBoxDisplayDriverActive et al. are using these functions. */
            BOOL (WINAPI * pfnEnumDisplayDevices)(IN LPCSTR lpDevice, IN DWORD iDevNum, OUT PDISPLAY_DEVICEA lpDisplayDevice, IN DWORD dwFlags);

            /* open adapter */
            PFND3DKMT_OPENADAPTERFROMHDC pfnD3DKMTOpenAdapterFromHdc;
            PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME pfnD3DKMTOpenAdapterFromGdiDisplayName;
            /* close adapter */
            PFND3DKMT_CLOSEADAPTER pfnD3DKMTCloseAdapter;
            /* escape */
            PFND3DKMT_ESCAPE pfnD3DKMTEscape;
            /* auto resize support */
            PFND3DKMT_INVALIDATEACTIVEVIDPN pfnD3DKMTInvalidateActiveVidPn;
            PFND3DKMT_POLLDISPLAYCHILDREN pfnD3DKMTPollDisplayChildren;
        } wddm;
#endif
    } modeData;
} VBOXDISPIF, *PVBOXDISPIF;
typedef const struct VBOXDISPIF *PCVBOXDISPIF;

/* initializes the DispIf
 * Initially the DispIf is configured to work in XPDM mode
 * call VBoxDispIfSwitchMode to switch the mode to WDDM */
DWORD VBoxDispIfInit(PVBOXDISPIF pIf);
DWORD VBoxDispIfSwitchMode(PVBOXDISPIF pIf, VBOXDISPIF_MODE enmMode, VBOXDISPIF_MODE *penmOldMode);
DECLINLINE(VBOXDISPIF_MODE) VBoxDispGetMode(PVBOXDISPIF pIf) { return pIf->enmMode; }
DWORD VBoxDispIfTerm(PVBOXDISPIF pIf);
DWORD VBoxDispIfEscape(PCVBOXDISPIF const pIf, PVBOXDISPIFESCAPE pEscape, int cbData);
DWORD VBoxDispIfEscapeInOut(PCVBOXDISPIF const pIf, PVBOXDISPIFESCAPE pEscape, int cbData);
DWORD VBoxDispIfResize(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel);
DWORD VBoxDispIfResizeModes(PCVBOXDISPIF const pIf, UINT iChangedMode, DISPLAY_DEVICE *paDisplayDevices, DEVMODE *paDeviceModes, UINT cDevModes);
DWORD VBoxDispIfCancelPendingResize(PCVBOXDISPIF const pIf);
DWORD vboxDispIfWddmEnableDisplay(PCVBOXDISPIF const pIf, UINT Id, bool fEnabled);
//DWORD VBoxDispIfReninitModes(PCVBOXDISPIF const pIf, uint8_t *pScreenIdMask, BOOL fReconnectDisplaysOnChange);
