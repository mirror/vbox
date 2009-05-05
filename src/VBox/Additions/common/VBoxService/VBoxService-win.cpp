/* $Id$ */
/** @file
 * VBoxService - Guest Additions Service Skeleton, Windows Specific Parts.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <iprt/assert.h>
#include <VBox/VBoxGuest.h>
#include "VBoxServiceInternal.h"

#include <Windows.h>
#include <process.h>
#include <aclapi.h>

DWORD                 g_rcWinService = 0;
SERVICE_STATUS_HANDLE g_hWinServiceStatus = NULL;

void WINAPI VBoxServiceWinMain (DWORD argc, LPTSTR *argv);

static SERVICE_TABLE_ENTRY const g_aServiceTable[]=
{
    {VBOXSERVICE_NAME, VBoxServiceWinMain},
    {NULL,NULL}
};

/**
 * @todo Format code style.
 * @todo Add full unicode support.
 * @todo Add event log capabilities / check return values.
 */
DWORD VBoxServiceWinAddAceToObjectsSecurityDescriptor (LPTSTR pszObjName,
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
    if (ERROR_SUCCESS != dwRes) {
        VBoxServiceError("GetNamedSecurityInfo: Error %u\n", dwRes);
        goto Cleanup;
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
    if (ERROR_SUCCESS != dwRes)  {
        VBoxServiceError("SetEntriesInAcl: Error %u\n", dwRes);
        goto Cleanup;
    }

    /* Attach the new ACL as the object's DACL. */
    dwRes = SetNamedSecurityInfo(pszObjName, ObjectType,
          DACL_SECURITY_INFORMATION,
          NULL, NULL, pNewDACL, NULL);
    if (ERROR_SUCCESS != dwRes)  {
        VBoxServiceError("SetNamedSecurityInfo: Error %u\n", dwRes);
        goto Cleanup;
    }

Cleanup:

    if(pSD != NULL)
        LocalFree((HLOCAL) pSD);
    if(pNewDACL != NULL)
        LocalFree((HLOCAL) pNewDACL);

    return dwRes;
}

BOOL VBoxServiceWinSetStatus (DWORD a_dwStatus)
{
    if (NULL == g_hWinServiceStatus) /* Program could be in testing mode, so no service environment available. */
        return FALSE;

    VBoxServiceVerbose(2, "Setting service status to: %ld\n", a_dwStatus);
    g_rcWinService  = a_dwStatus;

    SERVICE_STATUS ss;
    ss.dwServiceType              = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState             = g_rcWinService;
    ss.dwControlsAccepted	      = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ss.dwWin32ExitCode            = NOERROR;
    ss.dwServiceSpecificExitCode  = NOERROR;
    ss.dwCheckPoint               = 0;
    ss.dwWaitHint                 = 3000;

    return SetServiceStatus (g_hWinServiceStatus, &ss);
}

int VBoxServiceWinInstall ()
{
    SC_HANDLE hService, hSCManager;
    TCHAR imagePath[MAX_PATH] = { 0 };

    GetModuleFileName(NULL,imagePath,MAX_PATH);
    VBoxServiceVerbose(1, "Installing service ...\n");

    hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    if (NULL == hSCManager) {
        VBoxServiceError("Could not open SCM! Error: %d\n", GetLastError());
        return 1;
    }

    hService = ::CreateService (hSCManager,
                                VBOXSERVICE_NAME, VBOXSERVICE_FRIENDLY_NAME,
                                SERVICE_ALL_ACCESS,
                                SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
                                imagePath, NULL, NULL, NULL, NULL, NULL);

    if (NULL == hService) {
        VBoxServiceError("Could not create service! Error: %d\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return 1;
    }
    else
    {
        VBoxServiceVerbose(0, "Service successfully installed!\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 0;
}

int VBoxServiceWinUninstall ()
{
    SC_HANDLE hService, hSCManager;
    hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    VBoxServiceVerbose(1, "Uninstalling service ...\n");

    if (NULL == hSCManager) {
        VBoxServiceError("Could not open SCM! Error: %d\n", GetLastError());
        return 1;
    }

    hService = OpenService (hSCManager, VBOXSERVICE_NAME, SERVICE_ALL_ACCESS );
    if (NULL == hService) {
        VBoxServiceError("Could not open service! Error: %d\n", GetLastError());
        CloseServiceHandle (hSCManager);
        return 1;
    }

    if (FALSE == DeleteService (hService))
    {
        VBoxServiceError("Could not remove service! Error: %d\n", GetLastError());
        CloseServiceHandle (hService);
        CloseServiceHandle (hSCManager);
        return 1;
    }
    else
    {
        HKEY hKey = NULL;
        if (RegOpenKeyEx(HKEY_LOCAL_MACHINE, _T("SYSTEM\\CurrentControlSet\\Services\\EventLog\\System"), 0, KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS) {
            RegDeleteKey(hKey, VBOXSERVICE_NAME);
            RegCloseKey(hKey);
        }

        VBoxServiceVerbose(0, "Service successfully uninstalled!\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 0;
}

int VBoxServiceWinStart()
{
    int rc = VINF_SUCCESS;

    /* Create a well-known SID for the "Builtin Users" group. */
    PSID pBuiltinUsersSID = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_LOCAL_SID_AUTHORITY;

    if(!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                 SECURITY_LOCAL_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 &pBuiltinUsersSID))
    {
        VBoxServiceError("AllocateAndInitializeSid: Error %u\n", GetLastError());
    }
    else
    {  
        DWORD dwRes = VBoxServiceWinAddAceToObjectsSecurityDescriptor (TEXT("\\\\.\\VBoxMiniRdrDN"),
                                                                       SE_FILE_OBJECT,
                                                                       (LPTSTR)pBuiltinUsersSID,
                                                                       TRUSTEE_IS_SID,
                                                                       FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                                                                       SET_ACCESS,
                                                                       NO_INHERITANCE);
        if (dwRes != 0)
        {
            rc = VERR_ACCESS_DENIED; /* Need to add some better code later. */
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Notify SCM *before* we're starting the services, because the last services
           always starts in main thread (which causes the SCM to wait because of the non-responding
           service). */
        VBoxServiceWinSetStatus (SERVICE_RUNNING);

        /*
         * Check that at least one service is enabled.
         */
        unsigned iMain = VBoxServiceGetStartedServices();
        rc = VBoxServiceStartServices(iMain); /* Start all the services. */

        if (RT_FAILURE(rc))
            VBoxServiceWinSetStatus (SERVICE_STOPPED);
    }

    if (RT_FAILURE(rc))
        VBoxServiceError("Service failed to start with rc=%Rrc!\n", rc);

    return rc;
}

DWORD WINAPI VBoxServiceWinCtrlHandler (DWORD dwControl,
                                        DWORD dwEventType,
                                        LPVOID lpEventData,
                                        LPVOID lpContext)
{
    DWORD rc = NO_ERROR;

    VBoxServiceVerbose(2, "Control handler: Control=%ld, EventType=%ld\n", dwControl, dwEventType);
    switch (dwControl)
    {

    case SERVICE_CONTROL_INTERROGATE:
        VBoxServiceWinSetStatus(g_rcWinService);
        break;

    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        {
            VBoxServiceWinSetStatus(SERVICE_STOP_PENDING);

            rc = VBoxServiceStopServices();

            VBoxServiceWinSetStatus(SERVICE_STOPPED);
        }
        break;

    case SERVICE_CONTROL_SESSIONCHANGE:     /* Only Win XP and up. */

        switch (dwEventType)
        {

        /*case WTS_SESSION_LOGON:
            VBoxServiceVerbose(2, "A user has logged on to the session.\n");
            break;

        case WTS_SESSION_LOGOFF:
            VBoxServiceVerbose(2, "A user has logged off from the session.\n");
            break;*/

        default:
            break;
        }
        break;

    default:

        VBoxServiceVerbose(1, "Service control function not implemented: %ld\n", dwControl);
        rc = ERROR_CALL_NOT_IMPLEMENTED;
        break;
    }
    return rc;
}

void WINAPI VBoxServiceWinMain (DWORD argc, LPTSTR *argv)
{
    int rc = VINF_SUCCESS;

    VBoxServiceVerbose(2, "Registering service control handler ...\n");
    g_hWinServiceStatus = RegisterServiceCtrlHandlerEx (VBOXSERVICE_NAME, VBoxServiceWinCtrlHandler, NULL);

    if (NULL == g_hWinServiceStatus)
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
    else 
    {
        VBoxServiceVerbose(2, "Service control handler registered.\n");

        rc = VBoxServiceWinStart();
    }
}

