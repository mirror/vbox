/* $Id$ */
/** @file
 * VBoxSeamless - Display notifications.
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define _WIN32_WINNT 0x0500
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include <VBoxDisplay.h>
#include <VBox/VMMDev.h>
#include <iprt/assert.h>
#include <malloc.h>
#include <VBoxGuestInternal.h>
#ifdef VBOX_WITH_WDDM
#include <iprt/asm.h>
#endif

typedef struct _VBOXDISPLAYCONTEXT
{
    const VBOXSERVICEENV *pEnv;

    BOOL fAnyX;

    /* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
    LONG (WINAPI * pfnChangeDisplaySettingsEx)(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);

    /* EnumDisplayDevices does not exist in NT. isVBoxDisplayDriverActive et al. are using these functions. */
    BOOL (WINAPI * pfnEnumDisplayDevices)(IN LPCSTR lpDevice, IN DWORD iDevNum, OUT PDISPLAY_DEVICEA lpDisplayDevice, IN DWORD dwFlags);
} VBOXDISPLAYCONTEXT;

static VBOXDISPLAYCONTEXT gCtx = {0};

#ifdef VBOX_WITH_WDDM
typedef enum
{
    VBOXDISPLAY_DRIVER_TYPE_UNKNOWN = 0,
    VBOXDISPLAY_DRIVER_TYPE_XPDM    = 1,
    VBOXDISPLAY_DRIVER_TYPE_WDDM    = 2
} VBOXDISPLAY_DRIVER_TYPE;

static VBOXDISPLAY_DRIVER_TYPE getVBoxDisplayDriverType (VBOXDISPLAYCONTEXT *pCtx);
#endif

int VBoxDisplayInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    Log(("VBoxTray: VBoxDisplayInit ...\n"));

    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);

    HMODULE hUser = GetModuleHandle("USER32");

    gCtx.pEnv = pEnv;

    if (NULL == hUser)
    {
        Log(("VBoxTray: VBoxDisplayInit: Could not get module handle of USER32.DLL!\n"));
        return VERR_NOT_IMPLEMENTED;
    }
    else if (OSinfo.dwMajorVersion >= 5)        /* APIs available only on W2K and up! */
    {
        *(uintptr_t *)&gCtx.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
        Log(("VBoxTray: VBoxDisplayInit: pfnChangeDisplaySettingsEx = %p\n", gCtx.pfnChangeDisplaySettingsEx));

        *(uintptr_t *)&gCtx.pfnEnumDisplayDevices = (uintptr_t)GetProcAddress(hUser, "EnumDisplayDevicesA");
        Log(("VBoxTray: VBoxDisplayInit: pfnEnumDisplayDevices = %p\n", gCtx.pfnEnumDisplayDevices));

#ifdef VBOX_WITH_WDDM
        if (OSinfo.dwMajorVersion >= 6)
        {
            /* this is vista and up, check if we need to switch the display driver if to WDDM mode */
            Log(("VBoxTray: VBoxDisplayInit: this is Windows Vista and up\n"));
            VBOXDISPLAY_DRIVER_TYPE enmType = getVBoxDisplayDriverType (&gCtx);
            if (enmType == VBOXDISPLAY_DRIVER_TYPE_WDDM)
            {
                Log(("VBoxTray: VBoxDisplayInit: WDDM driver is installed, switching display driver if to WDDM mode\n"));
                /* this is hacky, but the most easiest way */
                DWORD err = VBoxDispIfSwitchMode(const_cast<PVBOXDISPIF>(&pEnv->dispIf), VBOXDISPIF_MODE_WDDM, NULL /* old mode, we don't care about it */);
                if (err == NO_ERROR)
                    Log(("VBoxTray: VBoxDisplayInit: DispIf switched to WDDM mode successfully\n"));
                else
                    Log(("VBoxTray: VBoxDisplayInit: Failed to switch DispIf to WDDM mode, err (%d)\n", err));
            }
        }
#endif
    }
    else if (OSinfo.dwMajorVersion <= 4)            /* Windows NT 4.0 */
    {
        /* Nothing to do here yet */
    }
    else                                /* Unsupported platform */
    {
        Log(("VBoxTray: VBoxDisplayInit: Warning, display for platform not handled yet!\n"));
        return VERR_NOT_IMPLEMENTED;
    }

    VBOXDISPIFESCAPE_ISANYX IsAnyX = {0};
    IsAnyX.EscapeHdr.escapeCode = VBOXESC_ISANYX;
    DWORD err = VBoxDispIfEscapeInOut(&pEnv->dispIf, &IsAnyX.EscapeHdr, sizeof (uint32_t));
    if (err == NO_ERROR)
        gCtx.fAnyX = !!IsAnyX.u32IsAnyX;
    else
        gCtx.fAnyX = TRUE;

    Log(("VBoxTray: VBoxDisplayInit: Display init successful\n"));

    *pfStartThread = true;
    *ppInstance = (void *)&gCtx;
    return VINF_SUCCESS;
}

void VBoxDisplayDestroy (const VBOXSERVICEENV *pEnv, void *pInstance)
{
    return;
}

