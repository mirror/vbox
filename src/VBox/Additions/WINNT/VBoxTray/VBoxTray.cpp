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
#include "VBoxHostVersion.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>
#include <VBoxGuestInternal.h>

#include "helpers.h"
#include <sddl.h>

#include <iprt/buildconfig.h>

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


/** Attempt to force Windows to reload the cursor image by attaching to the
 * thread of the window currently under the mouse, hiding the cursor and
 * showing it again.  This could fail to work in any number of ways (no
 * window under the cursor, the cursor has moved to a different window while
 * we are processing), but we just accept this, as the cursor will be reloaded
 * at some point anyway. */
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
            VBoxServiceReloadCursor();
    }

    Log(("VBoxTray: Window Handle = %p, Status = %p\n", gToolWindow, status));

    OSVERSIONINFO           info;
    DWORD                   dwMajorVersion = 5; /* default XP */
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(&info))
    {
        Log(("VBoxTray: Windows version major %d minor %d\n", info.dwMajorVersion, info.dwMinorVersion));
        dwMajorVersion = info.dwMajorVersion;
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

    /* initializes disp-if to default (XPDM) mode */
    status = VBoxDispIfInit(&svcEnv.dispIf);
#ifdef VBOXWDDM
    /* for now the display mode will be adjusted to WDDM mode if needed
     * on display service initialization when it detects the display driver type */
#endif

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
    RT_ZERO (ndata);
    ndata.cbSize           = NOTIFYICONDATA_V1_SIZE; // sizeof(NOTIFYICONDATA);
    ndata.hWnd             = gToolWindow;
    ndata.uID              = ID_TRAYICON;
    ndata.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    ndata.uCallbackMessage = WM_VBOX_TRAY;
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

                /* We're ready to create the tooltip balloon. */
                if (fTrayIconCreated && dwMajorVersion >= 5)
                {
                    /* Check in 10 seconds (@todo make seconds configurable) ... */
                    SetTimer(gToolWindow,
                             WM_VBOX_CHECK_HOSTVERSION,
                             10000, /* 10 seconds */
                             NULL   /* no timerproc */);
                }
            }
        }
    }

    Log(("VBoxTray: Returned from main loop, exiting ...\n"));

    /* remove the system tray icon and refresh system tray */
    Shell_NotifyIcon(NIM_DELETE, &ndata);
    HWND hTrayWnd = FindWindow("Shell_TrayWnd", NULL); /* We assume we only have one tray atm */
    HWND hTrayNotifyWnd = FindWindowEx(hTrayWnd, 0, "TrayNotifyWnd", NULL);
    if (hTrayNotifyWnd)
        SendMessage(hTrayNotifyWnd, WM_PAINT, 0, NULL);

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

    LogRel(("VBoxTray: %s r%s started.\n", RTBldCfgVersion(), RTBldCfgRevisionStr()));

    gInstance = hInstance;
    VBoxServiceStart();

    LogRel(("VBoxTray: Ended.\n"));

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
    switch (msg)
    {
        case WM_CLOSE:
            break;

        case WM_DESTROY:
            KillTimer(gToolWindow, WM_VBOX_CHECK_HOSTVERSION);
            break;

        case WM_TIMER:
            switch (wParam)
            {
                case WM_VBOX_CHECK_HOSTVERSION:
                    if (RT_SUCCESS(VBoxCheckHostVersion()))
                    {
                        /* After successful run we don't need to check again. */
                        KillTimer(gToolWindow, WM_VBOX_CHECK_HOSTVERSION);
                    }
                    return 0;

                default:
                    break;
            }

            break;

        case WM_VBOX_TRAY:
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

/* display driver interface abstraction for XPDM & WDDM
 * with WDDM we can not use ExtEscape to communicate with our driver
 * because we do not have XPDM display driver any more, i.e. escape requests are handled by cdd
 * that knows nothing about us */
DWORD VBoxDispIfInit(PVBOXDISPIF pIf)
{
    pIf->enmMode = VBOXDISPIF_MODE_XPDM;
    return NO_ERROR;
}

DWORD VBoxDispIfTerm(PVBOXDISPIF pIf)
{
    pIf->enmMode = VBOXDISPIF_MODE_UNKNOWN;
    return NO_ERROR;
}

static DWORD vboxDispIfEscapeXPDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    HDC  hdc = GetDC(HWND_DESKTOP);
    VOID *pvData = cbData ? VBOXDISPIFESCAPE_DATA(pEscape, VOID) : NULL;
    int iRet = ExtEscape(hdc, pEscape->escapeCode, cbData, (LPCSTR)pvData, 0, NULL);
    ReleaseDC(HWND_DESKTOP, hdc);
    if (iRet > 0)
        return VINF_SUCCESS;
    else if (iRet == 0)
        return ERROR_NOT_SUPPORTED;
    /* else */
    return ERROR_GEN_FAILURE;
}

