/* $Id$ */
/** @file
 * helpers - Guest Additions Service helper functions header.
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
 */

#ifndef GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxHelpers_h
#define GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxHelpers_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

// #define DEBUG_DISPLAY_CHANGE

#ifdef DEBUG_DISPLAY_CHANGE
#   define DDCLOG(a) Log(a)
#else
#   define DDCLOG(a) do {} while (0)
#endif /* !DEBUG_DISPLAY_CHANGE */

extern int hlpReportStatus(VBoxGuestFacilityStatus statusCurrent);
extern void hlpReloadCursor(void);
extern void hlpResizeRect(RECTL *paRects, unsigned nRects, unsigned uPrimary, unsigned uResized, int iNewWidth, int iNewHeight, int iNewPosX, int iNewPosY);
extern int hlpShowBalloonTip(HINSTANCE hInst, HWND hWnd, UINT uID, const char *pszMsg, const char *pszTitle, UINT uTimeout, DWORD dwInfoFlags);

#endif /* !GA_INCLUDED_SRC_WINNT_VBoxTray_VBoxHelpers_h */

