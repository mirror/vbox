/** @file
 * VBoxService - Guest Additions Service
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 *
 */

#include "VBoxService.h"
#include "resource.h"

// #define DEBUG_DISPLAY_CHANGE

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


VOID SvcDebugOut2(LPSTR String, ...)
{
   va_list va;

   va_start(va, String);

   CHAR  Buffer[1024];
   if (strlen(String) < 1000)
   {
      vsprintf (Buffer, String, va);

      OutputDebugStringA(Buffer);

#ifdef DEBUG_DISPLAY_CHANGE
      FILE *f = fopen ("vboxservice.log", "ab");
      if (f)
      {
          fprintf (f, "%s", Buffer);
          fclose (f);
      }
#endif /* DEBUG_DISPLAY_CHANGE */
   }

   va_end (va);
}

#ifdef DEBUG_DISPLAY_CHANGE
#define DDCLOG(a) do { SvcDebugOut2 a; } while (0)
#else
#define DDCLOG(a) do {} while (0)
#endif /* DEBUG_DISPLAY_CHANGE */


/**
 * Helper function to send a message to WinDbg
 *
 * @param String message string
 * @param Status error code, will be inserted into String's placeholder
 */
VOID SvcDebugOut(LPSTR String, DWORD Status)
{
    SvcDebugOut2(String, Status);
}

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
        SvcDebugOut2 ("Starting %s...\n", pTable->pszName);
        
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
            SvcDebugOut2 ("Failed to initialize rc = %Vrc.\n", rc);
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
                SvcDebugOut2 ("Failed to start the thread.\n");
                
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
    SvcDebugOut2("VBoxService: Start\n");
    
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
        SvcDebugOut("VBoxService: could not open VBox Guest Additions driver! rc = %d\n", GetLastError());
        status = ERROR_GEN_FAILURE;
    }

    SvcDebugOut2("VBoxService: Driver h %p, st %p\n", gVBoxDriver, status);
    
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
    
    SvcDebugOut2("VBoxService: Class st %p\n", status);

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
    
    SvcDebugOut2("VBoxService: Window h %p, st %p\n", gToolWindow, status);

    if (status == NO_ERROR)
    {
        gStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (gStopSem == NULL)
        {
            SvcDebugOut("VBoxService: CreateEvent failed: rc = %d\n", GetLastError());
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

    SvcDebugOut2("VBoxService: hDisplayChangeThread h %p, st %p\n", hDisplayChangeThread, status);

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
    sprintf(ndata.szTip, "InnoTek VirtualBox Guest Additions %d.%d.%d", VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD);

    SvcDebugOut2("VBoxService: ndata.hWnd %08X, ndata.hIcon = %p\n", ndata.hWnd, ndata.hIcon);
    
    /*
     * Main execution loop
     * Wait for the stop semaphore to be posted or a window event to arrive
     */
    while(true)
    {
        DWORD waitResult = MsgWaitForMultipleObjectsEx(1, &gStopSem, 500, QS_ALLINPUT, 0);
        if (waitResult == WAIT_OBJECT_0)
        {
            SvcDebugOut2("VBoxService: exit\n");
            /* exit */
            break;
        }
        else if (waitResult == WAIT_OBJECT_0 + 1)
        {
            /* a window message, handle it */
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                SvcDebugOut2("VBoxService: msg %p\n", msg.message);
                if (msg.message == WM_QUIT)
                {
                    SvcDebugOut2("VBoxService: WM_QUIT!\n");
                    SetEvent(gStopSem);
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
        else /* timeout */
        {
            SvcDebugOut2("VBoxService: timed out\n");
            
            /* we might have to repeat this operation because the shell might not be loaded yet */
            if (!fTrayIconCreated)
            {
                fTrayIconCreated = Shell_NotifyIcon(NIM_ADD, &ndata);
                SvcDebugOut2("VBoxService: fTrayIconCreated = %d, err %08X\n", fTrayIconCreated, GetLastError ());
            }
        }
    }

    SvcDebugOut("VBoxService: returned from main loop, exiting...\n", 0);

    /* remove the system tray icon */
    Shell_NotifyIcon(NIM_DELETE, &ndata);

    SvcDebugOut("VBoxService: waiting for display change thread...\n", 0);

    /* wait for the display change thread to terminate */
    WaitForSingleObject(hDisplayChangeThread, INFINITE);

    vboxStopServices (&svcEnv, vboxServiceTable);
    
    SvcDebugOut("VBoxService: destroying tool window...\n", 0);

    /* destroy the tool window */
    DestroyWindow(gToolWindow);

    UnregisterClass("VirtualBoxTool", gInstance);

    CloseHandle(gVBoxDriver);
    CloseHandle(gStopSem);

    SvcDebugOut("VBoxService: leaving service main function\n", 0);

    return;
}


/**
 * Main function
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    SvcDebugOut2("VBoxService: WinMain\n");
    gInstance = hInstance;
    VBoxServiceStart ();
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

/**
 * Thread function to wait for and process display change
 * requests
 */
VOID DisplayChangeThread(void *dummy)
{
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        DDCLOG(("VBoxService: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        SvcDebugOut2("VBoxService: DeviceIOControl(CtlMask) failed, DisplayChangeThread exited\n");
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
                    VMMDevDisplayChangeRequest displayChangeRequest = {0};
                    displayChangeRequest.header.size        = sizeof(VMMDevDisplayChangeRequest);
                    displayChangeRequest.header.version     = VMMDEV_REQUEST_HEADER_VERSION;
                    displayChangeRequest.header.requestType = VMMDevReq_GetDisplayChangeRequest;
                    displayChangeRequest.eventAck           = VMMDEV_EVENT_DISPLAY_CHANGE_REQUEST;
                    if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &displayChangeRequest, sizeof(displayChangeRequest),
                                        &displayChangeRequest, sizeof(displayChangeRequest), &cbReturned, NULL))
                    {
                        DDCLOG(("VBoxService: VMMDevReq_GetDisplayChangeRequest: %dx%dx%d\n", displayChangeRequest.xres, displayChangeRequest.yres, displayChangeRequest.bpp));

                        /*
                         * Only try to change video mode if the active display driver is VBox additions.
                         */
                        if (isVBoxDisplayDriverActive ())
                        {
                            DEVMODE devMode;

                            /* get the current screen setup */
                            if (EnumDisplaySettings(NULL, ENUM_CURRENT_SETTINGS, &devMode))
                            {
                                SvcDebugOut2("VBoxService: Current mode: %dx%dx%d\n", devMode.dmPelsWidth, devMode.dmPelsHeight, devMode.dmBitsPerPel);

                                /* Horizontal resolution must be a multiple of 8, round down. */
                                displayChangeRequest.xres &= 0xfff8;

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
                                    SvcDebugOut2("VBoxService: already at desired resolution.\n");
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
                                LONG status;
                                status = ChangeDisplaySettings(&devMode, CDS_UPDATEREGISTRY);
                                if (status != DISP_CHANGE_SUCCESSFUL)
                                {
                                    SvcDebugOut("VBoxService: error from ChangeDisplaySettings: %d\n", status);
                                }
                                else
                                {
                                    /* Successfully set new video mode. */
                                    break;
                                }
                            }
                            else
                            {
                                SvcDebugOut("VBoxService: error from EnumDisplaySettings\n", 0);
                            }
                        }
                        else
                        {
                            SvcDebugOut2("VBoxService: vboxDisplayDriver is not active.\n", 0);
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
                        SvcDebugOut("VBoxService: error from DeviceIoControl IOCTL_VBOXGUEST_VMMREQUEST\n", 0);
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
            SvcDebugOut("VBoxService: error from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n", 0);
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
        SvcDebugOut2 ("VBoxService: DeviceIOControl(CtlMask) failed\n");
    }

    SvcDebugOut2("VBoxService: finished display change request thread\n");
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

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
