/* $Id$ */
/** @file
 * VBoxService - The Guest Additions Helper Service.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#include "VBoxService.h"
#ifdef VBOX_WITH_GUEST_PROPS
    #include "VBoxVMInfo.h"
#endif
#include "VBoxTimeSync.h"
#include "resource.h"

#define VBOXSERVICE_NAME _T("VBoxService")
#define VBOXSERVICE_FRIENDLY_NAME _T("VBoxService")

/* Global variables. */
HANDLE                gvboxDriver;
HANDLE                gStopSem;

SERVICE_STATUS        gvboxServiceStatus;
DWORD                 gvboxServiceStatusCode;
SERVICE_STATUS_HANDLE gvboxServiceStatusHandle;

VBOXSERVICEENV        gServiceEnv;

/* Prototypes. */
void writeLog (char* a_pszText, ...);

/* The service table. */
static VBOXSERVICEINFO vboxServiceTable[] =
{
#ifdef VBOX_WITH_GUEST_PROPS
    {
        "VMInfo",
        vboxVMInfoInit,
        vboxVMInfoThread,
        vboxVMInfoDestroy,
    },
#endif
    {
        "TimeSync",
        vboxTimeSyncInit,
        vboxTimeSyncThread,
        vboxTimeSyncDestroy,
    },
    {
        NULL
    }
};

/**
 * @todo Format code style.
 * @todo Add full unicode support.
 * @todo Add event log capabilities / check return values.
 */

DWORD AddAceToObjectsSecurityDescriptor (LPTSTR pszObjName,
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
        writeLog("VBoxService: GetNamedSecurityInfo: Error %u\n", dwRes);
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
        writeLog("VBoxService: SetEntriesInAcl: Error %u\n", dwRes);
        goto Cleanup;
    }

    /* Attach the new ACL as the object's DACL. */
    dwRes = SetNamedSecurityInfo(pszObjName, ObjectType,
          DACL_SECURITY_INFORMATION,
          NULL, NULL, pNewDACL, NULL);
    if (ERROR_SUCCESS != dwRes)  {
        writeLog("VBoxService: SetNamedSecurityInfo: Error %u\n", dwRes);
        goto Cleanup;
    }

Cleanup:

    if(pSD != NULL)
        LocalFree((HLOCAL) pSD);
    if(pNewDACL != NULL)
        LocalFree((HLOCAL) pNewDACL);

    return dwRes;
}

static void SetStatus (DWORD a_dwStatus)
{
    if (NULL == gvboxServiceStatusHandle)    /* Program could be in testing mode, so no service environment available. */
        return;

    gvboxServiceStatusCode  = a_dwStatus;

    SERVICE_STATUS ss;
    ss.dwServiceType              = SERVICE_WIN32_OWN_PROCESS;
    ss.dwCurrentState             = gvboxServiceStatusCode;
    ss.dwControlsAccepted	      = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    ss.dwWin32ExitCode            = NOERROR;
    ss.dwServiceSpecificExitCode  = NOERROR;
    ss.dwCheckPoint               = 0;
    ss.dwWaitHint                 = 3000;

    SetServiceStatus (gvboxServiceStatusHandle, &ss);
}

