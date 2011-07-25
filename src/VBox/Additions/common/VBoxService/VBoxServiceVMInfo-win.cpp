/* $Id$ */
/** @file
 * VBoxService - Virtual Machine Information for the Host, Windows specifics.
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
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0502
# undef  _WIN32_WINNT
# define _WIN32_WINNT 0x0502 /* CachedRemoteInteractive in recent SDKs. */
#endif
#include <Windows.h>
#include <wtsapi32.h>       /* For WTS* calls. */
#include <psapi.h>          /* EnumProcesses. */
#include <Ntsecapi.h>       /* Needed for process security information. */

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
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Structure for storing the looked up user information. */
typedef struct
{
    WCHAR wszUser[_MAX_PATH];
    WCHAR wszAuthenticationPackage[_MAX_PATH];
    WCHAR wszLogonDomain[_MAX_PATH];
} VBOXSERVICEVMINFOUSER, *PVBOXSERVICEVMINFOUSER;

/** Structure for the file information lookup. */
typedef struct
{
    char *pszFilePath;
    char *pszFileName;
} VBOXSERVICEVMINFOFILE, *PVBOXSERVICEVMINFOFILE;

/** Structure for process information lookup. */
typedef struct
{
    DWORD id;
    LUID luid;
} VBOXSERVICEVMINFOPROC, *PVBOXSERVICEVMINFOPROC;


/*******************************************************************************
*   Prototypes
*******************************************************************************/
bool VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession, VBOXSERVICEVMINFOPROC const *paProcs, DWORD cProcs);
bool VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
int  VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
void VBoxServiceVMInfoWinProcessesFree(PVBOXSERVICEVMINFOPROC paProcs);



#ifndef TARGET_NT4

/**
 * Fills in more data for a process.
 *
 * @returns VBox status code.
 * @param   pProc           The process structure to fill data into.
 * @param   tkClass         The kind of token information to get.
 */
static int VBoxServiceVMInfoWinProcessesGetTokenInfo(PVBOXSERVICEVMINFOPROC pProc,
                                                     TOKEN_INFORMATION_CLASS tkClass)
{
    AssertPtr(pProc);
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pProc->id);
    if (h == NULL)
        return RTErrConvertFromWin32(GetLastError());

     int    rc = VERR_NO_MEMORY;
     HANDLE hToken;
     if (OpenProcessToken(h, TOKEN_QUERY, &hToken))
     {
         void *pvTokenInfo = NULL;
         DWORD dwTokenInfoSize;
         switch (tkClass)
         {
             case TokenStatistics:
                 dwTokenInfoSize = sizeof(TOKEN_STATISTICS);
                 pvTokenInfo = RTMemAlloc(dwTokenInfoSize);
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
             if (GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
             {
                 switch (tkClass)
                 {
                     case TokenStatistics:
                     {
                         TOKEN_STATISTICS *pStats = (TOKEN_STATISTICS*)pvTokenInfo;
                         pProc->luid = pStats->AuthenticationId;
                         /** @todo Add more information of TOKEN_STATISTICS as needed. */
                         break;
                     }

                     default:
                         /* Should never get here! */
                         break;
                 }
                 rc = VINF_SUCCESS;
             }
             else
                 rc = RTErrConvertFromWin32(GetLastError());
             RTMemFree(pvTokenInfo);
         }
         CloseHandle(hToken);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
    CloseHandle(h);
    return rc;
}


/**
 * Enumerate all the processes in the system and get the logon user IDs for
 * them.
 *
 * @returns VBox status code.
 * @param   ppaProcs    Where to return the process snapshot.  This must be
 *                      freed by calling VBoxServiceVMInfoWinProcessesFree.
 *
 * @param   pcProcs     Where to store the returned process count.
 */
int VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppaProcs, PDWORD pcProcs)
{
    AssertPtr(ppaProcs);
    AssertPtr(pcProcs);

    /*
     * Call EnumProcesses with an increasingly larger buffer until it all fits
     * or we think something is screwed up.
     */
    DWORD   cProcesses  = 64;
    PDWORD  paPids      = NULL;
    int     rc          = VINF_SUCCESS;
    do
    {
        /* Allocate / grow the buffer first. */
        cProcesses *= 2;
        void *pvNew = RTMemRealloc(paPids, cProcesses * sizeof(DWORD));
        if (!pvNew)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        paPids = (PDWORD)pvNew;

        /* Query the processes. Not the cbRet == buffer size means there could be more work to be done. */
        DWORD cbRet;
        if (!EnumProcesses(paPids, cProcesses * sizeof(DWORD), &cbRet))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }
        if (cbRet < cProcesses * sizeof(DWORD))
        {
            cProcesses = cbRet / sizeof(DWORD);
            break;
        }
    } while (cProcesses <= 32768); /* Should be enough; see: http://blogs.technet.com/markrussinovich/archive/2009/07/08/3261309.aspx */
    if (RT_SUCCESS(rc))
    {
        /*
         * Allocate out process structures and fill data into them.
         * We currently only try lookup their LUID's.
         */
        PVBOXSERVICEVMINFOPROC paProcs;
        paProcs = (PVBOXSERVICEVMINFOPROC)RTMemAllocZ(cProcesses * sizeof(VBOXSERVICEVMINFOPROC));
        if (paProcs)
        {
            for (DWORD i = 0; i < cProcesses; i++)
            {
                paProcs[i].id = paPids[i];
                rc = VBoxServiceVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenStatistics);
                if (RT_FAILURE(rc))
                {
                    /* Because some processes cannot be opened/parsed on
                       Windows, we should not consider to be this an error here. */
                    rc = VINF_SUCCESS;
                }
            }

            /* Save number of processes */
            if (RT_SUCCESS(rc))
            {
                *pcProcs  = cProcesses;
                *ppaProcs = paProcs;
            }
            else
                RTMemFree(paProcs);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTMemFree(paPids);
    return rc;
}

