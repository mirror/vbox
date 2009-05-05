/* $Id$ */
/** @file
 * VBoxVMInfo-win - Virtual machine (guest) information for the host.
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
#include <windows.h>
#include <Ntsecapi.h>
#include <wtsapi32.h>       /* For WTS* calls. */
#include <psapi.h>          /* EnumProcesses. */

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <VBox/VBoxGuest.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Function prototypes for dynamic loading. */
extern fnWTSGetActiveConsoleSessionId g_pfnWTSGetActiveConsoleSessionId;
/** The vminfo interval (millseconds). */
uint32_t g_VMInfoLoggedInUsersCount = 0;


/* Function GetLUIDsFromProcesses() written by Stefan Kuhr. */
DWORD VboxServiceVMInfoWinGetLUIDsFromProcesses(PLUID *ppLuid)
{
    DWORD dwSize, dwSize2, dwIndex ;
    LPDWORD lpdwPIDs ;
    DWORD dwLastError = ERROR_SUCCESS;

    if (!ppLuid)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0L;
    }

    /* Call the PSAPI function EnumProcesses to get all of the
       ProcID's currently in the system.
       NOTE: In the documentation, the third parameter of
       EnumProcesses is named cbNeeded, which implies that you
       can call the function once to find out how much space to
       allocate for a buffer and again to fill the buffer.
       This is not the case. The cbNeeded parameter returns
       the number of PIDs returned, so if your buffer size is
       zero cbNeeded returns zero.
       NOTE: The "HeapAlloc" loop here ensures that we
       actually allocate a buffer large enough for all the
       PIDs in the system. */
    dwSize2 = 256 * sizeof(DWORD);

    lpdwPIDs = NULL;
    do
    {
        if (lpdwPIDs)
        {
            HeapFree(GetProcessHeap(), 0, lpdwPIDs) ;
            dwSize2 *= 2;
        }
        lpdwPIDs = (unsigned long *)HeapAlloc(GetProcessHeap(), 0, dwSize2);
        if (lpdwPIDs == NULL)
            return 0L; // Last error will be that of HeapAlloc

        if (!EnumProcesses( lpdwPIDs, dwSize2, &dwSize))
        {
            DWORD dw = GetLastError();
            HeapFree(GetProcessHeap(), 0, lpdwPIDs);
            SetLastError(dw);
            return 0L;
        }
    }
    while (dwSize == dwSize2);

    /* At this point we have an array of the PIDs at the
       time of the last EnumProcesses invocation. We will
       allocate an array of LUIDs passed back via the out
       param ppLuid of exactly the number of PIDs. We will
       only fill the first n values of this array, with n
       being the number of unique LUIDs found in these PIDs. */

    /* How many ProcIDs did we get? */
    dwSize /= sizeof(DWORD);
    dwSize2 = 0L; /* Our return value of found luids. */

    *ppLuid = (LUID *)LocalAlloc(LPTR, dwSize*sizeof(LUID));
    if (!(*ppLuid))
    {
        dwLastError = GetLastError();
        goto CLEANUP;
    }
    for (dwIndex = 0; dwIndex < dwSize; dwIndex++)
    {
        (*ppLuid)[dwIndex].LowPart =0L;
        (*ppLuid)[dwIndex].HighPart=0;

        /* Open the process (if we can... security does not
           permit every process in the system). */
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION,FALSE, lpdwPIDs[dwIndex]);
        if ( hProcess != NULL )
        {
            HANDLE hAccessToken;
            if (OpenProcessToken(hProcess, TOKEN_QUERY, &hAccessToken))
            {
                TOKEN_STATISTICS ts;
                DWORD dwSize;
                if (GetTokenInformation(hAccessToken, TokenStatistics, &ts, sizeof ts, &dwSize))
                {
                    DWORD dwTmp = 0L;
                    BOOL bFound = FALSE;
                    for (;dwTmp<dwSize2 && !bFound;dwTmp++)
                        bFound = (*ppLuid)[dwTmp].HighPart == ts.AuthenticationId.HighPart &&
                                 (*ppLuid)[dwTmp].LowPart == ts.AuthenticationId.LowPart;

                    if (!bFound)
                        (*ppLuid)[dwSize2++] = ts.AuthenticationId;
                }
                CloseHandle(hAccessToken);
            }

            CloseHandle(hProcess);
        }

        /* We don't really care if OpenProcess or OpenProcessToken fail or succeed, because
           there are quite a number of system processes we cannot open anyway, not even as SYSTEM. */
    }

    CLEANUP:

    if (lpdwPIDs)
        HeapFree(GetProcessHeap(), 0, lpdwPIDs);

    if (ERROR_SUCCESS !=dwLastError)
        SetLastError(dwLastError);

    return dwSize2;
}

