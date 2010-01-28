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
#include <wtsapi32.h>       /* For WTS* calls. */
#include <psapi.h>          /* EnumProcesses. */

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifndef TARGET_NT4
 /** Function prototypes for dynamic loading. */
 extern fnWTSGetActiveConsoleSessionId g_pfnWTSGetActiveConsoleSessionId;
 /** The vminfo interval (millseconds). */
 uint32_t g_VMInfoLoggedInUsersCount = 0;
#endif


#ifndef TARGET_NT4
int VBoxServiceVMInfoWinProcessesGetTokenInfo(PVBOXSERVICEVMINFOPROC pProc, 
                                              TOKEN_INFORMATION_CLASS tkClass)
{
    AssertPtr(pProc);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pProc->id);
    if (h == NULL)
        return RTErrConvertFromWin32(GetLastError());

     HANDLE hToken;
     int rc;
     if (FALSE == OpenProcessToken(h, TOKEN_QUERY, &hToken))
     {
         rc = RTErrConvertFromWin32(GetLastError());
     }
     else
     {
         void *pvTokenInfo = NULL;
         DWORD dwTokenInfoSize;
         switch (tkClass)
         {
         case TokenStatistics:
             dwTokenInfoSize = sizeof(TOKEN_STATISTICS);
             pvTokenInfo = (TOKEN_STATISTICS*)RTMemAlloc(dwTokenInfoSize);
             AssertPtr(pvTokenInfo);
             break;

         /** @todo Implement more token classes here. */

         default:
             VBoxServiceError("Token class not implemented: %ld", tkClass);
             rc = VERR_NOT_IMPLEMENTED;
             break;
         }

         if (pvTokenInfo)
         {     
             DWORD dwRetLength;
             if (FALSE == GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
             {
                 rc = RTErrConvertFromWin32(GetLastError());
             }
             else
             {
                 switch (tkClass)
                 {
                 case TokenStatistics:
                     {
                         TOKEN_STATISTICS *pStats = (TOKEN_STATISTICS*)pvTokenInfo;
                         AssertPtr(pStats);
                         pProc->luid = pStats->AuthenticationId;
                         /* @todo Add more information of TOKEN_STATISTICS as needed. */
                         break;
                     }

                 default:
                     /* Should never get here! */
                     break;                
                 }
                 rc = VINF_SUCCESS;
             }
             RTMemFree(pvTokenInfo);
         }
         CloseHandle(hToken);
    }   
    CloseHandle(h);
    return rc;
}

int VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount)
{
    AssertPtr(ppProc);
    AssertPtr(pdwCount);

    DWORD dwNumProcs = 128; /* Number of processes our array can hold */
    DWORD *pdwProcIDs = (DWORD*)RTMemAlloc(dwNumProcs * sizeof(DWORD));
    if (pdwProcIDs == NULL)
        return VERR_NO_MEMORY;

    int rc;
    DWORD cbRet; /* Returned size in bytes */
    do
    {
        if (FALSE == EnumProcesses(pdwProcIDs, dwNumProcs * sizeof(DWORD), &cbRet))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }

        /* Was our array big enough? Or do we need more space? */
        if (cbRet >= dwNumProcs * sizeof(DWORD))
        {
            /* Apparently not, so try next bigger size */
            dwNumProcs += 128;
            pdwProcIDs = (DWORD*)RTMemRealloc(pdwProcIDs, dwNumProcs * sizeof(DWORD));
            if (pdwProcIDs == NULL)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
        }
        else
        {
            rc = VINF_SUCCESS;
            break;
        }
    } while(cbRet >= dwNumProcs * sizeof(DWORD));

    if (RT_SUCCESS(rc))
    {
        /* Allocate our process structure */
        *ppProc = (PVBOXSERVICEVMINFOPROC)RTMemAlloc(dwNumProcs * sizeof(VBOXSERVICEVMINFOPROC));
        if (ppProc == NULL)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            /* We now have the PIDs, fill them into the struct and lookup their LUID's */
            PVBOXSERVICEVMINFOPROC pCur = *ppProc;
            DWORD *pCurProcID = pdwProcIDs;
            for (DWORD i=0; i<dwNumProcs; i++)
            {
                RT_BZERO(pCur, sizeof(VBOXSERVICEVMINFOPROC));
                pCur->id = *pCurProcID;
                rc = VBoxServiceVMInfoWinProcessesGetTokenInfo(pCur, TokenStatistics);
                if (RT_FAILURE(rc))
                {
                    /* Because some processes cannot be opened/parsed on Windows, we should not consider to
                       be this an error here. */
                    rc = VINF_SUCCESS;
                }
                pCur++;
                pCurProcID++;
            }
            /* Save number of processes */
            *pdwCount = dwNumProcs;
        }
    }

    RTMemFree(pdwProcIDs);
    if (RT_FAILURE(rc))
        VBoxServiceVMInfoWinProcessesFree(*ppProc);
    return rc;
}

void VBoxServiceVMInfoWinProcessesFree(PVBOXSERVICEVMINFOPROC pProc)
{
    if (pProc != NULL)
    {
        RTMemFree(pProc);
        pProc = NULL;
    }
}