/**
 * Frees the process structures returned by
 * VBoxServiceVMInfoWinProcessesEnumerate() before.
 *
 * @param   paProcs     What
 */
void VBoxServiceVMInfoWinProcessesFree(PVBOXSERVICEVMINFOPROC paProcs)
{
    RTMemFree(paProcs);
}

/**
 * Determines whether the specified session has processes on the system.
 *
 * @returns true if it has, false if it doesn't.
 * @param   pSession        The session.
 * @param   paProcs         The process snapshot.
 * @param   cProcs          The number of processes in the snaphot.
 */
bool VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession, VBOXSERVICEVMINFOPROC const *paProcs, DWORD cProcs)
{
    if (!pSession)
    {
        VBoxServiceVerbose(1, "VMInfo/Users: Session became invalid while enumerating!\n");
        return false;
    }

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = LsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        VBoxServiceError("VMInfo/Users: Could not get logon session data! rcNt=%#x", rcNt);
        return false;
    }

    /*
     * Even if a user seems to be logged in, it could be a stale/orphaned logon
     * session. So check if we have some processes bound to it by comparing the
     * session <-> process LUIDs.
     */
    uint32_t cNumProcs = 0;
    for (DWORD i = 0; i < cProcs; i++)
    {
        /*VBoxServiceVerbose(3, "%ld:%ld <-> %ld:%ld\n",
                             paProcs[i].luid.HighPart, paProcs[i].luid.LowPart,
                             pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart);*/
        if (   paProcs[i].luid.HighPart == pSessionData->LogonId.HighPart
            && paProcs[i].luid.LowPart  == pSessionData->LogonId.LowPart)
        {
            cNumProcs++;
            if (g_cVerbosity < 4) /* We want a bit more info on high verbosity. */
                break;
        }
    }

    VBoxServiceVerbose(3, "VMInfo/Users: Session %u has %u processes\n",
                       pSessionData->Session, cNumProcs);

    LsaFreeReturnBuffer(pSessionData);
    return cNumProcs ? true : false;
}


/**
 * Save and noisy string copy.
 *
 * @param   pwszDst             Destination buffer.
 * @param   cbDst               Size in bytes - not WCHAR count!
 * @param   pSrc                Source string.
 * @param   pszWhat             What this is. For the log.
 */
static void VBoxServiceVMInfoWinSafeCopy(PWCHAR pwszDst, size_t cbDst, LSA_UNICODE_STRING const *pSrc, const char *pszWhat)
{
    Assert(RT_ALIGN(cbDst, sizeof(WCHAR)) == cbDst);

    size_t cbCopy = pSrc->Length;
    if (cbCopy + sizeof(WCHAR) > cbDst)
    {
        VBoxServiceVerbose(0, "%s is too long - %u bytes, buffer %u bytes! It will be truncated.\n",
                           pszWhat, cbCopy, cbDst);
        cbCopy = cbDst - sizeof(WCHAR);
    }
    if (cbCopy)
        memcpy(pwszDst, pSrc->Buffer, cbCopy);
    pwszDst[cbCopy / sizeof(WCHAR)] = '\0';
}


