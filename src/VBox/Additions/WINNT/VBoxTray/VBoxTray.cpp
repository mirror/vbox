/** @file
 * VBoxTray - Guest Additions Tray Application
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBoxTray.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include "VBoxClipboard.h"
#include "VBoxDisplay.h"
#include "VBoxRestore.h"
#include "VBoxVRDP.h"
#include "VBoxHostVersion.h"
#include "VBoxSharedFolders.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>
#include <VBoxGuestInternal.h>

#include <sddl.h>

#include <iprt/buildconfig.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
HANDLE                gVBoxDriver;
HANDLE                gStopSem;
HANDLE                ghSeamlessNotifyEvent = 0;
SERVICE_STATUS        gVBoxServiceStatus;
SERVICE_STATUS_HANDLE gVBoxServiceStatusHandle;
HINSTANCE             gInstance;
HWND                  gToolWindow;
NOTIFYICONDATA        gNotifyIconData;
DWORD                 gMajorVersion;

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

static BOOL vboxTrayIconAdd()
{
    HICON hIcon = LoadIcon(gInstance, MAKEINTRESOURCE(IDI_VIRTUALBOX));
    if (hIcon == NULL)
    {
        Log(("VBoxTray: Could not load tray icon, err %08X\n", GetLastError()));
        return FALSE;
    }

    /* Prepare the system tray icon. */
    RT_ZERO(gNotifyIconData);
    gNotifyIconData.cbSize           = NOTIFYICONDATA_V1_SIZE; // sizeof(NOTIFYICONDATA);
    gNotifyIconData.hWnd             = gToolWindow;
    gNotifyIconData.uID              = ID_TRAYICON;
    gNotifyIconData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gNotifyIconData.uCallbackMessage = WM_VBOXTRAY_TRAY_ICON;
    gNotifyIconData.hIcon            = hIcon;

    sprintf(gNotifyIconData.szTip, "%s Guest Additions %d.%d.%dr%d",
            VBOX_PRODUCT, VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);

    BOOL fCreated = Shell_NotifyIcon(NIM_ADD, &gNotifyIconData);
    if (!fCreated)
    {
        Log(("VBoxTray: Could not create tray icon, err %08X\n", GetLastError()));
        RT_ZERO(gNotifyIconData);
    }

    if (hIcon)
        DestroyIcon(hIcon);
    return fCreated;
}

static void vboxTrayIconRemove()
{
    if (gNotifyIconData.cbSize > 0)
    {
        /* Remove the system tray icon and refresh system tray. */
        Shell_NotifyIcon(NIM_DELETE, &gNotifyIconData);
        HWND hTrayWnd = FindWindow("Shell_TrayWnd", NULL); /* We assume we only have one tray atm. */
        if (hTrayWnd)
        {
            HWND hTrayNotifyWnd = FindWindowEx(hTrayWnd, 0, "TrayNotifyWnd", NULL);
            if (hTrayNotifyWnd)
                SendMessage(hTrayNotifyWnd, WM_PAINT, 0, NULL);
        }
        RT_ZERO(gNotifyIconData);
    }
}

static int vboxStartServices(VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    Log(("VBoxTray: Starting services ...\n"));

    pEnv->hStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

    if (!pEnv->hStopEvent)
    {
        /* Could not create event. */
        return VERR_NOT_SUPPORTED;
    }

    while (pTable->pszName)
    {
        Log(("VBoxTray: Starting %s ...\n", pTable->pszName));

        int rc = VINF_SUCCESS;

        bool fStartThread = false;

        pTable->hThread = (HANDLE)0;
        pTable->pInstance = NULL;
        pTable->fStarted = false;

        if (pTable->pfnInit)
            rc = pTable->pfnInit (pEnv, &pTable->pInstance, &fStartThread);

        if (RT_FAILURE(rc))
        {
            Log(("VBoxTray: Failed to initialize rc = %Rrc\n", rc));
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
                    rc = VERR_NOT_SUPPORTED;
            }

            if (RT_SUCCESS(rc))
                pTable->fStarted = true;
            else
            {
                Log(("VBoxTray: Failed to start the thread\n"));
                if (pTable->pfnDestroy)
                    pTable->pfnDestroy(pEnv, pTable->pInstance);
            }
        }

        /* Advance to next table element. */
        pTable++;
    }

    return VINF_SUCCESS;
}