#ifdef VBOXWDDM
static DWORD vboxDispIfSwitchToWDDM(PVBOXDISPIF pIf)
{
    DWORD err = NO_ERROR;
    OSVERSIONINFO OSinfo;
    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
    GetVersionEx (&OSinfo);
    if (OSinfo.dwMajorVersion >= 6)
    {
        /* this is vista and up */
        Log((__FUNCTION__": this is vista and up\n"));
        HMODULE hGdi32 = GetModuleHandle("gdi32");
        if (hGdi32 != NULL)
        {
            bool bSupported = true;
            pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc = (PFND3DKMT_OPENADAPTERFROMHDC)GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromHdc");
            Log((__FUNCTION__"pfnD3DKMTOpenAdapterFromHdc = %p\n", pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc));
            bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromHdc);

            pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName = (PFND3DKMT_OPENADAPTERFROMGDIDISPLAYNAME)GetProcAddress(hGdi32, "D3DKMTOpenAdapterFromGdiDisplayName");
            Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName = %p\n", pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName));
            bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName);

            pIf->modeData.wddm.pfnD3DKMTCloseAdapter = (PFND3DKMT_CLOSEADAPTER)GetProcAddress(hGdi32, "D3DKMTCloseAdapter");
            Log((__FUNCTION__": pfnD3DKMTCloseAdapter = %p\n", pIf->modeData.wddm.pfnD3DKMTCloseAdapter));
            bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTCloseAdapter);

            pIf->modeData.wddm.pfnD3DKMTEscape = (PFND3DKMT_ESCAPE)GetProcAddress(hGdi32, "D3DKMTEscape");
            Log((__FUNCTION__": pfnD3DKMTEscape = %p\n", pIf->modeData.wddm.pfnD3DKMTEscape));
            bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTCloseAdapter);

            pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn = (PFND3DKMT_INVALIDATEACTIVEVIDPN)GetProcAddress(hGdi32, "D3DKMTInvalidateActiveVidPn");
            Log((__FUNCTION__": pfnD3DKMTInvalidateActiveVidPn = %p\n", pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn));
            bSupported &= !!(pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn);

            if (!bSupported)
            {
                Log((__FUNCTION__": one of pfnD3DKMT function pointers failed to initialize\n"));
                err = ERROR_NOT_SUPPORTED;
            }
        }
        else
        {
            Log((__FUNCTION__": GetModuleHandle(gdi32) failed, err(%d)\n", GetLastError()));
            err = ERROR_NOT_SUPPORTED;
        }
    }
    else
    {
        Log((__FUNCTION__": can not switch to VBOXDISPIF_MODE_WDDM, because os is not Vista or upper\n"));
        err = ERROR_NOT_SUPPORTED;
    }

    return err;
}

typedef DECLCALLBACK(BOOLEAN) FNVBOXDISPIFWDDM_ADAPTEROP(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext);
typedef FNVBOXDISPIFWDDM_ADAPTEROP *PFNVBOXDISPIFWDDM_ADAPTEROP;
static DWORD vboxDispIfWDDMAdapterOp(PCVBOXDISPIF pIf, LPCWSTR pDevName, PFNVBOXDISPIFWDDM_ADAPTEROP pfnOp, PVOID pContext)
{
    D3DKMT_OPENADAPTERFROMGDIDISPLAYNAME OpenAdapterData = {0};
    wcsncpy(OpenAdapterData.DeviceName, pDevName, RT_ELEMENTS(OpenAdapterData.DeviceName) - 1 /* the last one is always \0 */);
    DWORD err = ERROR_GEN_FAILURE;
    NTSTATUS Status = pIf->modeData.wddm.pfnD3DKMTOpenAdapterFromGdiDisplayName(&OpenAdapterData);
    if (!Status)
    {
        BOOLEAN bCloseAdapter = pfnOp(pIf, OpenAdapterData.hAdapter, OpenAdapterData.DeviceName, pContext);

        if (bCloseAdapter)
        {
            D3DKMT_CLOSEADAPTER ClosaAdapterData = {0};
            ClosaAdapterData.hAdapter = OpenAdapterData.hAdapter;
            Status = pIf->modeData.wddm.pfnD3DKMTCloseAdapter(&ClosaAdapterData);
            if (Status)
            {
                Log((__FUNCTION__": pfnD3DKMTCloseAdapter failed, Status (0x%x)\n", Status));
                /* ignore */
                Status = 0;
            }
        }
    }
    else
        Log((__FUNCTION__": pfnD3DKMTOpenAdapterFromGdiDisplayName failed, Status (0x%x)\n", Status));

    return err;
}

