/* $Id$ */
/** @file
 * VBoxVMInfoUser - User information for the host.
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
#include "VBoxVMInfo.h"
#include "VBoxVMInfoUser.h"

#include <Ntsecapi.h>
#include <wtsapi32.h>       /* For WTS* calls. */
#include <psapi.h>          /* EnumProcesses. */

/* Function GetLUIDsFromProcesses() written by Stefan Kuhr. */
static DWORD GetLUIDsFromProcesses(PLUID *ppLuid)
{
    DWORD dwSize, dwSize2, dwIndex ;
    LPDWORD        lpdwPIDs ;
    DWORD dwLastError = ERROR_SUCCESS;

    if (!ppLuid)
    {
        SetLastError(ERROR_INVALID_PARAMETER);
        return 0L;
    }

    // Call the PSAPI function EnumProcesses to get all of the
    // ProcID's currently in the system.
    // NOTE: In the documentation, the third parameter of
    // EnumProcesses is named cbNeeded, which implies that you
    // can call the function once to find out how much space to
    // allocate for a buffer and again to fill the buffer.
    // This is not the case. The cbNeeded parameter returns
    // the number of PIDs returned, so if your buffer size is
    // zero cbNeeded returns zero.
    // NOTE: The "HeapAlloc" loop here ensures that we
    // actually allocate a buffer large enough for all the
    // PIDs in the system.
    dwSize2 = 256 * sizeof( DWORD ) ;

    lpdwPIDs = NULL ;
    do
    {
        if( lpdwPIDs )
        {
            HeapFree( GetProcessHeap(), 0, lpdwPIDs ) ;
            dwSize2 *= 2 ;
        }
        lpdwPIDs = (unsigned long *)HeapAlloc( GetProcessHeap(), 0, dwSize2 );
        if( lpdwPIDs == NULL )
            return 0L; // Last error will be that of HeapAlloc

        if( !EnumProcesses( lpdwPIDs, dwSize2, &dwSize ) )
        {
            DWORD dw = GetLastError();
            HeapFree( GetProcessHeap(), 0, lpdwPIDs ) ;
            SetLastError(dw);
            return 0L;
        }
    }
    while( dwSize == dwSize2 ) ;

    /* At this point we have an array of the PIDs at the
       time of the last EnumProcesses invocation. We will
       allocate an array of LUIDs passed back via the out
       param ppLuid of exactly the number of PIDs. We will
       only fill the first n values of this array, with n
       being the number of unique LUIDs found in these PIDs. */

    // How many ProcIDs did we get?
    dwSize /= sizeof( DWORD ) ;
    dwSize2 = 0L;   /* Our return value of found luids. */

    *ppLuid = (LUID *)LocalAlloc(LPTR, dwSize*sizeof(LUID));
    if (!(*ppLuid))
    {
        dwLastError = GetLastError();
        goto CLEANUP;
    }
    for( dwIndex = 0 ; dwIndex < dwSize ; dwIndex++ )
    {
        (*ppLuid)[dwIndex].LowPart =0L;
        (*ppLuid)[dwIndex].HighPart=0;


        // Open the process (if we can... security does not
        // permit every process in the system).
        HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION,FALSE, lpdwPIDs[ dwIndex ] ) ;
        if( hProcess != NULL )
        {
            HANDLE hAccessToken;
            if (OpenProcessToken( hProcess, TOKEN_QUERY, &hAccessToken))
            {
                TOKEN_STATISTICS ts;
                DWORD dwSize;
                if  (GetTokenInformation(hAccessToken, TokenStatistics, &ts, sizeof ts, &dwSize))
                {
                    DWORD dwTmp = 0L;
                    BOOL bFound = FALSE;
                    for (;dwTmp<dwSize2 && !bFound;dwTmp++)
                        bFound = (*ppLuid)[dwTmp].HighPart == ts.AuthenticationId.HighPart &&
                            (*ppLuid)[dwTmp].LowPart == ts.AuthenticationId.LowPart;

                    if (!bFound)
                        (*ppLuid)[dwSize2++] = ts.AuthenticationId;
                }
                CloseHandle(hAccessToken) ;
            }

            CloseHandle( hProcess ) ;
        }

        /// we don't really care if OpenProcess or OpenProcessToken fail or succeed, because
        /// there are quite a number of system processes we cannot open anyway, not even as SYSTEM.
    }

CLEANUP:

    if (lpdwPIDs)
        HeapFree( GetProcessHeap(), 0, lpdwPIDs ) ;

    if (ERROR_SUCCESS !=dwLastError)
        SetLastError(dwLastError);

    return dwSize2;
}