/**
 * Detects whether a user is logged on.
 *
 * @returns true if logged in, false if not (or error).
 * @param   pUserInfo           Where to return the user information.
 * @param   pSession            The session to check.
 */
bool VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER pUserInfo, PLUID pSession)
{
    AssertPtr(pUserInfo);
    if (!pSession)
        return false;

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = LsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG ulError = LsaNtStatusToWinError(rcNt);
        switch (ulError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                /* If we don't have enough memory it's hard to judge whether the specified user
                 * is logged in or not, so just assume he/she's not. */
                VBoxServiceVerbose(3, "VMInfo/Users: Not enough memory to retrieve logon session data!\n");
                break;

            case ERROR_NO_SUCH_LOGON_SESSION:
                /* Skip session data which is not valid anymore because it may have been
                 * already terminated. */
                break;

            default:
                VBoxServiceError("VMInfo/Users: LsaGetLogonSessionData failed with error %ul\n", ulError);
                break;
        }
        if (pSessionData)
            LsaFreeReturnBuffer(pSessionData);
        return false;
    }
    if (!pSessionData)
    {
        VBoxServiceError("VMInfo/Users: Invalid logon session data!\n");
        return false;
    }

    /*
     * Only handle users which can login interactively or logged in
     * remotely over native RDP.
     */
    bool fFoundUser = false;
    DWORD dwErr = NO_ERROR;
    if (   IsValidSid(pSessionData->Sid)
        && (   (SECURITY_LOGON_TYPE)pSessionData->LogonType == Interactive
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == RemoteInteractive
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == CachedInteractive
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == CachedRemoteInteractive))
    {
        VBoxServiceVerbose(3, "VMInfo/Users: Session data: Name=%ls, Len=%d, SID=%s, LogonID=%ld,%ld\n",
                           pSessionData->UserName.Buffer,
                           pSessionData->UserName.Length,
                           pSessionData->Sid != NULL ? "1" : "0",
                           pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart);

        /*
         * Copy out relevant data.
         */
        VBoxServiceVMInfoWinSafeCopy(pUserInfo->wszUser, sizeof(pUserInfo->wszUser),
                                     &pSessionData->UserName, "User name");
        VBoxServiceVMInfoWinSafeCopy(pUserInfo->wszAuthenticationPackage, sizeof(pUserInfo->wszAuthenticationPackage),
                                     &pSessionData->AuthenticationPackage, "Authentication pkg name");
        VBoxServiceVMInfoWinSafeCopy(pUserInfo->wszLogonDomain, sizeof(pUserInfo->wszLogonDomain),
                                     &pSessionData->LogonDomain, "Logon domain name");

        TCHAR           szOwnerName[_MAX_PATH]  = { 0 };
        DWORD           dwOwnerNameSize         = sizeof(szOwnerName);
        TCHAR           szDomainName[_MAX_PATH] = { 0 };
        DWORD           dwDomainNameSize        = sizeof(szDomainName);
        SID_NAME_USE    enmOwnerType            = SidTypeInvalid;
        if (!LookupAccountSid(NULL,
                              pSessionData->Sid,
                              szOwnerName,
                              &dwOwnerNameSize,
                              szDomainName,
                              &dwDomainNameSize,
                              &enmOwnerType))
        {
            DWORD dwErr = GetLastError();
            /*
             * If a network time-out prevents the function from finding the name or
             * if a SID that does not have a corresponding account name (such as a
             * logon SID that identifies a logon session), we get ERROR_NONE_MAPPED
             * here that we just skip.
             */
            if (dwErr != ERROR_NONE_MAPPED)
                VBoxServiceError("VMInfo/Users: Failed looking up account info for user=%ls, error=$ld!\n",
                                 pUserInfo->wszUser, dwErr);
        }
        else
        {
            if (enmOwnerType == SidTypeUser) /* Only recognize users; we don't care about the rest! */
            {
                VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls, Session=%ld, LUID=%ld,%ld, AuthPkg=%ls, Domain=%ls\n",
                                   pUserInfo->wszUser, pSessionData->Session, pSessionData->LogonId.HighPart,
                                   pSessionData->LogonId.LowPart, pUserInfo->wszAuthenticationPackage,
                                   pUserInfo->wszLogonDomain);

                /* Detect RDP sessions as well. */
                LPTSTR  pBuffer = NULL;
                DWORD   cbRet   = 0;
                int     iState  = 0;
                if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                               pSessionData->Session,
                                               WTSConnectState,
                                               &pBuffer,
                                               &cbRet))
                {
                    if (cbRet)
                        iState = *pBuffer;
                    VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls, WTSConnectState=%d\n",
                                       pUserInfo->wszUser, iState);
                    if (    iState == WTSActive           /* User logged on to WinStation. */
                         || iState == WTSShadow           /* Shadowing another WinStation. */
                         || iState == WTSDisconnected)    /* WinStation logged on without client. */
                    {
                        /** @todo On Vista and W2K, always "old" user name are still
                         *        there. Filter out the old one! */
                        VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls using TCS/RDP, state=%d\n",
                                           pUserInfo->wszUser, iState);
                        fFoundUser = true;
                    }
                    if (pBuffer)
                        WTSFreeMemory(pBuffer);
                }
                else
                {
                    DWORD dwLastErr = GetLastError();
                    switch (dwLastErr)
                    {
                        /*
                         * Terminal services don't run (for example in W2K,
                         * nothing to worry about ...).  ... or is on the Vista
                         * fast user switching page!
                         */
                        case ERROR_CTX_WINSTATION_NOT_FOUND:
                            VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls, no WinSta found\n",
                                               pUserInfo->wszUser);
                            break;

                        default:
                            VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls, error=%ld\n",
                                               pUserInfo->wszUser, dwLastErr);
                            break;
                    }

                    fFoundUser = true;
                }
            }
        }

        VBoxServiceVerbose(3, "VMInfo/Users: Account User=%ls %s logged in\n",
                           pUserInfo->wszUser, fFoundUser ? "is" : "is not");
    }

    LsaFreeReturnBuffer(pSessionData);
    return fFoundUser;
}


