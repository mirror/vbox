/* $Id$ */
/** @file
 * VBoxTray - Guest Additions Tray Application
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include "VBoxTrayMsg.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include "VBoxClipboard.h"
#include "VBoxDisplay.h"
#include "VBoxRestore.h"
#include "VBoxVRDP.h"
#include "VBoxHostVersion.h"
#include "VBoxSharedFolders.h"
#include "VBoxIPC.h"
#include "VBoxLA.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>
#include <VBoxGuestInternal.h>

#include <sddl.h>

#include <iprt/buildconfig.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vboxTrayCreateTrayIcon(void);
static LRESULT CALLBACK vboxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Global message handler prototypes. */
static int vboxTrayGlMsgTaskbarCreated(WPARAM lParam, LPARAM wParam);
/*static int vboxTrayGlMsgShowBalloonMsg(WPARAM lParam, LPARAM wParam);*/


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
HANDLE                ghVBoxDriver;
HANDLE                ghStopSem;
HANDLE                ghSeamlessNotifyEvent = 0;
SERVICE_STATUS        gVBoxServiceStatus;
SERVICE_STATUS_HANDLE gVBoxServiceStatusHandle;
HINSTANCE             ghInstance;
HWND                  ghwndToolWindow;
NOTIFYICONDATA        gNotifyIconData;
DWORD                 gMajorVersion;


/* The service table. */
static VBOXSERVICEINFO vboxServiceTable[] =
{
    {
        "Display",
        VBoxDisplayInit,
        VBoxDisplayThread,
        VBoxDisplayDestroy
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
        VBoxRestoreDestroy
    },
#endif
    {
        "VRDP",
        VBoxVRDPInit,
        VBoxVRDPThread,
        VBoxVRDPDestroy
    },
    {
        "IPC",
        VBoxIPCInit,
        VBoxIPCThread,
        VBoxIPCDestroy
    },
    {
        "Location Awareness",
        VBoxLAInit,
        VBoxLAThread,
        VBoxLADestroy
    },
    {
        NULL
    }
};

/* The global message table. */
static VBOXGLOBALMESSAGE s_vboxGlobalMessageTable[] =
{
    /* Windows specific stuff. */
    {
        "TaskbarCreated",
        vboxTrayGlMsgTaskbarCreated
    },

    /* VBoxTray specific stuff. */
    /** @todo Add new messages here! */

    {
        NULL
    }
};

/**
 * Gets called whenever the Windows main taskbar
 * get (re-)created. Nice to install our tray icon.
 *
 * @return  IPRT status code.
 * @param   wParam
 * @param   lParam
 */
static int vboxTrayGlMsgTaskbarCreated(WPARAM wParam, LPARAM lParam)
{
    return vboxTrayCreateTrayIcon();
}