BOOL isLoggedIn(VBOXINFORMATIONCONTEXT* a_pCtx,
                VBOXUSERINFO* a_pUserInfo,
                PLUID a_pSession,
                PLUID a_pLuid,
                DWORD a_dwNumOfProcLUIDs)
{
    BOOL bLoggedIn = FALSE;
    BOOL bFoundUser = FALSE;
    PSECURITY_LOGON_SESSION_DATA sessionData = NULL;
    NTSTATUS ret = 0;
    WCHAR szAuthPkg[256] = { 0 };
    WCHAR szLogonDomain[256] = { 0 };
    WCHAR *usBuffer = NULL;
    int usLength = 0;

    if (!a_pSession)
        return FALSE;

    ret = LsaGetLogonSessionData (a_pSession, &sessionData);
    if (ret != STATUS_SUCCESS)
    {
        Log(("vboxVMInfoThread: LsaGetLogonSessionData failed %lu\n", LsaNtStatusToWinError(ret)));

        if (sessionData)
            LsaFreeReturnBuffer(sessionData);

        return FALSE;
    }

    if (!sessionData)
    {
        Log(("vboxVMInfoThread: Invalid logon session data.\n"));
        return FALSE;
    }

    if ((sessionData->UserName.Buffer != NULL) &&
        (sessionData->Sid != NULL) &&
        (sessionData->LogonId.LowPart != 0))
    {
        /* Get the user name. */
        usBuffer = (sessionData->UserName).Buffer;
        usLength = (sessionData->UserName).Length;
        if (usLength > 256)
        {
            Log(("vboxVMInfoThread: User name too long for buffer! Length: %d, Buffer: 256\n", usLength));
        }
        else
        {
            /** @todo r=bird: Check this code for buffer overruns. the if check above is wrong as it's making assumptions about _MAX_PATH (which is 260 not 256 as stated). */
            wcsncpy (a_pUserInfo->szUser, usBuffer, usLength);
            wcscat (a_pUserInfo->szUser, L"");

            usBuffer = (sessionData->AuthenticationPackage).Buffer;
            usLength = (sessionData->AuthenticationPackage).Length;
            wcsncpy (szAuthPkg, usBuffer, usLength);
            wcscat (szAuthPkg, L"");

            usBuffer = (sessionData->LogonDomain).Buffer;
            usLength = (sessionData->LogonDomain).Length;
            wcsncpy (szLogonDomain, usBuffer, usLength);
            wcscat (szLogonDomain, L""); /** @todo r=bird: There is a potential buffer overrun here. */

            /* Only handle users which can login interactively. */
            if (    ((SECURITY_LOGON_TYPE)sessionData->LogonType == Interactive)
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
                    Log(("vboxVMInfoThread: Account User=%ls, Session=%ld, LUID=%ld,%ld, AuthPkg=%ls, Domain=%ls\n",
                         a_pUserInfo->szUser, sessionData->Session, sessionData->LogonId.HighPart, sessionData->LogonId.LowPart, szAuthPkg, szLogonDomain));

                    /* The session ID increments/decrements on Vista often! So don't compare
                       the session data SID with the current SID here. */
                    DWORD dwActiveSession = 0;
                    if (a_pCtx->pfnWTSGetActiveConsoleSessionId != NULL)            /* Check terminal session ID. */
                        dwActiveSession = a_pCtx->pfnWTSGetActiveConsoleSessionId();

                    /*Log(("vboxVMInfoThread: Current active session ID: %ld\n", dwActiveSession));*/

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
                            /*Log(("vboxVMInfoThread: WTSQuerySessionInformation returned %ld bytes, p=%p, state=%d\n", dwBytesRet, pBuffer, pBuffer != NULL ? (INT)*pBuffer : -1));*/
                            if(dwBytesRet)
                                iState = *pBuffer;

                            if (    (iState == WTSActive)           /* User logged on to WinStation. */
                                 || (iState == WTSShadow)           /* Shadowing another WinStation. */
                                 || (iState == WTSDisconnected))    /* WinStation logged on without client. */
                            {
                                /** @todo On Vista and W2K, always "old" user name are still there. Filter out the old! */
                                Log(("vboxVMInfoThread: Account User=%ls is logged in via TCS/RDP. State=%d\n", a_pUserInfo->szUser, iState));
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
                                Log(("vboxVMInfoThread: User \"%ls\" is logged in!\n", a_pUserInfo->szUser));
                                break;
                            }
                        }
                    }
                }
            }
        }
    }

    LsaFreeReturnBuffer(sessionData);
    return bLoggedIn;
}

int vboxVMInfoUser(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    PLUID pSessions = NULL;
    ULONG ulCount = 0;
    NTSTATUS ret = 0;

    int iUserCount = 0;
    char szUserList[4096] = {0};
    char* pszTemp = NULL;

    /* This function can report stale or orphaned interactive logon sessions of already logged
       off users (especially in Windows 2000). */
    ret = LsaEnumerateLogonSessions(&ulCount, &pSessions);
    Log(("vboxVMInfoThread: Found %d users.\n", ulCount));

    if (ret != STATUS_SUCCESS)
    {
        Log(("vboxVMInfoThread: LsaEnumerate failed %lu\n", LsaNtStatusToWinError(ret)));
        return 1;
    }

    PLUID pLuid = NULL;
    DWORD dwNumOfProcLUIDs = GetLUIDsFromProcesses(&pLuid);

    VBOXUSERINFO userInfo;
    ZeroMemory (&userInfo, sizeof(VBOXUSERINFO));

    for (int i = 0; i<(int)ulCount; i++)
    {
        if (isLoggedIn(a_pCtx, &userInfo, &pSessions[i], pLuid, dwNumOfProcLUIDs))
        {
            if (iUserCount > 0)
                strcat (szUserList, ",");

            iUserCount++;

            RTUtf16ToUtf8(userInfo.szUser, &pszTemp);
            strcat(szUserList, pszTemp);
            RTMemFree(pszTemp);
        }
    }

    if (NULL != pLuid)
        LocalFree (pLuid);

    LsaFreeReturnBuffer(pSessions);

    /* Write information to host. */
    vboxVMInfoWriteProp(a_pCtx, "GuestInfo/OS/LoggedInUsersList", (iUserCount > 0) ? szUserList : NULL);
    vboxVMInfoWritePropInt(a_pCtx, "GuestInfo/OS/LoggedInUsers", iUserCount);

    return ret;
}

