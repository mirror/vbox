/** @file
 * VBoxService - Guest Additions Service
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBoxService.h"
#include "VBoxSeamless.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>

#include "helpers.h"

/* global variables */
HANDLE                gVBoxDriver;
HANDLE                gStopSem;
SERVICE_STATUS        gVBoxServiceStatus;
SERVICE_STATUS_HANDLE gVBoxServiceStatusHandle;
HINSTANCE             gInstance;
HWND                  gToolWindow;


/* prototypes */
VOID DisplayChangeThread(void *dummy);
LRESULT CALLBACK VBoxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);


#ifdef DEBUG
/**
 * Helper function to send a message to WinDbg
 *
 * @param String message string
 */
void WriteLog(char *String, ...)
{
    DWORD cbReturned;
    CHAR Buffer[1024];
    VMMDevReqLogString *pReq = (VMMDevReqLogString *)Buffer;

    va_list va;

    va_start(va, String);

    vmmdevInitRequest(&pReq->header, VMMDevReq_LogString);
    vsprintf(pReq->szString, String, va);
    OutputDebugStringA(pReq->szString);
    pReq->header.size += strlen(pReq->szString);

    DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, pReq, pReq->header.size,
                    pReq, pReq->header.size, &cbReturned, NULL);

    va_end (va);
    return;
}
#endif



/* The shared clipboard service prototypes. */
int                VBoxClipboardInit    (const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread);
unsigned __stdcall VBoxClipboardThread  (void *pInstance);
void               VBoxClipboardDestroy (const VBOXSERVICEENV *pEnv, void *pInstance);

/* The service table. */
static VBOXSERVICEINFO vboxServiceTable[] =
{
    {
        "Shared Clipboard",
        VBoxClipboardInit,
        VBoxClipboardThread,
        VBoxClipboardDestroy
    },
    {
        "Seamless Windows",
        VBoxSeamlessInit,
        VBoxSeamlessThread,
        VBoxSeamlessDestroy
    },
    {
        NULL
    }
};

static int vboxStartServices (VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    pEnv->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!pEnv->hStopEvent)
    {
        /* Could not create event. */
        return VERR_NOT_SUPPORTED;
    }

    while (pTable->pszName)
    {
        dprintf(("Starting %s...\n", pTable->pszName));

        int rc = VINF_SUCCESS;

        bool fStartThread = false;

        pTable->hThread = (HANDLE)0;
        pTable->pInstance = NULL;
        pTable->fStarted = false;

        if (pTable->pfnInit)
        {
            rc = pTable->pfnInit (pEnv, &pTable->pInstance, &fStartThread);
        }

        if (VBOX_FAILURE (rc))
        {
            dprintf(("Failed to initialize rc = %Vrc.\n", rc));
        }
        else
        {
            if (pTable->pfnThread && fStartThread)
            {
                unsigned threadid;

                pTable->hThread = (HANDLE)_beginthreadex (NULL,  /* security */
                                                          0,     /* stacksize */
                                                          pTable->pfnThread,
                                                          pTable->pInstance,
                                                          0,     /* initflag */
                                                          &threadid);

                if (pTable->hThread == (HANDLE)(0))
                {
                    rc = VERR_NOT_SUPPORTED;
                }
            }

            if (VBOX_FAILURE (rc))
            {
                dprintf(("Failed to start the thread.\n"));

                if (pTable->pfnDestroy)
                {
                    pTable->pfnDestroy (pEnv, pTable->pInstance);
                }
            }
            else
            {
                pTable->fStarted = true;
            }
        }

        /* Advance to next table element. */
        pTable++;
    }

    return VINF_SUCCESS;
}

static void vboxStopServices (VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    if (!pEnv->hStopEvent)
    {
        return;
    }

    /* Signal to all threads. */
    SetEvent(pEnv->hStopEvent);

    while (pTable->pszName)
    {
        if (pTable->fStarted)
        {
            if (pTable->pfnThread)
            {
                /* There is a thread, wait for termination. */
                WaitForSingleObject(pTable->hThread, INFINITE);

                CloseHandle (pTable->hThread);
                pTable->hThread = 0;
            }

            if (pTable->pfnDestroy)
            {
                pTable->pfnDestroy (pEnv, pTable->pInstance);
            }

            pTable->fStarted = false;
        }

        /* Advance to next table element. */
        pTable++;
    }

    CloseHandle (pEnv->hStopEvent);
}