/**
 * Retrieves the currently logged in users and stores their names along with the
 * user count.
 *
 * @returns VBox status code.
 * @param   ppszUserList    Where to store the user list (separated by commas).
 *                          Must be freed with RTStrFree().
 * @param   pcUsersInList   Where to store the number of users in the list.
 */
int VBoxServiceVMInfoWinWriteUsers(char **ppszUserList, uint32_t *pcUsersInList)
{
    PLUID       paSessions = NULL;
    ULONG       cSession = 0;

    /* This function can report stale or orphaned interactive logon sessions
       of already logged off users (especially in Windows 2000). */
    NTSTATUS rcNt = LsaEnumerateLogonSessions(&cSession, &paSessions);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG ulError = LsaNtStatusToWinError(rcNt);
        switch (ulError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                VBoxServiceVerbose(3, "VMInfo/Users: Not enough memory to enumerate logon sessions!\n");
                break;

            case ERROR_SHUTDOWN_IN_PROGRESS:
                /* If we're about to shutdown when we were in the middle of enumerating the logon
                 * sessions, skip the error to not confuse the user with an unnecessary log message. */
                VBoxServiceVerbose(3, "VMInfo/Users: Shutdown in progress ...\n");
                ulError = ERROR_SUCCESS;
                break;

            default:
                VBoxServiceError("VMInfo/Users: LsaEnumerate failed with error %ul\n", ulError);
                break;
        }

        return RTErrConvertFromWin32(ulError);
    }
    VBoxServiceVerbose(3, "VMInfo/Users: Found %ld sessions\n", cSession);

    PVBOXSERVICEVMINFOPROC  paProcs;
    DWORD                   cProcs;
    int rc = VBoxServiceVMInfoWinProcessesEnumerate(&paProcs, &cProcs);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NO_MEMORY)
            VBoxServiceVerbose(3, "VMInfo/Users: Not enough memory to enumerate processes for a session!\n");
        else
            VBoxServiceError("VMInfo/Users: Failed to enumerate processes for a session, rc=%Rrc\n", rc);
    }
    else
    {
        *pcUsersInList = 0;
        for (ULONG i = 0; i < cSession; i++)
        {
            VBOXSERVICEVMINFOUSER UserInfo;
            if (   VBoxServiceVMInfoWinIsLoggedIn(&UserInfo, &paSessions[i])
                && VBoxServiceVMInfoWinSessionHasProcesses(&paSessions[i], paProcs, cProcs))
            {
                if (*pcUsersInList > 0)
                {
                    rc = RTStrAAppend(ppszUserList, ",");
                    AssertRCBreakStmt(rc, RTStrFree(*ppszUserList));
                }

                *pcUsersInList += 1;

                char *pszTemp;
                int rc2 = RTUtf16ToUtf8(UserInfo.wszUser, &pszTemp);
                if (RT_SUCCESS(rc2))
                {
                    rc = RTStrAAppend(ppszUserList, pszTemp);
                    RTMemFree(pszTemp);
                }
                else
                    rc = RTStrAAppend(ppszUserList, "<string-conversion-error>");
                AssertRCBreakStmt(rc, RTStrFree(*ppszUserList));
            }
        }
        VBoxServiceVMInfoWinProcessesFree(paProcs);
    }
    LsaFreeReturnBuffer(paSessions);
    return rc;
}

