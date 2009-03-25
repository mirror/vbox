/** @file
 * VBoxTray - Guest Additions Tray Application
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include "VBoxTray.h"
#include "VBoxSeamless.h"
#include "VBoxClipboard.h"
#include "VBoxDisplay.h"
#include "VBoxRestore.h"
#include "VBoxVRDP.h"
#include "VBoxStatistics.h"
#include "VBoxMemBalloon.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>
#include <VBoxGuestInternal.h>

#include "helpers.h"
#include <sddl.h>

/* global variables */
HANDLE                gVBoxDriver;
HANDLE                gStopSem;
HANDLE                ghSeamlessNotifyEvent = 0;
SERVICE_STATUS        gVBoxServiceStatus;
SERVICE_STATUS_HANDLE gVBoxServiceStatusHandle;
HINSTANCE             gInstance;
HWND                  gToolWindow;

/* Prototypes */
VOID DisplayChangeThread(void *dummy);
LRESULT CALLBACK VBoxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* The service table. */
static VBOXSERVICEINFO vboxServiceTable[] =
{
    {
        "Display",
        VBoxDisplayInit,
        VBoxDisplayThread,
        VBoxDisplayDestroy,
    },
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
#ifdef VBOX_WITH_MANAGEMENT
    {
        "Memory Balloon",
        VBoxMemBalloonInit,
        VBoxMemBalloonThread,
        VBoxMemBalloonDestroy,
    },
    {
        "Guest Statistics",
        VBoxStatsInit,
        VBoxStatsThread,
        VBoxStatsDestroy,
    },
#endif
#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
    {
        "Restore",
        VBoxRestoreInit,
        VBoxRestoreThread,
        VBoxRestoreDestroy,
    },
#endif
    {
        "VRDP",
        VBoxVRDPInit,
        VBoxVRDPThread,
        VBoxVRDPDestroy,
    },
    {
        NULL
    }
};

