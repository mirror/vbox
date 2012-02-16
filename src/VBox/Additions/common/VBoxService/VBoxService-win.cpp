/* $Id$ */
/** @file
 * VBoxService - Guest Additions Service Skeleton, Windows Specific Parts.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/err.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"

#include <Windows.h>
#include <process.h>
#include <aclapi.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void WINAPI vboxServiceWinMain(DWORD argc, LPTSTR *argv);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static DWORD          g_dwWinServiceLastStatus = 0;
SERVICE_STATUS_HANDLE g_hWinServiceStatus = NULL;
/** The semaphore for the dummy Windows service. */
static RTSEMEVENT     g_WindowsEvent = NIL_RTSEMEVENT;

static SERVICE_TABLE_ENTRY const g_aServiceTable[] =
{
    { VBOXSERVICE_NAME, vboxServiceWinMain },
    { NULL,             NULL}
};


/**
 * @todo Format code style.
 * @todo Add full unicode support.
 * @todo Add event log capabilities / check return values.
 */
static DWORD vboxServiceWinAddAceToObjectsSecurityDescriptor(LPTSTR pszObjName,
                                                             SE_OBJECT_TYPE ObjectType,
                                                             LPTSTR pszTrustee,
                                                             TRUSTEE_FORM TrusteeForm,
                                                             DWORD dwAccessRights,
                                                             ACCESS_MODE AccessMode,
                                                             DWORD dwInheritance)
{
    DWORD dwRes = 0;
    PACL pOldDACL = NULL, pNewDACL = NULL;
    PSECURITY_DESCRIPTOR pSD = NULL;
    EXPLICIT_ACCESS ea;

    if (NULL == pszObjName)
        return ERROR_INVALID_PARAMETER;

    /* Get a pointer to the existing DACL. */
    dwRes = GetNamedSecurityInfo(pszObjName, ObjectType,
                                 DACL_SECURITY_INFORMATION,
                                 NULL, NULL, &pOldDACL, NULL, &pSD);
    if (ERROR_SUCCESS != dwRes)
    {
        if (dwRes == ERROR_FILE_NOT_FOUND)
            VBoxServiceError("AddAceToObjectsSecurityDescriptor: Object not found/installed: %s\n", pszObjName);
        else
            VBoxServiceError("AddAceToObjectsSecurityDescriptor: GetNamedSecurityInfo: Error %u\n", dwRes);
        goto l_Cleanup;
    }

    /* Initialize an EXPLICIT_ACCESS structure for the new ACE. */
    ZeroMemory(&ea, sizeof(EXPLICIT_ACCESS));
    ea.grfAccessPermissions = dwAccessRights;
    ea.grfAccessMode = AccessMode;
    ea.grfInheritance= dwInheritance;
    ea.Trustee.TrusteeForm = TrusteeForm;
    ea.Trustee.ptstrName = pszTrustee;

    /* Create a new ACL that merges the new ACE into the existing DACL. */
    dwRes = SetEntriesInAcl(1, &ea, pOldDACL, &pNewDACL);
    if (ERROR_SUCCESS != dwRes)
    {
        VBoxServiceError("AddAceToObjectsSecurityDescriptor: SetEntriesInAcl: Error %u\n", dwRes);
        goto l_Cleanup;
    }

    /* Attach the new ACL as the object's DACL. */
    dwRes = SetNamedSecurityInfo(pszObjName, ObjectType,
                                 DACL_SECURITY_INFORMATION,
                                 NULL, NULL, pNewDACL, NULL);
    if (ERROR_SUCCESS != dwRes)
    {
        VBoxServiceError("AddAceToObjectsSecurityDescriptor: SetNamedSecurityInfo: Error %u\n", dwRes);
        goto l_Cleanup;
    }

    /** @todo get rid of that spaghetti jump ... */
l_Cleanup:

    if(pSD != NULL)
        LocalFree((HLOCAL) pSD);
    if(pNewDACL != NULL)
        LocalFree((HLOCAL) pNewDACL);

    return dwRes;
}


