/** @file
 *
 * VBox host drivers - USB drivers - Filter & driver installation
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

int usblibOsCreateService(void);

int __cdecl main(int argc, char **argv)
{
    LPSTR  lpszFilePart;
    TCHAR  szFullPath[MAX_PATH];
    TCHAR  szCurDir[MAX_PATH];
    DWORD  len;

    printf("USB installation\n");

    usblibOsCreateService();

    BOOL rc;

    len = GetFullPathName(".\\VBoxUSB.inf", sizeof(szFullPath), szFullPath, &lpszFilePart);
    Assert(len);

    if (GetCurrentDirectory(sizeof(szCurDir), szCurDir) == 0)
    {
        printf("GetCurrentDirectory failed with %x\n", GetLastError());
        return 1;
    }

    /* Copy INF file to Windows\INF, so Windows will automatically install it when our USB device is detected */
    rc = SetupCopyOEMInf(szFullPath, NULL, SPOST_PATH, 0, NULL, 0, NULL, NULL);
    if (rc == FALSE)
    {
        printf("SetupCopyOEMInf failed with rc=%x\n", GetLastError());
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
 * Creates the service.
 *
 * @returns 0 on success.
 * @returns -1 on failure.
 */
int usblibOsCreateService(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    SC_HANDLE   hSMgrCreate = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    DWORD LastError = GetLastError(); NOREF(LastError);
    AssertMsg(hSMgrCreate, ("OpenSCManager(,,create) failed rc=%d\n", LastError));
    if (hSMgrCreate)
    {
        char szDriver[RTPATH_MAX];
        int rc = RTPathProgram(szDriver, sizeof(szDriver) - sizeof("\\VBoxUSBMon.sys"));
        if (VBOX_SUCCESS(rc))
        {
            strcat(szDriver, "\\VBoxUSBMon.sys");
            SC_HANDLE hService = CreateService(hSMgrCreate,
                                               SERVICE_NAME,
                                               "VBox USB Monitor Driver",
                                               SERVICE_QUERY_STATUS,
                                               SERVICE_KERNEL_DRIVER,
                                               SERVICE_DEMAND_START,
                                               SERVICE_ERROR_NORMAL,
                                               szDriver,
                                               NULL, NULL, NULL, NULL, NULL);
            DWORD LastError = GetLastError(); NOREF(LastError);
            AssertMsg(hService, ("CreateService failed! LastError=%Rwa szDriver=%s\n", LastError, szDriver));
            CloseServiceHandle(hService);
            CloseServiceHandle(hSMgrCreate);
            return hService ? 0 : -1;
        }
        CloseServiceHandle(hSMgrCreate);
        return rc;
    }
    return -1;
}
