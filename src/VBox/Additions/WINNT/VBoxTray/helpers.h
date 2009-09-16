/* $Id$ */
/** @file
 * helpers - Guest Additions Service helper functions header
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

#ifndef ___VBOXTRAY_HELPERS_H
#define ___VBOXTRAY_HELPERS_H

// #define DEBUG_DISPLAY_CHANGE

#ifdef DEBUG_DISPLAY_CHANGE
#   define DDCLOG(a) Log(a)
#else
#   define DDCLOG(a) do {} while (0)
#endif /* !DEBUG_DISPLAY_CHANGE */

void resizeRect(RECTL *paRects, unsigned nRects, unsigned iPrimary, unsigned iResized, int NewWidth, int NewHeight);
int showBalloonTip (HINSTANCE hInst, HWND hWnd, UINT uID, const char *pszMsg, const char *pszTitle, UINT uTimeout, DWORD dwInfoFlags);
int getAdditionsVersion(char *pszVer, size_t cbSizeVer, char *pszRev, size_t cbSizeRev);

#endif /* !___VBOXTRAY_HELPERS_H */

