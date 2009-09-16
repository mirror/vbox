/** @file
 * helpers - Guest Additions Service helper functions
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

#include <malloc.h>
#include <windows.h>

#include <iprt/string.h>
#include <VBox/VBoxGuestLib.h>

#include <VBoxGuestInternal.h>

#include "helpers.h"
#include "resource.h"

static unsigned nextAdjacentRectXP (RECTL *paRects, unsigned nRects, unsigned iRect)
{
    unsigned i;
    for (i = 0; i < nRects; i++)
    {
        if (paRects[iRect].right == paRects[i].left)
        {
            return i;
        }
    }
    return ~0;
}

static unsigned nextAdjacentRectXN (RECTL *paRects, unsigned nRects, unsigned iRect)
{
    unsigned i;
    for (i = 0; i < nRects; i++)
    {
        if (paRects[iRect].left == paRects[i].right)
        {
            return i;
        }
    }
    return ~0;
}

static unsigned nextAdjacentRectYP (RECTL *paRects, unsigned nRects, unsigned iRect)
{
    unsigned i;
    for (i = 0; i < nRects; i++)
    {
        if (paRects[iRect].bottom == paRects[i].top)
        {
            return i;
        }
    }
    return ~0;
}

unsigned nextAdjacentRectYN (RECTL *paRects, unsigned nRects, unsigned iRect)
{
    unsigned i;
    for (i = 0; i < nRects; i++)
    {
        if (paRects[iRect].top == paRects[i].bottom)
        {
            return i;
        }
    }
    return ~0;
}

void resizeRect(RECTL *paRects, unsigned nRects, unsigned iPrimary, unsigned iResized, int NewWidth, int NewHeight)
{
    RECTL *paNewRects = (RECTL *)alloca (sizeof (RECTL) * nRects);
    memcpy (paNewRects, paRects, sizeof (RECTL) * nRects);
    paNewRects[iResized].right += NewWidth - (paNewRects[iResized].right - paNewRects[iResized].left);
    paNewRects[iResized].bottom += NewHeight - (paNewRects[iResized].bottom - paNewRects[iResized].top);

    /* Verify all pairs of originally adjacent rectangles for all 4 directions.
     * If the pair has a "good" delta (that is the first rectangle intersects the second)
     * at a direction and the second rectangle is not primary one (which can not be moved),
     * move the second rectangle to make it adjacent to the first one.
     */

    /* X positive. */
    unsigned iRect;
    for (iRect = 0; iRect < nRects; iRect++)
    {
        /* Find the next adjacent original rect in x positive direction. */
        unsigned iNextRect = nextAdjacentRectXP (paRects, nRects, iRect);
        DDCLOG(("next %d -> %d\n", iRect, iNextRect));

        if (iNextRect == ~0 || iNextRect == iPrimary)
        {
            continue;
        }

        /* Check whether there is an X intesection between these adjacent rects in the new rectangles
         * and fix the intersection if delta is "good".
         */
        int delta = paNewRects[iRect].right - paNewRects[iNextRect].left;

        if (delta > 0)
        {
            DDCLOG(("XP intersection right %d left %d, diff %d\n",
                     paNewRects[iRect].right, paNewRects[iNextRect].left,
                     delta));

            paNewRects[iNextRect].left += delta;
            paNewRects[iNextRect].right += delta;
        }
    }

    /* X negative. */
    for (iRect = 0; iRect < nRects; iRect++)
    {
        /* Find the next adjacent original rect in x negative direction. */
        unsigned iNextRect = nextAdjacentRectXN (paRects, nRects, iRect);
        DDCLOG(("next %d -> %d\n", iRect, iNextRect));

        if (iNextRect == ~0 || iNextRect == iPrimary)
        {
            continue;
        }

        /* Check whether there is an X intesection between these adjacent rects in the new rectangles
         * and fix the intersection if delta is "good".
         */
        int delta = paNewRects[iRect].left - paNewRects[iNextRect].right;

        if (delta < 0)
        {
            DDCLOG(("XN intersection left %d right %d, diff %d\n",
                     paNewRects[iRect].left, paNewRects[iNextRect].right,
                     delta));

            paNewRects[iNextRect].left += delta;
            paNewRects[iNextRect].right += delta;
        }
    }

    /* Y positive (in the computer sence, top->down). */
    for (iRect = 0; iRect < nRects; iRect++)
    {
        /* Find the next adjacent original rect in y positive direction. */
        unsigned iNextRect = nextAdjacentRectYP (paRects, nRects, iRect);
        DDCLOG(("next %d -> %d\n", iRect, iNextRect));

        if (iNextRect == ~0 || iNextRect == iPrimary)
        {
            continue;
        }

        /* Check whether there is an Y intesection between these adjacent rects in the new rectangles
         * and fix the intersection if delta is "good".
         */
        int delta = paNewRects[iRect].bottom - paNewRects[iNextRect].top;

        if (delta > 0)
        {
            DDCLOG(("YP intersection bottom %d top %d, diff %d\n",
                     paNewRects[iRect].bottom, paNewRects[iNextRect].top,
                     delta));

            paNewRects[iNextRect].top += delta;
            paNewRects[iNextRect].bottom += delta;
        }
    }

    /* Y negative (in the computer sence, down->top). */
    for (iRect = 0; iRect < nRects; iRect++)
    {
        /* Find the next adjacent original rect in x negative direction. */
        unsigned iNextRect = nextAdjacentRectYN (paRects, nRects, iRect);
        DDCLOG(("next %d -> %d\n", iRect, iNextRect));

        if (iNextRect == ~0 || iNextRect == iPrimary)
        {
            continue;
        }

        /* Check whether there is an Y intesection between these adjacent rects in the new rectangles
         * and fix the intersection if delta is "good".
         */
        int delta = paNewRects[iRect].top - paNewRects[iNextRect].bottom;

        if (delta < 0)
        {
            DDCLOG(("YN intersection top %d bottom %d, diff %d\n",
                     paNewRects[iRect].top, paNewRects[iNextRect].bottom,
                     delta));

            paNewRects[iNextRect].top += delta;
            paNewRects[iNextRect].bottom += delta;
        }
    }

    memcpy (paRects, paNewRects, sizeof (RECTL) * nRects);
    return;
}

