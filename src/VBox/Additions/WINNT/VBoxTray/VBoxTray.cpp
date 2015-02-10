/* $Id$ */
/** @file
 * VBoxTray - Guest Additions Tray Application
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <package-generated.h>
#include "product-generated.h"

#include "VBoxTray.h"
#include "VBoxTrayMsg.h"
#include "VBoxHelpers.h"
#include "VBoxSeamless.h"
#include "VBoxClipboard.h"
#include "VBoxDisplay.h"
#include "VBoxVRDP.h"
#include "VBoxHostVersion.h"
#include "VBoxSharedFolders.h"
#ifdef VBOX_WITH_DRAG_AND_DROP
# include "VBoxDnD.h"
#endif
#include "VBoxIPC.h"
#include "VBoxLA.h"
#include <VBoxHook.h>
#include "resource.h"
#include <malloc.h>
#include <VBoxGuestInternal.h>

#include <sddl.h>

#include <iprt/buildconfig.h>
#include <iprt/ldr.h>
#include <iprt/process.h>
#include <iprt/system.h>
#include <iprt/time.h>

#ifdef DEBUG
# define LOG_ENABLED
# define LOG_GROUP LOG_GROUP_DEFAULT
#endif
#include <VBox/log.h>

/* Default desktop state tracking */
#include <Wtsapi32.h>

/*
 * St (session [state] tracking) functionality API
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * it is supposed to be called & used from within the window message handler thread
 * of the window passed to vboxStInit */
static int vboxStInit(HWND hWnd);
static void vboxStTerm(void);
/* @returns true on "IsActiveConsole" state change */
static BOOL vboxStHandleEvent(WPARAM EventID, LPARAM SessionID);
static BOOL vboxStIsActiveConsole();
static BOOL vboxStCheckTimer(WPARAM wEvent);

/*
 * Dt (desktop [state] tracking) functionality API
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * */
static int vboxDtInit();
static void vboxDtTerm();
/* @returns true on "IsInputDesktop" state change */
static BOOL vboxDtHandleEvent();
/* @returns true iff the application (VBoxTray) desktop is input */
static BOOL vboxDtIsInputDesktop();
static HANDLE vboxDtGetNotifyEvent();
static BOOL vboxDtCheckTimer(WPARAM wParam);

/* caps API */
#define VBOXCAPS_ENTRY_IDX_SEAMLESS  0
#define VBOXCAPS_ENTRY_IDX_GRAPHICS  1
#define VBOXCAPS_ENTRY_IDX_COUNT     2

typedef enum VBOXCAPS_ENTRY_FUNCSTATE
{
    /* the cap is unsupported */
    VBOXCAPS_ENTRY_FUNCSTATE_UNSUPPORTED = 0,
    /* the cap is supported */
    VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED,
    /* the cap functionality is started, it can be disabled however if its AcState is not ACQUIRED */
    VBOXCAPS_ENTRY_FUNCSTATE_STARTED,
} VBOXCAPS_ENTRY_FUNCSTATE;


static void VBoxCapsEntryFuncStateSet(uint32_t iCup, VBOXCAPS_ENTRY_FUNCSTATE enmFuncState);
static int VBoxCapsInit();
static int VBoxCapsReleaseAll();
static void VBoxCapsTerm();
static BOOL VBoxCapsEntryIsAcquired(uint32_t iCap);
static BOOL VBoxCapsEntryIsEnabled(uint32_t iCap);
static BOOL VBoxCapsCheckTimer(WPARAM wParam);
static int VBoxCapsEntryRelease(uint32_t iCap);
static int VBoxCapsEntryAcquire(uint32_t iCap);
static int VBoxCapsAcquireAllSupported();

/* console-related caps API */
static BOOL VBoxConsoleIsAllowed();
static void VBoxConsoleEnable(BOOL fEnable);
static void VBoxConsoleCapSetSupported(uint32_t iCap, BOOL fSupported);

static void VBoxGrapicsSetSupported(BOOL fSupported);

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int vboxTrayCreateTrayIcon(void);
static LRESULT CALLBACK vboxToolWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

/* Global message handler prototypes. */
static int vboxTrayGlMsgTaskbarCreated(WPARAM lParam, LPARAM wParam);
/*static int vboxTrayGlMsgShowBalloonMsg(WPARAM lParam, LPARAM wParam);*/

static int VBoxAcquireGuestCaps(uint32_t fOr, uint32_t fNot, bool fCfg);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
HANDLE                ghVBoxDriver;
HANDLE                ghStopSem;
HANDLE                ghSeamlessWtNotifyEvent = 0;
HANDLE                ghSeamlessKmNotifyEvent = 0;
SERVICE_STATUS        gVBoxServiceStatus;
SERVICE_STATUS_HANDLE gVBoxServiceStatusHandle;
HINSTANCE             ghInstance;
HWND                  ghwndToolWindow;
NOTIFYICONDATA        gNotifyIconData;
DWORD                 gMajorVersion;

static PRTLOGGER      g_pLoggerRelease = NULL;
static uint32_t       g_cHistory = 10;                   /* Enable log rotation, 10 files. */
static uint32_t       g_uHistoryFileTime = RT_SEC_1DAY;  /* Max 1 day per file. */
static uint64_t       g_uHistoryFileSize = 100 * _1M;    /* Max 100MB per file. */

