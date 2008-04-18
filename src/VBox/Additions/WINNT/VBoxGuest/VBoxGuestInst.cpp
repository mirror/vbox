/** @file
 *
 * Small tool to (un)install the VBoxGuest device driver
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <VBox/VBoxGuest.h>


//#define TESTMODE



int installDriver(void)
{
    /*
     * Assume it didn't exist, so we'll create the service.
     */
    SC_HANDLE   hSMgrCreate = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    if (!hSMgrCreate)
    {
        printf("OpenSCManager(,,create) failed rc=%d\n", GetLastError());
        return -1;
    }

    char szDriver[MAX_PATH];
    GetSystemDirectory(szDriver, sizeof(szDriver));
    strcat(szDriver, "\\drivers\\VBoxGuestNT.sys");

    SC_HANDLE hService = CreateService(hSMgrCreate,
                                       VBOXGUEST_SERVICE_NAME,
                                       "VBoxGuest Support Driver",
                                       SERVICE_QUERY_STATUS,
                                       SERVICE_KERNEL_DRIVER,
                                       SERVICE_BOOT_START,
                                       SERVICE_ERROR_NORMAL,
                                       szDriver,
                                       "System",
                                       NULL, NULL, NULL, NULL);
    if (!hService)
    {
        printf("CreateService failed! lasterr=%d\n", GetLastError());
    } else
    {
        CloseServiceHandle(hService);
    }
    CloseServiceHandle(hSMgrCreate);
    return hService ? 0 : -1;
}

int uninstallDriver(void)
{
    int rc = -1;
    SC_HANDLE   hSMgr = OpenSCManager(NULL, NULL, SERVICE_CHANGE_CONFIG);
    if (!hSMgr)
    {
        printf("OpenSCManager(,,delete) failed rc=%d\n", GetLastError());
        return -1;
    }
    SC_HANDLE hService = OpenService(hSMgr, VBOXGUEST_SERVICE_NAME, DELETE);
    if (hService)
    {
        /*
         * Delete the service.
         */
        if (DeleteService(hService))
            rc = 0;
        else
            printf("DeleteService failed lasterr=%d\n", GetLastError());
        CloseServiceHandle(hService);
    }
    else if (GetLastError() == ERROR_SERVICE_DOES_NOT_EXIST)
        rc = 0;
    else
        printf("OpenService failed lasterr=%d\n", GetLastError());
    CloseServiceHandle(hSMgr);
    return rc;
}


HANDLE openDriver(void)
{
    HANDLE hDevice;

    hDevice = CreateFile(VBOXGUEST_DEVICE_NAME, // Win2k+: VBOXGUEST_DEVICE_NAME_GLOBAL
                         GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE,
                         NULL,
                         OPEN_EXISTING,
                         FILE_ATTRIBUTE_NORMAL,
                         NULL);
    if (hDevice == INVALID_HANDLE_VALUE)
    {
        printf("CreateFile did not work. GetLastError() 0x%x\n", GetLastError());
    }
    return hDevice;
}

int closeDriver(HANDLE hDevice)
{
    CloseHandle(hDevice);
    return 0;
}

#ifdef TESTMODE
typedef struct TESTFOO
{
    int values[16];
} TESTFOO, *PTESTFOO;

int performTest(void)
{
    int rc = 0;

    HANDLE hDevice = openDriver();

    if (hDevice != INVALID_HANDLE_VALUE)
    {
        DWORD cbReturned;

        closeDriver(hDevice);
    } else
    {
        printf("openDriver failed!\n");
    }


    return rc;
}
#endif

void displayHelpAndExit(char *programName)
{
    printf("error, syntax: %s [install|uninstall]\n", programName);
    exit(1);
}

int main(int argc, char **argv)
{
    bool installMode;
#ifdef TESTMODE
    bool testMode = false;
#endif

    if (argc != 2)
    {
        displayHelpAndExit(argv[0]);
    }
    if (strcmp(argv[1], "install") == 0)
    {
        installMode = true;
    } else
    if (strcmp(argv[1], "uninstall") == 0)
    {
        installMode = false;
    } else
#ifdef TESTMODE
    if (strcmp(argv[1], "test") == 0)
    {
        testMode = true;
    } else
#endif
    {
        displayHelpAndExit(argv[0]);
    }

    int rc;

#ifdef TESTMODE
    if (testMode)
    {
        rc = performTest();
    } else
#endif
    if (installMode)
    {
        rc = installDriver();
    } else
    {
        rc = uninstallDriver();
    }

    if (rc == 0)
    {
        printf("operation completed successfully!\n");
    } else
    {
        printf("error: operation failed with status code %d\n", rc);
    }

    return rc;
}