/** Reports our current status to the SCM. */
static BOOL vboxServiceWinSetStatus(DWORD dwStatus, DWORD dwCheckPoint)
{
    if (g_hWinServiceStatus == NULL) /* Program could be in testing mode, so no service environment available. */
        return FALSE;

    VBoxServiceVerbose(2, "Setting service status to: %ld\n", dwStatus);
    g_dwWinServiceLastStatus = dwStatus;

    SERVICE_STATUS ss;
    ss.dwServiceType              = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState             = dwStatus;
    ss.dwControlsAccepted         = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
#ifndef TARGET_NT4
    ss.dwControlsAccepted        |= SERVICE_ACCEPT_SESSIONCHANGE;
#endif
    ss.dwWin32ExitCode            = NO_ERROR;
    ss.dwServiceSpecificExitCode  = 0; /* Not used */
    ss.dwCheckPoint               = dwCheckPoint;
    ss.dwWaitHint                 = 3000;

    return SetServiceStatus(g_hWinServiceStatus, &ss);
}


/**
 * Reports SERVICE_STOP_PENDING to SCM.
 *
 * @param   uCheckPoint         Some number.
 */
void VBoxServiceWinSetStopPendingStatus(uint32_t uCheckPoint)
{
    vboxServiceWinSetStatus(SERVICE_STOP_PENDING, uCheckPoint);
}


static RTEXITCODE vboxServiceWinSetDesc(SC_HANDLE hService)
{
#ifndef TARGET_NT4
    /* On W2K+ there's ChangeServiceConfig2() which lets us set some fields
       like a longer service description. */
    /** @todo On Vista+ SERVICE_DESCRIPTION also supports localized strings! */
    SERVICE_DESCRIPTION desc;
    desc.lpDescription = VBOXSERVICE_DESCRIPTION;
    if (!ChangeServiceConfig2(hService,
                              SERVICE_CONFIG_DESCRIPTION, /* Service info level */
                              &desc))
    {
        VBoxServiceError("Cannot set the service description! Error: %ld\n", GetLastError());
        return RTEXITCODE_FAILURE;
    }
#endif
    return RTEXITCODE_SUCCESS;
}


/**
 * Installs the service.
 */
RTEXITCODE VBoxServiceWinInstall(void)
{
    VBoxServiceVerbose(1, "Installing service ...\n");

    TCHAR imagePath[MAX_PATH] = { 0 };
    GetModuleFileName(NULL, imagePath, sizeof(imagePath));

    SC_HANDLE hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        VBoxServiceError("Could not open SCM! Error: %ld\n", GetLastError());
        return RTEXITCODE_FAILURE;
    }

    RTEXITCODE rc       = RTEXITCODE_SUCCESS;
    SC_HANDLE  hService = CreateService(hSCManager,
                                        VBOXSERVICE_NAME, VBOXSERVICE_FRIENDLY_NAME,
                                        SERVICE_ALL_ACCESS,
                                        SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
                                        imagePath, NULL, NULL, NULL, NULL, NULL);
    if (hService != NULL)
        VBoxServiceVerbose(0, "Service successfully installed!\n");
    else
    {
        DWORD dwErr = GetLastError();
        switch (dwErr)
        {
            case ERROR_SERVICE_EXISTS:
                VBoxServiceVerbose(1, "Service already exists, just updating the service config.\n");
                hService = OpenService(hSCManager, VBOXSERVICE_NAME, SERVICE_ALL_ACCESS);
                if (hService)
                {
                    if (ChangeServiceConfig (hService,
                                             SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                             SERVICE_DEMAND_START,
                                             SERVICE_ERROR_NORMAL,
                                             imagePath,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             NULL,
                                             VBOXSERVICE_FRIENDLY_NAME))
                        VBoxServiceVerbose(1, "The service config has been successfully updated.\n");
                    else
                        rc = VBoxServiceError("Could not change service config! Error: %ld\n", GetLastError());
                }
                else
                    rc = VBoxServiceError("Could not open service! Error: %ld\n", GetLastError());
                break;

            default:
                rc = VBoxServiceError("Could not create service! Error: %ld\n", dwErr);
                break;
        }
    }

    if (rc == RTEXITCODE_SUCCESS)
        rc = vboxServiceWinSetDesc(hService);

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);
    return rc;
}

/**
 * Uninstalls the service.
 */
