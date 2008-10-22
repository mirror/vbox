/* $Id$ */
/** @file
 * VBoxVMInfoAdditions - Guest Additions information for the host.
 */

/*
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

#include "VBoxService.h"
#include "VBoxUtils.h"
#include "VBoxVMInfo.h"
#include "VBoxVMInfoAdditions.h"

int getAddVersion(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    Assert(a_pCtx);
    if (FALSE == a_pCtx->fFirstRun)     /* Only do this at the initial run. */
        return 0;

    char szInstDir[_MAX_PATH] = {0};
    char szRev[_MAX_PATH] = {0};
    char szVer[_MAX_PATH] = {0};

    HKEY hKey = NULL;
    int rc = 0;
    DWORD dwSize = 0;

    rc = RegOpenKeyExA (HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, &hKey);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        Log(("vboxVMInfoThread: Failed to open registry key! Error: %d\n", GetLastError()));
        return 1;
    }

    /* Installation directory. */
    dwSize = sizeof(szInstDir);
    rc = RegQueryValueExA (hKey, "InstallDir", 0, 0, (BYTE*)(LPCTSTR)szInstDir, &dwSize);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        RegCloseKey (hKey);
        Log(("vboxVMInfoThread: Failed to query registry key! Error: %d\n", GetLastError()));
        return 1;
    }

    /* Flip slashes. */
    for (char* pszTmp = &szInstDir[0]; *pszTmp; ++pszTmp)
        if (*pszTmp == '\\')
            *pszTmp = '/';

    /* Revision. */
    dwSize = sizeof(szRev);
    rc = RegQueryValueExA (hKey, "Revision", 0, 0, (BYTE*)(LPCTSTR)szRev, &dwSize);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        RegCloseKey (hKey);
        Log(("vboxVMInfoThread: Failed to query registry key! Error: %d\n",  GetLastError()));
        return 1;
    }

    /* Version. */
    dwSize = sizeof(szVer);
    rc = RegQueryValueExA (hKey, "Version", 0, 0, (BYTE*)(LPCTSTR)szVer, &dwSize);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        RegCloseKey (hKey);
        LogRel(("vboxVMInfoThread: Failed to query registry key! Error: %Rrc\n",  GetLastError()));
        return 1;
    }

    /* Write information to host. */
    vboxVMInfoWriteProp(a_pCtx, "GuestAdd/InstallDir", szInstDir);
    vboxVMInfoWriteProp(a_pCtx, "GuestAdd/Revision", szRev);
    vboxVMInfoWriteProp(a_pCtx, "GuestAdd/Version", szVer);

    RegCloseKey (hKey);

    return 0;
}

int getComponentVersions(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    Assert(a_pCtx);
    if (FALSE == a_pCtx->fFirstRun)     /* Only do this at the initial run. */
        return 0;

    char szVer[_MAX_PATH] = {0};
    char szPropPath[_MAX_PATH] = {0};
    TCHAR szSysDir[_MAX_PATH] = {0};
    TCHAR szWinDir[_MAX_PATH] = {0};
    TCHAR szDriversDir[_MAX_PATH + 32] = {0};

    GetSystemDirectory(szSysDir, _MAX_PATH);
    GetWindowsDirectory(szWinDir, _MAX_PATH);
    swprintf(szDriversDir, (_MAX_PATH + 32), TEXT("%s\\drivers"), szSysDir);

    /* The file information table. */
    VBOXFILEINFO vboxFileInfoTable[] =
    {
        { szSysDir, TEXT("VBoxControl.exe"), },
        { szSysDir, TEXT("VBoxHook.dll"), },
        { szSysDir, TEXT("VBoxDisp.dll"), },
        { szSysDir, TEXT("VBoxMRXNP.dll"), },
        { szSysDir, TEXT("VBoxService.exe"), },
        { szSysDir, TEXT("VBoxTray.exe"), },

        { szDriversDir, TEXT("VBoxGuest.sys"), },
        { szDriversDir, TEXT("VBoxMouse.sys"), },
        { szDriversDir, TEXT("VBoxSF.sys"), },
        { szDriversDir, TEXT("VBoxVideo.sys"), },

        {
            NULL
        }
    };

    VBOXFILEINFO* pTable = vboxFileInfoTable;
    Assert(pTable);
    while (pTable->pszFileName)
    {
        vboxGetFileVersionString(pTable->pszFilePath, pTable->pszFileName, szVer, sizeof(szVer));
        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestAdd/Components/%ls", pTable->pszFileName);
        vboxVMInfoWriteProp(a_pCtx, szPropPath, szVer);
        pTable++;
    }

    return 0;
}

int vboxVMInfoAdditions(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    Assert(a_pCtx);

    getAddVersion(a_pCtx);
    getComponentVersions(a_pCtx);

    return 0;
}