int showBalloonTip (HINSTANCE hInst, HWND hWnd, UINT uID, const char *pszMsg, const char *pszTitle, UINT uTimeout, DWORD dwInfoFlags)
{
    NOTIFYICONDATA niData;
    niData.cbSize = sizeof(NOTIFYICONDATA);
    niData.uFlags = NIF_INFO;
    niData.hWnd = hWnd;
    niData.uID = uID;
    niData.uTimeout = uTimeout;
    if (dwInfoFlags == 0)
        dwInfoFlags = NIIF_INFO;
    niData.dwInfoFlags = dwInfoFlags;

    OSVERSIONINFO   info;
    DWORD           dwMajorVersion = 5; /* default XP */

    info.dwOSVersionInfoSize = sizeof(info);
    if (FALSE == GetVersionEx(&info))
        return FALSE;

    if (info.dwMajorVersion >= 5)
    {
        niData.uFlags |= NIF_ICON;
        if (   info.dwMajorVersion == 5
            && info.dwMinorVersion == 1) /* WinXP */
        {
            //niData.dwInfoFlags = NIIF_USER; /* Use an own icon instead of the default one */
            niData.hIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_VIRTUALBOX));
        }
#ifdef BALLOON_WITH_VISTA_TOOLTIP_ICON
        else if (info.dwMajorVersion == 6) /* Vista and up */
        {
            niData.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON; /* Use an own icon instead of the default one */
            niData.hBalloonIcon = LoadIcon(hInst, MAKEINTRESOURCE(IDI_VIRTUALBOX));
        }
#endif
    }

    strcpy(niData.szInfo, pszMsg ? pszMsg : "");
    strcpy(niData.szInfoTitle, pszTitle ? pszTitle : "");

    if (!Shell_NotifyIcon(NIM_MODIFY, &niData))
        return GetLastError();
    return 0;
}

/** @todo move this in guest lib, also used in a similar way in VBoxService */
int getAdditionsVersion(char *pszVer, size_t cbSizeVer, char *pszRev, size_t cbSizeRev)
{
    HKEY hKey;
    int rc;

    /* Check the new path first. */
    rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, &hKey);
#ifdef RT_ARCH_AMD64
    if (rc != ERROR_SUCCESS)
    {
        /* Check Wow6432Node (for new entries). */
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, &hKey);
    }
#endif

    /* Still no luck? Then try the old xVM paths ... */
    if (RT_FAILURE(rc))
    {
        rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, &hKey);
#ifdef RT_ARCH_AMD64
        if (rc != ERROR_SUCCESS)
        {
            /* Check Wow6432Node (for new entries). */
            rc = RegOpenKeyEx(HKEY_LOCAL_MACHINE, "SOFTWARE\\Wow6432Node\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, &hKey);
        }
#endif
    }

    /* Did we get something worth looking at? */
    if (RT_SUCCESS(rc))
    {
        DWORD dwSize;
        DWORD dwType;

        /* Revision. */
        dwSize = cbSizeRev;
        rc = RegQueryValueEx(hKey, "Revision", NULL, &dwType, (BYTE*)(LPCTSTR)pszRev, &dwSize);
        /* Version. */
        dwSize = cbSizeVer;
        rc = RegQueryValueEx(hKey, "Version", NULL, &dwType, (BYTE*)(LPCTSTR)pszVer, &dwSize);
    }

    if (NULL != hKey)
        RegCloseKey(hKey);

    return rc;
}
