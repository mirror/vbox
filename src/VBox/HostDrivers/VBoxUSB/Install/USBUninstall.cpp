/** @file
 *
 * VBox host drivers - USB drivers - Filter & driver uninstallation
 *
 * Installation code
 *
 * Copyright (C) 2006-2007 innotek GmbH
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <VBox/err.h>
#include <stdio.h>


int usblibOsStopService(void);
int usblibOsDeleteService(void);


int __cdecl main(int argc, char **argv)
{
    BOOL   rc;
    TCHAR  szFullPath[MAX_PATH];
    CHAR  *lpszFilePart;
    int    len;

    printf("USB uninstallation\n");

    usblibOsStopService();
    usblibOsDeleteService();

    len = GetFullPathName(".\\VBoxUSB.inf", sizeof(szFullPath), szFullPath, &lpszFilePart);
    Assert(len);

    /* Remove the inf plus all associated files. */
    rc = SetupUninstallOEMInf(szFullPath, SUOI_FORCEDELETE, NULL);
    if (rc == FALSE)
    {
        printf("SetupUninstallOEMInf failed with rc=%x\n", GetLastError());
        return 1;
    }
    return 0;
}

/** The support service name. */
#define SERVICE_NAME    "VBoxUSBMon"
/** Win32 Device name. */
#define DEVICE_NAME     "\\\\.\\VBoxUSBMon"
/** NT Device name. */
#define DEVICE_NAME_NT   L"\\Device\\VBoxUSBMon"
/** Win32 Symlink name. */
#define DEVICE_NAME_DOS  L"\\DosDevices\\VBoxUSBMon"

/**
 * Stops a possibly running service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int usblibOsStopService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_STOP | SERVICE_QUERY_STATUS);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed rc=%d\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, SERVICE_STOP | SERVICE_QUERY_STATUS);
        if (hService)
        {
            /*
             * Stop the service.
             */
            SERVICE_STATUS  Status;
            QueryServiceStatus(hService, &Status);
            if (Status.dwCurrentState == SERVICE_STOPPED)
                rc = 0;
            else if (ControlService(hService, SERVICE_CONTROL_STOP, &Status))
            {
                int iWait = 100;
                while (Status.dwCurrentState == SERVICE_STOP_PENDING && iWait-- > 0)
                {
                    Sleep(100);
                    QueryServiceStatus(hService, &Status);
                }
                if (Status.dwCurrentState == SERVICE_STOPPED)
                    rc = 0;
                else
                   AssertMsgFailed(("Failed to stop service. status=%d\n", Status.dwCurrentState));
            }
            else
            {
                DWORD LastError = GetLastError(); NOREF(LastError);
                AssertMsgFailed(("ControlService failed with LastError=%Rwa. status=%d\n", LastError, Status.dwCurrentState));
            }
            CloseServiceHandle(hService);
        }
        else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            rc = 0;
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return rc;
}


/**
 * Deletes the service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int usblibOsDeleteService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgr, ("OpenSCManager(,,delete) failed rc=%d\n", LastError));
    if (hSMgr)
    {
        SC_HANDLE hService = OpenService(hSMgr, SERVICE_NAME, DELETE);
        if (hService)
        {
            /*
             * Delete the service.
             */
            if (DeleteService(hService))
                rc = 0;
            else
            {
                DWORD LastError = GetLastError(); NOREF(LastError);
                AssertMsgFailed(("DeleteService failed LastError=%Rwa\n", LastError));
            }
            CloseServiceHandle(hService);
        }
        else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
            rc = 0;
        else
        {
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsgFailed(("OpenService failed LastError=%Rwa\n", LastError));
        }
        CloseServiceHandle(hSMgr);
    }
    return rc;
}
