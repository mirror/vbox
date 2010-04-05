/* $Id$ */
/** @file
 * VBoxTray - Guest Additions Tray, Internal Header.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBOXTRAY_H
#define ___VBOXTRAY_H

#include <windows.h>
#include <tchar.h>
#include <stdio.h>
#include <stdarg.h>
#include <process.h>

#include <iprt/initterm.h>
#include <iprt/string.h>

#include <VBox/version.h>
#include <VBox/Log.h>
#include <VBox/VBoxGuest.h> /** @todo use the VbglR3 interface! */
#include <VBox/VBoxGuestLib.h>
#include <VBoxDisplay.h>
#ifdef VBOXWDDM
# include <d3dkmthk.h>
#endif

#define WM_VBOX_RESTORED                WM_APP + 1
#define WM_VBOX_CHECK_VRDP              WM_APP + 2
#define WM_VBOX_CHECK_HOSTVERSION       WM_APP + 3
#define WM_VBOX_TRAY                    WM_APP + 4

#define ID_TRAYICON                     2000

typedef enum
{
    VBOXDISPIF_MODE_UNKNOWN  = 0,
    VBOXDISPIF_MODE_XPDM_NT4 = 1,
    VBOXDISPIF_MODE_XPDM
#ifdef VBOXWDDM
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
#ifdef VBOXWDDM
        struct
        {
            /* open adapter */
            PFND3DKMT_OPENADAPTERFROMHDC pfnD3DKMTOpenAdapterFromHdc;
            PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME pfnD3DKMTOpenAdapterFromGdiDisplayName;
            /* close adapter */
            PFND3DKMT_CLOSEADAPTER pfnD3DKMTCloseAdapter;
            /* escape */
            PFND3DKMT_ESCAPE pfnD3DKMTEscape;
            /* auto resize support */
            PFND3DKMT_INVALIDATEACTIVEVIDPN pfnD3DKMTInvalidateActiveVidPn;
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
DWORD VBoxDispIfResize(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel);

/* The environment information for services. */
typedef struct _VBOXSERVICEENV
{
    HINSTANCE hInstance;
    HANDLE    hDriver;
    HANDLE    hStopEvent;
    /* display driver interface, XPDM - WDDM abstraction see VBOXDISPIF** definitions above */
    VBOXDISPIF dispIf;
} VBOXSERVICEENV;

/* The service initialization info and runtime variables. */
typedef struct _VBOXSERVICEINFO
{
    char     *pszName;
    int      (* pfnInit)             (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
    unsigned (__stdcall * pfnThread) (void *pInstance);
    void     (* pfnDestroy)          (const VBOXSERVICEENV *pEnv, void *pInstance);

    /* Variables. */
    HANDLE hThread;
    void  *pInstance;
    bool   fStarted;

} VBOXSERVICEINFO;


extern HWND         gToolWindow;
extern HINSTANCE    gInstance;

extern void VBoxServiceReloadCursor(void);

#endif /* !___VBOXTRAY_H */