void WINAPI VBoxServiceStart(void)
{
    dprintf(("VBoxService: Start\n"));

    VBOXSERVICEENV svcEnv;

    DWORD status = NO_ERROR;

    /* open VBox guest driver */
    gVBoxDriver = CreateFile(VBOXGUEST_DEVICE_NAME,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                             NULL);
    if (gVBoxDriver == INVALID_HANDLE_VALUE)
    {
        dprintf(("VBoxService: could not open VBox Guest Additions driver! rc = %d\n", GetLastError()));
        status = ERROR_GEN_FAILURE;
    }

    dprintf(("VBoxService: Driver h %p, st %p\n", gVBoxDriver, status));

    if (status == NO_ERROR)
    {
        /* create a custom window class */
        WNDCLASS windowClass = {0};
        windowClass.style         = CS_NOCLOSE;
        windowClass.lpfnWndProc   = (WNDPROC)VBoxToolWndProc;
        windowClass.hInstance     = gInstance;
        windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
        windowClass.lpszClassName = "VirtualBoxTool";
        if (!RegisterClass(&windowClass))
            status = GetLastError();
    }

    dprintf(("VBoxService: Class st %p\n", status));

    if (status == NO_ERROR)
    {
        /* create our window */
        gToolWindow = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                     "VirtualBoxTool", "VirtualBoxTool",
                                     WS_POPUPWINDOW,
                                     -200, -200, 100, 100, NULL, NULL, gInstance, NULL);
        if (!gToolWindow)
            status = GetLastError();
        else
        {
            /* move the window beneach the mouse pointer so that we get access to it */
            POINT mousePos;
            GetCursorPos(&mousePos);
            SetWindowPos(gToolWindow, HWND_TOPMOST, mousePos.x - 10, mousePos.y - 10, 0, 0,
                         SWP_NOACTIVATE | SWP_SHOWWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);
            /* change the mouse pointer so that we can go for a hardware shape */
            SetCursor(LoadCursor(NULL, IDC_APPSTARTING));
            SetCursor(LoadCursor(NULL, IDC_ARROW));
            /* move back our tool window */
            SetWindowPos(gToolWindow, HWND_TOPMOST, -200, -200, 0, 0,
                         SWP_NOACTIVATE | SWP_HIDEWINDOW | SWP_NOCOPYBITS | SWP_NOREDRAW | SWP_NOSIZE);
        }
    }

    dprintf(("VBoxService: Window h %p, st %p\n", gToolWindow, status));

    if (status == NO_ERROR)
    {
        gStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (gStopSem == NULL)
        {
            dprintf(("VBoxService: CreateEvent failed: rc = %d\n", GetLastError()));
            return;
        }
    }

    /*
     * Start services listed in the vboxServiceTable.
     */
    svcEnv.hInstance  = gInstance;
    svcEnv.hDriver    = gVBoxDriver;

    if (status == NO_ERROR)
    {
        int rc = vboxStartServices (&svcEnv, vboxServiceTable);

        if (VBOX_FAILURE (rc))
        {
            status = ERROR_GEN_FAILURE;
        }
    }

    /* create display change thread */
    HANDLE hDisplayChangeThread;
    if (status == NO_ERROR)
    {
        hDisplayChangeThread = (HANDLE)_beginthread(DisplayChangeThread, 0, NULL);
        if ((int)hDisplayChangeThread == -1L)
            status = ERROR_GEN_FAILURE;
    }

    dprintf(("VBoxService: hDisplayChangeThread h %p, st %p\n", hDisplayChangeThread, status));

    /* terminate service if something went wrong */
    if (status != NO_ERROR)
    {
        vboxStopServices (&svcEnv, vboxServiceTable);
        return;
    }

    BOOL fTrayIconCreated = false;

    /* prepare the system tray icon */
    NOTIFYICONDATA ndata;
    memset (&ndata, 0, sizeof (ndata));
    ndata.cbSize           = NOTIFYICONDATA_V1_SIZE; // sizeof(NOTIFYICONDATA);
    ndata.hWnd             = gToolWindow;
    ndata.uID              = 2000;
    ndata.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    ndata.uCallbackMessage = WM_USER;
    ndata.hIcon            = LoadIcon(gInstance, MAKEINTRESOURCE(IDI_VIRTUALBOX));
    sprintf(ndata.szTip, "innotek VirtualBox Guest Additions %d.%d.%d", VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD);

    dprintf(("VBoxService: ndata.hWnd %08X, ndata.hIcon = %p\n", ndata.hWnd, ndata.hIcon));

    /*
     * Main execution loop
     * Wait for the stop semaphore to be posted or a window event to arrive
     */
    while(true)
    {
        DWORD waitResult = MsgWaitForMultipleObjectsEx(1, &gStopSem, 500, QS_ALLINPUT, 0);
        if (waitResult == WAIT_OBJECT_0)
        {
            dprintf(("VBoxService: exit\n"));
            /* exit */
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            /* a window message, handle it */
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                dprintf(("VBoxService: msg %p\n", msg.message));
                if (msg.message == WM_QUIT)
                {
                    dprintf(("VBoxService: WM_QUIT!\n"));
                    SetEvent(gStopSem);
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else /* timeout */
        {
#ifndef DEBUG_sandervl
            dprintf(("VBoxService: timed out\n"));
#endif
            /* we might have to repeat this operation because the shell might not be loaded yet */
            if (!fTrayIconCreated)
            {
                fTrayIconCreated = Shell_NotifyIcon(NIM_ADD, &ndata);
                dprintf(("VBoxService: fTrayIconCreated = %d, err %08X\n", fTrayIconCreated, GetLastError ()));
            }
        }
    }

    dprintf(("VBoxService: returned from main loop, exiting...\n"));

    /* remove the system tray icon */
    Shell_NotifyIcon(NIM_DELETE, &ndata);

    dprintf(("VBoxService: waiting for display change thread...\n"));

    /* wait for the display change thread to terminate */
    WaitForSingleObject(hDisplayChangeThread, INFINITE);

    vboxStopServices (&svcEnv, vboxServiceTable);

    dprintf(("VBoxService: destroying tool window...\n"));

    /* destroy the tool window */
    DestroyWindow(gToolWindow);

    UnregisterClass("VirtualBoxTool", gInstance);

    CloseHandle(gVBoxDriver);
    CloseHandle(gStopSem);

    dprintf(("VBoxService: leaving service main function\n"));

    return;
}


/**
 * Main function
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    dprintf(("VBoxService: WinMain\n"));
    gInstance = hInstance;
    VBoxServiceStart ();
    return 0;
}

static bool isVBoxDisplayDriverActive (void)
{
    bool result = false;

    DISPLAY_DEVICE dispDevice;

    FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);

    dispDevice.cb = sizeof(DISPLAY_DEVICE);

    INT devNum = 0;

    while (EnumDisplayDevices(NULL,
                              devNum,
                              &dispDevice,
                              0))
    {
        DDCLOG(("DevNum:%d\nName:%s\nString:%s\nID:%s\nKey:%s\nFlags=%08X\n\n",
                      devNum,
                      &dispDevice.DeviceName[0],
                      &dispDevice.DeviceString[0],
                      &dispDevice.DeviceID[0],
                      &dispDevice.DeviceKey[0],
                      dispDevice.StateFlags));

        if (dispDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            DDCLOG(("Primary device.\n"));

            if (strcmp(&dispDevice.DeviceString[0], "VirtualBox Graphics Adapter") == 0)
            {
                DDCLOG(("VBox display driver is active.\n"));
                result = true;
            }

            break;
        }

        FillMemory(&dispDevice, sizeof(DISPLAY_DEVICE), 0);

        dispDevice.cb = sizeof(DISPLAY_DEVICE);

        devNum++;
    }

    return result;
}

/* ChangeDisplaySettingsEx does not exist in NT. ResizeDisplayDevice uses the function. */
typedef LONG WINAPI defChangeDisplaySettingsEx(LPCTSTR lpszDeviceName, LPDEVMODE lpDevMode, HWND hwnd, DWORD dwflags, LPVOID lParam);
static defChangeDisplaySettingsEx *pChangeDisplaySettingsEx = NULL;

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
        DDCLOG(("[%d] %s\n", i, DisplayDevice.DeviceName));

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            DDCLOG(("Found primary device. err %d\n", GetLastError ()));
            NumDevices++;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {
            
            DDCLOG(("Found secondary device. err %d\n", GetLastError ()));
            NumDevices++;
        }
        
        ZeroMemory(&DisplayDevice, sizeof(DisplayDevice));
        DisplayDevice.cb = sizeof(DisplayDevice);
        i++;
    }
    
    DDCLOG(("Found total %d devices. err %d\n", NumDevices, GetLastError ()));
    
    if (NumDevices == 0 || Id >= NumDevices)
    {
        DDCLOG(("Requested identifier %d is invalid. err %d\n", Id, GetLastError ()));
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
        DDCLOG(("[%d(%d)] %s\n", i, DevNum, DisplayDevice.DeviceName));
        
        BOOL bFetchDevice = FALSE;

        if (DisplayDevice.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE)
        {
            DDCLOG(("Found primary device. err %d\n", GetLastError ()));
            DevPrimaryNum = DevNum;
            bFetchDevice = TRUE;
        }
        else if (!(DisplayDevice.StateFlags & DISPLAY_DEVICE_MIRRORING_DRIVER))
        {
            
            DDCLOG(("Found secondary device. err %d\n", GetLastError ()));
            bFetchDevice = TRUE;
        }
        
        if (bFetchDevice)
        {
            if (DevNum >= NumDevices)
            {
                DDCLOG(("%d >= %d\n", NumDevices, DevNum));
                return FALSE;
            }
        
            paDisplayDevices[DevNum] = DisplayDevice;
            
            ZeroMemory(&paDeviceModes[DevNum], sizeof(DEVMODE));
            paDeviceModes[DevNum].dmSize = sizeof(DEVMODE);
            if (!EnumDisplaySettings((LPSTR)DisplayDevice.DeviceName,
                 ENUM_REGISTRY_SETTINGS, &paDeviceModes[DevNum]))
            {
                DDCLOG(("EnumDisplaySettings err %d\n", GetLastError ()));
                return FALSE;
            }
            
            DDCLOG(("%dx%d at %d,%d\n",
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
        DDCLOG(("VBoxService: already at desired resolution.\n"));
        return FALSE;
    }

    resizeRect(paRects, NumDevices, DevPrimaryNum, Id, Width, Height);
#ifdef DDCLOG
    for (i = 0; i < NumDevices; i++)
    {
        DDCLOG(("[%d]: %d,%d %dx%d\n",
                i, paRects[i].left, paRects[i].top,
                paRects[i].right - paRects[i].left,
                paRects[i].bottom - paRects[i].top));
    }
#endif /* DDCLOG */
    
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
        
        pChangeDisplaySettingsEx((LPSTR)paDisplayDevices[i].DeviceName, 
                 &paDeviceModes[i], NULL, CDS_NORESET | CDS_UPDATEREGISTRY, NULL); 
        DDCLOG(("ChangeDisplaySettings position err %d\n", GetLastError ()));
    }
    
    /* A second call to ChangeDisplaySettings updates the monitor. */
    LONG status = ChangeDisplaySettings(NULL, 0); 
    DDCLOG(("ChangeDisplaySettings update status %d\n", status));
    if (status == DISP_CHANGE_SUCCESSFUL || status == DISP_CHANGE_BADMODE)
    {
        /* Successfully set new video mode or our driver can not set the requested mode. Stop trying. */
        return FALSE;
    }

    /* Retry the request. */
    return TRUE;
}

/**
 * Thread function to wait for and process display change
 * requests
 */
VOID DisplayChangeThread(void *dummy)
{
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;
    
    HMODULE hUser = GetModuleHandle("USER32");

    if (hUser)
    {
        pChangeDisplaySettingsEx = (defChangeDisplaySettingsEx *)GetProcAddress(hUser, "ChangeDisplaySettingsExA"); 
        DDCLOG(("VBoxService: pChangeDisplaySettingsEx = %p\n", pChangeDisplaySettingsEx));
    }

    maskInfo.u32OrMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        DDCLOG(("VBoxService: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxService: DeviceIOControl(CtlMask) failed, DisplayChangeThread exited\n"));
        return;
    }

    do
    {
        /* wait for a display change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = 100;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            DDCLOG(("VBoxService: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(gStopSem, 0) == WAIT_OBJECT_0)
                break;

            DDCLOG(("VBoxService: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST)
            {
                DDCLOG(("VBoxService: going to get display change information.\n"));

                /* We got at least one event. Read the requested resolution
                 * and try to set it until success. New events will not be seen
                 * but a new resolution will be read in this poll loop.
                 */
                for (;;)
                {
                    /* get the display change request */
                    VMMDevDisplayChangeRequest2 displayChangeRequest = {0};
                    displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest2);
                    displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                    displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest2;
                    displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                    BOOL fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2),
                                                                 &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest2), &cbReturned, NULL);
                    if (!fDisplayChangeQueried)
                    {
                        /* Try the old version of the request for old VBox hosts. */
                        displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest);
                        displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                        displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest;
                        displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                        fDisplayChangeQueried = DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest),
                                                                 &displayChangeRequest, sizeof(VMMDevDisplayChangeRequest), &cbReturned, NULL);
                        displayChangeRequest.display = 0;
                    }

                    if (fDisplayChangeQueried)
                    {
                        DDCLOG(("VBoxService: VMMDevReq_GetDisplayChangeRequest2: %dx%dx%d at %d\n", displayChangeRequest.xres, displayChangeRequest.yres, displayChangeRequest.bpp, displayChangeRequest.display));

                        /* Horizontal resolution must be a multiple of 8, round down. */
                        displayChangeRequest.xres &= 0xfff8;

                        /*
                         * Only try to change video mode if the active display driver is VBox additions.
                         */
                        if (isVBoxDisplayDriverActive ())
                        {
                            if (pChangeDisplaySettingsEx != 0)
                            {
                                /* W2K or later. */
                                if (!ResizeDisplayDevice(displayChangeRequest.display,
                                                         displayChangeRequest.xres,
                                                         displayChangeRequest.yres,
                                                         displayChangeRequest.bpp))
                                {
                                    break;
                                }
                            }
                            else
                            {
                                /* Single monitor NT. */
                                DEVMODE devMode;
                                memset (&devMode, 0, sizeof (devMode));
                                devMode.dmSize = sizeof(DEVMODE);

                                /* get the current screen setup */
                                if (EnumDisplaySettings(NULL, ENUM_REGISTRY_SETTINGS, &devMode))
                                {
                                    dprintf(("VBoxService: Current mode: %dx%dx%d at %d,%d\n", devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel, devMode.dmPosition.x, devMode.dmPosition.y));

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
                                    }

                                    /* Verify that the mode is indeed changed. */
                                    if (   devMode.dmPelsWidth  == displayChangeRequest.xres
                                        && devMode.dmPelsHeight == displayChangeRequest.yres
                                        && devMode.dmBitsPerPel == displayChangeRequest.bpp)
                                    {
                                        dprintf(("VBoxService: already at desired resolution.\n"));
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

                                    DDCLOG(("VBoxService: setting the new mode %dx%dx%d\n", devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel));

                                    /* set the new mode */
                                    LONG status = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
                                    if (status != DISP_CHANGE_SUCCESSFUL)
                                    {
                                        dprintf(("VBoxService: error from ChangeDisplaySettings: %d\n", status));

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
                                    dprintf(("VBoxService: error from EnumDisplaySettings: %d\n", GetLastError ()));
                                    break;
                                }
                            }
                        }
                        else
                        {
                            dprintf(("VBoxService: vboxDisplayDriver is not active.\n"));
                        }

                        /* Retry the change a bit later. */
                        /* are we supposed to stop? */
                        if (WaitForSingleObject(gStopSem, 1000) == WAIT_OBJECT_0)
                        {
                            fTerminate = true;
                            break;
                        }
                    }
                    else
                    {
                        dprintf(("VBoxService: error from DeviceIoControl IOCTL_VBOXGUEST_VMMREQUEST\n"));
                        /* sleep a bit to not eat too much CPU while retrying */
                        /* are we supposed to stop? */
                        if (WaitForSingleObject(gStopSem, 50) == WAIT_OBJECT_0)
                        {
                            fTerminate = true;
                            break;
                        }
                    }
                }
            }
        } else
        {
            dprintf(("VBoxService: error 0 from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n"));
            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(gStopSem, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
    } while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        DDCLOG(("VBoxService: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxService: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxService: finished display change request thread\n"));
}

/**
 * Window procedure for our tool window
 */
LRESULT CALLBACK VBoxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
        case WM_CLOSE:
            break;

        case WM_DESTROY:
            break;

        case WM_VBOX_INSTALL_SEAMLESS_HOOK:
            VBoxSeamlessInstallHook();
            break;

        case WM_VBOX_REMOVE_SEAMLESS_HOOK:
            VBoxSeamlessRemoveHook();
            break;

        case WM_VBOX_SEAMLESS_UPDATE:
            VBoxSeamlessCheckWindows();
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