/* The service table. */
static VBOXSERVICEINFO vboxServiceTable[] =
{
    {
        "Display",
        VBoxDisplayInit,
        VBoxDisplayThread,
        NULL /* pfnStop */,
        VBoxDisplayDestroy
    },
    {
        "Shared Clipboard",
        VBoxClipboardInit,
        VBoxClipboardThread,
        NULL /* pfnStop */,
        VBoxClipboardDestroy
    },
    {
        "Seamless Windows",
        VBoxSeamlessInit,
        VBoxSeamlessThread,
        NULL /* pfnStop */,
        VBoxSeamlessDestroy
    },
    {
        "VRDP",
        VBoxVRDPInit,
        VBoxVRDPThread,
        NULL /* pfnStop */,
        VBoxVRDPDestroy
    },
    {
        "IPC",
        VBoxIPCInit,
        VBoxIPCThread,
        VBoxIPCStop,
        VBoxIPCDestroy
    },
    {
        "Location Awareness",
        VBoxLAInit,
        VBoxLAThread,
        NULL /* pfnStop */,
        VBoxLADestroy
    },
#ifdef VBOX_WITH_DRAG_AND_DROP
    {
        "Drag and Drop",
        VBoxDnDInit,
        VBoxDnDThread,
        VBoxDnDStop,
        VBoxDnDDestroy
    },
#endif
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
        Log(("Could not load tray icon, error %08X\n", dwErr));
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
        Log(("Could not create tray icon, error = %08X\n", dwErr));
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
    AssertPtrReturn(pEnv, VERR_INVALID_POINTER);
    AssertPtrReturn(pTable, VERR_INVALID_POINTER);

    Log(("Starting services ...\n"));

    /** @todo Use IPRT events here. */
    pEnv->hStopEvent = CreateEvent(NULL, TRUE /* bManualReset */,
                                   FALSE /* bInitialState */, NULL);

    if (!pEnv->hStopEvent)
    {
        /* Could not create event. */
        return VERR_NOT_SUPPORTED;
    }

    while (   pTable
           && pTable->pszName)
    {
        Log(("Starting %s ...\n", pTable->pszName));

        int rc = VINF_SUCCESS;

        bool fStartThread = false;

        pTable->hThread = (HANDLE)0;
        pTable->pInstance = NULL;
        pTable->fStarted = false;

        if (pTable->pfnInit)
            rc = pTable->pfnInit(pEnv, &pTable->pInstance, &fStartThread);

        if (RT_FAILURE(rc))
        {
            LogRel(("Failed to initialize service \"%s\", rc=%Rrc\n",
                    pTable->pszName, rc));
        }
        else
        {
            if (   pTable->pfnThread
                && fStartThread)
            {
                unsigned threadid;
                /** @todo Use RTThread* here. */
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
                Log(("Failed to start the thread\n"));
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

    VBOXSERVICEINFO *pCurTable = pTable;
    while (   pCurTable
           && pCurTable->pszName)
    {
        if (pCurTable->pfnStop)
            pCurTable->pfnStop(pEnv, pCurTable->pInstance);

        /* Advance to next table element. */
        pCurTable++;
    }

    pCurTable = pTable; /* Reset to first element. */
    while (   pCurTable
           && pCurTable->pszName)
    {
        if (pCurTable->fStarted)
        {
            if (pCurTable->pfnThread)
            {
                /* There is a thread, wait for termination. */
                /** @todo Use RTThread* here. */
                /** @todo Don't wait forever here. Use a sensible default. */
                WaitForSingleObject(pCurTable->hThread, INFINITE);

                /** @todo Dito. */
                CloseHandle(pCurTable->hThread);
                pCurTable->hThread = NULL;
            }

            if (pCurTable->pfnDestroy)
                pCurTable->pfnDestroy(pEnv, pCurTable->pInstance);
            pCurTable->fStarted = false;
        }

        /* Advance to next table element. */
        pCurTable++;
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
            Log(("Registering global message \"%s\" failed, error = %08X\n", dwErr));
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
        LogRel(("Could not open VirtualBox Guest Additions driver! Please install / start it first! Error = %08X\n", dwErr));
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

/**
 * Release logger callback.
 *
 * @return  IPRT status code.
 * @param   pLoggerRelease
 * @param   enmPhase
 * @param   pfnLog
 */
static void vboxTrayLogHeaderFooter(PRTLOGGER pLoggerRelease, RTLOGPHASE enmPhase, PFNRTLOGPHASEMSG pfnLog)
{
    /* Some introductory information. */
    static RTTIMESPEC s_TimeSpec;
    char szTmp[256];
    if (enmPhase == RTLOGPHASE_BEGIN)
        RTTimeNow(&s_TimeSpec);
    RTTimeSpecToString(&s_TimeSpec, szTmp, sizeof(szTmp));

    switch (enmPhase)
    {
        case RTLOGPHASE_BEGIN:
        {
            pfnLog(pLoggerRelease,
                   "VBoxTray %s r%s %s (%s %s) release log\n"
                   "Log opened %s\n",
                   RTBldCfgVersion(), RTBldCfgRevisionStr(), VBOX_BUILD_TARGET,
                   __DATE__, __TIME__, szTmp);

            int vrc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Product: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Release: %s\n", szTmp);
            vrc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szTmp, sizeof(szTmp));
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Version: %s\n", szTmp);
            if (RT_SUCCESS(vrc) || vrc == VERR_BUFFER_OVERFLOW)
                pfnLog(pLoggerRelease, "OS Service Pack: %s\n", szTmp);

            /* the package type is interesting for Linux distributions */
            char szExecName[RTPATH_MAX];
            char *pszExecName = RTProcGetExecutablePath(szExecName, sizeof(szExecName));
            pfnLog(pLoggerRelease,
                   "Executable: %s\n"
                   "Process ID: %u\n"
                   "Package type: %s"
#ifdef VBOX_OSE
                   " (OSE)"
#endif
                   "\n",
                   pszExecName ? pszExecName : "unknown",
                   RTProcSelf(),
                   VBOX_PACKAGE_STRING);
            break;
        }

        case RTLOGPHASE_PREROTATE:
            pfnLog(pLoggerRelease, "Log rotated - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_POSTROTATE:
            pfnLog(pLoggerRelease, "Log continuation - Log started %s\n", szTmp);
            break;

        case RTLOGPHASE_END:
            pfnLog(pLoggerRelease, "End of log file - Log started %s\n", szTmp);
            break;

        default:
            /* nothing */;
    }
}

/**
 * Creates the default release logger outputting to the specified file.
 * Pass NULL for disabled logging.
 *
 * @return  IPRT status code.
 * @param   pszLogFile              Filename for log output.  Optional.
 */
static int vboxTrayLogCreate(const char *pszLogFile)
{
    /* Create release logger (stdout + file). */
    static const char * const s_apszGroups[] = VBOX_LOGGROUP_NAMES;
    RTUINT fFlags = RTLOGFLAGS_PREFIX_THREAD | RTLOGFLAGS_PREFIX_TIME_PROG;
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    fFlags |= RTLOGFLAGS_USECRLF;
#endif
    char szError[RTPATH_MAX + 128] = "";
    int rc = RTLogCreateEx(&g_pLoggerRelease, fFlags,
#ifdef DEBUG
                           "all.e.l.f",
                           "VBOXTRAY_LOG",
#else
                           "all",
                           "VBOXTRAY_RELEASE_LOG",
#endif
                           RT_ELEMENTS(s_apszGroups), s_apszGroups, RTLOGDEST_STDOUT,
                           vboxTrayLogHeaderFooter, g_cHistory, g_uHistoryFileSize, g_uHistoryFileTime,
                           szError, sizeof(szError), pszLogFile);
    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG
        RTLogSetDefaultInstance(g_pLoggerRelease);
#else
        /* Register this logger as the release logger. */
        RTLogRelSetDefaultInstance(g_pLoggerRelease);
#endif
        /* Explicitly flush the log in case of VBOXTRAY_RELEASE_LOG=buffered. */
        RTLogFlush(g_pLoggerRelease);
    }
    else
        MessageBox(GetDesktopWindow(),
                   szError, "VBoxTray - Logging Error", MB_OK | MB_ICONERROR);

    return rc;
}

static void vboxTrayLogDestroy(void)
{
    RTLogDestroy(RTLogRelSetDefaultInstance(NULL));
}

static void vboxTrayDestroyToolWindow(void)
{
    if (ghwndToolWindow)
    {
        Log(("Destroying tool window ...\n"));

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
        Log(("Registering invisible tool window failed, error = %08X\n", dwErr));
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
            Log(("Creating invisible tool window failed, error = %08X\n", dwErr));
        }
        else
        {
            /* Reload the cursor(s). */
            hlpReloadCursor();

            Log(("Invisible tool window handle = %p\n", ghwndToolWindow));
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
        Log(("Windows version %ld.%ld\n", info.dwMajorVersion, info.dwMinorVersion));
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
        Log(("SetSecurityDescriptorDacl failed with last error = %08X\n", dwErr));
    }
    else
    {
        /* For Vista and up we need to change the integrity of the security descriptor, too. */
        if (gMajorVersion >= 6)
        {
            BOOL (WINAPI * pfnConvertStringSecurityDescriptorToSecurityDescriptorA)(LPCSTR StringSecurityDescriptor, DWORD StringSDRevision, PSECURITY_DESCRIPTOR  *SecurityDescriptor, PULONG  SecurityDescriptorSize);
            *(void **)&pfnConvertStringSecurityDescriptorToSecurityDescriptorA =
                RTLdrGetSystemSymbol("advapi32.dll", "ConvertStringSecurityDescriptorToSecurityDescriptorA");
            Log(("pfnConvertStringSecurityDescriptorToSecurityDescriptorA = %x\n", pfnConvertStringSecurityDescriptorToSecurityDescriptorA));
            if (pfnConvertStringSecurityDescriptorToSecurityDescriptorA)
            {
                PSECURITY_DESCRIPTOR    pSD;
                PACL                    pSacl          = NULL;
                BOOL                    fSaclPresent   = FALSE;
                BOOL                    fSaclDefaulted = FALSE;

                fRC = pfnConvertStringSecurityDescriptorToSecurityDescriptorA("S:(ML;;NW;;;LW)", /* this means "low integrity" */
                                                                              SDDL_REVISION_1, &pSD, NULL);
                if (!fRC)
                {
                    dwErr = GetLastError();
                    Log(("ConvertStringSecurityDescriptorToSecurityDescriptorA failed with last error = %08X\n", dwErr));
                }
                else
                {
                    fRC = GetSecurityDescriptorSacl(pSD, &fSaclPresent, &pSacl, &fSaclDefaulted);
                    if (!fRC)
                    {
                        dwErr = GetLastError();
                        Log(("GetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                    }
                    else
                    {
                        fRC = SetSecurityDescriptorSacl(SecAttr.lpSecurityDescriptor, TRUE, pSacl, FALSE);
                        if (!fRC)
                        {
                            dwErr = GetLastError();
                            Log(("SetSecurityDescriptorSacl failed with last error = %08X\n", dwErr));
                        }
                    }
                }
            }
        }

        if (   dwErr == ERROR_SUCCESS
            && gMajorVersion >= 5) /* Only for W2K and up ... */
        {
            ghSeamlessWtNotifyEvent = CreateEvent(&SecAttr, FALSE, FALSE, VBOXHOOK_GLOBAL_WT_EVENT_NAME);
            if (ghSeamlessWtNotifyEvent == NULL)
            {
                dwErr = GetLastError();
                Log(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            }

            ghSeamlessKmNotifyEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
            if (ghSeamlessKmNotifyEvent == NULL)
            {
                dwErr = GetLastError();
                Log(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            }
        }
    }
    return RTErrConvertFromWin32(dwErr);
}

static void vboxTrayShutdownSeamless(void)
{
    if (ghSeamlessWtNotifyEvent)
    {
        CloseHandle(ghSeamlessWtNotifyEvent);
        ghSeamlessWtNotifyEvent = NULL;
    }

    if (ghSeamlessKmNotifyEvent)
    {
        CloseHandle(ghSeamlessKmNotifyEvent);
        ghSeamlessKmNotifyEvent = NULL;
    }
}

static void VBoxTrayCheckDt()
{
    BOOL fOldAllowedState = VBoxConsoleIsAllowed();
    if (vboxDtHandleEvent())
    {
        if (!VBoxConsoleIsAllowed() != !fOldAllowedState)
            VBoxConsoleEnable(!fOldAllowedState);
    }
}

static int vboxTrayServiceMain(void)
{
    int rc = VINF_SUCCESS;
    Log(("Entering vboxTrayServiceMain\n"));

    ghStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (ghStopSem == NULL)
    {
        rc = RTErrConvertFromWin32(GetLastError());
        Log(("CreateEvent for stopping VBoxTray failed, rc=%Rrc\n", rc));
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

                HANDLE hWaitEvent[4] = {0};
                DWORD dwEventCount = 0;

                hWaitEvent[dwEventCount++] = ghStopSem;

                /* Check if seamless mode is not active and add seamless event to the list */
                if (0 != ghSeamlessWtNotifyEvent)
                {
                    hWaitEvent[dwEventCount++] = ghSeamlessWtNotifyEvent;
                }

                if (0 != ghSeamlessKmNotifyEvent)
                {
                    hWaitEvent[dwEventCount++] = ghSeamlessKmNotifyEvent;
                }

                if (0 != vboxDtGetNotifyEvent())
                {
                    hWaitEvent[dwEventCount++] = vboxDtGetNotifyEvent();
                }

                Log(("Number of events to wait in main loop: %ld\n", dwEventCount));
                while (true)
                {
                    DWORD waitResult = MsgWaitForMultipleObjectsEx(dwEventCount, hWaitEvent, 500, QS_ALLINPUT, 0);
                    waitResult = waitResult - WAIT_OBJECT_0;

                    /* Only enable for message debugging, lots of traffic! */
                    //Log(("Wait result  = %ld\n", waitResult));

                    if (waitResult == 0)
                    {
                        Log(("Event 'Exit' triggered\n"));
                        /* exit */
                        break;
                    }
                    else
                    {
                        BOOL fHandled = FALSE;
                        if (waitResult < RT_ELEMENTS(hWaitEvent))
                        {
                            if (hWaitEvent[waitResult])
                            {
                                if (hWaitEvent[waitResult] == ghSeamlessWtNotifyEvent)
                                {
                                    Log(("Event 'Seamless' triggered\n"));

                                    /* seamless window notification */
                                    VBoxSeamlessCheckWindows(false);
                                    fHandled = TRUE;
                                }
                                else if (hWaitEvent[waitResult] == ghSeamlessKmNotifyEvent)
                                {
                                    Log(("Event 'Km Seamless' triggered\n"));

                                    /* seamless window notification */
                                    VBoxSeamlessCheckWindows(true);
                                    fHandled = TRUE;
                                }
                                else if (hWaitEvent[waitResult] == vboxDtGetNotifyEvent())
                                {
                                    Log(("Event 'Dt' triggered\n"));
                                    VBoxTrayCheckDt();
                                    fHandled = TRUE;
                                }
                            }
                        }

                        if (!fHandled)
                        {
                            /* timeout or a window message, handle it */
                            MSG msg;
                            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                            {
#ifndef DEBUG_andy
                                Log(("msg %p\n", msg.message));
#endif
                                if (msg.message == WM_QUIT)
                                {
                                    Log(("WM_QUIT!\n"));
                                    SetEvent(ghStopSem);
                                }
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }
                    }
                }
                Log(("Returned from main loop, exiting ...\n"));
            }
            Log(("Waiting for services to stop ...\n"));
            vboxTrayStopServices(&svcEnv, vboxServiceTable);
        } /* Services started */
        CloseHandle(ghStopSem);
    } /* Stop event created */

    vboxTrayRemoveTrayIcon();

    Log(("Leaving vboxTrayServiceMain with rc=%Rrc\n", rc));
    return rc;
}

/**
 * Main function
 */
int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    /* Note: Do not use a global namespace ("Global\\") for mutex name here,
     * will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex(NULL, FALSE, "VBoxTray");
    if (   hMutexAppRunning != NULL
        && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        /* VBoxTray already running? Bail out. */
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
        return 0;
    }

    LogRel(("%s r%s\n", RTBldCfgVersion(), RTBldCfgRevisionStr()));

    int rc = RTR3InitExeNoArguments(0);
    if (RT_SUCCESS(rc))
        rc = vboxTrayLogCreate(NULL /* pszLogFile */);

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
            VBoxCapsInit();

            rc = vboxStInit(ghwndToolWindow);
            if (!RT_SUCCESS(rc))
            {
                LogFlowFunc(("vboxStInit failed, rc %d\n"));
                /* ignore the St Init failure. this can happen for < XP win that do not support WTS API
                 * in that case the session is treated as active connected to the physical console
                 * (i.e. fallback to the old behavior that was before introduction of VBoxSt) */
                Assert(vboxStIsActiveConsole());
            }

            rc = vboxDtInit();
            if (!RT_SUCCESS(rc))
            {
                LogFlowFunc(("vboxDtInit failed, rc %d\n"));
                /* ignore the Dt Init failure. this can happen for < XP win that do not support WTS API
                 * in that case the session is treated as active connected to the physical console
                 * (i.e. fallback to the old behavior that was before introduction of VBoxSt) */
                Assert(vboxDtIsInputDesktop());
            }

            rc = VBoxAcquireGuestCaps(VMMDEV_GUEST_SUPPORTS_SEAMLESS | VMMDEV_GUEST_SUPPORTS_GRAPHICS, 0, true);
            if (!RT_SUCCESS(rc))
            {
                LogFlowFunc(("VBoxAcquireGuestCaps cfg failed rc %d, ignoring..\n", rc));
            }

            rc = vboxTraySetupSeamless();
            if (RT_SUCCESS(rc))
            {
                Log(("Init successful\n"));
                rc = vboxTrayServiceMain();
                if (RT_SUCCESS(rc))
                    hlpReportStatus(VBoxGuestFacilityStatus_Terminating);
                vboxTrayShutdownSeamless();
            }

            /* it should be safe to call vboxDtTerm even if vboxStInit above failed */
            vboxDtTerm();

            /* it should be safe to call vboxStTerm even if vboxStInit above failed */
            vboxStTerm();

            VBoxCapsTerm();

            vboxTrayDestroyToolWindow();
        }
        if (RT_SUCCESS(rc))
            hlpReportStatus(VBoxGuestFacilityStatus_Terminated);
    }

    if (RT_FAILURE(rc))
    {
        LogRel(("Error while starting, rc=%Rrc\n", rc));
        hlpReportStatus(VBoxGuestFacilityStatus_Failed);
    }
    LogRel(("Ended\n"));
    vboxTrayCloseBaseDriver();

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL)
    {
        CloseHandle(hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    VbglR3Term();

    vboxTrayLogDestroy();

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
            Log(("Tool window created\n"));

            int rc = vboxTrayRegisterGlobalMessages(&s_vboxGlobalMessageTable[0]);
            if (RT_FAILURE(rc))
                Log(("Error registering global window messages, rc=%Rrc\n", rc));
            return 0;
        }

        case WM_CLOSE:
            return 0;

        case WM_DESTROY:
            Log(("Tool window destroyed\n"));
            KillTimer(ghwndToolWindow, TIMERID_VBOXTRAY_CHECK_HOSTVERSION);
            return 0;

        case WM_TIMER:
            if (VBoxCapsCheckTimer(wParam))
                return 0;
            if (vboxDtCheckTimer(wParam))
                return 0;
            if (vboxStCheckTimer(wParam))
                return 0;

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

        case WM_VBOX_SEAMLESS_ENABLE:
            VBoxCapsEntryFuncStateSet(VBOXCAPS_ENTRY_IDX_SEAMLESS, VBOXCAPS_ENTRY_FUNCSTATE_STARTED);
            return 0;

        case WM_VBOX_SEAMLESS_DISABLE:
            VBoxCapsEntryFuncStateSet(VBOXCAPS_ENTRY_IDX_SEAMLESS, VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);
            return 0;

        case WM_DISPLAYCHANGE:
        case WM_VBOX_SEAMLESS_UPDATE:
            if (VBoxCapsEntryIsEnabled(VBOXCAPS_ENTRY_IDX_SEAMLESS))
                VBoxSeamlessCheckWindows(true);
            return 0;

        case WM_VBOX_GRAPHICS_SUPPORTED:
            VBoxGrapicsSetSupported(TRUE);
            return 0;

        case WM_VBOX_GRAPHICS_UNSUPPORTED:
            VBoxGrapicsSetSupported(FALSE);
            return 0;

        case WM_WTSSESSION_CHANGE:
        {
            BOOL fOldAllowedState = VBoxConsoleIsAllowed();
            if (vboxStHandleEvent(wParam, lParam))
            {
                if (!VBoxConsoleIsAllowed() != !fOldAllowedState)
                    VBoxConsoleEnable(!fOldAllowedState);
            }
            return 0;
        }
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

/* St (session [state] tracking) functionality API impl */

typedef struct VBOXST
{
    HWND hWTSAPIWnd;
    RTLDRMOD hLdrModWTSAPI32;
    BOOL fIsConsole;
    WTS_CONNECTSTATE_CLASS enmConnectState;
    UINT_PTR idDelayedInitTimer;
    BOOL (WINAPI * pfnWTSRegisterSessionNotification)(HWND hWnd, DWORD dwFlags);
    BOOL (WINAPI * pfnWTSUnRegisterSessionNotification)(HWND hWnd);
    BOOL (WINAPI * pfnWTSQuerySessionInformationA)(HANDLE hServer, DWORD SessionId, WTS_INFO_CLASS WTSInfoClass, LPTSTR *ppBuffer, DWORD *pBytesReturned);
} VBOXST;

static VBOXST gVBoxSt;

static int vboxStCheckState()
{
    int rc = VINF_SUCCESS;
    WTS_CONNECTSTATE_CLASS *penmConnectState = NULL;
    USHORT *pProtocolType = NULL;
    DWORD cbBuf = 0;
    if (gVBoxSt.pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSConnectState,
                                               (LPTSTR *)&penmConnectState, &cbBuf))
    {
        if (gVBoxSt.pfnWTSQuerySessionInformationA(WTS_CURRENT_SERVER_HANDLE, WTS_CURRENT_SESSION, WTSClientProtocolType,
                                                   (LPTSTR *)&pProtocolType, &cbBuf))
        {
            gVBoxSt.fIsConsole = (*pProtocolType == 0);
            gVBoxSt.enmConnectState = *penmConnectState;
            return VINF_SUCCESS;
        }

        DWORD dwErr = GetLastError();
        LogFlowFunc(("WTSQuerySessionInformationA WTSClientProtocolType failed, error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }
    else
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("WTSQuerySessionInformationA WTSConnectState failed, error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }

    /* failure branch, set to "console-active" state */
    gVBoxSt.fIsConsole = TRUE;
    gVBoxSt.enmConnectState = WTSActive;

    return rc;
}

static int vboxStInit(HWND hWnd)
{
    RT_ZERO(gVBoxSt);
    int rc = RTLdrLoadSystem("WTSAPI32.DLL", false /*fNoUnload*/, &gVBoxSt.hLdrModWTSAPI32);
    if (RT_SUCCESS(rc))
    {
        rc = RTLdrGetSymbol(gVBoxSt.hLdrModWTSAPI32, "WTSRegisterSessionNotification",
                            (void **)&gVBoxSt.pfnWTSRegisterSessionNotification);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(gVBoxSt.hLdrModWTSAPI32, "WTSUnRegisterSessionNotification",
                                (void **)&gVBoxSt.pfnWTSUnRegisterSessionNotification);
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrGetSymbol(gVBoxSt.hLdrModWTSAPI32, "WTSQuerySessionInformationA",
                                    (void **)&gVBoxSt.pfnWTSQuerySessionInformationA);
                if (RT_FAILURE(rc))
                    LogFlowFunc(("WTSQuerySessionInformationA not found\n"));
            }
            else
                LogFlowFunc(("WTSUnRegisterSessionNotification not found\n"));
        }
        else
            LogFlowFunc(("WTSRegisterSessionNotification not found\n"));
        if (RT_SUCCESS(rc))
        {
            gVBoxSt.hWTSAPIWnd = hWnd;
            if (gVBoxSt.pfnWTSRegisterSessionNotification(gVBoxSt.hWTSAPIWnd, NOTIFY_FOR_THIS_SESSION))
                vboxStCheckState();
            else
            {
                DWORD dwErr = GetLastError();
                LogFlowFunc(("WTSRegisterSessionNotification failed, error = %08X\n", dwErr));
                if (dwErr == RPC_S_INVALID_BINDING)
                {
                    gVBoxSt.idDelayedInitTimer = SetTimer(gVBoxSt.hWTSAPIWnd, TIMERID_VBOXTRAY_ST_DELAYED_INIT_TIMER,
                                                          2000, (TIMERPROC)NULL);
                    gVBoxSt.fIsConsole = TRUE;
                    gVBoxSt.enmConnectState = WTSActive;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = RTErrConvertFromWin32(dwErr);
            }

            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
        }

        RTLdrClose(gVBoxSt.hLdrModWTSAPI32);
    }
    else
        LogFlowFunc(("WTSAPI32 load failed, rc = %Rrc\n", rc));

    RT_ZERO(gVBoxSt);
    gVBoxSt.fIsConsole = TRUE;
    gVBoxSt.enmConnectState = WTSActive;
    return rc;
}

static void vboxStTerm(void)
{
    if (!gVBoxSt.hWTSAPIWnd)
    {
        LogFlowFunc(("vboxStTerm called for non-initialized St\n"));
        return;
    }

    if (gVBoxSt.idDelayedInitTimer)
    {
        /* notification is not registered, just kill timer */
        KillTimer(gVBoxSt.hWTSAPIWnd, gVBoxSt.idDelayedInitTimer);
        gVBoxSt.idDelayedInitTimer = 0;
    }
    else
    {
        if (!gVBoxSt.pfnWTSUnRegisterSessionNotification(gVBoxSt.hWTSAPIWnd))
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("WTSAPI32 load failed, error = %08X\n", dwErr));
        }
    }

    RTLdrClose(gVBoxSt.hLdrModWTSAPI32);
    RT_ZERO(gVBoxSt);
}

#define VBOXST_DBG_MAKECASE(_val) case _val: return #_val;

static const char* vboxStDbgGetString(DWORD val)
{
    switch (val)
    {
        VBOXST_DBG_MAKECASE(WTS_CONSOLE_CONNECT);
        VBOXST_DBG_MAKECASE(WTS_CONSOLE_DISCONNECT);
        VBOXST_DBG_MAKECASE(WTS_REMOTE_CONNECT);
        VBOXST_DBG_MAKECASE(WTS_REMOTE_DISCONNECT);
        VBOXST_DBG_MAKECASE(WTS_SESSION_LOGON);
        VBOXST_DBG_MAKECASE(WTS_SESSION_LOGOFF);
        VBOXST_DBG_MAKECASE(WTS_SESSION_LOCK);
        VBOXST_DBG_MAKECASE(WTS_SESSION_UNLOCK);
        VBOXST_DBG_MAKECASE(WTS_SESSION_REMOTE_CONTROL);
        default:
            LogFlowFunc(("invalid WTS state %d\n", val));
            return "Unknown";
    }
}

static BOOL vboxStCheckTimer(WPARAM wEvent)
{
    if (wEvent != gVBoxSt.idDelayedInitTimer)
        return FALSE;

    if (gVBoxSt.pfnWTSRegisterSessionNotification(gVBoxSt.hWTSAPIWnd, NOTIFY_FOR_THIS_SESSION))
    {
        KillTimer(gVBoxSt.hWTSAPIWnd, gVBoxSt.idDelayedInitTimer);
        gVBoxSt.idDelayedInitTimer = 0;
        vboxStCheckState();
    }
    else
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("timer WTSRegisterSessionNotification failed, error = %08X\n", dwErr));
        Assert(gVBoxSt.fIsConsole == TRUE);
        Assert(gVBoxSt.enmConnectState == WTSActive);
    }

    return TRUE;
}


static BOOL vboxStHandleEvent(WPARAM wEvent, LPARAM SessionID)
{
    LogFlowFunc(("WTS Event: %s\n", vboxStDbgGetString(wEvent)));
    BOOL fOldIsActiveConsole = vboxStIsActiveConsole();

    vboxStCheckState();

    return !vboxStIsActiveConsole() != !fOldIsActiveConsole;
}

static BOOL vboxStIsActiveConsole()
{
    return (gVBoxSt.enmConnectState == WTSActive && gVBoxSt.fIsConsole);
}

/*
 * Dt (desktop [state] tracking) functionality API impl
 *
 * !!!NOTE: this API is NOT thread-safe!!!
 * */

typedef struct VBOXDT
{
    HANDLE hNotifyEvent;
    BOOL fIsInputDesktop;
    UINT_PTR idTimer;
    RTLDRMOD hLdrModHook;
    BOOL (* pfnVBoxHookInstallActiveDesktopTracker)(HMODULE hDll);
    BOOL (* pfnVBoxHookRemoveActiveDesktopTracker)();
    HDESK (WINAPI * pfnGetThreadDesktop)(DWORD dwThreadId);
    HDESK (WINAPI * pfnOpenInputDesktop)(DWORD dwFlags, BOOL fInherit, ACCESS_MASK dwDesiredAccess);
    BOOL (WINAPI * pfnCloseDesktop)(HDESK hDesktop);
} VBOXDT;

static VBOXDT gVBoxDt;

static BOOL vboxDtCalculateIsInputDesktop()
{
    BOOL fIsInputDt = FALSE;
    HDESK hInput = gVBoxDt.pfnOpenInputDesktop(0, FALSE, DESKTOP_CREATEWINDOW);
    if (hInput)
    {
//        DWORD dwThreadId = GetCurrentThreadId();
//        HDESK hThreadDt = gVBoxDt.pfnGetThreadDesktop(dwThreadId);
//        if (hThreadDt)
//        {
            fIsInputDt = TRUE;
//        }
//        else
//        {
//            DWORD dwErr = GetLastError();
//            LogFlowFunc(("pfnGetThreadDesktop for Seamless failed, last error = %08X\n", dwErr));
//        }

        gVBoxDt.pfnCloseDesktop(hInput);
    }
    else
    {
        DWORD dwErr = GetLastError();
//        LogFlowFunc(("pfnOpenInputDesktop for Seamless failed, last error = %08X\n", dwErr));
    }
    return fIsInputDt;
}

static BOOL vboxDtCheckTimer(WPARAM wParam)
{
    if (wParam != gVBoxDt.idTimer)
        return FALSE;

    VBoxTrayCheckDt();

    return TRUE;
}

static int vboxDtInit()
{
    int rc = VINF_SUCCESS;
    OSVERSIONINFO info;
    gMajorVersion = 5; /* Default to Windows XP. */
    info.dwOSVersionInfoSize = sizeof(info);
    if (GetVersionEx(&info))
    {
        LogRel(("Windows version %ld.%ld\n", info.dwMajorVersion, info.dwMinorVersion));
        gMajorVersion = info.dwMajorVersion;
    }

    RT_ZERO(gVBoxDt);

    gVBoxDt.hNotifyEvent = CreateEvent(NULL, FALSE, FALSE, VBOXHOOK_GLOBAL_DT_EVENT_NAME);
    if (gVBoxDt.hNotifyEvent != NULL)
    {
        /* Load the hook dll and resolve the necessary entry points. */
        rc = RTLdrLoadAppPriv(VBOXHOOK_DLL_NAME, &gVBoxDt.hLdrModHook);
        if (RT_SUCCESS(rc))
        {
            rc = RTLdrGetSymbol(gVBoxDt.hLdrModHook, "VBoxHookInstallActiveDesktopTracker",
                                (void **)&gVBoxDt.pfnVBoxHookInstallActiveDesktopTracker);
            if (RT_SUCCESS(rc))
            {
                rc = RTLdrGetSymbol(gVBoxDt.hLdrModHook, "VBoxHookRemoveActiveDesktopTracker",
                                    (void **)&gVBoxDt.pfnVBoxHookRemoveActiveDesktopTracker);
                if (RT_FAILURE(rc))
                    LogFlowFunc(("VBoxHookRemoveActiveDesktopTracker not found\n"));
            }
            else
                LogFlowFunc(("VBoxHookInstallActiveDesktopTracker not found\n"));
            if (RT_SUCCESS(rc))
            {
                /* Try get the system APIs we need. */
                *(void **)&gVBoxDt.pfnGetThreadDesktop = RTLdrGetSystemSymbol("user32.dll", "GetThreadDesktop");
                if (!gVBoxDt.pfnGetThreadDesktop)
                {
                    LogFlowFunc(("GetThreadDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                *(void **)&gVBoxDt.pfnOpenInputDesktop = RTLdrGetSystemSymbol("user32.dll", "OpenInputDesktop");
                if (!gVBoxDt.pfnOpenInputDesktop)
                {
                    LogFlowFunc(("OpenInputDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                *(void **)&gVBoxDt.pfnCloseDesktop = RTLdrGetSystemSymbol("user32.dll", "CloseDesktop");
                if (!gVBoxDt.pfnCloseDesktop)
                {
                    LogFlowFunc(("CloseDesktop not found\n"));
                    rc = VERR_NOT_SUPPORTED;
                }

                if (RT_SUCCESS(rc))
                {
                    BOOL fRc = FALSE;
                    /* For Vista and up we need to change the integrity of the security descriptor, too. */
                    if (gMajorVersion >= 6)
                    {
                        HMODULE hModHook = (HMODULE)RTLdrGetNativeHandle(gVBoxDt.hLdrModHook);
                        Assert((uintptr_t)hModHook != ~(uintptr_t)0);
                        fRc = gVBoxDt.pfnVBoxHookInstallActiveDesktopTracker(hModHook);
                        if (!fRc)
                        {
                            DWORD dwErr = GetLastError();
                            LogFlowFunc(("pfnVBoxHookInstallActiveDesktopTracker failed, last error = %08X\n", dwErr));
                        }
                    }

                    if (!fRc)
                    {
                        gVBoxDt.idTimer = SetTimer(ghwndToolWindow, TIMERID_VBOXTRAY_DT_TIMER, 500, (TIMERPROC)NULL);
                        if (!gVBoxDt.idTimer)
                        {
                            DWORD dwErr = GetLastError();
                            LogFlowFunc(("SetTimer error %08X\n", dwErr));
                            rc = RTErrConvertFromWin32(dwErr);
                        }
                    }

                    if (RT_SUCCESS(rc))
                    {
                        gVBoxDt.fIsInputDesktop = vboxDtCalculateIsInputDesktop();
                        return VINF_SUCCESS;
                    }
                }
            }

            RTLdrClose(gVBoxDt.hLdrModHook);
        }
        else
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
            rc = RTErrConvertFromWin32(dwErr);
        }

        CloseHandle(gVBoxDt.hNotifyEvent);
    }
    else
    {
        DWORD dwErr = GetLastError();
        LogFlowFunc(("CreateEvent for Seamless failed, last error = %08X\n", dwErr));
        rc = RTErrConvertFromWin32(dwErr);
    }


    RT_ZERO(gVBoxDt);
    gVBoxDt.fIsInputDesktop = TRUE;

    return rc;
}

static void vboxDtTerm()
{
    if (!gVBoxDt.hLdrModHook)
        return;

    gVBoxDt.pfnVBoxHookRemoveActiveDesktopTracker();

    RTLdrClose(gVBoxDt.hLdrModHook);
    CloseHandle(gVBoxDt.hNotifyEvent);

    RT_ZERO(gVBoxDt);
}
/* @returns true on "IsInputDesktop" state change */
static BOOL vboxDtHandleEvent()
{
    BOOL fIsInputDesktop = gVBoxDt.fIsInputDesktop;
    gVBoxDt.fIsInputDesktop = vboxDtCalculateIsInputDesktop();
    return !fIsInputDesktop != !gVBoxDt.fIsInputDesktop;
}

static HANDLE vboxDtGetNotifyEvent()
{
    return gVBoxDt.hNotifyEvent;
}

/* @returns true iff the application (VBoxTray) desktop is input */
static BOOL vboxDtIsInputDesktop()
{
    return gVBoxDt.fIsInputDesktop;
}


/* we need to perform Acquire/Release using the file handled we use for rewuesting events from VBoxGuest
 * otherwise Acquisition mechanism will treat us as different client and will not propagate necessary requests
 * */
static int VBoxAcquireGuestCaps(uint32_t fOr, uint32_t fNot, bool fCfg)
{
    DWORD cbReturned = 0;
    VBoxGuestCapsAquire Info;
    Log(("VBoxAcquireGuestCaps or(0x%x), not(0x%x), cfx(%d)\n", fOr, fNot, fCfg));
    Info.enmFlags = fCfg ? VBOXGUESTCAPSACQUIRE_FLAGS_CONFIG_ACQUIRE_MODE : VBOXGUESTCAPSACQUIRE_FLAGS_NONE;
    Info.rc = VERR_NOT_IMPLEMENTED;
    Info.u32OrMask = fOr;
    Info.u32NotMask = fNot;
    if (!DeviceIoControl(ghVBoxDriver, VBOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE, &Info, sizeof(Info), &Info, sizeof(Info), &cbReturned, NULL))
    {
        DWORD LastErr = GetLastError();
        LogFlowFunc(("DeviceIoControl VBOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE failed LastErr %d\n", LastErr));
        return RTErrConvertFromWin32(LastErr);
    }

    int rc = Info.rc;
    if (!RT_SUCCESS(rc))
    {
        LogFlowFunc(("VBOXGUEST_IOCTL_GUEST_CAPS_ACQUIRE failed rc %d\n", rc));
        return rc;
    }

    return rc;
}

typedef enum VBOXCAPS_ENTRY_ACSTATE
{
    /* the given cap is released */
    VBOXCAPS_ENTRY_ACSTATE_RELEASED = 0,
    /* the given cap acquisition is in progress */
    VBOXCAPS_ENTRY_ACSTATE_ACQUIRING,
    /* the given cap is acquired */
    VBOXCAPS_ENTRY_ACSTATE_ACQUIRED
} VBOXCAPS_ENTRY_ACSTATE;


struct VBOXCAPS_ENTRY;
struct VBOXCAPS;

typedef DECLCALLBACKPTR(void, PFNVBOXCAPS_ENTRY_ON_ENABLE)(struct VBOXCAPS *pConsole, struct VBOXCAPS_ENTRY *pCap, BOOL fEnabled);

typedef struct VBOXCAPS_ENTRY
{
    uint32_t fCap;
    uint32_t iCap;
    VBOXCAPS_ENTRY_FUNCSTATE enmFuncState;
    VBOXCAPS_ENTRY_ACSTATE enmAcState;
    PFNVBOXCAPS_ENTRY_ON_ENABLE pfnOnEnable;
} VBOXCAPS_ENTRY;


typedef struct VBOXCAPS
{
    UINT_PTR idTimer;
    VBOXCAPS_ENTRY aCaps[VBOXCAPS_ENTRY_IDX_COUNT];
} VBOXCAPS;

static VBOXCAPS gVBoxCaps;

static DECLCALLBACK(void) vboxCapsOnEnableSeamles(struct VBOXCAPS *pConsole, struct VBOXCAPS_ENTRY *pCap, BOOL fEnabled)
{
    if (fEnabled)
    {
        Log(("vboxCapsOnEnableSeamles: ENABLED\n"));
        Assert(pCap->enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED);
        Assert(pCap->enmFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED);
        VBoxSeamlessEnable();
    }
    else
    {
        Log(("vboxCapsOnEnableSeamles: DISABLED\n"));
        Assert(pCap->enmAcState != VBOXCAPS_ENTRY_ACSTATE_ACQUIRED || pCap->enmFuncState != VBOXCAPS_ENTRY_FUNCSTATE_STARTED);
        VBoxSeamlessDisable();
    }
}

static void vboxCapsEntryAcStateSet(VBOXCAPS_ENTRY *pCap, VBOXCAPS_ENTRY_ACSTATE enmAcState)
{
    VBOXCAPS *pConsole = &gVBoxCaps;

    Log(("vboxCapsEntryAcStateSet: new state enmAcState(%d); pCap: fCap(%d), iCap(%d), enmFuncState(%d), enmAcState(%d)\n",
            enmAcState, pCap->fCap, pCap->iCap, pCap->enmFuncState, pCap->enmAcState));

    if (pCap->enmAcState == enmAcState)
        return;

    VBOXCAPS_ENTRY_ACSTATE enmOldAcState = pCap->enmAcState;
    pCap->enmAcState = enmAcState;

    if (enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED)
    {
        if (pCap->enmFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED)
        {
            if (pCap->pfnOnEnable)
                pCap->pfnOnEnable(pConsole, pCap, TRUE);
        }
    }
    else if (enmOldAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED && pCap->enmFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        if (pCap->pfnOnEnable)
            pCap->pfnOnEnable(pConsole, pCap, FALSE);
    }
}

static void vboxCapsEntryFuncStateSet(VBOXCAPS_ENTRY *pCap, VBOXCAPS_ENTRY_FUNCSTATE enmFuncState)
{
    VBOXCAPS *pConsole = &gVBoxCaps;

    Log(("vboxCapsEntryFuncStateSet: new state enmAcState(%d); pCap: fCap(%d), iCap(%d), enmFuncState(%d), enmAcState(%d)\n",
            enmFuncState, pCap->fCap, pCap->iCap, pCap->enmFuncState, pCap->enmAcState));

    if (pCap->enmFuncState == enmFuncState)
        return;

    VBOXCAPS_ENTRY_FUNCSTATE enmOldFuncState = pCap->enmFuncState;

    pCap->enmFuncState = enmFuncState;

    if (enmFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        Assert(enmOldFuncState == VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);
        if (pCap->enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED)
        {
            if (pCap->pfnOnEnable)
                pCap->pfnOnEnable(pConsole, pCap, TRUE);
        }
    }
    else if (pCap->enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED && enmOldFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED)
    {
        if (pCap->pfnOnEnable)
            pCap->pfnOnEnable(pConsole, pCap, FALSE);
    }
}

static void VBoxCapsEntryFuncStateSet(uint32_t iCup, VBOXCAPS_ENTRY_FUNCSTATE enmFuncState)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    VBOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCup];
    vboxCapsEntryFuncStateSet(pCap, enmFuncState);
}

static int VBoxCapsInit()
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    memset(pConsole, 0, sizeof (*pConsole));
    pConsole->aCaps[VBOXCAPS_ENTRY_IDX_SEAMLESS].fCap = VMMDEV_GUEST_SUPPORTS_SEAMLESS;
    pConsole->aCaps[VBOXCAPS_ENTRY_IDX_SEAMLESS].iCap = VBOXCAPS_ENTRY_IDX_SEAMLESS;
    pConsole->aCaps[VBOXCAPS_ENTRY_IDX_SEAMLESS].pfnOnEnable = vboxCapsOnEnableSeamles;
    pConsole->aCaps[VBOXCAPS_ENTRY_IDX_GRAPHICS].fCap = VMMDEV_GUEST_SUPPORTS_GRAPHICS;
    pConsole->aCaps[VBOXCAPS_ENTRY_IDX_GRAPHICS].iCap = VBOXCAPS_ENTRY_IDX_GRAPHICS;
    return VINF_SUCCESS;
}

static int VBoxCapsReleaseAll()
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    Log(("VBoxCapsReleaseAll\n"));
    int rc = VBoxAcquireGuestCaps(0, VMMDEV_GUEST_SUPPORTS_SEAMLESS | VMMDEV_GUEST_SUPPORTS_GRAPHICS, false);
    if (!RT_SUCCESS(rc))
    {
        LogFlowFunc(("vboxCapsEntryReleaseAll VBoxAcquireGuestCaps failed rc %d\n", rc));
        return rc;
    }

    if (pConsole->idTimer)
    {
        Log(("killing console timer\n"));
        KillTimer(ghwndToolWindow, pConsole->idTimer);
        pConsole->idTimer = 0;
    }

    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        vboxCapsEntryAcStateSet(&pConsole->aCaps[i], VBOXCAPS_ENTRY_ACSTATE_RELEASED);
    }

    return rc;
}

static void VBoxCapsTerm()
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    VBoxCapsReleaseAll();
    memset(pConsole, 0, sizeof (*pConsole));
}

static BOOL VBoxCapsEntryIsAcquired(uint32_t iCap)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    return pConsole->aCaps[iCap].enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED;
}

static BOOL VBoxCapsEntryIsEnabled(uint32_t iCap)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    return pConsole->aCaps[iCap].enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED
            && pConsole->aCaps[iCap].enmFuncState == VBOXCAPS_ENTRY_FUNCSTATE_STARTED;
}

