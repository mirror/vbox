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
#include <windows.h>
#include <d3dkmthk.h>

#include <iprt/cdefs.h>
#include <iprt/ctype.h>

#include <VBox/VBoxVideo.h>

#include "../../Miniport/wddm/VBoxVideoIf.h"

typedef struct VBOXDISPIFESCAPE_SCREENLAYOUT_DATA
{
    VBOXDISPIFESCAPE_SCREENLAYOUT ScreenLayout;
    POINT buffer[VBOX_VIDEO_MAX_SCREENS];
} VBOXDISPIFESCAPE_SCREENLAYOUT_DATA, *PVBOXDISPIFESCAPE_SCREENLAYOUT_DATA;

typedef struct VBOXSCREENMON
{
    BOOL bInited;

    HMODULE hGdi32;

    HWND hWnd;

    /* open adapter */
    PFND3DKMT_OPENADAPTERFROMHDC pfnD3DKMTOpenAdapterFromHdc;
    PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME pfnD3DKMTOpenAdapterFromGdiDisplayName;
    /* close adapter */
    PFND3DKMT_CLOSEADAPTER pfnD3DKMTCloseAdapter;
    /* escape */
    PFND3DKMT_ESCAPE pfnD3DKMTEscape;

    VBOXDISPIFESCAPE_SCREENLAYOUT_DATA LoData;
} VBOXSCREENMON, *PVBOXSCREENMON;

typedef DECLCALLBACK(HRESULT) FNVBOXDISPESCAPECB(void * pvBuffer, uint32_t cbBuffer, void *pvContext);
typedef FNVBOXDISPESCAPECB *PFNVBOXDISPESCAPECB;

typedef struct VBOXSCREENMONRUNNER
{
//    VBOXSCREENMON Screen;
    HANDLE hThread;
    DWORD idThread;
    HANDLE hEvent;
} VBOXSCREENMONRUNNER, *PVBOXSCREENMONRUNNER;

HRESULT VBoxScreenMRunnerStart(PVBOXSCREENMONRUNNER pMon);
HRESULT VBoxScreenMRunnerStop(PVBOXSCREENMONRUNNER pMon);