static void vboxStopServices(VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    if (!pEnv->hStopEvent)
        return;

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

                CloseHandle(pTable->hThread);
                pTable->hThread = 0;
            }

            if (pTable->pfnDestroy)
                pTable->pfnDestroy (pEnv, pTable->pInstance);
            pTable->fStarted = false;
        }

        /* Advance to next table element. */
        pTable++;
    }

    CloseHandle(pEnv->hStopEvent);
}


/**
 * Attempt to force Windows to reload the cursor image by attaching to the
 * thread of the window currently under the mouse, hiding the cursor and
 * showing it again.  This could fail to work in any number of ways (no
 * window under the cursor, the cursor has moved to a different window while
 * we are processing), but we just accept this, as the cursor will be reloaded
 * at some point anyway.
 */
void VBoxServiceReloadCursor(void)
{
    LogFlowFunc(("\n"));
    POINT mousePos;
    HWND hWin;
    DWORD hThread, hCurrentThread;

    GetCursorPos(&mousePos);
    hWin = WindowFromPoint(mousePos);
    if (hWin)
    {
        hThread = GetWindowThreadProcessId(hWin, NULL);
        hCurrentThread = GetCurrentThreadId();
        if (hCurrentThread != hThread)
            AttachThreadInput(hCurrentThread, hThread, TRUE);
    }
    ShowCursor(false);
    ShowCursor(true);
    if (hWin && (hCurrentThread != hThread))
        AttachThreadInput(hCurrentThread, hThread, FALSE);
    LogFlowFunc(("exiting\n"));
}