typedef struct
{
    NTSTATUS Status;
    PVBOXDISPIFESCAPE pEscape;
    int cbData;
} VBOXDISPIFWDDM_ESCAPEOP_CONTEXT, *PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxDispIfEscapeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext)
{
    PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT pCtx = (PVBOXDISPIFWDDM_ESCAPEOP_CONTEXT)pContext;

    D3DKMT_ESCAPE EscapeData = {0};
    EscapeData.hAdapter = hAdapter;
    //EscapeData.hDevice = NULL;
    EscapeData.Type = D3DKMT_ESCAPE_DRIVERPRIVATE;
    EscapeData.Flags.HardwareAccess = 1;
    EscapeData.pPrivateDriverData = pCtx->pEscape;
    EscapeData.PrivateDriverDataSize = VBOXDISPIFESCAPE_SIZE(pCtx->cbData);
    //EscapeData.hContext = NULL;

    pCtx->Status = pIf->modeData.wddm.pfnD3DKMTEscape(&EscapeData);

    return TRUE;
}

static DWORD vboxDispIfEscapeWDDM(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    VBOXDISPIFWDDM_ESCAPEOP_CONTEXT Ctx = {0};
    Ctx.pEscape = pEscape;
    Ctx.cbData = cbData;
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, L"\\\\.\\DISPLAY1", vboxDispIfEscapeWDDMOp, &Ctx);
    if (err == NO_ERROR)
    {
        if (!Ctx.Status)
            err = NO_ERROR;
        else
        {
            if (Ctx.Status == 0xC00000BBL) /* not supported */
                err = ERROR_NOT_SUPPORTED;
            else
                err = ERROR_GEN_FAILURE;
            Log((__FUNCTION__": pfnD3DKMTEscape failed, Status (0x%x)\n", Ctx.Status));
        }
    }
    else
        Log((__FUNCTION__": vboxDispIfWDDMAdapterOp failed, err (%d)\n", err));

    return err;
}

typedef struct
{
    NTSTATUS Status;
    VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO Info;
} VBOXDISPIFWDDM_RESIZEOP_CONTEXT, *PVBOXDISPIFWDDM_RESIZEOP_CONTEXT;

DECLCALLBACK(BOOLEAN) vboxDispIfResizeWDDMOp(PCVBOXDISPIF pIf, D3DKMT_HANDLE hAdapter, LPCWSTR pDevName, PVOID pContext)
{
    PVBOXDISPIFWDDM_RESIZEOP_CONTEXT pCtx = (PVBOXDISPIFWDDM_RESIZEOP_CONTEXT)pContext;

    D3DKMT_INVALIDATEACTIVEVIDPN IAVidPnData = {0};
    uint32_t cbData = VBOXWDDM_RECOMMENDVIDPN_SIZE(1);
    PVBOXWDDM_RECOMMENDVIDPN pData = (PVBOXWDDM_RECOMMENDVIDPN)malloc(cbData);
    if (pData)
    {
        memset(pData, 0, cbData);
        pData->cScreenInfos = 1;
        memcpy(&pData->aScreenInfos[0], &pCtx->Info, sizeof (VBOXWDDM_RECOMMENDVIDPN_SCREEN_INFO));

        IAVidPnData.hAdapter = hAdapter;
        IAVidPnData.pPrivateDriverData = pData;
        IAVidPnData.PrivateDriverDataSize = cbData;

        pCtx->Status = pIf->modeData.wddm.pfnD3DKMTInvalidateActiveVidPn(&IAVidPnData);
        if (pCtx->Status)
            Log((__FUNCTION__": pfnD3DKMTInvalidateActiveVidPn failed, Status (0x%x)\n", pCtx->Status));

        free(pData);
    }
    else
    {
        Log((__FUNCTION__": malloc failed\n"));
        pCtx->Status = -1;
    }

    return TRUE;
}

static DWORD vboxDispIfResizeWDDM(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    VBOXDISPIFWDDM_RESIZEOP_CONTEXT Ctx = {0};
    Ctx.Info.Id = Id;
    Ctx.Info.Width = Width;
    Ctx.Info.Height = Height;
    Ctx.Info.BitsPerPixel = BitsPerPixel;
    DWORD err = vboxDispIfWDDMAdapterOp(pIf, L"\\\\.\\DISPLAY1", vboxDispIfResizeWDDMOp, &Ctx);
    if (err == NO_ERROR)
    {
        if (!Ctx.Status)
            err = NO_ERROR;
        else
        {
            if (Ctx.Status == 0xC00000BBL) /* not supported */
                err = ERROR_NOT_SUPPORTED;
            else
                err = ERROR_GEN_FAILURE;
            Log((__FUNCTION__": vboxDispIfResizeWDDMOp failed, Status (0x%x)\n", Ctx.Status));
        }
    }
    else
        Log((__FUNCTION__": vboxDispIfWDDMAdapterOp failed, err (%d)\n", err));

    return err;
}
#endif