static int vboxStartServices (VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    Log(("VBoxService: Starting services ...\n"));

    Assert(pEnv);
    pEnv->hStopEvent = CreateEvent (NULL, TRUE, FALSE, NULL);

    if (!pEnv->hStopEvent)
    {
        /* Could not create event. */
        return VERR_NOT_SUPPORTED;
    }

    while (pTable->pszName)
    {
        Log(("VBoxService: Starting %s ...\n", pTable->pszName));

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
            writeLog("VBoxService: Failed to initialize! Error = %Rrc.\n", rc);
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
                Log(("VBoxService: Failed to start the thread: %s\n", pTable->pszName));

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

    Log(("VBoxService: All threads started.\n"));
    return VINF_SUCCESS;
}

static void vboxStopServices (BOOL bAlert, VBOXSERVICEENV *pEnv, VBOXSERVICEINFO *pTable)
{
    /* Signal to all threads. */
    Assert(pEnv);
    Assert(pTable);
    if (bAlert && (NULL != pEnv->hStopEvent))
    {
        Log(("VBoxService: Setting stop event ...\n"));
        SetEvent(pEnv->hStopEvent);
    }

    while (pTable->pszName)
    {
        if (pTable->fStarted)
        {
            if (pTable->pfnThread)
            {
                Log(("VBoxService: Waiting for thread %s to close ...\n", pTable->pszName));

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
    SetStatus (SERVICE_STOPPED);
}

void vboxServiceStart()
{
    gStopSem = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (gStopSem == NULL)
    {
        writeLog("VBoxService: CreateEvent for stopping failed: rc = %d\n", GetLastError());
        return;
    }

    /* Create a well-known SID for the "Builtin Users" group. */
    PSID pBuiltinUsersSID = NULL;
    SID_IDENTIFIER_AUTHORITY SIDAuthWorld = SECURITY_LOCAL_SID_AUTHORITY;

    if(!AllocateAndInitializeSid(&SIDAuthWorld, 1,
                                 SECURITY_LOCAL_RID,
                                 0, 0, 0, 0, 0, 0, 0,
                                 &pBuiltinUsersSID))
    {
        writeLog("VBoxService: AllocateAndInitializeSid: Error %u\n", GetLastError());
    }

    AddAceToObjectsSecurityDescriptor (TEXT("\\\\.\\VBoxMiniRdrDN"),
                                       SE_FILE_OBJECT,
                                       (LPTSTR)pBuiltinUsersSID,
                                       TRUSTEE_IS_SID,
                                       FILE_GENERIC_READ | FILE_GENERIC_WRITE,
                                       SET_ACCESS,
                                       NO_INHERITANCE);

    /* Start service threads. */
    int rc = vboxStartServices(&gServiceEnv, vboxServiceTable);

    /** @todo Add error handling. */

    SetStatus (SERVICE_RUNNING);
}

DWORD WINAPI ServiceCtrlHandler (DWORD dwControl,
                                 DWORD dwEventType,
                                 LPVOID lpEventData,
                                 LPVOID lpContext)
{
    DWORD rc = NO_ERROR;

    switch (dwControl)
    {

    case SERVICE_CONTROL_INTERROGATE:
        SetStatus(gvboxServiceStatusCode);
        break;

    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        {
            SetStatus(SERVICE_STOP_PENDING);

            vboxStopServices(TRUE, &gServiceEnv, vboxServiceTable);

            /* Don't close VBox driver here - the service might be
             * started again. */
            CloseHandle(gStopSem);

            SetStatus(SERVICE_STOPPED);
        }
        break;

    case SERVICE_CONTROL_SESSIONCHANGE:     /* Only Win XP and up. */

        switch (dwEventType)
        {
        case WTS_SESSION_LOGON:
            Log(("VBoxService: A user has logged on to the session.\n"));
            break;
        case WTS_SESSION_LOGOFF:
            Log(("VBoxService: A user has logged off from the session.\n"));
            break;
        default:
            break;
        }
        break;

    default:

        Log(("VBoxService: Service control function not implemented: %ld\n", dwControl));
        rc = ERROR_CALL_NOT_IMPLEMENTED;
        break;
    }
    return rc;
}

void WINAPI ServiceMain (DWORD argc, LPTSTR *argv)
{
    Log(("VBoxService: Registering service control handler ...\n"));
    gvboxServiceStatusHandle = RegisterServiceCtrlHandlerEx (VBOXSERVICE_NAME, ServiceCtrlHandler, NULL);

    if (NULL == gvboxServiceStatusHandle)
    {
        DWORD dwErr = GetLastError();

        switch (dwErr)
        {
        case ERROR_INVALID_NAME:
            writeLog("VBoxService: Invalid service name!\n");
            break;
        case ERROR_SERVICE_DOES_NOT_EXIST:
            writeLog("VBoxService: Service does not exist!\n");
            break;
        default:
            writeLog("VBoxService: Could not register service control handle! Error: %ld\n", dwErr);
            break;
        }
    }
    else
    {
        vboxServiceStart();

        while (1) {
            Sleep (100);       /** @todo */
        }
    }
}

int Install ()
{
    SC_HANDLE hService, hSCManager;
    TCHAR imagePath[MAX_PATH] = { 0 };

    GetModuleFileName(NULL,imagePath,MAX_PATH);
    writeLog("VBoxService: Installing service ...\n");

    hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    if (NULL == hSCManager) {
        writeLog("VBoxService: Could not open SCM! Error: %d\n", GetLastError());
        return 1;
    }

    hService = ::CreateService (hSCManager,
                                VBOXSERVICE_NAME, VBOXSERVICE_FRIENDLY_NAME,
                                SERVICE_ALL_ACCESS,
                                SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS,
                                SERVICE_DEMAND_START,SERVICE_ERROR_NORMAL,
                                imagePath, NULL, NULL, NULL, NULL, NULL);

    if (NULL == hService) {
        writeLog("VBoxService: Could not create service! Error: %d\n", GetLastError());
        CloseServiceHandle(hSCManager);
        return 1;
    }
    else
    {
        writeLog("VBoxService: Service successfully installed!\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 0;
}

int Uninstall ()
{
    SC_HANDLE hService, hSCManager;
    hSCManager = OpenSCManager(NULL,NULL,SC_MANAGER_ALL_ACCESS);

    writeLog("VBoxService: Uninstalling service ...\n");

    if (NULL == hSCManager) {
        writeLog("VBoxService: Could not open SCM! Error: %d\n", GetLastError());
        return 1;
    }

    hService = OpenService (hSCManager, VBOXSERVICE_NAME, SERVICE_ALL_ACCESS );
    if (NULL == hService) {
        writeLog("VBoxService: Could not open service! Error: %d\n", GetLastError());
        CloseServiceHandle (hSCManager);
        return 1;
    }

    if (FALSE == DeleteService (hService))
    {
        writeLog("VBoxService: Could not remove service! Error: %d\n", GetLastError());
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

        writeLog("VBoxService: Service successfully uninstalled!\n");
    }

    CloseServiceHandle(hService);
    CloseServiceHandle(hSCManager);

    return 0;
}

void writeLog (char* a_pszText, ...)
{
    char szBuffer[1024] = { 0 };

    Assert(a_pszText);

    va_list va;
    va_start(va, a_pszText);

    RTStrPrintfV(szBuffer, sizeof(szBuffer), a_pszText, va);
    printf(szBuffer);
    LogRel((szBuffer));

    va_end(va);
}

void printHelp (_TCHAR* a_pszName)
{
    Assert(a_pszName);
    _tprintf(_T("VBoxService - Guest Additions Helper Service for Windows XP/2K/Vista\n"));
    _tprintf(_T("Version: %d.%d.%d.%d\n\n"), VBOX_VERSION_MAJOR, VBOX_VERSION_MINOR, VBOX_VERSION_BUILD, VBOX_SVN_REV);
    _tprintf(_T("Syntax:\n"));
        _tprintf(_T("\tTo install: %ws /i\n"), a_pszName);
        _tprintf(_T("\tTo uninstall: %ws /u\n"), a_pszName);
        _tprintf(_T("\tTo execute from command line: %ws /t\n"), a_pszName);
        _tprintf(_T("\tThis help text: %ws /h\n"), a_pszName);
}

int _tmain(int argc, _TCHAR* argv[])
{
    /* Do not use a global namespace ("Global\\") for mutex name here, will blow up NT4 compatibility! */
    HANDLE hMutexAppRunning = CreateMutex (NULL, FALSE, VBOXSERVICE_NAME);
    if (   (hMutexAppRunning != NULL)
        && (GetLastError() == ERROR_ALREADY_EXISTS))
    {
      /* Close the mutex for this application instance. */
      CloseHandle(hMutexAppRunning);
      hMutexAppRunning = NULL;
    }

    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        writeLog("VBoxService: Failed to initialise the VirtualBox runtime! Error: %d\n", rc);
        return rc;
    }

    rc = VbglR3Init();
    if (RT_FAILURE(rc))
    {
        writeLog("VBoxService: Failed to contact the VirtualBox host! Program maybe not running in a VM? Error: %d\n", rc);
        return rc;
    }

    LogRel(("VBoxService: Started.\n"));

    static SERVICE_TABLE_ENTRY const s_serviceTable[]=
    {
        {VBOXSERVICE_NAME, ServiceMain},
        {NULL,NULL}
    };

    if (argc > 1)
    {
        if (0 == _tcsicmp(argv[1], _T("/i")))
            Install();
        else if (0 == _tcsicmp(argv[1], _T("/u")))
            Uninstall();
        else if (0 == _tcsicmp(argv[1], _T("/t")))
        {
            vboxServiceStart();

            while (1) {
                Sleep( 100 );       /** @todo */
            }
        }
        else if (0 == _tcsicmp(argv[1], _T("/h")))
            printHelp(argv[0]);

        else {
            _tprintf(_T("Invalid command line argument: %ws\n"), argv[1]);
            _tprintf(_T("Type %s /h to display help.\n"), argv[0]);
        }
    }
    else    /* Normal service. */
    {
        if (FALSE == StartServiceCtrlDispatcher (s_serviceTable))
            printHelp(argv[0]);     /* Application called from command line, print some help. */
    }

    /* Release instance mutex. */
    if (hMutexAppRunning != NULL) {
        CloseHandle (hMutexAppRunning);
        hMutexAppRunning = NULL;
    }

    writeLog("VBoxService: Ended.\n");

    VbglR3Term();
    return 0;
}