static int vboxStartServices (VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    Log(("VBoxTray: Starting services...\n"));

    pEnv->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!pEnv->hStopEvent)
    {
        /* Could not create event. */
        return VERR_NOT_SUPPORTED;
    }

    while (pTable->pszName)
    {
        Log(("Starting %s...\n", pTable->pszName));

        int rc = VINF_SUCCESS;

        bool fStartThread = false;

        pTable->hThread = (HANDLE)0;
        pTable->pInstance = NULL;
        pTable->fStarted = false;

        if (pTable->pfnInit)
        {
            rc = pTable->pfnInit (pEnv, &pTable->pInstance, &fStartThread);
        }

        if (RT_FAILURE (rc))
        {
            Log(("Failed to initialize rc = %Rrc.\n", rc));
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

            if (RT_FAILURE (rc))
            {
                Log(("Failed to start the thread.\n"));

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
    Log(("VBoxTray: Leaving service main function"));

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
        LogRel(("VBoxTray: Could not open VirtualBox Guest Additions driver! Please install / start it first! rc = %d\n", GetLastError()));
        status = ERROR_GEN_FAILURE;
    }

    Log(("VBoxTray: Driver Handle = %p, Status = %p\n", gVBoxDriver, status));

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

    Log(("VBoxTray: Class st %p\n", status));

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

    Log(("VBoxTray: Window Handle = %p, Status = %p\n", gToolWindow, status));

    if (status == NO_ERROR)
    {
        gStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (gStopSem == NULL)
        {
            Log(("VBoxTray: CreateEvent for Stopping failed: rc = %d\n", GetLastError()));
            return;
        }

        /* We need to setup a security descriptor to allow other processes modify access to the seamless notification event semaphore */
        SECURITY_ATTRIBUTES     SecAttr;
        OSVERSIONINFO           info;
        char                    secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
        DWORD                   dwMajorVersion = 5; /* default XP */
        BOOL                    ret;

        SecAttr.nLength              = sizeof(SecAttr);
        SecAttr.bInheritHandle       = FALSE;
        SecAttr.lpSecurityDescriptor = &secDesc;
        InitializeSecurityDescriptor(SecAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
        ret = SetSecurityDescriptorDacl(SecAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
        if (!ret)
            Log(("VBoxTray: SetSecurityDescriptorDacl failed with %d\n", GetLastError()));

        info.dwOSVersionInfoSize = sizeof(info);
        if (GetVersionEx(&info))
        {
            Log(("VBoxTray: Windows version major %d minor %d\n", info.dwMajorVersion, info.dwMinorVersion));
            dwMajorVersion = info.dwMajorVersion;
        }

        /* For Vista and up we need to change the integrity of the security descriptor too */
        if (dwMajorVersion >= 6)
        {
            HMODULE hModule;

            BOOL (WINAPI * pfnConvertStringSecurityDescriptorToSecurityDescriptorA)(LPCSTR StringSecurityDescriptor, DWORD StringSDRevision, PSECURITY_DESCRIPTOR  *SecurityDescriptor, PULONG  SecurityDescriptorSize);

            hModule = LoadLibrary("ADVAPI32.DLL");
            if (hModule)
            {
                PSECURITY_DESCRIPTOR    pSD;
                PACL                    pSacl          = NULL;
                BOOL                    fSaclPresent   = FALSE;
                BOOL                    fSaclDefaulted = FALSE;

                *(uintptr_t *)&pfnConvertStringSecurityDescriptorToSecurityDescriptorA = (uintptr_t)GetProcAddress(hModule, "ConvertStringSecurityDescriptorToSecurityDescriptorA");

                Log(("VBoxTray: pfnConvertStringSecurityDescriptorToSecurityDescriptorA = %x\n", pfnConvertStringSecurityDescriptorToSecurityDescriptorA));
                if (pfnConvertStringSecurityDescriptorToSecurityDescriptorA)
                {
                    ret = pfnConvertStringSecurityDescriptorToSecurityDescriptorA("S:(ML;;NW;;;LW)", /* this means "low integrity" */
                                                                                  SDDL_REVISION_1, &pSD, NULL);
                    if (!ret)
                        Log(("VBoxTray: ConvertStringSecurityDescriptorToSecurityDescriptorA failed with %d\n", GetLastError()));

                    ret = GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
                    if (!ret)
                        Log(("VBoxTray: GetSecurityDescriptorSacl failed with %d\n", GetLastError()));

                    ret = SetSecurityDescriptorSacl(SecAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
                    if (!ret)
                        Log(("VBoxTray: SetSecurityDescriptorSacl failed with %d\n", GetLastError()));
                }
            }
        }

        if (dwMajorVersion >= 5)        /* Only for W2K and up ... */
        {
            ghSeamlessNotifyEvent = CreateEvent(&SecAttr, FALSE, FALSE, VBOXHOOK_GLOBAL_EVENT_NAME);
            if (ghSeamlessNotifyEvent == NULL)
            {
                Log(("VBoxTray: CreateEvent for Seamless failed: rc = %d\n", GetLastError()));
                return;
            }
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

        if (RT_FAILURE (rc))
        {
            status = ERROR_GEN_FAILURE;
        }
    }

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
    sprintf(ndata.szTip, "Sun VirtualBox Guest Additions %d.%d.%dr%d", VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);

    Log(("VBoxTray: ndata.hWnd %08X, ndata.hIcon = %p\n", ndata.hWnd, ndata.hIcon));

    /* Boost thread priority to make sure we wake up early for seamless window notifications (not sure if it actually makes any difference though) */
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    /*
     * Main execution loop
     * Wait for the stop semaphore to be posted or a window event to arrive
     */

    DWORD dwEventCount = 2;
    HANDLE hWaitEvent[2] = {gStopSem, ghSeamlessNotifyEvent};

    if (0 == ghSeamlessNotifyEvent)         /* If seamless mode is not active / supported, reduce event array count */
        dwEventCount = 1;

    Log(("VBoxTray: Number of events to wait in main loop: %ld\n", dwEventCount));

    while(true)
    {
        DWORD waitResult = MsgWaitForMultipleObjectsEx(dwEventCount, hWaitEvent, 500, QS_ALLINPUT, 0);
        waitResult = waitResult - WAIT_OBJECT_0;

        Log(("VBoxTray: Wait result  = %ld.\n", waitResult));

        if (waitResult == 0)
        {
            Log(("VBoxTray: Event 'Exit' triggered.\n"));
            /* exit */
            break;
        }
        else if ((waitResult == 1) && (ghSeamlessNotifyEvent!=0))       /* Only jump in, if seamless is active! */
        {
            Log(("VBoxTray: Event 'Seamless' triggered.\n"));

            /* seamless window notification */
            VBoxSeamlessCheckWindows();
        }
        else
        {
            /* timeout or a window message, handle it */
            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                Log(("VBoxTray: msg %p\n", msg.message));
                if (msg.message == WM_QUIT)
                {
                    Log(("VBoxTray: WM_QUIT!\n"));
                    SetEvent(gStopSem);
                    continue;
                }
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            /* we might have to repeat this operation because the shell might not be loaded yet */
            if (!fTrayIconCreated)
            {
                fTrayIconCreated = Shell_NotifyIcon(NIM_ADD, &ndata);
                Log(("VBoxTray: fTrayIconCreated = %d, err %08X\n", fTrayIconCreated, GetLastError ()));
            }
        }
    }

    Log(("VBoxTray: Returned from main loop, exiting ...\n"));

    /* remove the system tray icon */
    Shell_NotifyIcon(NIM_DELETE, &ndata);

    Log(("VBoxTray: waiting for display change thread ...\n"));

    vboxStopServices (&svcEnv, vboxServiceTable);

    Log(("VBoxTray: Destroying tool window ...\n"));

    /* destroy the tool window */
    DestroyWindow(gToolWindow);

    UnregisterClass("VirtualBoxTool", gInstance);

    CloseHandle(gVBoxDriver);
    CloseHandle(gStopSem);
    CloseHandle(ghSeamlessNotifyEvent);

    Log(("VBoxTray: Leaving service main function\n"));

    return;
}


/**
 * Main function
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* Do not use a global namespace ("Global\\") for mutex name here, will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex (NULL, FALSE, "VBoxTray");
    if (   (hMutexAppRunning != NULL)
        && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
      /* Close the mutex for this application instance. */
      CloseHandle (hMutexAppRunning);
      hMutexAppRunning = NULL;
      return 0;
    }

    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return rc;

    rc = VbglR3Init();
    if (RT_FAILURE(rc))
        return rc;

    LogRel(("VBoxTray: Started.\n"));

    gInstance = hInstance;
    VBoxServiceStart();

    LogRel(("VBoxTray: Ended.\n"));

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL) {
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    VbglR3Term();
    return 0;
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

        case WM_VBOX_RESTORED:
            VBoxRestoreSession();
            break;

        case WM_VBOX_CHECK_VRDP:
            VBoxRestoreCheckVRDP();
            break;

        default:
            return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