void WINAPI VBoxServiceStart(void)
{
    Log(("VBoxTray: Entering service main function\n"));

    VBOXSERVICEENV svcEnv;

    DWORD status = NO_ERROR;

    /* Open VBox guest driver. */
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
        /* Create a custom window class. */
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
        /* Create our (invisible) tool window. */
        gToolWindow = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                     "VirtualBoxTool", "VirtualBoxTool",
                                     WS_POPUPWINDOW,
                                     -200, -200, 100, 100, NULL, NULL, gInstance, NULL);
        if (!gToolWindow)
            status = GetLastError();
        else
            VBoxServiceReloadCursor();
    }

    Log(("VBoxTray: Window Handle = %p, Status = %p\n", gToolWindow, status));

    OSVERSIONINFO info;
    gMajorVersion = 5; /* default XP */
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(&info))
    {
        Log(("VBoxTray: Windows version major %d minor %d\n", info.dwMajorVersion, info.dwMinorVersion));
        gMajorVersion = info.dwMajorVersion;
    }

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
        char                    secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
        BOOL                    ret;

        SecAttr.nLength              = sizeof(SecAttr);
        SecAttr.bInheritHandle       = FALSE;
        SecAttr.lpSecurityDescriptor = &secDesc;
        InitializeSecurityDescriptor(SecAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
        ret = SetSecurityDescriptorDacl(SecAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
        if (!ret)
            Log(("VBoxTray: SetSecurityDescriptorDacl failed with %d\n", GetLastError()));

        /* For Vista and up we need to change the integrity of the security descriptor too */
        if (gMajorVersion >= 6)
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

        if (gMajorVersion >= 5)        /* Only for W2K and up ... */
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

    /* initializes disp-if to default (XPDM) mode */
    status = VBoxDispIfInit(&svcEnv.dispIf);
#ifdef VBOX_WITH_WDDM
    /*
     * For now the display mode will be adjusted to WDDM mode if needed
     * on display service initialization when it detects the display driver type.
     */
#endif

    if (status == NO_ERROR)
    {
        int rc = vboxStartServices(&svcEnv, vboxServiceTable);

        if (RT_FAILURE (rc))
        {
            status = ERROR_GEN_FAILURE;
        }
    }

    /* terminate service if something went wrong */
    if (status != NO_ERROR)
    {
        vboxStopServices(&svcEnv, vboxServiceTable);
        return;
    }

    if (   vboxTrayIconAdd()
        && gMajorVersion >= 5) /* Only for W2K and up ... */
    {
        /* We're ready to create the tooltip balloon. */
        /* Check in 10 seconds (@todo make seconds configurable) ... */
        SetTimer(gToolWindow,
                 TIMERID_VBOXTRAY_CHECK_HOSTVERSION,
                 10 * 1000, /* 10 seconds */
                 NULL       /* No timerproc */);
    }

    VBoxSharedFoldersAutoMount();

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
    while (true)
    {
        DWORD waitResult = MsgWaitForMultipleObjectsEx(dwEventCount, hWaitEvent, 500, QS_ALLINPUT, 0);
        waitResult = waitResult - WAIT_OBJECT_0;

        /* Only enable for message debugging, lots of traffic! */
        //Log(("VBoxTray: Wait result  = %ld\n", waitResult));

        if (waitResult == 0)
        {
            Log(("VBoxTray: Event 'Exit' triggered\n"));
            /* exit */
            break;
        }
        else if (   (waitResult == 1)
                 && (ghSeamlessNotifyEvent!=0)) /* Only jump in, if seamless is active! */
        {
            Log(("VBoxTray: Event 'Seamless' triggered\n"));

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
        }
    }

    Log(("VBoxTray: Returned from main loop, exiting ...\n"));

    vboxTrayIconRemove();

    Log(("VBoxTray: Waiting for display change thread ...\n"));

    vboxStopServices(&svcEnv, vboxServiceTable);

    Log(("VBoxTray: Destroying tool window ...\n"));

    /* Destroy the tool window. */
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
    HANDLE hMutexAppRunning = CreateMutex(NULL, FALSE, "VBoxTray");
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

    LogRel(("VBoxTray: %s r%s started\n", RTBldCfgVersion(), RTBldCfgRevisionStr()));

    gInstance = hInstance;
    VBoxServiceStart();

    LogRel(("VBoxTray: Ended\n"));

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL) {
        CloseHandle(hMutexAppRunning);
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
    static UINT s_uTaskbarCreated = 0;

    switch (msg)
    {
        case WM_CREATE:
            Log(("VBoxTray: Tool window created\n"));
            s_uTaskbarCreated = RegisterWindowMessage(TEXT("TaskbarCreated"));
            if (!s_uTaskbarCreated)
                Log(("VBoxTray: Cannot register message \"TaskbarCreated\"! Error = %ld\n", GetLastError()));
            break;

        case WM_CLOSE:
            break;

        case WM_DESTROY:
            Log(("VBoxTray: Tool window destroyed\n"));
            KillTimer(gToolWindow, TIMERID_VBOXTRAY_CHECK_HOSTVERSION);
            break;

        case WM_TIMER:
            switch (wParam)
            {
                case WM_VBOXTRAY_CHECK_HOSTVERSION:
                    if (RT_SUCCESS(VBoxCheckHostVersion()))
                    {
                        /* After successful run we don't need to check again. */
                        KillTimer(gToolWindow, TIMERID_VBOXTRAY_CHECK_HOSTVERSION);
                    }
                    return 0;

                default:
                    break;
            }

            break;

        case WM_VBOXTRAY_TRAY_ICON:
            switch (lParam)
            {
                case WM_LBUTTONDBLCLK:
                    break;

                case WM_RBUTTONDOWN:
                    break;
            }
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

        case WM_VBOXTRAY_VM_RESTORED:
            VBoxRestoreSession();
            break;

        case WM_VBOXTRAY_VRDP_CHECK:
            VBoxRestoreCheckVRDP();
            break;

        default:

            if(msg == s_uTaskbarCreated)
            {
                Log(("VBoxTray: Taskbar (re-)created, installing tray icon ...\n"));
                vboxTrayIconAdd();
            }
            break;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