RTEXITCODE VBoxServiceWinUninstall(void)
{
    VBoxServiceVerbose(1, "Uninstalling service ...\n");

    SC_HANDLE hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);
    if (hSCManager == NULL)
    {
        VBoxServiceError("Could not open SCM! Error: %d\n", GetLastError());
        return RTEXITCODE_FAILURE;
    }

    RTEXITCODE rcExit;
    SC_HANDLE  hService = OpenService(hSCManager, VBOXSERVICE_NAME, SERVICE_ALL_ACCESS );
    if (hService != NULL)
    {
        if (DeleteService(hService))
        {
            /*
             * ???
             */
            HKEY hKey = NULL;
            if (RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                             "SYSTEM\\CurrentControlSet\\Services\\EventLog\\System",
                             0,
                             KEY_ALL_ACCESS,
                             &hKey)
                == ERROR_SUCCESS)
            {
                RegDeleteKey(hKey, VBOXSERVICE_NAME);
                RegCloseKey(hKey);
            }

            VBoxServiceVerbose(0, "Service successfully uninstalled!\n");
            rcExit = RTEXITCODE_SUCCESS;
        }
        else
            rcExit = VBoxServiceError("Could not remove service! Error: %d\n", GetLastError());
        CloseServiceHandle(hService);
    }
    else
        rcExit = VBoxServiceError("Could not open service! Error: %d\n", GetLastError());
    CloseServiceHandle(hSCManager);

    return rcExit;
}


static int vboxServiceWinStart(void)
{
    int rc = VINF_SUCCESS;

#ifndef TARGET_NT4
    /* Create a well-known SID for the "Builtin Users" group. */
    PSID pBuiltinUsersSID = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_LOCAL_SID_AUTHORITY;

    if (!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                  SECURITY_LOCAL_RID,
                                  0, 0, 0, 0, 0, 0, 0,
                                  &pBuiltinUsersSID))
    {
        rc = RTErrConvertFromWin32(GetLastError());
    }
    else
    {
        DWORD dwRes = vboxServiceWinAddAceToObjectsSecurityDescriptor(TEXT("\\\\.\\VBoxMiniRdrDN"),
                                                                      SE_FILE_OBJECT,
                                                                      (LPTSTR)pBuiltinUsersSID,
                                                                      TRUSTEE_IS_SID,
                                                                      FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                                                                      SET_ACCESS,
                                                                      NO_INHERITANCE);
        if (dwRes != ERROR_SUCCESS)
        {
            if (dwRes == ERROR_FILE_NOT_FOUND)
            {
                /* If we don't find our "VBoxMiniRdrDN" (for Shared Folders) object above,
                   don't report an error; it just might be not installed. Otherwise this
                  would cause the SCM to hang on starting up the service. */
                rc = VINF_SUCCESS;
            }
            else
                rc = RTErrConvertFromWin32(dwRes);
        }
    }
#endif

    if (RT_SUCCESS(rc))
    {
        rc = VBoxServiceStartServices();
        if (RT_SUCCESS(rc))
        {
            vboxServiceWinSetStatus(SERVICE_RUNNING, 0);
            VBoxServiceMainWait();
        }
        else
        {
            vboxServiceWinSetStatus(SERVICE_STOPPED, 0);
#if 0 /** @todo r=bird: Enable this if SERVICE_CONTROL_STOP isn't triggered automatically */
            VBoxServiceStopServices();
#endif
        }
    }
    else
        vboxServiceWinSetStatus(SERVICE_STOPPED, 0);

    if (RT_FAILURE(rc))
        VBoxServiceError("Service failed to start with rc=%Rrc!\n", rc);

    return rc;
}


/**
 * Call StartServiceCtrlDispatcher.
 *
 * The main() thread invokes this when not started in foreground mode.  It
 * won't return till the service is being shutdown (unless start up fails).
 *
 * @returns RTEXITCODE_SUCCESS on normal return after service shutdown.
 *          Something else on failure, error will have been reported.
 */
RTEXITCODE VBoxServiceWinEnterCtrlDispatcher(void)
{
    if (!StartServiceCtrlDispatcher(&g_aServiceTable[0]))
        return VBoxServiceError("StartServiceCtrlDispatcher: %u. Please start %s with option -f (foreground)!\n",
                                GetLastError(), g_pszProgName);
    return RTEXITCODE_SUCCESS;
}