#ifdef VBOX_WITH_WDDM
static VBOXDISPLAY_DRIVER_TYPE getVBoxDisplayDriverType(VBOXDISPLAYCONTEXT *pCtx)
#else
static bool isVBoxDisplayDriverActive(VBOXDISPLAYCONTEXT *pCtx)
#endif
{
#ifdef VBOX_WITH_WDDM
    VBOXDISPLAY_DRIVER_TYPE enmType = VBOXDISPLAY_DRIVER_TYPE_UNKNOWN;
#else
    bool result = false;
#endif

    if( pCtx->pfnEnumDisplayDevices )
    {
        INT devNum = 0;
        DISPLAY_DEVICE dispDevice;
        FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);
        dispDevice.cb = sizeof(DISPLAY_DEVICE);

        Log(("VBoxTray: isVBoxDisplayDriverActive: Checking for active VBox display driver (W2K+) ...\n"));

        while (EnumDisplayDevices(NULL,
                                  devNum,
                                  &dispDevice,
                                  0))
        {
            Log(("VBoxTray: isVBoxDisplayDriverActive: DevNum:%d\nName:%s\nString:%s\nID:%s\nKey:%s\nFlags=%08X\n\n",
                          devNum,
                          &dispDevice.DeviceName[0],
                          &dispDevice.DeviceString[0],
                          &dispDevice.DeviceID[0],
                          &dispDevice.DeviceKey[0],
                          dispDevice.StateFlags));

            if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
            {
                Log(("VBoxTray: isVBoxDisplayDriverActive: Primary device\n"));

                if (strcmp(&dispDevice.DeviceString[0], "VirtualBox Graphics Adapter") == 0)
#ifndef VBOX_WITH_WDDM
                    result = true;
#else
                    enmType = VBOXDISPLAY_DRIVER_TYPE_XPDM;
                /* WDDM driver can now have multiple incarnations,
                 * if the driver name contains VirtualBox, and does NOT match the XPDM name,
                 * assume it to be WDDM */
                else if (strstr(&dispDevice.DeviceString[0], "VirtualBox"))
                    enmType = VBOXDISPLAY_DRIVER_TYPE_WDDM;
#endif
                break;
            }

            FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);

            dispDevice.cb = sizeof(DISPLAY_DEVICE);

            devNum++;
        }
    }
    else    /* This must be NT 4 or something really old, so don't use EnumDisplayDevices() here  ... */
    {
        Log(("VBoxTray: isVBoxDisplayDriverActive: Checking for active VBox display driver (NT or older) ...\n"));

        DEVMODE tempDevMode;
        ZeroMemory (&tempDevMode, sizeof (tempDevMode));
        tempDevMode.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &tempDevMode);     /* Get current display device settings */

        /* Check for the short name, because all long stuff would be truncated */
        if (strcmp((char*)&tempDevMode.dmDeviceName[0], "VBoxDisp") == 0)
#ifndef VBOX_WITH_WDDM
            result = true;
#else
            enmType = VBOXDISPLAY_DRIVER_TYPE_XPDM;
#endif
    }

#ifndef VBOX_WITH_WDDM
    return result;
#else
    return enmType;
#endif
}