DWORD VBoxServiceVMInfoWinSessionGetProcessCount(PLUID pSession,
                                                 PVBOXSERVICEVMINFOPROC pProc, DWORD dwProcCount)
{
    AssertPtr(pSession);

    if (dwProcCount <= 0) /* To be on the safe side. */
        return 0;
    AssertPtr(pProc);

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    if (STATUS_SUCCESS != LsaGetLogonSessionData (pSession, &pSessionData))
    {
        VBoxServiceError("Could not get logon session data! rc=%Rrc", RTErrConvertFromWin32(GetLastError()));
        return 0;
    }
    AssertPtr(pSessionData);

    /* Even if a user seems to be logged in, it could be a stale/orphaned logon session.
     * So check if we have some processes bound to it by comparing the session <-> process LUIDs. */
    PVBOXSERVICEVMINFOPROC pCur = pProc;
    for (DWORD i=0; i<dwProcCount; i++)
    {
        /*VBoxServiceVerbose(3, "%ld:%ld <-> %ld:%ld\n",
                             pCur->luid.HighPart, pCur->luid.LowPart,
                             pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart);*/
        if (   pCur->luid.HighPart == pSessionData->LogonId.HighPart
            && pCur->luid.LowPart  == pSessionData->LogonId.LowPart)
        {
            VBoxServiceVerbose(3, "Users: Session %ld:%ld has active processes\n",
                               pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart);
            LsaFreeReturnBuffer(pSessionData);
            return 1;
        }          
        pCur++;
    }
    LsaFreeReturnBuffer(pSessionData);
    return 0;
}

BOOL VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo,
                                    PLUID a_pSession)
{
    BOOL bFoundUser = FALSE;
    PSECURITY_LOGON_SESSION_DATA sessionData = NULL;
    NTSTATUS r = 0;
    WCHAR *usBuffer = NULL;
    int iLength = 0;

    if (!a_pSession)
        return FALSE;

    r = LsaGetLogonSessionData(a_pSession, &sessionData);
    if (r != STATUS_SUCCESS)
    {
        VBoxServiceError("LsaGetLogonSessionData failed, LSA error %lu\n", LsaNtStatusToWinError(r));

        if (sessionData)
            LsaFreeReturnBuffer(sessionData);

        return FALSE;
    }

    if (!sessionData)
    {
        VBoxServiceError("Invalid logon session data.\n");
        return FALSE;
    }

    VBoxServiceVerbose(3, "Users: Session data: Name = %ls, Len = %d, SID = %s, LogonID = %d,%d\n",
        (sessionData->UserName).Buffer, 
        (sessionData->UserName).Length, 
        (sessionData->Sid != NULL) ? "1" : "0", sessionData->LogonId.HighPart, sessionData->LogonId.LowPart);

    if ((sessionData->UserName.Buffer != NULL) &&
        (sessionData->Sid != NULL) &&
        (sessionData->LogonId.LowPart != 0))
    {
        /* Get the user name. */
        usBuffer = (sessionData->UserName).Buffer;
        iLength = (sessionData->UserName).Length;
        if (iLength > sizeof(a_pUserInfo->szUser) - sizeof(WCHAR))   /* -sizeof(WCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "User name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szUser) - sizeof(WCHAR);
        }
        wcsncpy (a_pUserInfo->szUser, usBuffer, iLength);

        /* Get authentication package. */
        usBuffer = (sessionData->AuthenticationPackage).Buffer;
        iLength = (sessionData->AuthenticationPackage).Length;
        if (iLength > sizeof(a_pUserInfo->szAuthenticationPackage) - sizeof(WCHAR))   /* -sizeof(WCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "Authentication pkg name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szAuthenticationPackage) - sizeof(WCHAR);
        }
        if (iLength)
            wcsncpy (a_pUserInfo->szAuthenticationPackage, usBuffer, iLength);

        /* Get logon domain. */
        usBuffer = (sessionData->LogonDomain).Buffer;
        iLength = (sessionData->LogonDomain).Length;
        if (iLength > sizeof(a_pUserInfo->szLogonDomain) - sizeof(WCHAR))   /* -sizeof(WCHAR) because we have to add the terminating null char at the end later. */
        {
            VBoxServiceVerbose(0, "Logon domain name too long (%d bytes) for buffer! Name will be truncated.\n", iLength);
            iLength = sizeof(a_pUserInfo->szLogonDomain) - sizeof(WCHAR);
        }
        if (iLength)
            wcsncpy (a_pUserInfo->szLogonDomain, usBuffer, iLength);

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

                /*VBoxServiceVerbose(3, ("Users: Current active session ID: %ld\n", dwActiveSession));*/

                if (SidTypeUser == ownerType)
                {
                    char* pBuffer = NULL;
                    DWORD dwBytesRet = 0;
                    int iState = 0;

                    if (WTSQuerySessionInformation(     /* Detect RDP sessions as well. */
                        WTS_CURRENT_SERVER_HANDLE,
                        WTS_CURRENT_SESSION,
                        WTSConnectState,
                        &pBuffer,
                        &dwBytesRet))
                    {
                        /*VBoxServiceVerbose(3, ("Users: WTSQuerySessionInformation returned %ld bytes, p=%p, state=%d\n", dwBytesRet, pBuffer, pBuffer != NULL ? (INT)*pBuffer : -1));*/
                        if(dwBytesRet)
                            iState = *pBuffer;

                        if (    (iState == WTSActive)           /* User logged on to WinStation. */
                             || (iState == WTSShadow)           /* Shadowing another WinStation. */
                             || (iState == WTSDisconnected))    /* WinStation logged on without client. */
                        {
                            /** @todo On Vista and W2K, always "old" user name are still
                             *        there. Filter out the old one! */
                            VBoxServiceVerbose(3, "Users: Account User=%ls is logged in via TCS/RDP. State=%d\n", a_pUserInfo->szUser, iState);
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
                }
            }
        }
    }

    LsaFreeReturnBuffer(sessionData);
    return bFoundUser;
}
#endif /* TARGET_NT4 */