BOOL VboxServiceVMInfoWinIsLoggedIn(VBOXSERVICEVMINFOUSER* a_pUserInfo,
                                    PLUID a_pSession,
                                    PLUID a_pLuid,
                                    DWORD a_dwNumOfProcLUIDs)
{
    BOOL bLoggedIn = FALSE;
    BOOL bFoundUser = FALSE;
    PSECURITY_LOGON_SESSION_DATA sessionData = NULL;
    NTSTATUS r = 0;
    WCHAR *usBuffer = NULL;
    int iLength = 0;

    if (!a_pSession)
        return FALSE;

    r = LsaGetLogonSessionData (a_pSession, &sessionData);
    if (r != STATUS_SUCCESS)
    {
        VBoxServiceError("LsaGetLogonSessionData failed %lu\n", LsaNtStatusToWinError(r));

        if (sessionData)
            LsaFreeReturnBuffer(sessionData);

        return FALSE;
    }

    if (!sessionData)
    {
        VBoxServiceError("Invalid logon session data.\n");
        return FALSE;
    }

    VBoxServiceVerbose(3, "vboxVMInfoThread: Users: Session data: Name = %ls, Len = %d, SID = %s, LogonID = %d,%d\n", 
        (sessionData->UserName).Buffer, (sessionData->UserName).Length, (sessionData->Sid != NULL) ? "1" : "0", sessionData->LogonId.HighPart, sessionData->LogonId.LowPart);

    if ((sessionData->UserName.Buffer != NULL) &&
        (sessionData->Sid != NULL) &&
        (sessionData->LogonId.LowPart != 0))
    {
        /* Get the user name. */
        usBuffer = (sessionData->UserName).Buffer;
        iLength = (sessionData->UserName).Length;
        if (iLength > sizeof(a_pUserInfo->szUser) - sizeof(TCHAR))   /* -sizeof(TCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "User name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szUser) - sizeof(TCHAR);
        }
        wcsncpy (a_pUserInfo->szUser, usBuffer, iLength);
        wcscat (a_pUserInfo->szUser, L"");      /* Add terminating null char. */

        /* Get authentication package. */
        usBuffer = (sessionData->AuthenticationPackage).Buffer;
        iLength = (sessionData->AuthenticationPackage).Length;
        if (iLength > sizeof(a_pUserInfo->szAuthenticationPackage) - sizeof(TCHAR))   /* -sizeof(TCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "Authentication pkg name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szAuthenticationPackage) - sizeof(TCHAR);
        }
        wcsncpy (a_pUserInfo->szAuthenticationPackage, usBuffer, iLength);
        wcscat (a_pUserInfo->szAuthenticationPackage, L"");     /* Add terminating null char. */

        /* Get logon domain. */
        usBuffer = (sessionData->LogonDomain).Buffer;
        iLength = (sessionData->LogonDomain).Length;
        if (iLength > sizeof(a_pUserInfo->szLogonDomain) - sizeof(TCHAR))   /* -sizeof(TCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "Logon domain name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szLogonDomain) - sizeof(TCHAR);
        }
        wcsncpy (a_pUserInfo->szLogonDomain, usBuffer, iLength);
        wcscat (a_pUserInfo->szLogonDomain, L"");       /* Add terminating null char. */

        /* Only handle users which can login interactively or logged in remotely over native RDP. */
        if (   (((SECURITY_LOGON_TYPE)sessionData->LogonType == Interactive) 
             || ((SECURITY_LOGON_TYPE)sessionData->LogonType == RemoteInteractive))
             && (sessionData->Sid != NULL))
        {
            TCHAR szOwnerName [_MAX_PATH] = { 0 };
            DWORD dwOwnerNameSize = _MAX_PATH;

            TCHAR szDomainName [_MAX_PATH] = { 0 };
            DWORD dwDomainNameSize = _MAX_PATH;

            SID_NAME_USE ownerType;

            if (LookupAccountSid(NULL,
                                 sessionData->Sid,
                                 szOwnerName,
                                 &dwOwnerNameSize,
                                 szDomainName,
                                 &dwDomainNameSize,
                                 &ownerType))
            {
                VBoxServiceVerbose(3, "Account User=%ls, Session=%ld, LUID=%ld,%ld, AuthPkg=%ls, Domain=%ls\n",
                     a_pUserInfo->szUser, sessionData->Session, sessionData->LogonId.HighPart, sessionData->LogonId.LowPart, a_pUserInfo->szAuthenticationPackage, a_pUserInfo->szLogonDomain);

                /* The session ID increments/decrements on Vista often! So don't compare
                   the session data SID with the current SID here. */
                DWORD dwActiveSession = 0;
                if (g_pfnWTSGetActiveConsoleSessionId != NULL)            /* Check terminal session ID. */
                    dwActiveSession = g_pfnWTSGetActiveConsoleSessionId();

                /*VBoxServiceVerbose(3, ("vboxVMInfoThread: Users: Current active session ID: %ld\n", dwActiveSession));*/

                if (SidTypeUser == ownerType)
                {
                    LPWSTR pBuffer = NULL;
                    DWORD dwBytesRet = 0;
                    int iState = 0;

                    if (WTSQuerySessionInformation(     /* Detect RDP sessions as well. */
                        WTS_CURRENT_SERVER_HANDLE,
                        WTS_CURRENT_SESSION,
                        WTSConnectState,
                        &pBuffer,
                        &dwBytesRet))
                    {
                        /*VBoxServiceVerbose(3, ("vboxVMInfoThread: Users: WTSQuerySessionInformation returned %ld bytes, p=%p, state=%d\n", dwBytesRet, pBuffer, pBuffer != NULL ? (INT)*pBuffer : -1));*/
                        if(dwBytesRet)
                            iState = *pBuffer;

                        if (    (iState == WTSActive)           /* User logged on to WinStation. */
                             || (iState == WTSShadow)           /* Shadowing another WinStation. */
                             || (iState == WTSDisconnected))    /* WinStation logged on without client. */
                        {
                            /** @todo On Vista and W2K, always "old" user name are still
                             *        there. Filter out the old one! */
                            VBoxServiceVerbose(3, "vboxVMInfoThread: Users: Account User=%ls is logged in via TCS/RDP. State=%d\n", a_pUserInfo->szUser, iState);
                            bFoundUser = TRUE;
                        }
                    }
                    else
                    {
                        /* Terminal services don't run (for example in W2K, nothing to worry about ...). */
                        /* ... or is on Vista fast user switching page! */
                        bFoundUser = TRUE;
                    }

                    if (pBuffer)
                        WTSFreeMemory(pBuffer);

                    /* A user logged in, but it could be a stale/orphaned logon session. */
                    BOOL bFoundInLUIDs = FALSE;
                    for (DWORD dwIndex = 0; dwIndex < a_dwNumOfProcLUIDs; dwIndex++)
                    {
                        if (   (a_pLuid[dwIndex].HighPart == sessionData->LogonId.HighPart)
                            && (a_pLuid[dwIndex].LowPart == sessionData->LogonId.LowPart))
                        {
                            bLoggedIn = TRUE;
                            VBoxServiceVerbose(3, "User \"%ls\" is logged in!\n", a_pUserInfo->szUser);
                            break;
                        }
                    }
                }
            }
        }
    }

    LsaFreeReturnBuffer(sessionData);
    return bLoggedIn;
}