static DWORD EnableAndResizeDispDev(DEVMODE *paDeviceModes, DISPLAY_DEVICE *paDisplayDevices, DWORD totalDispNum, ULONG Id, DWORD aWidth, DWORD aHeight,
                                    DWORD aBitsPerPixel, DWORD aPosX, DWORD aPosY, BOOL fEnabled, BOOL fExtDispSup, VBOXDISPLAYCONTEXT *pCtx)
{
    DISPLAY_DEVICE displayDeviceTmp;
    DISPLAY_DEVICE displayDevice;
    DEVMODE deviceMode;
    DWORD dwStatus = DISP_CHANGE_SUCCESSFUL;
    DWORD iter ;

    deviceMode = paDeviceModes[Id];
    displayDevice = paDisplayDevices[Id];

    for (iter = 0; iter < totalDispNum; iter++)
    {
        if (iter != 0 && iter != Id && !(paDisplayDevices[iter].StateFlags & DISPLAY_DEVICE_ACTIVE))
        {
            LogRel(("VBoxTray:Initially disabling the monitor with id = %d . Total Monitor=%d\n", iter, totalDispNum));
            DEVMODE deviceModeTmp;
            ZeroMemory(&deviceModeTmp, sizeof(DEVMODE));
            deviceModeTmp.dmSize = sizeof(DEVMODE);
            deviceModeTmp.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_POSITION
                                     | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS ;
            displayDeviceTmp = paDisplayDevices[iter];
            gCtx.pfnChangeDisplaySettingsEx(displayDeviceTmp.DeviceName, &deviceModeTmp, NULL,
                                           (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
        }
    }

    if (fExtDispSup) /* Extended Display Support possible*/
    {
        if (fEnabled)
        {
            /* Special case for enabling the secondary monitor. */
            if(!(displayDevice.StateFlags & DISPLAY_DEVICE_ACTIVE))
            {
                LogRel(("VBoxTray: Secondary Monitor with ID=%d and name=%s Not Enabled. Enabling it.\n", Id, displayDevice.DeviceName));
                deviceMode.dmPosition.x = paDeviceModes[0].dmPelsWidth;
                deviceMode.dmPosition.y = 0;
                deviceMode.dmBitsPerPel = 32;
                OSVERSIONINFO OSinfo;
                OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
                GetVersionEx (&OSinfo);

                if (OSinfo.dwMajorVersion < 6)
                    /* dont any more flags here as, only DM_POISITON is used to enable the secondary display */
                    deviceMode.dmFields = DM_POSITION;
                else /* for win 7 and above */
                    /* for vista and aboce DM_BITSPERPELis necessary */
                    deviceMode.dmFields =   DM_BITSPERPEL | DM_DISPLAYFLAGS | DM_DISPLAYFREQUENCY  | DM_POSITION;

                dwStatus = gCtx.pfnChangeDisplaySettingsEx((LPSTR)displayDevice.DeviceName,&deviceMode, NULL, (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
                /* A second call to ChangeDisplaySettings updates the monitor.*/
                gCtx.pfnChangeDisplaySettingsEx(NULL, NULL, NULL,0, NULL);
            }
            else /* secondary monitor already enabled. Request to change the resolution or position. */
            {
                if (aWidth !=0 && aHeight != 0)
                {
                    LogRel(("VBoxTray: Display : %s , Change Height: %d & Width: %d\n", displayDevice.DeviceName, aWidth, aHeight));
                    deviceMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL
                                          | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS;
                    deviceMode.dmPelsWidth = aWidth;
                    deviceMode.dmPelsHeight = aHeight;
                    deviceMode.dmBitsPerPel = aBitsPerPixel;
                }
                if (aPosX != 0 || aPosY != 0)
                {
                    LogRel(("VBoxTray: Display: %s PosX: %d, PosY: %d\n", displayDevice.DeviceName, aPosX, aPosY));
                    deviceMode.dmFields |=  DM_POSITION;
                    deviceMode.dmPosition.x = aPosX;
                    deviceMode.dmPosition.y = aPosY;
                }
                dwStatus = gCtx.pfnChangeDisplaySettingsEx((LPSTR)displayDevice.DeviceName,
                                                   &deviceMode, NULL, CDS_NORESET|CDS_UPDATEREGISTRY, NULL);
                /* A second call to ChangeDisplaySettings updates the monitor. */
                gCtx.pfnChangeDisplaySettingsEx(NULL, NULL, NULL,0, NULL);
            }
        }
        else /* Request is there to disable the monitor with ID = Id*/
        {
            LogRel(("VBoxTray: Disable the Display: %d\n", displayDevice.DeviceName));

            DEVMODE deviceModeTmp;
            ZeroMemory(&deviceModeTmp, sizeof(DEVMODE));
            deviceModeTmp.dmSize = sizeof(DEVMODE);
            deviceModeTmp.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL | DM_POSITION
                                     | DM_DISPLAYFREQUENCY | DM_DISPLAYFLAGS ;
            displayDeviceTmp = paDisplayDevices[Id];
            dwStatus = gCtx.pfnChangeDisplaySettingsEx(displayDeviceTmp.DeviceName, &deviceModeTmp, NULL,
                                           (CDS_UPDATEREGISTRY | CDS_NORESET), NULL);
            gCtx.pfnChangeDisplaySettingsEx(NULL, NULL, NULL,0, NULL);
        }
    }
    return dwStatus;
 }

/* Returns TRUE to try again. */
static BOOL ResizeDisplayDevice(ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel,
                                BOOL fEnabled, DWORD dwNewPosX, DWORD dwNewPosY,
                                VBOXDISPLAYCONTEXT *pCtx, BOOL fExtDispSup)
{
    BOOL fDispAlreadyEnabled = false; /* check whether the monitor with ID is already enabled. */
    BOOL fModeReset = (Width == 0 && Height == 0 && BitsPerPixel == 0 &&
                       dwNewPosX == 0 && dwNewPosY == 0);
    BOOL fChangePosRequest = false; /* change in position requested */

    Log(("VBoxTray: ResizeDisplayDevice Width= %d, Height=%d , PosX=%d and PosY=%d \
         fEnabled = %d, fExtDisSup = %d\n",
          Width, Height, dwNewPosX, dwNewPosY, fEnabled, fExtDispSup));

    if (!gCtx.fAnyX)
        Width &= 0xFFF8;

    DISPLAY_DEVICE DisplayDevice;
    DWORD dwStatus;

    ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
    DisplayDevice.cb = sizeof(DISPLAY_DEVICE);

    VBoxDispIfCancelPendingResize(&pCtx->pEnv->dispIf);

    /* Find out how many display devices the system has */
    DWORD NumDevices = 0;
    DWORD i = 0;
    while (EnumDisplayDevices (NULL, i, &DisplayDevice, 0))
    {
        Log(("VBoxTray: ResizeDisplayDevice: [%d] %s\n", i, DisplayDevice.DeviceName));

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("VBoxTray: ResizeDisplayDevice: Found primary device. err %d\n", GetLastError ()));
            NumDevices++;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("VBoxTray: ResizeDisplayDevice: Found secondary device. err %d\n", GetLastError ()));
            NumDevices++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
        DisplayDevice.cb = sizeof(DisplayDevice);
        i++;
    }

    Log(("VBoxTray: ResizeDisplayDevice: Found total %d devices. err %d\n", NumDevices, GetLastError ()));

    if (NumDevices == 0 || Id >= NumDevices)
    {
        Log(("VBoxTray: ResizeDisplayDevice: Requested identifier %d is invalid. err %d\n", Id, GetLastError ()));
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
        Log(("VBoxTray: ResizeDisplayDevice: [%d(%d)] %s\n", i, DevNum, DisplayDevice.DeviceName));

        BOOL bFetchDevice = FALSE;

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            Log(("VBoxTray: ResizeDisplayDevice: Found primary device. err %d\n", GetLastError ()));
            DevPrimaryNum = DevNum;
            bFetchDevice = TRUE;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {

            Log(("VBoxTray: ResizeDisplayDevice: Found secondary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }

        if (bFetchDevice)
        {
            if (DevNum >= NumDevices)
            {
                Log(("VBoxTray: ResizeDisplayDevice: %d >= %d\n", NumDevices, DevNum));
                return FALSE;
            }

            paDisplayDevices[DevNum] = DisplayDevice;

            /* First try to get the video mode stored in registry (ENUM_REGISTRY_SETTINGS).
             * A secondary display could be not active at the moment and would not have
             * a current video mode (ENUM_CURRENT_SETTINGS).
             */
            ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
            paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_REGISTRY_SETTINGS, &paDeviceModes[DevNum]))
            {
                Log(("VBoxTray: ResizeDisplayDevice: EnumDisplaySettings error %d\n", GetLastError ()));
            }

            if (   paDeviceModes[DevNum].dmPelsWidth == 0
                || paDeviceModes[DevNum].dmPelsHeight == 0)
            {
                /* No ENUM_REGISTRY_SETTINGS yet. Seen on Vista after installation.
                 * Get the current video mode then.
                 */
                ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
                paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
                if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                     ENUM_CURRENT_SETTINGS, &paDeviceModes[DevNum]))
                {
                    /* ENUM_CURRENT_SETTINGS returns FALSE when the display is not active:
                     * for example a disabled secondary display.
                     * Do not return here, ignore the error and set the display info to 0x0x0.
                     */
                    Log(("VBoxTray: ResizeDisplayDevice: EnumDisplaySettings(ENUM_CURRENT_SETTINGS) error %d\n", GetLastError ()));
                }
            }

            if (fExtDispSup)
            {
                LogRel(("VBoxTray: Extended Display Support.\n"));
                Log(("VBoxTray: ResizeDisplayDevice1: %dx%dx%d at %d,%d . Id = %d and DevNum=%d, fEnabled=%d\n",
                      paDeviceModes[Id].dmPelsWidth,
                      paDeviceModes[Id].dmPelsHeight,
                      paDeviceModes[Id].dmBitsPerPel,
                      paDeviceModes[Id].dmPosition.x,
                      paDeviceModes[Id].dmPosition.y,
                      Id, DevNum, fEnabled));
                if ((DevNum == Id && fEnabled == 1))
                {
                    /* Calculation of new position for enabled
                     * secondary monitor .
                     */
                    /* Used when a secondary monitor just needs to be enabled, without any
                     * change in its position
                     */
                     if (dwNewPosX != 0)
                    {
                        LogRel(("VBoxTray: Setting Rectangle position x=%d*y=%d\n", dwNewPosX, dwNewPosY));
                        paDeviceModes[DevNum].dmPosition.x = dwNewPosX;
                        paDeviceModes[DevNum].dmPosition.y = dwNewPosY;
                        fChangePosRequest = true;
                    }
                    paRects[DevNum].left   = paDeviceModes[DevNum].dmPosition.x;
                    paRects[DevNum].top    = paDeviceModes[DevNum].dmPosition.y;
                    paRects[DevNum].right  = paDeviceModes[DevNum].dmPosition.x + paDeviceModes[DevNum].dmPelsWidth;
                    paRects[DevNum].bottom = paDeviceModes[DevNum].dmPosition.y + paDeviceModes[DevNum].dmPelsHeight;
                }
                else
                {
                    paRects[DevNum].left   = paDeviceModes[DevNum].dmPosition.x;
                    paRects[DevNum].top    = paDeviceModes[DevNum].dmPosition.y;
                    paRects[DevNum].right  = paDeviceModes[DevNum].dmPosition.x + paDeviceModes[DevNum].dmPelsWidth;
                    paRects[DevNum].bottom = paDeviceModes[DevNum].dmPosition.y + paDeviceModes[DevNum].dmPelsHeight;
                }
            }
            else
            {
                    LogRel(("VBoxTray: NO Ext Display Support \n"));
                    paRects[DevNum].left   = paDeviceModes[DevNum].dmPosition.x;
                    paRects[DevNum].top    = paDeviceModes[DevNum].dmPosition.y;
                    paRects[DevNum].right  = paDeviceModes[DevNum].dmPosition.x + paDeviceModes[DevNum].dmPelsWidth;
                    paRects[DevNum].bottom = paDeviceModes[DevNum].dmPosition.y + paDeviceModes[DevNum].dmPelsHeight;
            }
            DevNum++;
        }

        ZeroMemory(&DisplayDevice, sizeof(DISPLAY_DEVICE));
        DisplayDevice.cb = sizeof(DISPLAY_DEVICE);
        i++;
    }
    /* Keep a record if the display with ID is already active or not. */
    if (paDisplayDevices[Id].StateFlags & DISPLAY_DEVICE_ACTIVE)
    {
        LogRel(("VBoxTray: Display with ID=%d already enabled\n", Id));
        fDispAlreadyEnabled = TRUE;
    }

    /* Width, height equal to 0 means that this value must be not changed.
     * Update input parameters if necessary.
     * Note: BitsPerPixel is taken into account later, when new rectangles
     *       are assigned to displays.
     */
    if (Width == 0)
    {
        Width = paRects[Id].right - paRects[Id].left;
    }

    if (Height == 0)
    {
        Height = paRects[Id].bottom - paRects[Id].top;
    }

    /* Check whether a mode reset or a change is requested.
     * Rectangle position is recalculated only if fEnabled is 1.
     * For non extended supported modes (old Host VMs), fEnabled
     * is always 1.
     */
    /* Handled the case where previouseresolution of secondary monitor
     * was for eg. 1024*768*32 and monitor was in disabled state.
     * User gives the command
     * setvideomode 1024 768 32 1 yes.
     * Now in this case the resolution request is same as previous one but
     * monitor is going from disabled to enabled state so the below condition
     * shour return false
     * The below condition will only return true , if no mode reset has
     * been requested AND fEnabled is 1 and fDispAlreadyEnabled is also 1 AND
     * all rect conditions are true. Thus in this case nothing has to be done.
     */
    if ( !fModeReset && fEnabled && fDispAlreadyEnabled && !fChangePosRequest
        && paRects[Id].right - paRects[Id].left == Width
        && paRects[Id].bottom - paRects[Id].top == Height
        && paDeviceModes[Id].dmBitsPerPel == BitsPerPixel)
    {
        LogRel(("VBoxTray: Already at desired resolution. No Change.\n"));
        return FALSE;
    }

    hlpResizeRect(paRects, NumDevices, DevPrimaryNum, Id, Width, Height);
#ifdef Log
    for (i = 0; i < NumDevices; i++)
    {
        Log(("VBoxTray: ResizeDisplayDevice: [%d]: %d,%d %dx%d\n",
                i, paRects[i].left, paRects[i].top,
                paRects[i].right - paRects[i].left,
                paRects[i].bottom - paRects[i].top));
    }
#endif /* Log */

#ifdef VBOX_WITH_WDDM
    VBOXDISPLAY_DRIVER_TYPE enmDriverType = getVBoxDisplayDriverType (pCtx);
    if (enmDriverType == VBOXDISPLAY_DRIVER_TYPE_WDDM)
    {
        /* Assign the new rectangles to displays. */
        for (i = 0; i < NumDevices; i++)
        {
            paDeviceModes[i].dmPosition.x = paRects[i].left;
            paDeviceModes[i].dmPosition.y = paRects[i].top;
            paDeviceModes[i].dmPelsWidth  = paRects[i].right - paRects[i].left;
            paDeviceModes[i].dmPelsHeight = paRects[i].bottom - paRects[i].top;

            /* On Vista one must specify DM_BITSPERPEL.
             * Note that the current mode dmBitsPerPel is already in the DEVMODE structure.
             */
            paDeviceModes[i].dmFields = DM_PELSHEIGHT | DM_PELSWIDTH | DM_BITSPERPEL;

            if (i == Id && BitsPerPixel != 0)
            {
                LogRel(("VBoxTray: (WDDM)Changing resolution and position. \n"));
                /* Change dmBitsPerPel if requested. */
                paDeviceModes[i].dmBitsPerPel = BitsPerPixel;
                paDeviceModes[i].dmPelsWidth = Width;
                paDeviceModes[i].dmPelsHeight = Height;
                if (dwNewPosX != 0 || dwNewPosY != 0)
                {
                    paDeviceModes[Id].dmPosition.x = dwNewPosX;
                    paDeviceModes[Id].dmPosition.y = dwNewPosY;
                }
                else
                {
                    paDeviceModes[i].dmFields |= DM_POSITION;
                    paDeviceModes[Id].dmPosition.x = 0;
                    paDeviceModes[Id].dmPosition.y = 0;
                }
            }

            Log(("VBoxTray: (WDDM) ResizeDisplayDevice: pfnChangeDisplaySettingsEx %x: %dx%dx%d at %d,%d\n",
                  gCtx.pfnChangeDisplaySettingsEx,
                  paDeviceModes[i].dmPelsWidth,
                  paDeviceModes[i].dmPelsHeight,
                  paDeviceModes[i].dmBitsPerPel,
                  paDeviceModes[i].dmPosition.x,
                  paDeviceModes[i].dmPosition.y));

        }
        /* Reques to enable /disable the secondary Display Device. Won't take the resize request now.*/
        if (!fDispAlreadyEnabled && fEnabled || fDispAlreadyEnabled && !fEnabled)
        {
              OSVERSIONINFO OSinfo;
              OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
              GetVersionEx (&OSinfo);

              /* for win 7 and above */
              if (OSinfo.dwMajorVersion >= 6 && OSinfo.dwMinorVersion >= 1)
              {
                  LogRel(("VBoxTray: (WDDM) Request to enable/disable %d display device\n", fEnabled));
                  DWORD dwStatus = vboxDispIfWddmEnableDisplay(&pCtx->pEnv->dispIf, Id, fEnabled);
                  if(dwStatus != ERROR_SUCCESS)
                  {
                      /* Not going to retry for enabling or disabling of the secondary display device.*/
                      LogRel(("VBoxTray: (WDDM) Failed to enable the Display Device \n"));
                  }
              }
              else /* case: vista in wddm mode. SetDisplayConfig APIs etc is not avilable in this mode. */
              {
                  /* use traditional approach of ChangeDisplaySettingEx to enable/disable secondary monitor for Vista WDDM mode.*/
                  dwStatus = EnableAndResizeDispDev(paDeviceModes, paDisplayDevices, DevNum,  Id, Width, Height, BitsPerPixel,
                                                    dwNewPosX, dwNewPosY, fEnabled, fExtDispSup, pCtx);
                  if (dwStatus != DISP_CHANGE_SUCCESSFUL )
                  {
                      /* Successfully set new video mode or our driver can not set
                       * the requested mode. Stop trying.
                       */
                       LogRel(("VBoxTray: (WDDM) Failed to enable/disable the Display Device \n"));
                  }
              }
              return FALSE; /* for enable disable not retrying */
        }
        else
        {
            /* Resize request. Secondary display device should be in an enabled state. */
            LogRel(("VBoxTray: (WDDM) Request to resize the display \n"));
            DWORD err = VBoxDispIfResizeModes(&pCtx->pEnv->dispIf, Id, paDisplayDevices, paDeviceModes, NumDevices);
            if (err == NO_ERROR || err != ERROR_RETRY)
            {
                if (err == NO_ERROR)
                    LogRel(("VBoxTray: VBoxDisplayThread: (WDDM) VBoxDispIfResizeModes succeeded\n"));
                else
                    LogRel(("VBoxTray: VBoxDisplayThread: (WDDM) Failure VBoxDispIfResizeModes (%d)\n", err));
                return FALSE;
            }
        }
        Log(("VBoxTray: ResizeDisplayDevice: (WDDM) RETRY requested\n"));
        return TRUE;
    }
#endif
    /* Without this, Windows will not ask the miniport for its
     * mode table but uses an internal cache instead.
     */
    for (i = 0; i < NumDevices; i++)
    {
        DEVMODE tempDevMode;
        ZeroMemory (&tempDevMode, sizeof (tempDevMode));
        tempDevMode.dmSize = sizeof(DEVMODE);
        EnumDisplaySettings((LPSTR)paDisplayDevices[i].DeviceName, 0xffffff, &tempDevMode);
        Log(("VBoxTray: ResizeDisplayDevice: EnumDisplaySettings last error %d\n", GetLastError ()));
    }

    /* Assign the new rectangles to displays. */
    for (i = 0; i < NumDevices; i++)
    {
        paDeviceModes[i].dmPosition.x = paRects[i].left;
        paDeviceModes[i].dmPosition.y = paRects[i].top;
        paDeviceModes[i].dmPelsWidth  = paRects[i].right - paRects[i].left;
        paDeviceModes[i].dmPelsHeight = paRects[i].bottom - paRects[i].top;

        /* On Vista one must specify DM_BITSPERPEL.
         * Note that the current mode dmBitsPerPel is already in the DEVMODE structure.
         */
        paDeviceModes[i].dmFields = DM_POSITION | DM_PELSHEIGHT | DM_PELSWIDTH | DM_BITSPERPEL;

        if (   i == Id
            && BitsPerPixel != 0)
        {
            /* Change dmBitsPerPel if requested. */
            paDeviceModes[i].dmBitsPerPel = BitsPerPixel;
        }

        Log(("VBoxTray: ResizeDisplayDevice: pfnChangeDisplaySettingsEx Current MonitorId=%d: %dx%dx%d at %d,%d\n",
              i,
              paDeviceModes[i].dmPelsWidth,
              paDeviceModes[i].dmPelsHeight,
              paDeviceModes[i].dmBitsPerPel,
              paDeviceModes[i].dmPosition.x,
              paDeviceModes[i].dmPosition.y));

        LONG status = gCtx.pfnChangeDisplaySettingsEx((LPSTR)paDisplayDevices[i].DeviceName,
                                        &paDeviceModes[i], NULL, CDS_NORESET | CDS_UPDATEREGISTRY, NULL);
        Log(("VBoxTray: ResizeDisplayDevice: ChangeDisplaySettingsEx position status %d, err %d\n", status, GetLastError ()));
    }

    Log(("VBoxTray: Enable And Resize Device. Id = %d, Width=%d Height=%d, \
         dwNewPosX = %d, dwNewPosY = %d fEnabled=%d & fExtDispSupport = %d \n",
         Id, Width, Height, dwNewPosX, dwNewPosY, fEnabled, fExtDispSup));
    dwStatus = EnableAndResizeDispDev(paDeviceModes, paDisplayDevices, DevNum, Id, Width, Height, BitsPerPixel,
                                      dwNewPosX, dwNewPosY, fEnabled, fExtDispSup, pCtx);
    if (dwStatus == DISP_CHANGE_SUCCESSFUL || dwStatus == DISP_CHANGE_BADMODE)
    {
        /* Successfully set new video mode or our driver can not set
         * the requested mode. Stop trying.
         */
        return FALSE;
    }
    /* Retry the request. */
    return TRUE;
}

/**
 * Thread function to wait for and process display change
 * requests
 */
unsigned __stdcall VBoxDisplayThread(void *pInstance)
{
    Log(("VBoxTray: VBoxDisplayThread: Entered\n"));

    VBOXDISPLAYCONTEXT *pCtx = (VBOXDISPLAYCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
    maskInfo.u32NotMask = 0;
    if (!DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        Log(("VBoxTray: VBoxDisplayThread: DeviceIOControl(CtlMask - or) failed, thread exiting\n"));
        return 0;
    }

    int rc = VbglR3SetGuestCaps(VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0);
    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxTray: VBoxDisplayThread: Failed to set the graphics capability with rc=%Rrc, thread exiting\n", rc));
        return 0;
    }

    do
    {
        BOOL fExtDispSup = TRUE;
        /* Wait for a display change event. */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 1000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
        if (DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            /*Log(("VBoxTray: VBoxDisplayThread: DeviceIOControl succeeded\n"));*/

            if (NULL == pCtx) {
                Log(("VBoxTray: VBoxDisplayThread: Invalid context detected!\n"));
                break;
            }

            if (NULL == pCtx->pEnv) {
                Log(("VBoxTray: VBoxDisplayThread: Invalid context environment detected!\n"));
                break;
            }

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            /*Log(("VBoxTray: VBoxDisplayThread: checking event\n"));*/

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
            {
                Log(("VBoxTray: VBoxDisplayThread: going to get display change information\n"));
                BOOL fDisplayChangeQueried;


                /* We got at least one event. Read the requested resolution
                 * and try to set it until success. New events will not be seen
                 * but a new resolution will be read in this poll loop.
                 */
                /* Try if extended mode display information is available from the host. */
                VMMDevDisplayChangeRequestEx displayChangeRequest = {0};
                fExtDispSup                             = TRUE;
                displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequestEx);
                displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequestEx;
                displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevDisplayChangeRequestEx)), &displayChangeRequest, sizeof(VMMDevDisplayChangeRequestEx),
                                                                 &displayChangeRequest, sizeof(VMMDevDisplayChangeRequestEx), &cbReturned, NULL);

               if (!fDisplayChangeQueried)
               {
                    Log(("VBoxTray: Extended Display Not Supported. Trying VMMDevDisplayChangeRequest2\n"));
                    fExtDispSup = FALSE; /* Extended display Change request is not supported */

                    displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest2);
                    displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                    displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest2;
                    displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                    fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevDisplayChangeRequest2)), &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2),
                                                             &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2), &cbReturned, NULL);
                    displayChangeRequest.cxOrigin = 0;
                    displayChangeRequest.cyOrigin = 0;
                    displayChangeRequest.fChangeOrigin = 0;
                    displayChangeRequest.fEnabled = 1; /* Always Enabled for old VMs on Host.*/
                }

                if (!fDisplayChangeQueried)
                {
                    Log(("VBoxTray: Extended Display Not Supported. Trying VMMDevDisplayChangeRequest\n"));
                    fExtDispSup = FALSE; /*Extended display Change request is not supported */
                    /* Try the old version of the request for old VBox hosts. */
                    displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest);
                    displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                    displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest;
                    displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                    fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(VMMDevDisplayChangeRequest)), &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest),
                                                             &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest), &cbReturned, NULL);
                    displayChangeRequest.display = 0;
                    displayChangeRequest.cxOrigin = 0;
                    displayChangeRequest.cyOrigin = 0;
                    displayChangeRequest.fChangeOrigin = 0;
                    displayChangeRequest.fEnabled = 1; /* Always Enabled for old VMs on Host.*/
                }

                if (fDisplayChangeQueried)
                {
                    /* Try to set the requested video mode. Repeat until it is successful or is rejected by the driver. */
                    for (;;)
                    {
                        Log(("VBoxTray: VBoxDisplayThread: VMMDevReq_GetDisplayChangeRequest2: %dx%dx%d at %d\n", displayChangeRequest.xres, displayChangeRequest.yres, displayChangeRequest.bpp, displayChangeRequest.display));

                        /*
                         * Only try to change video mode if the active display driver is VBox additions.
                         */
#ifdef VBOX_WITH_WDDM
                        VBOXDISPLAY_DRIVER_TYPE enmDriverType = getVBoxDisplayDriverType (pCtx);

                        if (enmDriverType == VBOXDISPLAY_DRIVER_TYPE_WDDM)
                            Log(("VBoxTray: VBoxDisplayThread: Detected WDDM Driver\n"));

                        if (enmDriverType != VBOXDISPLAY_DRIVER_TYPE_UNKNOWN)
#else
                        if (isVBoxDisplayDriverActive (pCtx))
#endif
                        {
                            Log(("VBoxTray: VBoxDisplayThread: Display driver is active!\n"));

                            if (pCtx->pfnChangeDisplaySettingsEx != 0)
                            {
                                Log(("VBoxTray: VBoxDisplayThread: Detected W2K or later\n"));
                                /* W2K or later. */
                                Log(("DisplayChangeReqEx parameters  aDisplay=%d x xRes=%d x yRes=%d x bpp=%d x SecondayMonEnb=%d x NewOriginX=%d x NewOriginY=%d x ChangeOrigin=%d\n",
                                     displayChangeRequest.display,
                                     displayChangeRequest.xres,
                                     displayChangeRequest.yres,
                                     displayChangeRequest.bpp,
                                     displayChangeRequest.fEnabled,
                                     displayChangeRequest.cxOrigin,
                                     displayChangeRequest.cyOrigin,
                                     displayChangeRequest.fChangeOrigin));
                                if (!ResizeDisplayDevice(displayChangeRequest.display,
                                                         displayChangeRequest.xres,
                                                         displayChangeRequest.yres,
                                                         displayChangeRequest.bpp,
                                                         displayChangeRequest.fEnabled,
                                                         displayChangeRequest.cxOrigin,
                                                         displayChangeRequest.cyOrigin,
                                                         pCtx,
                                                         fExtDispSup
                                                         ))
                                {
                                    Log(("ResizeDipspalyDevice return 0\n"));
                                    break;
                                }

                            }
                            else
                            {
                                Log(("VBoxTray: VBoxDisplayThread: Detected NT\n"));

                                /* Single monitor NT. */
                                DEVMODE devMode;
                                RT_ZERO(devMode);
                                devMode.dmSize = sizeof(DEVMODE);

                                /* get the current screen setup */
                                if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &devMode))
                                {
                                    Log(("VBoxTray: VBoxDisplayThread: Current mode: %d x %d x %d at %d,%d\n",
                                          devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel, devMode.dmPosition.x, devMode.dmPosition.y));

                                    /* Check whether a mode reset or a change is requested. */
                                    if (displayChangeRequest.xres || displayChangeRequest.yres || displayChangeRequest.bpp)
                                    {
                                        /* A change is requested.
                                         * Set values which are not to be changed to the current values.
                                         */
                                        if (!displayChangeRequest.xres)
                                            displayChangeRequest.xres = devMode.dmPelsWidth;
                                        if (!displayChangeRequest.yres)
                                            displayChangeRequest.yres = devMode.dmPelsHeight;
                                        if (!displayChangeRequest.bpp)
                                            displayChangeRequest.bpp = devMode.dmBitsPerPel;
                                    }
                                    else
                                    {
                                        /* All zero values means a forced mode reset. Do nothing. */
                                        Log(("VBoxTray: VBoxDisplayThread: Forced mode reset\n"));
                                    }

                                    /* Verify that the mode is indeed changed. */
                                    if (   devMode.dmPelsWidth  == displayChangeRequest.xres
                                        && devMode.dmPelsHeight == displayChangeRequest.yres
                                        && devMode.dmBitsPerPel == displayChangeRequest.bpp)
                                    {
                                        Log(("VBoxTray: VBoxDisplayThread: already at desired resolution\n"));
                                        break;
                                    }

                                    // without this, Windows will not ask the miniport for its
                                    // mode table but uses an internal cache instead
                                    DEVMODE tempDevMode = {0};
                                    tempDevMode.dmSize = sizeof(DEVMODE);
                                    EnumDisplaySettings(NULL, 0xffffff, &tempDevMode);

                                    /* adjust the values that are supposed to change */
                                    if (displayChangeRequest.xres)
                                        devMode.dmPelsWidth  = displayChangeRequest.xres;
                                    if (displayChangeRequest.yres)
                                        devMode.dmPelsHeight = displayChangeRequest.yres;
                                    if (displayChangeRequest.bpp)
                                        devMode.dmBitsPerPel = displayChangeRequest.bpp;

                                    Log(("VBoxTray: VBoxDisplayThread: setting new mode %d x %d, %d BPP\n",
                                         devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel));

                                    /* set the new mode */
                                    LONG status = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
                                    if (status != DISP_CHANGE_SUCCESSFUL)
                                    {
                                        Log(("VBoxTray: VBoxDisplayThread: error from ChangeDisplaySettings: %d\n", status));

                                        if (status == DISP_CHANGE_BADMODE)
                                        {
                                            /* Our driver can not set the requested mode. Stop trying. */
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        /* Successfully set new video mode. */
                                        break;
                                    }
                                }
                                else
                                {
                                    Log(("VBoxTray: VBoxDisplayThread: error from EnumDisplaySettings: %d\n", GetLastError ()));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            Log(("VBoxTray: VBoxDisplayThread: vboxDisplayDriver is not active\n"));
                        }

                        /* Retry the change a bit later. */
                        /* are we supposed to stop? */
                        if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 1000) == WAIT_OBJECT_0)
                        {
                            fTerminate = true;
                            break;
                        }
                    }
                }
                else
                {
                    Log(("VBoxTray: VBoxDisplayThread: error from DeviceIoControl VBOXGUEST_IOCTL_VMMREQUEST\n"));
                    /* sleep a bit to not eat too much CPU while retrying */
                    /* are we supposed to stop? */
                    if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 50) == WAIT_OBJECT_0)
                    {
                        fTerminate = true;
                        break;
                    }
                }
            }
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED)
                hlpReloadCursor();
        } else
        {
            Log(("VBoxTray: VBoxDisplayThread: error 0 from DeviceIoControl VBOXGUEST_IOCTL_WAITEVENT\n"));
            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
    } while (!fTerminate);

    /*
     * Remove event filter and graphics capability report.
     */
    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST | VMMDEV_EVENT_MOUSE_CAPABILITIES_CHANGED;
    if (!DeviceIoControl(gVBoxDriver, VBOXGUEST_IOCTL_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
        Log(("VBoxTray: VBoxDisplayThread: DeviceIOControl(CtlMask - not) failed\n"));
    VbglR3SetGuestCaps(0, VMMDEV_GUEST_SUPPORTS_GRAPHICS);

    Log(("VBoxTray: VBoxDisplayThread: finished display change request thread\n"));
    return 0;
}
