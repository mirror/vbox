/** $Id$ */
/** @file
 * VBoxControl - Guest Additions Command Line Management Interface
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/path.h>
#include <iprt/initterm.h>
#include <iprt/autores.h>
#include <VBox/log.h>
#include <VBox/VBoxGuest.h>
#include <VBox/version.h>
#ifdef RT_OS_WINDOWS
# include <windows.h>
# include <malloc.h>  /* for alloca */
#endif
#ifdef VBOX_WITH_GUEST_PROPS
# include <VBox/HostServices/GuestPropertySvc.h>
#endif
#include "VBoxControl.h"

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The program name (derived from argv[0]). */
char const *g_pszProgName = "";
/** The current verbosity level. */
int g_cVerbosity = 0;


/**
 * Displays the program usage message.
 *
 * @param u64Which
 *
 * @{
 */

/** Helper function */
static void doUsage(char const *line, char const *name = "", char const *command = "")
{
    RTPrintf("%s %-*s%s", name, 32 - strlen(name), command, line);
}

/** Enumerate the different parts of the usage we might want to print out */
enum g_eUsage
{
#ifdef RT_OS_WINDOWS
    GET_VIDEO_ACCEL,
    SET_VIDEO_ACCEL,
    LIST_CUST_MODES,
    ADD_CUST_MODE,
    REMOVE_CUST_MODE,
    SET_VIDEO_MODE,
#endif
#ifdef VBOX_WITH_GUEST_PROPS
    GUEST_PROP,
#endif
    USAGE_ALL = UINT32_MAX
};