int VboxServiceWinGetAddsVersion(uint32_t uiClientID)
{
    char szInstDir[_MAX_PATH] = {0};
    char szRev[_MAX_PATH] = {0};
    char szVer[_MAX_PATH] = {0};

    HKEY hKey = NULL;
    int rc = 0;
    DWORD dwSize = 0;

    rc = RegOpenKeyExA (HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\xVM VirtualBox Guest Additions", 0, KEY_READ, &hKey);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        rc = RegOpenKeyExA (HKEY_LOCAL_MACHINE, "SOFTWARE\\Sun\\VirtualBox Guest Additions", 0, KEY_READ, &hKey);
        if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
        {
            VBoxServiceError("Failed to open registry key (guest additions)! Error: %d\n", GetLastError());
            return 1;
        }
    }

    /* Installation directory. */
    dwSize = sizeof(szInstDir);
    rc = RegQueryValueExA (hKey, "InstallDir", 0, 0, (BYTE*)(LPCTSTR)szInstDir, &dwSize);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        RegCloseKey (hKey);
        VBoxServiceError("Failed to query registry key (install directory)! Error: %d\n", GetLastError());
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
        VBoxServiceError("Failed to query registry key (revision)! Error: %d\n",  GetLastError());
        return 1;
    }

    /* Version. */
    dwSize = sizeof(szVer);
    rc = RegQueryValueExA (hKey, "Version", 0, 0, (BYTE*)(LPCTSTR)szVer, &dwSize);
    if ((rc != ERROR_SUCCESS ) && (rc != ERROR_FILE_NOT_FOUND))
    {
        RegCloseKey (hKey);
        VBoxServiceError("Failed to query registry key (version)! Error: %Rrc\n",  GetLastError());
        return 1;
    }

    /* Write information to host. */
    VboxServiceWriteProp(uiClientID, "GuestAdd/InstallDir", szInstDir);
    VboxServiceWriteProp(uiClientID, "GuestAdd/Revision", szRev);
    VboxServiceWriteProp(uiClientID, "GuestAdd/Version", szVer);

    RegCloseKey (hKey);

    return VINF_SUCCESS;
}