#endif /* TARGET_NT4 */

int VBoxServiceWinGetComponentVersions(uint32_t uClientID)
{
    int rc;
    char szSysDir[_MAX_PATH] = {0};
    char szWinDir[_MAX_PATH] = {0};
    char szDriversDir[_MAX_PATH + 32] = {0};

    /* ASSUME: szSysDir and szWinDir and derivatives are always ASCII compatible. */
    GetSystemDirectory(szSysDir, _MAX_PATH);
    GetWindowsDirectory(szWinDir, _MAX_PATH);
    RTStrPrintf(szDriversDir, sizeof(szDriversDir), "%s\\drivers", szSysDir);
#ifdef RT_ARCH_AMD64
    char szSysWowDir[_MAX_PATH + 32] = {0};
    RTStrPrintf(szSysWowDir, sizeof(szSysWowDir), "%s\\SysWow64", szWinDir);
#endif

    /* The file information table. */
#ifndef TARGET_NT4
    const VBOXSERVICEVMINFOFILE aVBoxFiles[] =
    {
        { szSysDir, "VBoxControl.exe" },
        { szSysDir, "VBoxHook.dll" },
        { szSysDir, "VBoxDisp.dll" },
        { szSysDir, "VBoxMRXNP.dll" },
        { szSysDir, "VBoxService.exe" },
        { szSysDir, "VBoxTray.exe" },
        { szSysDir, "VBoxGINA.dll" },
        { szSysDir, "VBoxCredProv.dll" },

 /* On 64-bit we don't yet have the OpenGL DLLs in native format.
    So just enumerate the 32-bit files in the SYSWOW directory. */
# ifdef RT_ARCH_AMD64
        { szSysWowDir, "VBoxOGLarrayspu.dll" },
        { szSysWowDir, "VBoxOGLcrutil.dll" },
        { szSysWowDir, "VBoxOGLerrorspu.dll" },
        { szSysWowDir, "VBoxOGLpackspu.dll" },
        { szSysWowDir, "VBoxOGLpassthroughspu.dll" },
        { szSysWowDir, "VBoxOGLfeedbackspu.dll" },
        { szSysWowDir, "VBoxOGL.dll" },
# else  /* !RT_ARCH_AMD64 */
        { szSysDir, "VBoxOGLarrayspu.dll" },
        { szSysDir, "VBoxOGLcrutil.dll" },
        { szSysDir, "VBoxOGLerrorspu.dll" },
        { szSysDir, "VBoxOGLpackspu.dll" },
        { szSysDir, "VBoxOGLpassthroughspu.dll" },
        { szSysDir, "VBoxOGLfeedbackspu.dll" },
        { szSysDir, "VBoxOGL.dll" },
# endif /* !RT_ARCH_AMD64 */

        { szDriversDir, "VBoxGuest.sys" },
        { szDriversDir, "VBoxMouse.sys" },
        { szDriversDir, "VBoxSF.sys"    },
        { szDriversDir, "VBoxVideo.sys" },
    };

#else  /* TARGET_NT4 */
    const VBOXSERVICEVMINFOFILE aVBoxFiles[] =
    {
        { szSysDir, "VBoxControl.exe" },
        { szSysDir, "VBoxHook.dll" },
        { szSysDir, "VBoxDisp.dll" },
        { szSysDir, "VBoxServiceNT.exe" },
        { szSysDir, "VBoxTray.exe" },

        { szDriversDir, "VBoxGuestNT.sys" },
        { szDriversDir, "VBoxMouseNT.sys" },
        { szDriversDir, "VBoxVideo.sys" },
    };
#endif /* TARGET_NT4 */

    for (unsigned i = 0; i < RT_ELEMENTS(aVBoxFiles); i++)
    {
        char szVer[128];
        VBoxServiceGetFileVersionString(aVBoxFiles[i].pszFilePath, aVBoxFiles[i].pszFileName, szVer, sizeof(szVer));
        char szPropPath[256];
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestAdd/Components/%s", aVBoxFiles[i].pszFileName);
        rc = VBoxServiceWritePropF(uClientID, szPropPath, "%s", szVer);
    }

    return VINF_SUCCESS;
}