DWORD VBoxDispIfEscape(PCVBOXDISPIF pIf, PVBOXDISPIFESCAPE pEscape, int cbData)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
        case VBOXDISPIF_MODE_XPDM:
            return vboxDispIfEscapeXPDM(pIf, pEscape, cbData);
#ifdef VBOXWDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfEscapeWDDM(pIf, pEscape, cbData);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vboxDispIfResizeXPDM(PCVBOXDISPIF const pIf, LPCSTR lpszDeviceName, LPDEVMODE lpDevMode, LONG *pResult)
{
    return ERROR_NOT_SUPPORTED;
}

DWORD VBoxDispIfResize(PCVBOXDISPIF const pIf, ULONG Id, DWORD Width, DWORD Height, DWORD BitsPerPixel)
{
    switch (pIf->enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            return ERROR_NOT_SUPPORTED;
        case VBOXDISPIF_MODE_XPDM:
            return vboxDispIfResizeXPDM(pIf, lpszDeviceName, lpDevMode, pResult);
#ifdef VBOXWDDM
        case VBOXDISPIF_MODE_WDDM:
            return vboxDispIfResizeWDDM(pIf, Id, Width, Height, BitsPerPixel);
#endif
        default:
            Log((__FUNCTION__": unknown mode (%d)\n", pIf->enmMode));
            return ERROR_INVALID_PARAMETER;
    }
}

static DWORD vboxDispIfSwitchToXPDM_NT4(PVBOXDISPIF pIf)
{
    return NO_ERROR;
}

static DWORD vboxDispIfSwitchToXPDM(PVBOXDISPIF pIf)
{
    DWORD err = NO_ERROR;
    AssertBreakpoint();
//    OSVERSIONINFO OSinfo;
//    OSinfo.dwOSVersionInfoSize = sizeof (OSinfo);
//    GetVersionEx (&OSinfo);
//    if (OSinfo.dwMajorVersion >= 5)
//    {
//        bool bSupported = true;
//        *(uintptr_t *)&pIf->modeData.xpdm.pfnChangeDisplaySettingsEx = (uintptr_t)GetProcAddress(hUser, "ChangeDisplaySettingsExA");
//        Log((__FUNCTION__": pfnChangeDisplaySettingsEx = %p\n", pIf->modeData.xpdm.pfnChangeDisplaySettingsEx));
//        bSupported &= !!(pIf->modeData.xpdm.pfnChangeDisplaySettingsEx);
//
//        if (!bSupported)
//        {
//            Log((__FUNCTION__": pfnChangeDisplaySettingsEx function pointer failed to initialize\n"));
//            err = ERROR_NOT_SUPPORTED;
//        }
//    }
//    else
//    {
//        Log((__FUNCTION__": can not switch to VBOXDISPIF_MODE_XPDM, because os is not >= w2k\n"));
//        err = ERROR_NOT_SUPPORTED;
//    }

    return err;
}

DWORD VBoxDispIfSwitchMode(PVBOXDISPIF pIf, VBOXDISPIF_MODE enmMode, VBOXDISPIF_MODE *penmOldMode)
{
    /* @todo: may need to addd synchronization in case we want to change modes dynamically
     * i.e. currently the mode is supposed to be initialized once on service initialization */
    if (penmOldMode)
        *penmOldMode = pIf->enmMode;

    if (enmMode == pIf->enmMode)
        return VINF_ALREADY_INITIALIZED;

    DWORD err = NO_ERROR;
    switch (enmMode)
    {
        case VBOXDISPIF_MODE_XPDM_NT4:
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_XPDM_NT4\n"));
            err = vboxDispIfSwitchToXPDM_NT4(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to XPDM_NT4 mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_XPDM_NT4;
            }
            else
                Log((__FUNCTION__": failed to switch to XPDM_NT4 mode, err (%d)\n", err));
            break;
        case VBOXDISPIF_MODE_XPDM:
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_XPDM\n"));
            err = vboxDispIfSwitchToXPDM(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to XPDM mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_XPDM;
            }
            else
                Log((__FUNCTION__": failed to switch to XPDM mode, err (%d)\n", err));
            break;
#ifdef VBOXWDDM
        case VBOXDISPIF_MODE_WDDM:
        {
            Log((__FUNCTION__": request to switch to VBOXDISPIF_MODE_WDDM\n"));
            err = vboxDispIfSwitchToWDDM(pIf);
            if (err == NO_ERROR)
            {
                Log((__FUNCTION__": successfully switched to WDDM mode\n"));
                pIf->enmMode = VBOXDISPIF_MODE_WDDM;
            }
            else
                Log((__FUNCTION__": failed to switch to WDDM mode, err (%d)\n", err));
            break;
        }
#endif
        default:
            err = ERROR_INVALID_PARAMETER;
            break;
    }
    return err;
}
