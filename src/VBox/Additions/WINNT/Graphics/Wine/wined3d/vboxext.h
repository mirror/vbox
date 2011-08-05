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

#include <windows.h>
#include <iprt/cdefs.h>


HRESULT VBoxExtCheckInit();
HRESULT VBoxExtCheckTerm();
/* Windows destroys HDC created by a given thread when the thread is terminated
 * this leads to a mess-up in Wine & Chromium code in some situations, e.g.
 * D3D device is created in one thread, then the thread is terminated,
 * then device is started to be used in another thread */
HDC VBoxExtGetDC(HWND hWnd);
int VBoxExtReleaseDC(HWND hWnd, HDC hDC);

/* API for creating & destroying windows */
HRESULT VBoxExtWndDestroy(HWND hWnd);
HRESULT VBoxExtWndCreate(DWORD width, DWORD height, HWND *phWnd);

#endif /* #ifndef ___VBOXEXT_H__*/