int VboxServiceWinGetComponentVersions(uint32_t uiClientID)
{
    int rc;
    char szVer[_MAX_PATH] = {0};
    char szPropPath[_MAX_PATH] = {0};
    TCHAR szSysDir[_MAX_PATH] = {0};
    TCHAR szWinDir[_MAX_PATH] = {0};
    TCHAR szDriversDir[_MAX_PATH + 32] = {0};

    GetSystemDirectory(szSysDir, _MAX_PATH);
    GetWindowsDirectory(szWinDir, _MAX_PATH);
    swprintf(szDriversDir, (_MAX_PATH + 32), TEXT("%s\\drivers"), szSysDir);

    /* The file information table. */
    VBOXSERVICEVMINFOFILE vboxFileInfoTable[] =
    {
        { szSysDir, TEXT("VBoxControl.exe"), },
        { szSysDir, TEXT("VBoxHook.dll"), },
        { szSysDir, TEXT("VBoxDisp.dll"), },
        { szSysDir, TEXT("VBoxMRXNP.dll"), },
        { szSysDir, TEXT("VBoxService.exe"), },
        { szSysDir, TEXT("VBoxTray.exe"), },
        { szSysDir, TEXT("VBoxGINA.dll"), },

        { szSysDir, TEXT("VBoxOGLarrayspu.dll"), },
        { szSysDir, TEXT("VBoxOGLcrutil.dll"), },
        { szSysDir, TEXT("VBoxOGLerrorspu.dll"), },
        { szSysDir, TEXT("VBoxOGLpackspu.dll"), },
        { szSysDir, TEXT("VBoxOGLpassthroughspu.dll"), },
        { szSysDir, TEXT("VBoxOGLfeedbackspu.dll"), },
        { szSysDir, TEXT("VBoxOGL.dll"), },

        { szDriversDir, TEXT("VBoxGuest.sys"), },
        { szDriversDir, TEXT("VBoxMouse.sys"), },
        { szDriversDir, TEXT("VBoxSF.sys"),    },
        { szDriversDir, TEXT("VBoxVideo.sys"), },

        {
            NULL
        }
    };

    PVBOXSERVICEVMINFOFILE pTable = vboxFileInfoTable;
    Assert(pTable);
    while (pTable->pszFileName)
    {
        rc = VboxServiceGetFileVersionString(pTable->pszFilePath, pTable->pszFileName, szVer, sizeof(szVer));
        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestAdd/Components/%ls", pTable->pszFileName);
        VboxServiceWriteProp(uiClientID, szPropPath, szVer);
        pTable++;
    }

    return VINF_SUCCESS;
}