static BOOL VBoxCapsCheckTimer(WPARAM wParam)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    if (wParam != pConsole->idTimer)
        return FALSE;

    uint32_t u32AcquiredCaps = 0;
    BOOL fNeedNewTimer = FALSE;

    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        VBOXCAPS_ENTRY *pCap = &pConsole->aCaps[i];
        if (pCap->enmAcState != VBOXCAPS_ENTRY_ACSTATE_ACQUIRING)
            continue;

        int rc = VBoxAcquireGuestCaps(pCap->fCap, 0, false);
        if (RT_SUCCESS(rc))
        {
            vboxCapsEntryAcStateSet(&pConsole->aCaps[i], VBOXCAPS_ENTRY_ACSTATE_ACQUIRED);
            u32AcquiredCaps |= pCap->fCap;
        }
        else
        {
            Assert(rc == VERR_RESOURCE_BUSY);
            fNeedNewTimer = TRUE;
        }
    }

    if (!fNeedNewTimer)
    {
        KillTimer(ghwndToolWindow, pConsole->idTimer);
        /* cleanup timer data */
        pConsole->idTimer = 0;
    }

    return TRUE;
}

static int VBoxCapsEntryRelease(uint32_t iCap)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    VBOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCap];
    if (pCap->enmAcState == VBOXCAPS_ENTRY_ACSTATE_RELEASED)
    {
        LogFlowFunc(("invalid cap[%d] state[%d] on release\n", iCap, pCap->enmAcState));
        return VERR_INVALID_STATE;
    }

    if (pCap->enmAcState == VBOXCAPS_ENTRY_ACSTATE_ACQUIRED)
    {
        int rc = VBoxAcquireGuestCaps(0, pCap->fCap, false);
        AssertRC(rc);
    }

    vboxCapsEntryAcStateSet(pCap, VBOXCAPS_ENTRY_ACSTATE_RELEASED);

    return VINF_SUCCESS;
}

