/** @file
 * Shared Clipboard - Common Guest and Host Code, for Windows OSes.
 */

/*
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#define VBOX_INCLUDED_GuestHost_SharedClipboard_win_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/win/windows.h>

#ifndef WM_CLIPBOARDUPDATE
# define WM_CLIPBOARDUPDATE 0x031D
#endif

#define VBOX_CLIPBOARD_WNDCLASS_NAME        "VBoxSharedClipboardClass"

/** Default timeout (in ms) for passing down messages down the clipboard chain. */
#define VBOX_CLIPBOARD_CBCHAIN_TIMEOUT_MS   5000

/** Sets announced clipboard formats from the host. */
#define VBOX_CLIPBOARD_WM_SET_FORMATS       WM_USER
/** Reads data from the clipboard and sends it to the host. */
#define VBOX_CLIPBOARD_WM_READ_DATA         WM_USER + 1

/* Dynamically load clipboard functions from User32.dll. */
typedef BOOL WINAPI FNADDCLIPBOARDFORMATLISTENER(HWND);
typedef FNADDCLIPBOARDFORMATLISTENER *PFNADDCLIPBOARDFORMATLISTENER;

typedef BOOL WINAPI FNREMOVECLIPBOARDFORMATLISTENER(HWND);
typedef FNREMOVECLIPBOARDFORMATLISTENER *PFNREMOVECLIPBOARDFORMATLISTENER;

/**
 * Structure for keeping function pointers for the new clipboard API.
 * If the new API is not available, those function pointer are NULL.
 */
typedef struct VBOXCLIPBOARDWINAPINEW
{
    PFNADDCLIPBOARDFORMATLISTENER    pfnAddClipboardFormatListener;
    PFNREMOVECLIPBOARDFORMATLISTENER pfnRemoveClipboardFormatListener;
} VBOXCLIPBOARDWINAPINEW, *PVBOXCLIPBOARDWINAPINEW;

/**
 * Structure for keeping variables which are needed to drive the old clipboard API.
 */
typedef struct VBOXCLIPBOARDWINAPIOLD
{
    /** Timer ID for the refresh timer. */
    UINT                   timerRefresh;
    /** Whether "pinging" the clipboard chain currently is in progress or not. */
    bool                   fCBChainPingInProcess;
} VBOXCLIPBOARDWINAPIOLD, *PVBOXCLIPBOARDWINAPIOLD;

typedef struct VBOXCLIPBOARDWINCTX
{
    /** Window handle of our (invisible) clipbaord window. */
    HWND                   hWnd;
    /** Window handle which is next to us in the clipboard chain. */
    HWND                   hWndNextInChain;
    /** Structure for maintaining the new clipboard API. */
    VBOXCLIPBOARDWINAPINEW newAPI;
    /** Structure for maintaining the old clipboard API. */
    VBOXCLIPBOARDWINAPIOLD oldAPI;
} VBOXCLIPBOARDWINCTX, *PVBOXCLIPBOARDWINCTX;

int VBoxClipboardWinOpen(HWND hWnd);
int VBoxClipboardWinClose(void);
int VBoxClipboardWinClear(void);
int VBoxClipboardWinCheckAndInitNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);
bool VBoxClipboardWinIsNewAPI(PVBOXCLIPBOARDWINAPINEW pAPI);
int VBoxClipboardWinAddToCBChain(PVBOXCLIPBOARDWINCTX pCtx);
int VBoxClipboardWinRemoveFromCBChain(PVBOXCLIPBOARDWINCTX pCtx);
VOID CALLBACK VBoxClipboardWinChainPingProc(HWND hWnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult);
int VBoxClipboardWinGetFormats(PVBOXCLIPBOARDWINCTX pCtx, uint32_t *puFormats);

#endif /* !VBOX_INCLUDED_GuestHost_SharedClipboard_win_h */