static void usage(g_eUsage eWhich = USAGE_ALL)
{
    RTPrintf("Usage:\n\n");
    RTPrintf("%s [-v|-version]        print version number and exit\n", g_pszProgName);
    RTPrintf("%s -nologo ...          suppress the logo\n\n", g_pszProgName);

/* Exclude the Windows bits from the test version.  Anyone who needs to test
 * them can fix this. */
#if defined(RT_OS_WINDOWS) && !defined(VBOX_CONTROL_TEST)
    if ((GET_VIDEO_ACCEL == eWhich) || (USAGE_ALL == eWhich))
        doUsage("\n", g_pszProgName, "getvideoacceleration");
    if ((SET_VIDEO_ACCEL == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<on|off>\n", g_pszProgName, "setvideoacceleration");
    if ((LIST_CUST_MODES == eWhich) || (USAGE_ALL == eWhich))
        doUsage("\n", g_pszProgName, "listcustommodes");
    if ((ADD_CUST_MODE == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<width> <height> <bpp>\n", g_pszProgName, "addcustommode");
    if ((REMOVE_CUST_MODE == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<width> <height> <bpp>\n", g_pszProgName, "removecustommode");
    if ((SET_VIDEO_MODE == eWhich) || (USAGE_ALL == eWhich))
        doUsage("<width> <height> <bpp> <screen>\n", g_pszProgName, "setvideomode");
#endif
#ifdef VBOX_WITH_GUEST_PROPS
    if ((GUEST_PROP == eWhich) || (USAGE_ALL == eWhich))
    {
        doUsage("get <property> [-verbose]\n", g_pszProgName, "guestproperty");
        doUsage("set <property> [<value> [-flags <flags>]]\n", g_pszProgName, "guestproperty");
        doUsage("enumerate [-patterns <patterns>]\n", g_pszProgName, "guestproperty");
        doUsage("wait <patterns> [-timestamp <last timestamp>]\n", g_pszProgName, "guestproperty");
        doUsage("[-timeout <timeout in ms>\n");
    }
#endif
}
/** @} */

/**
 * Displays an error message.
 *
 * @param   pszFormat   The message text.
 * @param   ...         Format arguments.
 */
static void VBoxControlError(const char *pszFormat, ...)
{
    // RTStrmPrintf(g_pStdErr, "%s: error: ", g_pszProgName);

    va_list va;
    va_start(va, pszFormat);
    RTStrmPrintfV(g_pStdErr, pszFormat, va);
    va_end(va);
}

#if defined(RT_OS_WINDOWS) && !defined(VBOX_CONTROL_TEST)

LONG (WINAPI * gpfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);

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
        Log(("next %d -> %d\n", iRect, iNextRect));

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
            Log(("XP intersection right %d left %d, diff %d\n",
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
        Log(("next %d -> %d\n", iRect, iNextRect));

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
            Log(("XN intersection left %d right %d, diff %d\n",
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
        Log(("next %d -> %d\n", iRect, iNextRect));

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
            Log(("YP intersection bottom %d top %d, diff %d\n",
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
        Log(("next %d -> %d\n", iRect, iNextRect));

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
            Log(("YN intersection top %d bottom %d, diff %d\n",
                     paNewRects[iRect].top, paNewRects[iNextRect].bottom,
                     delta));

            paNewRects[iNextRect].top += delta;
            paNewRects[iNextRect].bottom += delta;
        }
    }

    memcpy (paRects, paNewRects, sizeof (RECTL) * nRects);
    return;
}

/* Returns TRUE to try again. */
static BOOL ResizeDisplayDevice(ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    BOOL fModeReset = (Width == 0 && Height == 0 && BitsPerPixel == 0);

    DISPLAY_DEVICE DisplayDevice;

    ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
    DisplayDevice.cb = sizeof(DisplayDevice);

    /* Find out how many display devices the system has */
    DWORD NumDevices = 0;
    DWORD i = 0;
    while (EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
    {
        Log(("[%d] %s\n", i, DisplayDevice.DeviceName));

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("Found primary device. err %d\n", GetLastError ()));
            NumDevices++;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("Found secondary device. err %d\n", GetLastError ()));
            NumDevices++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
        DisplayDevice.cb = sizeof(DisplayDevice);
        i++;
    }

    Log(("Found total %d devices. err %d\n", NumDevices, GetLastError ()));

    if (NumDevices == 0 || Id >= NumDevices)
    {
        Log(("Requested identifier %d is invalid. err %d\n", Id, GetLastError ()));
        return FALSE;
    }

    DISPLAY_DEVICE *paDisplayDevices = (DISPLAY_DEVICE *)alloca (sizeof (DISPLAY_DEVICE) * NumDevices);
    DEVMODE *paDeviceModes = (DEVMODE *)alloca (sizeof (DEVMODE) * NumDevices);
    RECTL *paRects = (RECTL *)alloca (sizeof (RECTL) * NumDevices);

    /* Fetch information about current devices and modes. */
    DWORD DevNum = 0;
    DWORD DevPrimaryNum = 0;

    ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
    DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

    i = 0;
    while (EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
    {
        Log(("[%d(%d)] %s\n", i, DevNum, DisplayDevice.DeviceName));

        BOOL bFetchDevice = FALSE;

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("Found primary device. err %d\n", GetLastError ()));
            DevPrimaryNum = DevNum;
            bFetchDevice = TRUE;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("Found secondary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }

        if (bFetchDevice)
        {
            if (DevNum >= NumDevices)
            {
                Log(("%d >= %d\n", NumDevices, DevNum));
                return FALSE;
            }

            paDisplayDevices[DevNum] = DisplayDevice;

            ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
            paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_REGISTRY_SETTINGS, &paDeviceModes[DevNum]))
            {
                Log(("EnumDisplaySettings err %d\n", GetLastError ()));
                return FALSE;
            }

            Log(("%dx%d at %d,%d\n",
                    paDeviceModes[DevNum].dmPelsWidth,
                    paDeviceModes[DevNum].dmPelsHeight,
                    paDeviceModes[DevNum].dmPosition.x,
                    paDeviceModes[DevNum].dmPosition.y));

            paRects[DevNum].left   = paDeviceModes[DevNum].dmPosition.x;
            paRects[DevNum].top    = paDeviceModes[DevNum].dmPosition.y;
            paRects[DevNum].right  = paDeviceModes[DevNum].dmPosition.x + paDeviceModes[DevNum].dmPelsWidth;
            paRects[DevNum].bottom = paDeviceModes[DevNum].dmPosition.y + paDeviceModes[DevNum].dmPelsHeight;
            DevNum++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
        DisplayDevice.cb = sizeof(DISPLAY_DEVICE);
        i++;
    }

    if (Width == 0)
    {
        Width = paRects[Id].right - paRects[Id].left;
    }

    if (Height == 0)
    {
        Height = paRects[Id].bottom - paRects[Id].top;
    }

    /* Check whether a mode reset or a change is requested. */
    if (   !fModeReset
        && paRects[Id].right - paRects[Id].left == Width
        && paRects[Id].bottom - paRects[Id].top == Height
        && paDeviceModes[Id].dmBitsPerPel == BitsPerPixel)
    {
        Log(("VBoxDisplayThread : already at desired resolution.\n"));
        return FALSE;
    }

    resizeRect(paRects, NumDevices, DevPrimaryNum, Id, Width, Height);
#ifdef Log
    for (i = 0; i < NumDevices; i++)
    {
        Log(("[%d]: %d,%d %dx%d\n",
                i, paRects[i].left, paRects[i].top,
                paRects[i].right - paRects[i].left,
                paRects[i].bottom - paRects[i].top));
    }
#endif /* Log */

    /* Without this, Windows will not ask the miniport for its
     * mode table but uses an internal cache instead.
     */
    DEVMODE tempDevMode;
    ZeroMemory (&tempDevMode, sizeof (tempDevMode));
    tempDevMode.dmSize = sizeof(DEVMODE);
    EnumDisplaySettings(NULL, 0xffffff, &tempDevMode);

    /* Assign the new rectangles to displays. */
    for (i = 0; i < NumDevices; i++)
    {
        paDeviceModes[i].dmPosition.x = paRects[i].left;
        paDeviceModes[i].dmPosition.y = paRects[i].top;
        paDeviceModes[i].dmPelsWidth  = paRects[i].right - paRects[i].left;
        paDeviceModes[i].dmPelsHeight = paRects[i].bottom - paRects[i].top;

        paDeviceModes[i].dmFields = DM_POSITION | DM_PELSHEIGHT | DM_PELSWIDTH;

        if (   i == Id
            && BitsPerPixel != 0)
        {
            paDeviceModes[i].dmFields |= DM_BITSPERPEL;
            paDeviceModes[i].dmBitsPerPel = BitsPerPixel;
        }
        Log(("calling pfnChangeDisplaySettingsEx %x\n", gpfnChangeDisplaySettingsEx));
        gpfnChangeDisplaySettingsEx((LPSTR)paDisplayDevices[i].DeviceName,
                 &paDeviceModes[i], NULL, CDS_NORESET | CDS_UPDATEREGISTRY, NULL);
        Log(("ChangeDisplaySettings position err %d\n", GetLastError ()));
    }

    /* A second call to ChangeDisplaySettings updates the monitor. */
    LONG status = ChangeDisplaySettings(NULL, 0);
    Log(("ChangeDisplaySettings update status %d\n", status));
    if (status == DISP_CHANGE_SUCCESSFUL || status == DISP_CHANGE_BADMODE)
    {
        /* Successfully set new video mode or our driver can not set the requested mode. Stop trying. */
        return FALSE;
    }

    /* Retry the request. */
    return TRUE;
}

int handleSetVideoMode(int argc, char *argv[])
{
    if (argc != 3 && argc != 4)
    {
        usage(SET_VIDEO_MODE);
        return 1;
    }

    DWORD xres = atoi(argv[0]);
    DWORD yres = atoi(argv[1]);
    DWORD bpp  = atoi(argv[2]);
    DWORD scr  = 0;

    if (argc == 4)
    {
        scr = atoi(argv[3]);
    }

    HMODULE hUser = GetModuleHandle("USER32");

    if (hUser)
    {
        *(uintptr_t *)&gpfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
        Log(("VBoxService: pChangeDisplaySettingsEx = %p\n", gpfnChangeDisplaySettingsEx));

        if (gpfnChangeDisplaySettingsEx)
        {
            /* The screen index is 0 based in the ResizeDisplayDevice call. */
            scr = scr > 0? scr - 1: 0;

            /* Horizontal resolution must be a multiple of 8, round down. */
            xres &= ~0x7;

            RTPrintf("Setting resolution of display %d to %dx%dx%d ...", scr, xres, yres, bpp);
            ResizeDisplayDevice(scr, xres, yres, bpp);
            RTPrintf("done.\n");
        }
        else VBoxControlError("Error retrieving API for display change!");
    }
    else VBoxControlError("Error retrieving handle to user32.dll!");

    return 0;
}

HKEY getVideoKey(bool writable)
{
    HKEY hkeyDeviceMap = 0;
    HKEY hkeyVideo = 0;
    LONG status;

    status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DEVICEMAP\\VIDEO", 0, KEY_READ, &hkeyDeviceMap);
    if ((status != ERROR_SUCCESS) || !hkeyDeviceMap)
    {
        VBoxControlError("Error opening video device map registry key!\n");
        return 0;
    }
    char szVideoLocation[256];
    DWORD dwKeyType;
    szVideoLocation[0] = 0;
    DWORD len = sizeof(szVideoLocation);
    status = RegQueryValueExA(hkeyDeviceMap, "\\Device\\Video0", NULL, &dwKeyType, (LPBYTE)szVideoLocation, &len);
    /*
     * This value will start with a weird value: \REGISTRY\Machine
     * Make sure this is true.
     */
    if (   (status == ERROR_SUCCESS)
        && (dwKeyType == REG_SZ)
        && (_strnicmp(szVideoLocation, "\\REGISTRY\\Machine", 17) == 0))
    {
        /* open that branch */
        status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, &szVideoLocation[18], 0, KEY_READ | (writable ? KEY_WRITE : 0), &hkeyVideo);
    }
    else
    {
        VBoxControlError("Error opening registry key '%s'\n", &szVideoLocation[18]);
    }
    RegCloseKey(hkeyDeviceMap);
    return hkeyVideo;
}

int handleGetVideoAcceleration(int argc, char *argv[])
{
    ULONG status;
    HKEY hkeyVideo = getVideoKey(false);

    if (hkeyVideo)
    {
        /* query the actual value */
        DWORD fAcceleration = 1;
        DWORD len = sizeof(fAcceleration);
        DWORD dwKeyType;
        status = RegQueryValueExA(hkeyVideo, "EnableVideoAccel", NULL, &dwKeyType, (LPBYTE)&fAcceleration, &len);
        if (status != ERROR_SUCCESS)
            RTPrintf("Video acceleration: default\n");
        else
            RTPrintf("Video acceleration: %s\n", fAcceleration ? "on" : "off");
        RegCloseKey(hkeyVideo);
    }
    return 0;
}

int handleSetVideoAcceleration(int argc, char *argv[])
{
    ULONG status;
    HKEY hkeyVideo;

    /* must have exactly one argument: the new offset */
    if (   (argc != 1)
        || (   strcmp(argv[0], "on")
            && strcmp(argv[0], "off")))
    {
        usage(SET_VIDEO_ACCEL);
        return 1;
    }

    hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        int fAccel = 0;
        if (!strcmp(argv[0], "on"))
            fAccel = 1;
        /* set a new value */
        status = RegSetValueExA(hkeyVideo, "EnableVideoAccel", 0, REG_DWORD, (LPBYTE)&fAccel, sizeof(fAccel));
        if (status != ERROR_SUCCESS)
        {
            VBoxControlError("Error %d writing video acceleration status!\n", status);
        }
        RegCloseKey(hkeyVideo);
    }
    return 0;
}

#define MAX_CUSTOM_MODES 128

/* the table of custom modes */
struct
{
    DWORD xres;
    DWORD yres;
    DWORD bpp;
} customModes[MAX_CUSTOM_MODES] = {0};

void getCustomModes(HKEY hkeyVideo)
{
    ULONG status;
    int curMode = 0;

    /* null out the table */
    memset(customModes, 0, sizeof(customModes));

    do
    {
        char valueName[20];
        DWORD xres, yres, bpp = 0;
        DWORD dwType;
        DWORD dwLen = sizeof(DWORD);

        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dWidth", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&xres, &dwLen);
        if (status != ERROR_SUCCESS)
            break;
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dHeight", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&yres, &dwLen);
        if (status != ERROR_SUCCESS)
            break;
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dBPP", curMode);
        status = RegQueryValueExA(hkeyVideo, valueName, NULL, &dwType, (LPBYTE)&bpp, &dwLen);
        if (status != ERROR_SUCCESS)
            break;

        /* check if the mode is OK */
        if (   (xres > (1 << 16))
            && (yres > (1 << 16))
            && (   (bpp != 16)
                || (bpp != 24)
                || (bpp != 32)))
            break;

        /* add mode to table */
        customModes[curMode].xres = xres;
        customModes[curMode].yres = yres;
        customModes[curMode].bpp  = bpp;

        ++curMode;

        if (curMode >= MAX_CUSTOM_MODES)
            break;
    } while(1);
}

void writeCustomModes(HKEY hkeyVideo)
{
    ULONG status;
    int tableIndex = 0;
    int modeIndex = 0;

    /* first remove all values */
    for (int i = 0; i < MAX_CUSTOM_MODES; i++)
    {
        char valueName[20];
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dWidth", i);
        RegDeleteValueA(hkeyVideo, valueName);
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dHeight", i);
        RegDeleteValueA(hkeyVideo, valueName);
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dBPP", i);
        RegDeleteValueA(hkeyVideo, valueName);
    }

    do
    {
        if (tableIndex >= MAX_CUSTOM_MODES)
            break;

        /* is the table entry present? */
        if (   (!customModes[tableIndex].xres)
            || (!customModes[tableIndex].yres)
            || (!customModes[tableIndex].bpp))
        {
            tableIndex++;
            continue;
        }

        RTPrintf("writing mode %d (%dx%dx%d)\n", modeIndex, customModes[tableIndex].xres, customModes[tableIndex].yres, customModes[tableIndex].bpp);
        char valueName[20];
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dWidth", modeIndex);
        status = RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].xres,
                                sizeof(customModes[tableIndex].xres));
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dHeight", modeIndex);
        RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].yres,
                       sizeof(customModes[tableIndex].yres));
        RTStrPrintf(valueName, sizeof(valueName), "CustomMode%dBPP", modeIndex);
        RegSetValueExA(hkeyVideo, valueName, 0, REG_DWORD, (LPBYTE)&customModes[tableIndex].bpp,
                       sizeof(customModes[tableIndex].bpp));

        modeIndex++;
        tableIndex++;

    } while(1);

}

int handleListCustomModes(int argc, char *argv[])
{
    if (argc != 0)
    {
        usage(LIST_CUST_MODES);
        return 1;
    }

    HKEY hkeyVideo = getVideoKey(false);

    if (hkeyVideo)
    {
        getCustomModes(hkeyVideo);
        for (int i = 0; i < (sizeof(customModes) / sizeof(customModes[0])); i++)
        {
            if (   !customModes[i].xres
                || !customModes[i].yres
                || !customModes[i].bpp)
                continue;

            RTPrintf("Mode: %d x %d x %d\n",
                             customModes[i].xres, customModes[i].yres, customModes[i].bpp);
        }
        RegCloseKey(hkeyVideo);
    }
    return 0;
}

int handleAddCustomMode(int argc, char *argv[])
{
    if (argc != 3)
    {
        usage(ADD_CUST_MODE);
        return 1;
    }

    DWORD xres = atoi(argv[0]);
    DWORD yres = atoi(argv[1]);
    DWORD bpp  = atoi(argv[2]);

    /** @todo better check including xres mod 8 = 0! */
    if (   (xres > (1 << 16))
        && (yres > (1 << 16))
        && (   (bpp != 16)
            || (bpp != 24)
            || (bpp != 32)))
    {
        VBoxControlError("Error: invalid mode specified!\n");
        return 1;
    }

    HKEY hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        int i;
        int fModeExists = 0;
        getCustomModes(hkeyVideo);
        for (i = 0; i < MAX_CUSTOM_MODES; i++)
        {
            /* mode exists? */
            if (   customModes[i].xres == xres
                && customModes[i].yres == yres
                && customModes[i].bpp  == bpp
               )
            {
                fModeExists = 1;
            }
        }
        if (!fModeExists)
        {
            for (i = 0; i < MAX_CUSTOM_MODES; i++)
            {
                /* item free? */
                if (!customModes[i].xres)
                {
                    customModes[i].xres = xres;
                    customModes[i].yres = yres;
                    customModes[i].bpp  = bpp;
                    break;
                }
            }
            writeCustomModes(hkeyVideo);
        }
        RegCloseKey(hkeyVideo);
    }
    return 0;
}

int handleRemoveCustomMode(int argc, char *argv[])
{
    if (argc != 3)
    {
        usage(REMOVE_CUST_MODE);
        return 1;
    }

    DWORD xres = atoi(argv[0]);
    DWORD yres = atoi(argv[1]);
    DWORD bpp  = atoi(argv[2]);

    HKEY hkeyVideo = getVideoKey(true);

    if (hkeyVideo)
    {
        getCustomModes(hkeyVideo);
        for (int i = 0; i < MAX_CUSTOM_MODES; i++)
        {
            /* correct item? */
            if (   (customModes[i].xres == xres)
                && (customModes[i].yres == yres)
                && (customModes[i].bpp  == bpp))
            {
                RTPrintf("found mode at index %d\n", i);
                memset(&customModes[i], 0, sizeof(customModes[i]));
                break;
            }
        }
        writeCustomModes(hkeyVideo);
        RegCloseKey(hkeyVideo);
    }

    return 0;
}

#endif /* RT_OS_WINDOWS */

#ifdef VBOX_WITH_GUEST_PROPS
/**
 * Retrieves a value from the guest property store.
 * This is accessed through the "VBoxGuestPropSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
int getGuestProperty(int argc, char **argv)
{
    using namespace guestProp;

    bool verbose = false;
    if ((2 == argc) && (0 == strcmp(argv[1], "-verbose")))
        verbose = true;
    else if (argc != 1)
    {
        usage(GUEST_PROP);
        return 1;
    }

    uint32_t u32ClientId = 0;
    int rc = VINF_SUCCESS;

    rc = VbglR3GuestPropConnect(&u32ClientId);
    if (!RT_SUCCESS(rc))
        VBoxControlError("Failed to connect to the guest property service, error %Rrc\n", rc);

/*
 * Here we actually retrieve the value from the host.
 */
    const char *pszName = argv[0];
    char *pszValue = NULL;
    uint64_t u64Timestamp = 0;
    char *pszFlags = NULL;
    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised. */
    void *pvBuf = NULL;
    uint32_t cbBuf = MAX_VALUE_LEN + MAX_FLAGS_LEN + 1024;
    if (RT_SUCCESS(rc))
    {
        /* Because there is a race condition between our reading the size of a
         * property and the guest updating it, we loop a few times here and
         * hope.  Actually this should never go wrong, as we are generous
         * enough with buffer space. */
        bool finish = false;
        for (unsigned i = 0; (i < 10) && !finish; ++i)
        {
            void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
            if (NULL == pvTmpBuf)
            {
                rc = VERR_NO_MEMORY;
                VBoxControlError("Out of memory\n");
            }
            else
            {
                pvBuf = pvTmpBuf;
                rc = VbglR3GuestPropRead(u32ClientId, pszName, pvBuf, cbBuf,
                                         &pszValue, &u64Timestamp, &pszFlags,
                                         &cbBuf);
            }
            if (VERR_BUFFER_OVERFLOW == rc)
                /* Leave a bit of extra space to be safe */
                cbBuf += 1024;
            else
                finish = true;
        }
        if (VERR_TOO_MUCH_DATA == rc)
            VBoxControlError("Temporarily unable to retrieve the property\n");
        else if (!RT_SUCCESS(rc) && (rc != VERR_NOT_FOUND))
            VBoxControlError("Failed to retrieve the property value, error %Rrc\n", rc);
    }
/*
 * And display it on the guest console.
 */
    if (VERR_NOT_FOUND == rc)
        RTPrintf("No value set!\n");
    else if (RT_SUCCESS(rc))
    {
        RTPrintf("Value: %S\n", pszValue);
        if (verbose)
        {
            RTPrintf("Timestamp: %lld ns\n", u64Timestamp);
            RTPrintf("Flags: %S\n", pszFlags);
        }
    }

    if (u32ClientId != 0)
        VbglR3GuestPropDisconnect(u32ClientId);
    RTMemFree(pvBuf);
    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Writes a value to the guest property store.
 * This is accessed through the "VBoxGuestPropSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int setGuestProperty(int argc, char *argv[])
{
/*
 * Check the syntax.  We can deduce the correct syntax from the number of
 * arguments.
 */
    bool usageOK = true;
    const char *pszName = NULL;
    const char *pszValue = NULL;
    const char *pszFlags = NULL;
    if (2 == argc)
    {
        pszValue = argv[1];
    }
    else if (3 == argc)
        usageOK = false;
    else if (4 == argc)
    {
        pszValue = argv[1];
        if (strcmp(argv[2], "-flags") != 0)
            usageOK = false;
        pszFlags = argv[3];
    }
    else if (argc != 1)
        usageOK = false;
    if (!usageOK)
    {
        usage(GUEST_PROP);
        return 1;
    }
    /* This is always needed. */
    pszName = argv[0];

/*
 * Do the actual setting.
 */
    uint32_t u32ClientId = 0;
    int rc = VINF_SUCCESS;
    rc = VbglR3GuestPropConnect(&u32ClientId);
    if (!RT_SUCCESS(rc))
        VBoxControlError("Failed to connect to the guest property service, error %Rrc\n", rc);
    if (RT_SUCCESS(rc))
    {
        if (pszFlags != NULL)
            rc = VbglR3GuestPropWrite(u32ClientId, pszName, pszValue, pszFlags);
        else
            rc = VbglR3GuestPropWriteValue(u32ClientId, pszName, pszValue);
        if (!RT_SUCCESS(rc))
            VBoxControlError("Failed to store the property value, error %Rrc\n", rc);
    }

    if (u32ClientId != 0)
        VbglR3GuestPropDisconnect(u32ClientId);
    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Enumerates the properties in the guest property store.
 * This is accessed through the "VBoxGuestPropSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int enumGuestProperty(int argc, char *argv[])
{
    /*
     * Check the syntax.  We can deduce the correct syntax from the number of
     * arguments.
     */
    char const * const *papszPatterns = NULL;
    uint32_t cPatterns = 0;
    if (    argc > 1
        && !strcmp(argv[0], "-patterns"))
    {
        papszPatterns = (char const * const *)&argv[1];
        cPatterns = argc - 1;
    }
    else if (argc != 0)
    {
        usage(GUEST_PROP);
        return 1;
    }

    /*
     * Do the actual enumeration.
     */
    uint32_t u32ClientId = 0;
    int rc = VbglR3GuestPropConnect(&u32ClientId);
    if (RT_SUCCESS(rc))
    {
        PVBGLR3GUESTPROPENUM pHandle;
        const char *pszName, *pszValue, *pszFlags;
        uint64_t u64Timestamp;

        rc = VbglR3GuestPropEnum(u32ClientId, papszPatterns, cPatterns, &pHandle,
                                 &pszName, &pszValue, &u64Timestamp, &pszFlags);
        if (RT_SUCCESS(rc))
        {
            while (RT_SUCCESS(rc) && pszName)
            {
                RTPrintf("Name: %s, value: %s, timestamp: %lld, flags: %s\n",
                         pszName, pszValue, u64Timestamp, pszFlags);

                rc = VbglR3GuestPropEnumNext(pHandle, &pszName, &pszValue, &u64Timestamp, &pszFlags);
                if (RT_FAILURE(rc))
                    VBoxControlError("Error while enumerating guest properties: %Rrc\n", rc);
            }

            VbglR3GuestPropEnumFree(pHandle);
        }
        else if (VERR_NOT_FOUND == rc)
            RTPrintf("No properties found.\n");
        else
            VBoxControlError("Failed to enumerate the guest properties! Error: %Rrc\n", rc);
        VbglR3GuestPropDisconnect(u32ClientId);
    }
    else
        VBoxControlError("Failed to connect to the guest property service! Error: %Rrc\n", rc);
    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Waits for notifications of changes to guest properties.
 * This is accessed through the "VBoxGuestPropSvc" HGCM service.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
int waitGuestProperty(int argc, char **argv)
{
    using namespace guestProp;

    /*
     * Handle arguments
     */
    const char *pszPatterns = NULL;
    uint64_t u64TimestampIn = 0;
    uint32_t u32Timeout = RT_INDEFINITE_WAIT;
    bool usageOK = true;
    if (argc < 1)
        usageOK = false;
    pszPatterns = argv[0];
    for (int i = 1; usageOK && i < argc; ++i)
    {
        if (strcmp(argv[i], "-timeout") == 0)
        {
            if (   i + 1 >= argc
                || RTStrToUInt32Full(argv[i + 1], 10, &u32Timeout)
                       != VINF_SUCCESS
               )
                usageOK = false;
            else
                ++i;
        }
        else if (strcmp(argv[i], "-timestamp") == 0)
        {
            if (   i + 1 >= argc
                || RTStrToUInt64Full(argv[i + 1], 10, &u64TimestampIn)
                       != VINF_SUCCESS
               )
                usageOK = false;
            else
                ++i;
        }
        else
            usageOK = false;
    }
    if (!usageOK)
    {
        usage(GUEST_PROP);
        return 1;
    }

    /*
     * Connect to the service
     */
    uint32_t u32ClientId = 0;
    int rc = VINF_SUCCESS;

    rc = VbglR3GuestPropConnect(&u32ClientId);
    if (!RT_SUCCESS(rc))
        VBoxControlError("Failed to connect to the guest property service, error %Rrc\n", rc);

    /*
     * Retrieve the notification from the host
     */
    char *pszName = NULL;
    char *pszValue = NULL;
    uint64_t u64TimestampOut = 0;
    char *pszFlags = NULL;
    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised. */
    void *pvBuf = NULL;
    uint32_t cbBuf = MAX_NAME_LEN + MAX_VALUE_LEN + MAX_FLAGS_LEN + 1024;
    /* Because there is a race condition between our reading the size of a
     * property and the guest updating it, we loop a few times here and
     * hope.  Actually this should never go wrong, as we are generous
     * enough with buffer space. */
    bool finish = false;
    for (unsigned i = 0;
         (RT_SUCCESS(rc) || rc == VERR_BUFFER_OVERFLOW) && !finish && (i < 10);
         ++i)
    {
        void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
        if (NULL == pvTmpBuf)
        {
            rc = VERR_NO_MEMORY;
            VBoxControlError("Out of memory\n");
        }
        else
        {
            pvBuf = pvTmpBuf;
            rc = VbglR3GuestPropWait(u32ClientId, pszPatterns, pvBuf, cbBuf,
                                     u64TimestampIn, u32Timeout,
                                     &pszName, &pszValue, &u64TimestampOut,
                                     &pszFlags, &cbBuf);
        }
        if (VERR_BUFFER_OVERFLOW == rc)
            /* Leave a bit of extra space to be safe */
            cbBuf += 1024;
        else
            finish = true;
        if (rc == VERR_TOO_MUCH_DATA)
            VBoxControlError("Temporarily unable to get a notification\n");
        else if (rc == VERR_INTERRUPTED)
            VBoxControlError("The request timed out or was interrupted\n");
#ifndef RT_OS_WINDOWS  /* Windows guests do not do this right */
        else if (!RT_SUCCESS(rc) && (rc != VERR_NOT_FOUND))
            VBoxControlError("Failed to get a notification, error %Rrc\n", rc);
#endif
    }
/*
 * And display it on the guest console.
 */
    if (VERR_NOT_FOUND == rc)
        RTPrintf("No value set!\n");
    else if (rc == VERR_BUFFER_OVERFLOW)
        RTPrintf("Internal error: unable to determine the size of the data!\n");
    else if (RT_SUCCESS(rc))
    {
        RTPrintf("Name: %s\n", pszName);
        RTPrintf("Value: %s\n", pszValue);
        RTPrintf("Timestamp: %lld ns\n", u64TimestampOut);
        RTPrintf("Flags: %s\n", pszFlags);
    }

    if (u32ClientId != 0)
        VbglR3GuestPropDisconnect(u32ClientId);
    RTMemFree(pvBuf);
    return RT_SUCCESS(rc) ? 0 : 1;
}


/**
 * Access the guest property store through the "VBoxGuestPropSvc" HGCM
 * service.
 *
 * @returns 0 on success, 1 on failure
 * @note see the command line API description for parameters
 */
static int handleGuestProperty(int argc, char *argv[])
{
    if (0 == argc)
    {
        usage(GUEST_PROP);
        return 1;
    }
    if (0 == strcmp(argv[0], "get"))
        return getGuestProperty(argc - 1, argv + 1);
    else if (0 == strcmp(argv[0], "set"))
        return setGuestProperty(argc - 1, argv + 1);
    else if (0 == strcmp(argv[0], "enumerate"))
        return enumGuestProperty(argc - 1, argv + 1);
    else if (0 == strcmp(argv[0], "wait"))
        return waitGuestProperty(argc - 1, argv + 1);
    /* else */
    usage(GUEST_PROP);
    return 1;
}

#endif

/** command handler type */
typedef DECLCALLBACK(int) FNHANDLER(int argc, char *argv[]);
typedef FNHANDLER *PFNHANDLER;

/** The table of all registered command handlers. */
struct COMMANDHANDLER
{
    const char *command;
    PFNHANDLER handler;
} g_commandHandlers[] =
{
#if defined(RT_OS_WINDOWS) && !defined(VBOX_CONTROL_TEST)
    { "getvideoacceleration", handleGetVideoAcceleration },
    { "setvideoacceleration", handleSetVideoAcceleration },
    { "listcustommodes", handleListCustomModes },
    { "addcustommode", handleAddCustomMode },
    { "removecustommode", handleRemoveCustomMode },
    { "setvideomode", handleSetVideoMode },
#endif
#ifdef VBOX_WITH_GUEST_PROPS
    { "guestproperty", handleGuestProperty },
#endif
    { NULL, NULL }  /* terminator */
};

/** Main function */
int main(int argc, char **argv)
{
    /** The application's global return code */
    int rc = 0;
    /** An IPRT return code for local use */
    int rrc = VINF_SUCCESS;
    /** The index of the command line argument we are currently processing */
    int iArg = 1;
    /** Should we show the logo text? */
    bool showlogo = true;
    /** Should we print the usage after the logo?  For the -help switch. */
    bool dohelp = false;
    /** Will we be executing a command or just printing information? */
    bool onlyinfo = false;

/*
 * Start by handling command line switches
 */

    /** Are we finished with handling switches? */
    bool done = false;
    while (!done && (iArg < argc))
    {
        if (   (0 == strcmp(argv[iArg], "-v"))
            || (0 == strcmp(argv[iArg], "--version"))
            || (0 == strcmp(argv[iArg], "-version"))
            || (0 == strcmp(argv[iArg], "getversion"))
           )
            {
                /* Print version number, and do nothing else. */
                RTPrintf("%sr%d\n", VBOX_VERSION_STRING, VBoxSVNRev ());
                onlyinfo = true;
                showlogo = false;
                done = true;
            }
        else if (0 == strcmp(argv[iArg], "-nologo"))
            showlogo = false;
        else if (0 == strcmp(argv[iArg], "-help"))
        {
            onlyinfo = true;
            dohelp = true;
            done = true;
        }
        else
            /* We have found an argument which isn't a switch.  Exit to the
             * command processing bit. */
            done = true;
        if (!done)
            ++iArg;
    }

/*
 * Find the application name, show our logo if the user hasn't suppressed it,
 * and show the usage if the user asked us to
 */

    g_pszProgName = RTPathFilename(argv[0]);
    if (showlogo)
        RTPrintf("VirtualBox Guest Additions Command Line Management Interface Version "
                 VBOX_VERSION_STRING "\n"
                 "(C) 2008 Sun Microsystems, Inc.\n"
                 "All rights reserved.\n\n");
    if (dohelp)
        usage();

/*
 * Do global initialisation for the programme if we will be handling a command
 */

    if (!onlyinfo)
    {
        rrc = RTR3Init(); /** @todo r=bird: This ALWAYS goes first, the only exception is when you have to parse args to figure out which to call! */
        if (!RT_SUCCESS(rrc))
        {
            VBoxControlError("Failed to initialise the VirtualBox runtime - error %Rrc\n", rrc);
            rc = 1;
        }
        if (0 == rc)
        {
            if (!RT_SUCCESS(VbglR3Init()))
            {
                VBoxControlError("Could not contact the host system.  Make sure that you are running this\n"
                                 "application inside a VirtualBox guest system, and that you have sufficient\n"
                                 "user permissions.\n");
                rc = 1;
            }
        }
    }

/*
 * Now look for an actual command in the argument list and handle it.
 */

    if (!onlyinfo && (0 == rc))
    {
        /*
         * The input is in the guest OS'es codepage (NT guarantees ACP).
         * For VBox we use UTF-8.  For simplicity, just convert the argv[] array
         * here.
         */
        for (int i = iArg; i < argc; i++)
        {
            char *converted;
            RTStrCurrentCPToUtf8(&converted, argv[i]);
            argv[i] = converted;
        }

        if (argc > iArg)
        {
            /** Is next parameter a known command? */
            bool found = false;
            /** And if so, what is its position in the table? */
            unsigned index = 0;
            while (   index < RT_ELEMENTS(g_commandHandlers)
                   && !found
                   && (g_commandHandlers[index].command != NULL))
            {
                if (0 == strcmp(argv[iArg], g_commandHandlers[index].command))
                    found = true;
                else
                    ++index;
            }
            if (found)
                rc = g_commandHandlers[index].handler(argc - iArg - 1, argv + iArg + 1);
            else
            {
                rc = 1;
                usage();
            }
        }
        else
        {
            /* The user didn't specify a command. */
            rc = 1;
            usage();
        }

        /*
         * Free converted argument vector
         */
        for (int i = iArg; i < argc; i++)
            RTStrFree(argv[i]);

    }

/*
 * And exit, returning the status
 */

    return rc;
}