static int VBoxCapsEntryAcquire(uint32_t iCap)
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    Assert(VBoxConsoleIsAllowed());
    VBOXCAPS_ENTRY *pCap = &pConsole->aCaps[iCap];
    Log(("VBoxCapsEntryAcquire %d\n", iCap));
    if (pCap->enmAcState != VBOXCAPS_ENTRY_ACSTATE_RELEASED)
    {
        LogFlowFunc(("invalid cap[%d] state[%d] on acquire\n", iCap, pCap->enmAcState));
        return VERR_INVALID_STATE;
    }

    vboxCapsEntryAcStateSet(pCap, VBOXCAPS_ENTRY_ACSTATE_ACQUIRING);
    int rc = VBoxAcquireGuestCaps(pCap->fCap, 0, false);
    if (RT_SUCCESS(rc))
    {
        vboxCapsEntryAcStateSet(pCap, VBOXCAPS_ENTRY_ACSTATE_ACQUIRED);
        return VINF_SUCCESS;
    }

    if (rc != VERR_RESOURCE_BUSY)
    {
        LogFlowFunc(("vboxCapsEntryReleaseAll VBoxAcquireGuestCaps failed rc %d\n", rc));
        return rc;
    }

    LogFlowFunc(("iCap %d is busy!\n", iCap));

    /* the cap was busy, most likely it is still used by other VBoxTray instance running in another session,
     * queue the retry timer */
    if (!pConsole->idTimer)
    {
        pConsole->idTimer = SetTimer(ghwndToolWindow, TIMERID_VBOXTRAY_CAPS_TIMER, 100, (TIMERPROC)NULL);
        if (!pConsole->idTimer)
        {
            DWORD dwErr = GetLastError();
            LogFlowFunc(("SetTimer error %08X\n", dwErr));
            return RTErrConvertFromWin32(dwErr);
        }
    }

    return rc;
}