#ifndef TARGET_NT4
static const char* vboxServiceWTSStateToString(DWORD dwEvent)
{
    switch (dwEvent)
    {
        case WTS_CONSOLE_CONNECT:
            return "A session was connected to the console terminal";

        case WTS_CONSOLE_DISCONNECT:
            return "A session was disconnected from the console terminal";

        case WTS_REMOTE_CONNECT:
            return "A session connected to the remote terminal";

        case WTS_REMOTE_DISCONNECT:
            return "A session was disconnected from the remote terminal";

        case WTS_SESSION_LOGON:
            return "A user has logged on to a session";

        case WTS_SESSION_LOGOFF:
            return "A user has logged off the session";

        case WTS_SESSION_LOCK:
            return "A session has been locked";

        case WTS_SESSION_UNLOCK:
            return "A session has been unlocked";

        case WTS_SESSION_REMOTE_CONTROL:
            return "A session has changed its remote controlled status";
#if 0
        case WTS_SESSION_CREATE:
            return "A session has been created";

        case WTS_SESSION_TERMINATE:
            return "The session has been terminated";
#endif
        default:
            break;
    }

    return "Uknonwn state";
}
#endif /* !TARGET_NT4 */


#ifdef TARGET_NT4
static VOID WINAPI vboxServiceWinCtrlHandler(DWORD dwControl)
#else
static DWORD WINAPI vboxServiceWinCtrlHandler(DWORD dwControl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
#endif
{
    DWORD rcRet = NO_ERROR;

#ifdef TARGET_NT4
    VBoxServiceVerbose(2, "Control handler: Control=%#x\n", dwControl);
#else
    VBoxServiceVerbose(2, "Control handler: Control=%#x, EventType=%#x\n", dwControl, dwEventType);
#endif

    switch (dwControl)
    {
        case SERVICE_CONTROL_INTERROGATE:
            vboxServiceWinSetStatus(g_dwWinServiceLastStatus, 0);
            break;

        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
        {
            vboxServiceWinSetStatus(SERVICE_STOP_PENDING, 0);

            int rc2 = VBoxServiceStopServices();
            if (RT_FAILURE(rc2))
                rcRet = ERROR_GEN_FAILURE;
            else
            {
                rc2 = VBoxServiceReportStatus(VBoxGuestFacilityStatus_Terminated);
                AssertRC(rc2);
            }

            vboxServiceWinSetStatus(SERVICE_STOPPED, 0);
            break;
        }

# ifndef TARGET_NT4
        case SERVICE_CONTROL_SESSIONCHANGE: /* Only Windows 2000 and up. */
        {
            AssertPtr(lpEventData);
            PWTSSESSION_NOTIFICATION pNotify = (PWTSSESSION_NOTIFICATION)lpEventData;
            Assert(pNotify->cbSize == sizeof(WTSSESSION_NOTIFICATION));

            VBoxServiceVerbose(1, "Control handler: %s (Session=%ld, Event=%#x)\n",
                               vboxServiceWTSStateToString(dwEventType),
                               pNotify->dwSessionId, dwEventType);

            /* Handle all events, regardless of dwEventType. */
            int rc2 = VBoxServiceVMInfoSignal();
            AssertRC(rc2);
            break;
        }
# endif /* !TARGET_NT4 */

        default:
            VBoxServiceVerbose(1, "Control handler: Function not implemented: %#x\n", dwControl);
            rcRet = ERROR_CALL_NOT_IMPLEMENTED;
            break;
    }

#ifndef TARGET_NT4
    return rcRet;
#endif
}


static void WINAPI vboxServiceWinMain(DWORD argc, LPTSTR *argv)
{
    VBoxServiceVerbose(2, "Registering service control handler ...\n");
#ifdef TARGET_NT4
    g_hWinServiceStatus = RegisterServiceCtrlHandler(VBOXSERVICE_NAME, vboxServiceWinCtrlHandler);
#else
    g_hWinServiceStatus = RegisterServiceCtrlHandlerEx(VBOXSERVICE_NAME, vboxServiceWinCtrlHandler, NULL);
#endif
    if (g_hWinServiceStatus != NULL)
    {
        VBoxServiceVerbose(2, "Service control handler registered.\n");
        vboxServiceWinStart();
    }
    else
    {
        DWORD dwErr = GetLastError();
        switch (dwErr)
        {
            case ERROR_INVALID_NAME:
                VBoxServiceError("Invalid service name!\n");
                break;
            case ERROR_SERVICE_DOES_NOT_EXIST:
                VBoxServiceError("Service does not exist!\n");
                break;
            default:
                VBoxServiceError("Could not register service control handle! Error: %ld\n", dwErr);
                break;
        }
    }
}