int VBoxServiceWinGetComponentVersions(uint32_t uiClientID)
{
    int rc;
    char szVer[_MAX_PATH] = {0};
    char szPropPath[_MAX_PATH] = {0};
    char szSysDir[_MAX_PATH] = {0};
    char szWinDir[_MAX_PATH] = {0};
    char szDriversDir[_MAX_PATH + 32] = {0};

    GetSystemDirectory(szSysDir, _MAX_PATH);
    GetWindowsDirectory(szWinDir, _MAX_PATH);
    RTStrPrintf(szDriversDir, (_MAX_PATH + 32), "%s\\drivers", szSysDir);
#ifdef RT_ARCH_AMD64
    char szSysWowDir[_MAX_PATH + 32] = {0};
    RTStrPrintf(szSysWowDir, (_MAX_PATH + 32), "%s\\SysWow64", szWinDir);
#endif

    /* The file information table. */
#ifndef TARGET_NT4
    VBOXSERVICEVMINFOFILE vboxFileInfoTable[] =
    {
        { szSysDir, "VBoxControl.exe", },
        { szSysDir, "VBoxHook.dll", },
        { szSysDir, "VBoxDisp.dll", },
        { szSysDir, "VBoxMRXNP.dll", },
        { szSysDir, "VBoxService.exe", },
        { szSysDir, "VBoxTray.exe", },
        { szSysDir, "VBoxGINA.dll", },
        { szSysDir, "VBoxCredProv.dll", },

 /* On 64-bit we don't yet have the OpenGL DLLs in native format.
    So just enumerate the 32-bit files in the SYSWOW directory. */
 #ifdef RT_ARCH_AMD64
        { szSysWowDir, "VBoxOGLarrayspu.dll", },
        { szSysWowDir, "VBoxOGLcrutil.dll", },
        { szSysWowDir, "VBoxOGLerrorspu.dll", },
        { szSysWowDir, "VBoxOGLpackspu.dll", },
        { szSysWowDir, "VBoxOGLpassthroughspu.dll", },
        { szSysWowDir, "VBoxOGLfeedbackspu.dll", },
        { szSysWowDir, "VBoxOGL.dll", },
 #else
        { szSysDir, "VBoxOGLarrayspu.dll", },
        { szSysDir, "VBoxOGLcrutil.dll", },
        { szSysDir, "VBoxOGLerrorspu.dll", },
        { szSysDir, "VBoxOGLpackspu.dll", },
        { szSysDir, "VBoxOGLpassthroughspu.dll", },
        { szSysDir, "VBoxOGLfeedbackspu.dll", },
        { szSysDir, "VBoxOGL.dll", },
 #endif

        { szDriversDir, "VBoxGuest.sys", },
        { szDriversDir, "VBoxMouse.sys", },
        { szDriversDir, "VBoxSF.sys",    },
        { szDriversDir, "VBoxVideo.sys", },

        {
            NULL
        }
    };
#else /* File lookup for NT4. */
    VBOXSERVICEVMINFOFILE vboxFileInfoTable[] =
    {
        { szSysDir, "VBoxControl.exe", },
        { szSysDir, "VBoxHook.dll", },
        { szSysDir, "VBoxDisp.dll", },
        { szSysDir, "VBoxService.exe", },
        { szSysDir, "VBoxTray.exe", },

        { szDriversDir, "VBoxGuestNT.sys", },
        { szDriversDir, "VBoxMouseNT.sys", },
        { szDriversDir, "VBoxVideo.sys", },

        {
            NULL
        }
    };
#endif

    PVBOXSERVICEVMINFOFILE pTable = vboxFileInfoTable;
    Assert(pTable);
    while (pTable->pszFileName)
    {
        rc = VBoxServiceGetFileVersionString(pTable->pszFilePath, pTable->pszFileName, szVer, sizeof(szVer));
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestAdd/Components/%s", pTable->pszFileName);
        rc = VBoxServiceWritePropF(uiClientID, szPropPath, "%s", szVer);
        pTable++;
    }

    return VINF_SUCCESS;
}