static int VBoxCapsAcquireAllSupported()
{
    VBOXCAPS *pConsole = &gVBoxCaps;
    Log(("VBoxCapsAcquireAllSupported\n"));
    for (int i = 0; i < RT_ELEMENTS(pConsole->aCaps); ++i)
    {
        if (pConsole->aCaps[i].enmFuncState >= VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED)
        {
            Log(("VBoxCapsAcquireAllSupported acquiring cap %d, state %d\n", i, pConsole->aCaps[i].enmFuncState));
            VBoxCapsEntryAcquire(i);
        }
        else
        {
            LogFlowFunc(("VBoxCapsAcquireAllSupported: WARN: cap %d not supported, state %d\n", i, pConsole->aCaps[i].enmFuncState));
        }
    }
    return VINF_SUCCESS;
}

static BOOL VBoxConsoleIsAllowed()
{
    return vboxDtIsInputDesktop() && vboxStIsActiveConsole();
}

static void VBoxConsoleEnable(BOOL fEnable)
{
    if (fEnable)
        VBoxCapsAcquireAllSupported();
    else
        VBoxCapsReleaseAll();
}

static void VBoxConsoleCapSetSupported(uint32_t iCap, BOOL fSupported)
{
    if (fSupported)
    {
        VBoxCapsEntryFuncStateSet(iCap, VBOXCAPS_ENTRY_FUNCSTATE_SUPPORTED);

        if (VBoxConsoleIsAllowed())
            VBoxCapsEntryAcquire(iCap);
    }
    else
    {
        VBoxCapsEntryFuncStateSet(iCap, VBOXCAPS_ENTRY_FUNCSTATE_UNSUPPORTED);

        VBoxCapsEntryRelease(iCap);
    }
}

void VBoxSeamlessSetSupported(BOOL fSupported)
{
    VBoxConsoleCapSetSupported(VBOXCAPS_ENTRY_IDX_SEAMLESS, fSupported);
}

static void VBoxGrapicsSetSupported(BOOL fSupported)
{
    VBoxConsoleCapSetSupported(VBOXCAPS_ENTRY_IDX_GRAPHICS, fSupported);
}