static int vboxTrayCreateTrayIcon(void)
{
    HICON hIcon = LoadIcon(ghInstance, MAKEINTRESOURCE(IDI_VIRTUALBOX));
    if (hIcon == NULL)
    {
        DWORD dwErr = GetLastError();
        Log(("VBoxTray: Could not load tray icon, error %08X\n", dwErr));
        return RTErrConvertFromWin32(dwErr);
    }

    /* Prepare the system tray icon. */
    RT_ZERO(gNotifyIconData);
    gNotifyIconData.cbSize           = NOTIFYICONDATA_V1_SIZE; // sizeof(NOTIFYICONDATA);
    gNotifyIconData.hWnd             = ghwndToolWindow;
    gNotifyIconData.uID              = ID_TRAYICON;
    gNotifyIconData.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gNotifyIconData.uCallbackMessage = WM_VBOXTRAY_TRAY_ICON;
    gNotifyIconData.hIcon            = hIcon;

    sprintf(gNotifyIconData.szTip, "%s Guest Additions %d.%d.%dr%d",
            VBOX_PRODUCT, VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);

    int rc = VINF_SUCCESS;
    if (!Shell_NotifyIcon(NIM_ADD, &gNotifyIconData))
    {
        DWORD dwErr = GetLastError();
        Log(("VBoxTray: Could not create tray icon, error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
        RT_ZERO(gNotifyIconData);
    }

    if (hIcon)
        DestroyIcon(hIcon);
    return rc;
}

static void vboxTrayRemoveTrayIcon()
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

static int vboxTrayStartServices(VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
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
                pTable->hThread = (HANDLE)_beginthreadex(NULL,  /* security */
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

static void vboxTrayStopServices(VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
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

static int vboxTrayRegisterGlobalMessages(PVBOXGLOBALMESSAGE pTable)
{
    int rc = VINF_SUCCESS;
    if (pTable == NULL) /* No table to register? Skip. */
        return rc;
    while (   pTable->pszName
           && RT_SUCCESS(rc))
    {
        /* Register global accessible window messages. */
        pTable->uMsgID = RegisterWindowMessage(TEXT(pTable->pszName));
        if (!pTable->uMsgID)
        {
            DWORD dwErr = GetLastError();
            Log(("VBoxTray: Registering global message \"%s\" failed, error = %08X\n", dwErr));
            rc = RTErrConvertFromWin32(dwErr);
        }

        /* Advance to next table element. */
        pTable++;
    }
    return rc;
}

static bool vboxTrayHandleGlobalMessages(PVBOXGLOBALMESSAGE pTable, UINT uMsg,
                                         WPARAM wParam, LPARAM lParam)
{
    if (pTable == NULL)
        return false;
    while (pTable && pTable->pszName)
    {
        if (pTable->uMsgID == uMsg)
        {
            if (pTable->pfnHandler)
                pTable->pfnHandler(wParam, lParam);
            return true;
        }

        /* Advance to next table element. */
        pTable++;
    }
    return false;
}

static int vboxTrayOpenBaseDriver(void)
{
    /* Open VBox guest driver. */
    DWORD dwErr = ERROR_SUCCESS;
    ghVBoxDriver = CreateFile(VBOXGUEST_DEVICE_NAME,
                             GENERIC_READ | GENERIC_WRITE,
                             FILE_SHARE_READ | FILE_SHARE_WRITE,
                             NULL,
                             OPEN_EXISTING,
                             FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED,
                             NULL);
    if (ghVBoxDriver == INVALID_HANDLE_VALUE)
    {
        dwErr = GetLastError();
        LogRel(("VBoxTray: Could not open VirtualBox Guest Additions driver! Please install / start it first! Error = %08X\n", dwErr));
    }
    return RTErrConvertFromWin32(dwErr);
}

static void vboxTrayCloseBaseDriver(void)
{
    if (ghVBoxDriver)
    {
        CloseHandle(ghVBoxDriver);
        ghVBoxDriver = NULL;
    }
}

static void vboxTrayDestroyToolWindow(void)
{
    if (ghwndToolWindow)
    {
        Log(("VBoxTray: Destroying tool window ...\n"));

        /* Destroy the tool window. */
        DestroyWindow(ghwndToolWindow);
        ghwndToolWindow = NULL;

        UnregisterClass("VBoxTrayToolWndClass", ghInstance);
    }
}

static int vboxTrayCreateToolWindow(void)
{
    DWORD dwErr = ERROR_SUCCESS;

    /* Create a custom window class. */
    WNDCLASS windowClass = {0};
    windowClass.style         = CS_NOCLOSE;
    windowClass.lpfnWndProc   = (WNDPROC)vboxToolWndProc;
    windowClass.hInstance     = ghInstance;
    windowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    windowClass.lpszClassName = "VBoxTrayToolWndClass";
    if (!RegisterClass(&windowClass))
    {
        dwErr = GetLastError();
        Log(("VBoxTray: Registering invisible tool window failed, error = %08X\n", dwErr));
    }
    else
    {
        /*
         * Create our (invisible) tool window.
         * Note: The window name ("VBoxTrayToolWnd") and class ("VBoxTrayToolWndClass") is
         * needed for posting globally registered messages to VBoxTray and must not be
         * changed! Otherwise things get broken!
         *
         */
        ghwndToolWindow = CreateWindowEx(WS_EX_TOOLWINDOW | WS_EX_TRANSPARENT | WS_EX_TOPMOST,
                                         "VBoxTrayToolWndClass", "VBoxTrayToolWnd",
                                         WS_POPUPWINDOW,
                                         -200, -200, 100, 100, NULL, NULL, ghInstance, NULL);
        if (!ghwndToolWindow)
        {
            dwErr = GetLastError();
            Log(("VBoxTray: Creating invisible tool window failed, error = %08X\n", dwErr));
        }
        else
        {
            /* Reload the cursor(s). */
            hlpReloadCursor();

            Log(("VBoxTray: Invisible tool window handle = %p\n", ghwndToolWindow));
        }
    }

    if (dwErr != ERROR_SUCCESS)
         vboxTrayDestroyToolWindow();
    return RTErrConvertFromWin32(dwErr);
}

static int vboxTraySetupSeamless(void)
{
    OSVERSIONINFO info;
    gMajorVersion = 5; /* Default to Windows XP. */
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(&info))
    {
        Log(("VBoxTray: Windows version %ld.%ld\n", info.dwMajorVersion, info.dwMinorVersion));
        gMajorVersion = info.dwMajorVersion;
    }

    /* We need to setup a security descriptor to allow other processes modify access to the seamless notification event semaphore. */
    SECURITY_ATTRIBUTES     SecAttr;
    DWORD                   dwErr = ERROR_SUCCESS;
    char                    secDesc[SECURITY_DESCRIPTOR_MIN_LENGTH];
    BOOL                    fRC;

    SecAttr.nLength              = sizeof(SecAttr);
    SecAttr.bInheritHandle       = FALSE;
    SecAttr.lpSecurityDescriptor = &secDesc;
    InitializeSecurityDescriptor(SecAttr.lpSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    fRC = SetSecurityDescriptorDacl(SecAttr.lpSecurityDescriptor, TRUE, 0, FALSE);
    if (!fRC)
    {
        dwErr = GetLastError();
        Log(("VBoxTray: SetSecurityDescriptorDacl failed with last error = %08X\n", dwErr));
    }
    else
    {
        /* For Vista and up we need to change the integrity of the security descriptor, too. */
        if (gMajorVersion >= 6)
        {
            BOOL (WINAPI * pfnConvertStringSecurityDescriptorToSecurityDescriptorA)(LPCSTR StringSecurityDescriptor, DWORD StringSDRevision, PSECURITY_DESCRIPTOR  *SecurityDescriptor, PULONG  SecurityDescriptorSize);

            HMODULE hModule = LoadLibrary("ADVAPI32.DLL");
            if (!hModule)
            {
                dwErr = GetLastError();
                Log(("VBoxTray: Loading module ADVAPI32.DLL failed with last error = %08X\n", dwErr));
            }
            else
            {
                PSECURITY_DESCRIPTOR    pSD;
                PACL                    pSacl          = NULL;
                BOOL                    fSaclPresent   = FALSE;
                BOOL                    fSaclDefaulted = FALSE;

                *(uintptr_t *)&pfnConvertStringSecurityDescriptorToSecurityDescriptorA = (uintptr_t)GetProcAddress(hModule, "ConvertStringSecurityDescriptorToSecurityDescriptorA");

                Log(("VBoxTray: pfnConvertStringSecurityDescriptorToSecurityDescriptorA = %x\n", pfnConvertStringSecurityDescriptorToSecurityDescriptorA));
                if (pfnConvertStringSecurityDescriptorToSecurityDescriptorA)
                {
                    fRC = pfnConvertStringSecurityDescriptorToSecurityDescriptorA("S:(ML;;NW;;;LW)", /* this means "low integrity" */
                                                                                  SDDL_REVISION_1, &pSD, NULL);
                    if (!fRC)
                    {
                        dwErr = GetLastError();
                        Log(("VBoxTray: ConvertStringSecurityDescriptorToSecurityDescriptorA failed with last error = %08X\n", dwErr));
                    }
                    else
                    {
                        fRC = GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
                        if (!fRC)
                        {
                            dwErr = GetLastError();
                            Log(("VBoxTray: GetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                        }
                        else
                        {
                            fRC = SetSecurityDescriptorSacl(SecAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
                            if (!fRC)
                            {
                                dwErr = GetLastError();
                                Log(("VBoxTray: SetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                            }
                        }
                    }
                }
            }
        }

        if (   dwErr == ERROR_SUCCESS
            && gMajorVersion >= 5) /* Only for W2K and up ... */
        {
            ghSeamlessNotifyEvent = CreateEvent(&SecAttr, FALSE, FALSE, VBOXHOOK_GLOBAL_EVENT_NAME);
            if (ghSeamlessNotifyEvent == NULL)
            {
                dwErr = GetLastError();
                Log(("VBoxTray: CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            }
        }
    }
    return RTErrConvertFromWin32(dwErr);
}

static void vboxTrayShutdownSeamless(void)
{
    if (ghSeamlessNotifyEvent)
    {
        CloseHandle(ghSeamlessNotifyEvent);
        ghSeamlessNotifyEvent = NULL;
    }
}

static int vboxTrayServiceMain(void)
{
    int rc = VINF_SUCCESS;
    Log(("VBoxTray: Entering vboxTrayServiceMain\n"));

    ghStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ghStopSem == NULL)
    {
        rc = RTErrConvertFromWin32(GetLastError());
        Log(("VBoxTray: CreateEvent for stopping VBoxTray failed, rc=%Rrc\n", rc));
    }
    else
    {
        /*
         * Start services listed in the vboxServiceTable.
         */
        VBOXSERVICEENV svcEnv;
        svcEnv.hInstance = ghInstance;
        svcEnv.hDriver   = ghVBoxDriver;

        /* Initializes disp-if to default (XPDM) mode. */
        VBoxDispIfInit(&svcEnv.dispIf); /* Cannot fail atm. */
    #ifdef VBOX_WITH_WDDM
        /*
         * For now the display mode will be adjusted to WDDM mode if needed
         * on display service initialization when it detects the display driver type.
         */
    #endif

        /* Finally start all the built-in services! */
        rc = vboxTrayStartServices(&svcEnv, vboxServiceTable);
        if (RT_FAILURE(rc))
        {
            /* Terminate service if something went wrong. */
            vboxTrayStopServices(&svcEnv, vboxServiceTable);
        }
        else
        {
            rc = vboxTrayCreateTrayIcon();
            if (   RT_SUCCESS(rc)
                && gMajorVersion >= 5) /* Only for W2K and up ... */
            {
                /* We're ready to create the tooltip balloon.
                   Check in 10 seconds (@todo make seconds configurable) ... */
                SetTimer(ghwndToolWindow,
                         TIMERID_VBOXTRAY_CHECK_HOSTVERSION,
                         10 * 1000, /* 10 seconds */
                         NULL       /* No timerproc */);
            }

            if (RT_SUCCESS(rc))
            {
                /* Do the Shared Folders auto-mounting stuff. */
                rc = VBoxSharedFoldersAutoMount();
                if (RT_SUCCESS(rc))
                {
                    /* Report the host that we're up and running! */
                    hlpReportStatus(VBoxGuestFacilityStatus_Active);
                }
            }

            if (RT_SUCCESS(rc))
            {
                /* Boost thread priority to make sure we wake up early for seamless window notifications
                 * (not sure if it actually makes any difference though). */
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

                /*
                 * Main execution loop
                 * Wait for the stop semaphore to be posted or a window event to arrive
                 */

                DWORD dwEventCount = 2;
                HANDLE hWaitEvent[2] = { ghStopSem, ghSeamlessNotifyEvent };

                if (0 == ghSeamlessNotifyEvent) /* If seamless mode is not active / supported, reduce event array count. */
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
                    else if (   waitResult == 1
                             && ghSeamlessNotifyEvent != 0) /* Only jump in, if seamless is active! */
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
                                SetEvent(ghStopSem);
                                continue;
                            }
                            TranslateMessage(&msg);
                            DispatchMessage(&msg);
                        }
                    }
                }
                Log(("VBoxTray: Returned from main loop, exiting ...\n"));
            }
            Log(("VBoxTray: Waiting for services to stop ...\n"));
            vboxTrayStopServices(&svcEnv, vboxServiceTable);
        } /* Services started */
        CloseHandle(ghStopSem);
    } /* Stop event created */

    vboxTrayRemoveTrayIcon();

    Log(("VBoxTray: Leaving vboxTrayServiceMain with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Main function
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* Do not use a global namespace ("Global\\") for mutex name here, will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex(NULL, FALSE, "VBoxTray");
    if (   hMutexAppRunning != NULL
        && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        /* Close the mutex for this application instance. */
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
        return 0;
    }

    LogRel(("VBoxTray: %s r%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr()));

    int rc = RTR3InitExeNoArguments(0);
    if (RT_SUCCESS(rc))
    {
        rc = VbglR3Init();
        if (RT_SUCCESS(rc))
            rc = vboxTrayOpenBaseDriver();
    }

    if (RT_SUCCESS(rc))
    {
        /* Save instance handle. */
        ghInstance = hInstance;

        hlpReportStatus(VBoxGuestFacilityStatus_Init);
        rc = vboxTrayCreateToolWindow();
        if (RT_SUCCESS(rc))
        {
            rc = vboxTraySetupSeamless();
            if (RT_SUCCESS(rc))
            {
                Log(("VBoxTray: Init successful\n"));
                rc = vboxTrayServiceMain();
                if (RT_SUCCESS(rc))
                    hlpReportStatus(VBoxGuestFacilityStatus_Terminating);
                vboxTrayShutdownSeamless();
            }
            vboxTrayDestroyToolWindow();
        }
        if (RT_SUCCESS(rc))
            hlpReportStatus(VBoxGuestFacilityStatus_Terminated);
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("VBoxTray: Error while starting, rc=%Rrc\n", rc));
        hlpReportStatus(VBoxGuestFacilityStatus_Failed);
    }
    LogRel(("VBoxTray: Ended\n"));
    vboxTrayCloseBaseDriver();

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL)
    {
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    VbglR3Term();
    return RT_SUCCESS(rc) ? 0 : 1;
}

/**
 * Window procedure for our tool window
 */
static LRESULT CALLBACK vboxToolWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
        case WM_CREATE:
        {
            Log(("VBoxTray: Tool window created\n"));

            int rc = vboxTrayRegisterGlobalMessages(&s_vboxGlobalMessageTable[0]);
            if (RT_FAILURE(rc))
                Log(("VBoxTray: Error registering global window messages, rc=%Rrc\n", rc));
            return 0;
        }

        case WM_CLOSE:
            return 0;

        case WM_DESTROY:
            Log(("VBoxTray: Tool window destroyed\n"));
            KillTimer(ghwndToolWindow, TIMERID_VBOXTRAY_CHECK_HOSTVERSION);
            return 0;

        case WM_TIMER:
            switch (wParam)
            {
                case TIMERID_VBOXTRAY_CHECK_HOSTVERSION:
                    if (RT_SUCCESS(VBoxCheckHostVersion()))
                    {
                        /* After successful run we don't need to check again. */
                        KillTimer(ghwndToolWindow, TIMERID_VBOXTRAY_CHECK_HOSTVERSION);
                    }
                    return 0;

                default:
                    break;
            }
            break; /* Make sure other timers get processed the usual way! */

        case WM_VBOXTRAY_TRAY_ICON:
            switch (lParam)
            {
                case WM_LBUTTONDBLCLK:
                    break;

                case WM_RBUTTONDOWN:
                    break;
            }
            return 0;

        case WM_VBOX_INSTALL_SEAMLESS_HOOK:
            VBoxSeamlessInstallHook();
            return 0;

        case WM_VBOX_REMOVE_SEAMLESS_HOOK:
            VBoxSeamlessRemoveHook();
            return 0;

        case WM_VBOX_SEAMLESS_UPDATE:
            VBoxSeamlessCheckWindows();
            return 0;

        case WM_VBOXTRAY_VM_RESTORED:
            VBoxRestoreSession();
            return 0;

        case WM_VBOXTRAY_VRDP_CHECK:
            VBoxRestoreCheckVRDP();
            return 0;

        default:

            /* Handle all globally registered window messages. */
            if (vboxTrayHandleGlobalMessages(&s_vboxGlobalMessageTable[0], uMsg,
                                             wParam, lParam))
            {
                return 0; /* We handled the message. @todo Add return value!*/
            }
            break; /* We did not handle the message, dispatch to DefWndProc. */
    }

    /* Only if message was *not* handled by our switch above, dispatch
     * to DefWindowProc. */
    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

