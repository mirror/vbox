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
#ifndef ___VBOXEXT_H__
#define ___VBOXEXT_H__

#ifdef VBOX_WINE_WITHOUT_LIBWINE
# include <windows.h>
#endif

HRESULT VBoxExtCheckInit();
HRESULT VBoxExtCheckTerm();
#if defined(VBOX_WINE_WITH_SINGLE_CONTEXT) || defined(VBOX_WINE_WITH_SINGLE_SWAPCHAIN_CONTEXT)
# ifndef VBOX_WITH_WDDM
/* Windows destroys HDC created by a given thread when the thread is terminated
 * this leads to a mess-up in Wine & Chromium code in some situations, e.g.
 * D3D device is created in one thread, then the thread is terminated,
 * then device is started to be used in another thread */
HDC VBoxExtGetDC(HWND hWnd);
int VBoxExtReleaseDC(HWND hWnd, HDC hDC);
# endif
/* We need to do a VBoxTlsRefRelease for the current thread context on thread exit to avoid memory leaking
 * Calling VBoxTlsRefRelease may result in a call to context dtor callback, which is supposed to be run under wined3d lock.
 * We can not acquire a wined3d lock in DllMain since this would result in a lock order violation, which may result in a deadlock.
 * In other words, wined3d may internally call Win32 API functions which result in a DLL lock acquisition while holding wined3d lock.
 * So lock order should always be "wined3d lock" -> "dll lock".
 * To avoid possible deadlocks we make an asynchronous call to a worker thread to make a context release from there. */
void VBoxExtReleaseContextAsync(struct wined3d_context *context);
#endif

/* API for creating & destroying windows */
HRESULT VBoxExtWndDestroy(HWND hWnd, HDC hDC);
HRESULT VBoxExtWndCreate(DWORD width, DWORD height, HWND *phWnd, HDC *phDC);

#endif /* #ifndef ___VBOXEXT_H__*/
