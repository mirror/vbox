/* $Id$ */
/** @file
 * VBoxService - Virtual Machine Information for the Host, Windows specifics.
 */

/*
 * Copyright (C) 2009-2013 Oracle Corporation
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
#include <iprt/ldr.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

static uint32_t s_uGuestPropClientID = 0;
static uint32_t s_uIter = 0;

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Structure for storing the looked up user information. */
typedef struct
{
    WCHAR wszUser[_MAX_PATH];
    WCHAR wszAuthenticationPackage[_MAX_PATH];
    WCHAR wszLogonDomain[_MAX_PATH];
    /** Number of assigned user processes. */
    ULONG ulNumProcs;
    /** Last (highest) session ID. This
     *  is needed for distinguishing old session
     *  process counts from new (current) session
     *  ones. */
    ULONG ulLastSession;
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
    /** The PID. */
    DWORD id;
    /** The SID. */
    PSID pSid;
    /** The LUID. */
    LUID luid;
    /** Interactive process. */
    bool fInteractive;
} VBOXSERVICEVMINFOPROC, *PVBOXSERVICEVMINFOPROC;


/*******************************************************************************
*   Prototypes
*******************************************************************************/
uint32_t VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession, PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs);
bool VBoxServiceVMInfoWinIsLoggedIn(PVBOXSERVICEVMINFOUSER a_pUserInfo, PLUID a_pSession);
int  VBoxServiceVMInfoWinProcessesEnumerate(PVBOXSERVICEVMINFOPROC *ppProc, DWORD *pdwCount);
void VBoxServiceVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs);

typedef BOOL WINAPI FNQUERYFULLPROCESSIMAGENAME(HANDLE,  DWORD, LPTSTR, PDWORD);
typedef FNQUERYFULLPROCESSIMAGENAME *PFNQUERYFULLPROCESSIMAGENAME;


#ifndef TARGET_NT4

static bool vboxServiceVMInfoSession0Separation(void)
{
    /** @todo Only do this once. Later. */
    OSVERSIONINFOEX OSInfoEx;
    RT_ZERO(OSInfoEx);
    OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (   !GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
        || OSInfoEx.dwPlatformId != VER_PLATFORM_WIN32_NT)
    {
        /* Platform other than NT (e.g. Win9x) not supported. */
        return false;
    }

    if (   OSInfoEx.dwMajorVersion >= 6
        && OSInfoEx.dwMinorVersion >= 0)
    {
        return true;
    }

    return false;
}

/**
 * Retrieves the module name of a given process.
 *
 * @return  IPRT status code.
 * @param   pProc
 * @param   pszBuf
 * @param   cbBuf
 */
static int VBoxServiceVMInfoWinProcessesGetModuleName(PVBOXSERVICEVMINFOPROC const pProc,
                                                      TCHAR *pszName, size_t cbName)
{
    AssertPtrReturn(pProc, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    AssertReturn(cbName, VERR_INVALID_PARAMETER);

    OSVERSIONINFOEX OSInfoEx;
    RT_ZERO(OSInfoEx);
    OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (   !GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
        || OSInfoEx.dwPlatformId != VER_PLATFORM_WIN32_NT)
    {
        /* Platform other than NT (e.g. Win9x) not supported. */
        return VERR_NOT_SUPPORTED;
    }

    int rc = VINF_SUCCESS;

    DWORD dwFlags = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ;
    if (OSInfoEx.dwMajorVersion >= 6 /* Vista or later */)
        dwFlags = 0x1000; /* = PROCESS_QUERY_LIMITED_INFORMATION; less privileges needed. */

    HANDLE h = OpenProcess(dwFlags, FALSE, pProc->id);
    if (h == NULL)
    {
        DWORD dwErr = GetLastError();
        if (g_cVerbosity)
            VBoxServiceError("Unable to open process with PID=%ld, error=%ld\n",
                             pProc->id, dwErr);
        rc = RTErrConvertFromWin32(dwErr);
    }
    else
    {
        /* Since GetModuleFileNameEx has trouble with cross-bitness stuff (32-bit apps cannot query 64-bit
           apps and vice verse) we have to use a different code path for Vista and up. */

        /* Note: For 2000 + NT4 we might just use GetModuleFileName() instead. */
        if (OSInfoEx.dwMajorVersion >= 6 /* Vista or later */)
        {
            /* Loading the module and getting the symbol for each and every process is expensive
             * -- since this function (at the moment) only is used for debugging purposes it's okay. */
            RTLDRMOD hMod;
            rc = RTLdrLoad("kernel32.dll", &hMod);
            if (RT_SUCCESS(rc))
            {
                PFNQUERYFULLPROCESSIMAGENAME pfnQueryFullProcessImageName;
                rc = RTLdrGetSymbol(hMod, "QueryFullProcessImageNameA", (void **)&pfnQueryFullProcessImageName);
                if (RT_SUCCESS(rc))
                {
                    DWORD dwLen = cbName / sizeof(TCHAR);
                    if (!pfnQueryFullProcessImageName(h, 0 /*PROCESS_NAME_NATIVE*/, pszName, &dwLen))
                        rc = VERR_ACCESS_DENIED;
                }

                RTLdrClose(hMod);
            }
        }
        else
        {
            if (!GetModuleFileNameEx(h, NULL /* Get main executable */, pszName, cbName / sizeof(TCHAR)))
                rc = VERR_ACCESS_DENIED;
        }

        CloseHandle(h);
    }

    return rc;
}


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
    AssertPtrReturn(pProc, VERR_INVALID_POINTER);

    DWORD  dwErr = ERROR_SUCCESS;
    HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pProc->id);
    if (h == NULL)
    {
        dwErr = GetLastError();
        if (g_cVerbosity > 4)
            VBoxServiceError("Unable to open process with PID=%ld, error=%ld\n",
                             pProc->id, dwErr);
        return RTErrConvertFromWin32(dwErr);
    }

    int    rc = VINF_SUCCESS;
    HANDLE hToken;
    if (OpenProcessToken(h, TOKEN_QUERY, &hToken))
    {
        void *pvTokenInfo = NULL;
        DWORD dwTokenInfoSize;
        switch (tkClass)
        {
            case TokenStatistics:
                dwTokenInfoSize = sizeof(TOKEN_STATISTICS);
                pvTokenInfo = HeapAlloc(GetProcessHeap(),
                                        HEAP_ZERO_MEMORY, dwTokenInfoSize);
                AssertPtr(pvTokenInfo);
                break;

            case TokenGroups:
                dwTokenInfoSize = 0;
                /* Allocation will follow in a second step. */
                break;

            case TokenUser:
                dwTokenInfoSize = 0;
                /* Allocation will follow in a second step. */
                break;

            default:
                VBoxServiceError("Token class not implemented: %ld", tkClass);
                rc = VERR_NOT_IMPLEMENTED;
                break;
        }

        if (RT_SUCCESS(rc))
        {
            DWORD dwRetLength;
            if (!GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
            {
                dwErr = GetLastError();
                if (dwErr == ERROR_INSUFFICIENT_BUFFER)
                {
                    dwErr = ERROR_SUCCESS;

                    switch (tkClass)
                    {
                        case TokenGroups:
                            pvTokenInfo = (PTOKEN_GROUPS)HeapAlloc(GetProcessHeap(),
                                                                   HEAP_ZERO_MEMORY, dwRetLength);
                            if (!pvTokenInfo)
                                dwErr = GetLastError();
                            dwTokenInfoSize = dwRetLength;
                            break;

                        case TokenUser:
                            pvTokenInfo = (PTOKEN_USER)HeapAlloc(GetProcessHeap(),
                                                                 HEAP_ZERO_MEMORY, dwRetLength);
                            if (!pvTokenInfo)
                                dwErr = GetLastError();
                            dwTokenInfoSize = dwRetLength;
                            break;

                        default:
                            AssertMsgFailed(("Re-allocating of token information for token class not implemented\n"));
                            break;
                    }

                    if (dwErr == ERROR_SUCCESS)
                    {
                        if (!GetTokenInformation(hToken, tkClass, pvTokenInfo, dwTokenInfoSize, &dwRetLength))
                            dwErr = GetLastError();
                    }
                }
            }

            if (dwErr == ERROR_SUCCESS)
            {
                rc = VINF_SUCCESS;

                switch (tkClass)
                {
                    case TokenStatistics:
                    {
                        PTOKEN_STATISTICS pStats = (PTOKEN_STATISTICS)pvTokenInfo;
                        AssertPtr(pStats);
                        memcpy(&pProc->luid, &pStats->AuthenticationId, sizeof(LUID));
                        /** @todo Add more information of TOKEN_STATISTICS as needed. */
                        break;
                    }

                    case TokenGroups:
                    {
                        pProc->fInteractive = false;

                        SID_IDENTIFIER_AUTHORITY sidAuthNT = SECURITY_NT_AUTHORITY;
                        PSID pSidInteractive = NULL; /*  S-1-5-4 */
                        if (!AllocateAndInitializeSid(&sidAuthNT, 1, 4, 0, 0, 0, 0, 0, 0, 0, &pSidInteractive))
                            dwErr = GetLastError();

                        PSID pSidLocal = NULL; /*  S-1-2-0 */
                        if (dwErr == ERROR_SUCCESS)
                        {
                            SID_IDENTIFIER_AUTHORITY sidAuthLocal = SECURITY_LOCAL_SID_AUTHORITY;
                            if (!AllocateAndInitializeSid(&sidAuthLocal, 1, 0, 0, 0, 0, 0, 0, 0, 0, &pSidLocal))
                                dwErr = GetLastError();
                        }

                        if (dwErr == ERROR_SUCCESS)
                        {
                            PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)pvTokenInfo;
                            AssertPtr(pGroups);
                            for (DWORD i = 0; i < pGroups->GroupCount; i++)
                            {
                                if (   EqualSid(pGroups->Groups[i].Sid, pSidInteractive)
                                    || EqualSid(pGroups->Groups[i].Sid, pSidLocal)
                                    || pGroups->Groups[i].Attributes & SE_GROUP_LOGON_ID)
                                {
                                    pProc->fInteractive = true;
                                    break;
                                }
                            }
                        }

                        if (pSidInteractive)
                            FreeSid(pSidInteractive);
                        if (pSidLocal)
                            FreeSid(pSidLocal);
                        break;
                    }

                    case TokenUser:
                    {
                        PTOKEN_USER pUser = (PTOKEN_USER)pvTokenInfo;
                        AssertPtr(pUser);

                        DWORD dwLength = GetLengthSid(pUser->User.Sid);
                        Assert(dwLength);
                        if (dwLength)
                        {
                            pProc->pSid = (PSID)RTMemAlloc(dwLength);
                            AssertPtr(pProc->pSid);
                            if (CopySid(dwLength, pProc->pSid, pUser->User.Sid))
                            {
                                if (!IsValidSid(pProc->pSid))
                                    dwErr = ERROR_INVALID_NAME;
                            }
                            else
                                dwErr = GetLastError();
                        }
                        else
                            dwErr = ERROR_NO_DATA;

                        if (dwErr != ERROR_SUCCESS)
                            VBoxServiceError("Error retrieving SID of process PID=%ld: %ld\n",
                                             pProc->id, dwErr);
                        break;
                    }

                    default:
                        AssertMsgFailed(("Unhandled token information class\n"));
                        break;
                }
            }

            if (pvTokenInfo)
                HeapFree(GetProcessHeap(), 0 /* Flags */, pvTokenInfo);
        }
        CloseHandle(hToken);
    }
    else
        dwErr = GetLastError();

    if (dwErr != ERROR_SUCCESS)
    {
        if (g_cVerbosity)
            VBoxServiceError("Unable to query token information for PID=%ld, error=%ld\n",
                             pProc->id, dwErr);
        rc = RTErrConvertFromWin32(dwErr);
    }

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
    PDWORD  paPID       = NULL;
    int     rc          = VINF_SUCCESS;
    do
    {
        /* Allocate / grow the buffer first. */
        cProcesses *= 2;
        void *pvNew = RTMemRealloc(paPID, cProcesses * sizeof(DWORD));
        if (!pvNew)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        paPID = (PDWORD)pvNew;

        /* Query the processes. Not the cbRet == buffer size means there could be more work to be done. */
        DWORD cbRet;
        if (!EnumProcesses(paPID, cProcesses * sizeof(DWORD), &cbRet))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }
        if (cbRet < cProcesses * sizeof(DWORD))
        {
            cProcesses = cbRet / sizeof(DWORD);
            break;
        }
    } while (cProcesses <= _32K); /* Should be enough; see: http://blogs.technet.com/markrussinovich/archive/2009/07/08/3261309.aspx */
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
                paProcs[i].id = paPID[i];
                paProcs[i].pSid = NULL;

                int rc2 = VBoxServiceVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenUser);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VBoxServiceError("Get token class \"user\" for process %ld failed, rc=%Rrc\n",
                                     paProcs[i].id, rc2);

                rc2 = VBoxServiceVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenGroups);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VBoxServiceError("Get token class \"groups\" for process %ld failed, rc=%Rrc\n",
                                     paProcs[i].id, rc2);

                rc2 = VBoxServiceVMInfoWinProcessesGetTokenInfo(&paProcs[i], TokenStatistics);
                if (RT_FAILURE(rc2) && g_cVerbosity)
                    VBoxServiceError("Get token class \"statistics\" for process %ld failed, rc=%Rrc\n",
                                     paProcs[i].id, rc2);
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

    RTMemFree(paPID);
    return rc;
}

/**
 * Frees the process structures returned by
 * VBoxServiceVMInfoWinProcessesEnumerate() before.
 *
 * @param   paProcs     What
 */
void VBoxServiceVMInfoWinProcessesFree(DWORD cProcs, PVBOXSERVICEVMINFOPROC paProcs)
{
    for (DWORD i = 0; i < cProcs; i++)
    {
        if (paProcs[i].pSid)
            RTMemFree(paProcs[i].pSid);
    }
    RTMemFree(paProcs);
}

/**
 * Determines whether the specified session has processes on the system.
 *
 * @returns Number of processes found for a specified session.
 * @param   pSession        The current user's SID.
 * @param   paProcs         The process snapshot.
 * @param   cProcs          The number of processes in the snaphot.
 * @param   puSession       Looked up session number.  Optional.
 */
uint32_t VBoxServiceVMInfoWinSessionHasProcesses(PLUID pSession,
                                                 PVBOXSERVICEVMINFOPROC const paProcs, DWORD cProcs,
                                                 PULONG puTerminalSession)
{
    if (!pSession)
    {
        VBoxServiceVerbose(1, "Session became invalid while enumerating!\n");
        return 0;
    }

    PSECURITY_LOGON_SESSION_DATA pSessionData = NULL;
    NTSTATUS rcNt = LsaGetLogonSessionData(pSession, &pSessionData);
    if (rcNt != STATUS_SUCCESS)
    {
        VBoxServiceError("Could not get logon session data! rcNt=%#x\n", rcNt);
        return 0;
    }

    if (!IsValidSid(pSessionData->Sid))
    {
       VBoxServiceError("User SID=%p is not valid\n", pSessionData->Sid);
       return 0;
    }

    int rc = VINF_SUCCESS;

    /*
     * Even if a user seems to be logged in, it could be a stale/orphaned logon
     * session. So check if we have some processes bound to it by comparing the
     * session <-> process LUIDs.
     */
    uint32_t cNumProcs = 0;
    for (DWORD i = 0; i < cProcs; i++)
    {
        if (g_cVerbosity)
        {
            TCHAR szModule[_1K];
            rc = VBoxServiceVMInfoWinProcessesGetModuleName(&paProcs[i], szModule, sizeof(szModule));
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(4, "PID=%ld: %s\n",
                                   paProcs[i].id, szModule);
        }

        PSID pProcSID = paProcs[i].pSid;
        if (   RT_SUCCESS(rc)
            && pProcSID
            && IsValidSid(pProcSID))
        {
            if (   EqualSid(pSessionData->Sid, paProcs[i].pSid)
                && paProcs[i].fInteractive)
            {
                cNumProcs++;
                if (!g_cVerbosity) /* We want a bit more info on higher verbosity. */
                    break;
            }
        }
    }

    if (puTerminalSession)
        *puTerminalSession = pSessionData->Session;

    LsaFreeReturnBuffer(pSessionData);

    return cNumProcs;
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
    AssertPtrReturn(pUserInfo, false);
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
                VBoxServiceVerbose(3, "Not enough memory to retrieve logon session data!\n");
                break;

            case ERROR_NO_SUCH_LOGON_SESSION:
                /* Skip session data which is not valid anymore because it may have been
                 * already terminated. */
                break;

            default:
                VBoxServiceError("LsaGetLogonSessionData failed with error %u\n", ulError);
                break;
        }
        if (pSessionData)
            LsaFreeReturnBuffer(pSessionData);
        return false;
    }
    if (!pSessionData)
    {
        VBoxServiceError("Invalid logon session data!\n");
        return false;
    }

    VBoxServiceVerbose(3, "Session data: Name=%ls, SessionID=%RU32, LogonID=%ld,%ld, LogonType=%ld\n",
                       pSessionData->UserName.Buffer,
                       pSessionData->Session,
                       pSessionData->LogonId.HighPart, pSessionData->LogonId.LowPart,
                       pSessionData->LogonType);

    if (vboxServiceVMInfoSession0Separation())
    {
        /* Starting at Windows Vista user sessions begin with session 1, so
         * ignore (stale) session 0 users. */
        if (   pSessionData->Session == 0
        /* Also check the logon time. */
            || pSessionData->LogonTime.QuadPart == 0)
        {
            LsaFreeReturnBuffer(pSessionData);
            return false;
        }
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
            /* Note: We also need CachedInteractive in case Windows cached the credentials
             *       or just wants to reuse them! */
            || (SECURITY_LOGON_TYPE)pSessionData->LogonType == CachedInteractive))
    {
        VBoxServiceVerbose(3, "Session LogonType=%ld is supported -- looking up SID + type ...\n",
                           pSessionData->LogonType);

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
                VBoxServiceError("Failed looking up account info for user=%ls, error=$ld!\n",
                                 pUserInfo->wszUser, dwErr);
        }
        else
        {
            if (enmOwnerType == SidTypeUser) /* Only recognize users; we don't care about the rest! */
            {
                VBoxServiceVerbose(3, "Account User=%ls, Session=%ld, LogonID=%ld,%ld, AuthPkg=%ls, Domain=%ls\n",
                                   pUserInfo->wszUser, pSessionData->Session, pSessionData->LogonId.HighPart,
                                   pSessionData->LogonId.LowPart, pUserInfo->wszAuthenticationPackage,
                                   pUserInfo->wszLogonDomain);

                /* Detect RDP sessions as well. */
                LPTSTR  pBuffer = NULL;
                DWORD   cbRet   = 0;
                int     iState  = -1;
                if (WTSQuerySessionInformation(WTS_CURRENT_SERVER_HANDLE,
                                               pSessionData->Session,
                                               WTSConnectState,
                                               &pBuffer,
                                               &cbRet))
                {
                    if (cbRet)
                        iState = *pBuffer;
                    VBoxServiceVerbose(3, "Account User=%ls, WTSConnectState=%d (%ld)\n",
                                       pUserInfo->wszUser, iState, cbRet);
                    if (    iState == WTSActive           /* User logged on to WinStation. */
                         || iState == WTSShadow           /* Shadowing another WinStation. */
                         || iState == WTSDisconnected)    /* WinStation logged on without client. */
                    {
                        /** @todo On Vista and W2K, always "old" user name are still
                         *        there. Filter out the old one! */
                        VBoxServiceVerbose(3, "Account User=%ls using TCS/RDP, state=%d \n",
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
                            VBoxServiceVerbose(3, "No WinStation found for user=%ls\n",
                                               pUserInfo->wszUser);
                            break;

                        default:
                            VBoxServiceVerbose(3, "Cannot query WTS connection state for user=%ls, error=%ld\n",
                                               pUserInfo->wszUser, dwLastErr);
                            break;
                    }

                    fFoundUser = true;
                }
            }
            else
                VBoxServiceVerbose(3, "SID owner type=%d not handled, skipping\n",
                                   enmOwnerType);
        }

        VBoxServiceVerbose(3, "Account User=%ls %s logged in\n",
                           pUserInfo->wszUser, fFoundUser ? "is" : "is not");
    }

    if (fFoundUser)
        pUserInfo->ulLastSession = pSessionData->Session;

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
    AssertPtrReturn(ppszUserList, VERR_INVALID_POINTER);
    AssertPtrReturn(pcUsersInList, VERR_INVALID_POINTER);

    PLUID       paSessions = NULL;
    ULONG       cSessions = 0;

    int rc2 = VbglR3GuestPropConnect(&s_uGuestPropClientID);
    AssertRC(rc2);

    /* This function can report stale or orphaned interactive logon sessions
       of already logged off users (especially in Windows 2000). */
    NTSTATUS rcNt = LsaEnumerateLogonSessions(&cSessions, &paSessions);
    if (rcNt != STATUS_SUCCESS)
    {
        ULONG ulError = LsaNtStatusToWinError(rcNt);
        switch (ulError)
        {
            case ERROR_NOT_ENOUGH_MEMORY:
                VBoxServiceError("Not enough memory to enumerate logon sessions!\n");
                break;

            case ERROR_SHUTDOWN_IN_PROGRESS:
                /* If we're about to shutdown when we were in the middle of enumerating the logon
                 * sessions, skip the error to not confuse the user with an unnecessary log message. */
                VBoxServiceVerbose(3, "Shutdown in progress ...\n");
                ulError = ERROR_SUCCESS;
                break;

            default:
                VBoxServiceError("LsaEnumerate failed with error %u\n", ulError);
                break;
        }

        return RTErrConvertFromWin32(ulError);
    }
    VBoxServiceVerbose(3, "Found %ld sessions\n", cSessions);

    PVBOXSERVICEVMINFOPROC  paProcs;
    DWORD                   cProcs;
    int rc = VBoxServiceVMInfoWinProcessesEnumerate(&paProcs, &cProcs);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NO_MEMORY)
            VBoxServiceError("Not enough memory to enumerate processes\n");
        else
            VBoxServiceError("Failed to enumerate processes, rc=%Rrc\n", rc);
    }
    else
    {
        PVBOXSERVICEVMINFOUSER pUserInfo;
        pUserInfo = (PVBOXSERVICEVMINFOUSER)RTMemAllocZ(cSessions * sizeof(VBOXSERVICEVMINFOUSER) + 1);
        if (!pUserInfo)
            VBoxServiceError("Not enough memory to store enumerated users!\n");
        else
        {
            ULONG cUniqueUsers = 0;

            /**
             * Note: The cSessions loop variable does *not* correlate with
             *       the Windows session ID!
             */
            for (ULONG i = 0; i < cSessions; i++)
            {
                VBoxServiceVerbose(3, "Handling session %RU32 (of %RU32)\n", i + 1, cSessions);

                VBOXSERVICEVMINFOUSER userSession;
                if (VBoxServiceVMInfoWinIsLoggedIn(&userSession, &paSessions[i]))
                {
                    VBoxServiceVerbose(4, "Handling user=%ls, domain=%ls, package=%ls, session=%RU32\n",
                                       userSession.wszUser, userSession.wszLogonDomain, userSession.wszAuthenticationPackage,
                                       userSession.ulLastSession);

                    /* Retrieve assigned processes of current session. */
                    uint32_t cCurSessionProcs = VBoxServiceVMInfoWinSessionHasProcesses(&paSessions[i], paProcs, cProcs,
                                                                                        NULL /* Terminal session ID */);
                    /* Don't return here when current session does not have assigned processes
                     * anymore -- in that case we have to search through the unique users list below
                     * and see if got a stale user/session entry. */

                    if (g_cVerbosity > 3)
                    {
                        char szDebugSessionPath[255]; RTStrPrintf(szDebugSessionPath,  sizeof(szDebugSessionPath), "/VirtualBox/GuestInfo/Debug/LSA/Session/%RU32",
                                                                  userSession.ulLastSession);
                        VBoxServiceWritePropF(s_uGuestPropClientID, szDebugSessionPath,
                                              "#%RU32: cSessionProcs=%RU32 (of %RU32 procs total)", s_uIter, cCurSessionProcs, cProcs);
                    }

                    bool fFoundUser = false;
                    for (ULONG a = 0; a < cUniqueUsers; a++)
                    {
                        PVBOXSERVICEVMINFOUSER pCurUser = &pUserInfo[a];
                        AssertPtr(pCurUser);

                        if (   !wcscmp(userSession.wszUser, pCurUser->wszUser)
                            && !wcscmp(userSession.wszLogonDomain, pCurUser->wszLogonDomain)
                            && !wcscmp(userSession.wszAuthenticationPackage, pCurUser->wszAuthenticationPackage))
                        {
                            /*
                             * Only respect the highest session for the current user.
                             */
                            if (userSession.ulLastSession > pCurUser->ulLastSession)
                            {
                                VBoxServiceVerbose(4, "Updating user=%ls to %u processes (last used session: %RU32)\n",
                                                   pCurUser->wszUser, cCurSessionProcs, userSession.ulLastSession);

                                if (!cCurSessionProcs)
                                    VBoxServiceVerbose(3, "Stale session for user=%ls detected! Processes: %RU32 -> %RU32, Session: %RU32 -> %RU32\n",
                                                       pCurUser->wszUser,
                                                       pCurUser->ulNumProcs, cCurSessionProcs,
                                                       pCurUser->ulLastSession, userSession.ulLastSession);

                                pCurUser->ulNumProcs = cCurSessionProcs;
                                pCurUser->ulLastSession  = userSession.ulLastSession;
                            }
                            /* There can be multiple session objects using the same session ID for the
                             * current user -- so when we got the same session again just add the found
                             * processes to it. */
                            else if (pCurUser->ulLastSession == userSession.ulLastSession)
                            {
                                VBoxServiceVerbose(4, "Updating processes for user=%ls (old procs=%RU32, new procs=%RU32, session=%RU32)\n",
                                                   pCurUser->wszUser, pCurUser->ulNumProcs, cCurSessionProcs, pCurUser->ulLastSession);

                                pCurUser->ulNumProcs = cCurSessionProcs;
                            }

                            fFoundUser = true;
                            break;
                        }
                    }

                    if (!fFoundUser)
                    {
                        VBoxServiceVerbose(4, "Adding new user=%ls (session=%RU32) with %RU32 processes\n",
                                           userSession.wszUser, userSession.ulLastSession, cCurSessionProcs);

                        memcpy(&pUserInfo[cUniqueUsers], &userSession, sizeof(VBOXSERVICEVMINFOUSER));
                        pUserInfo[cUniqueUsers].ulNumProcs = cCurSessionProcs;
                        cUniqueUsers++;
                        Assert(cUniqueUsers <= cSessions);
                    }
                }
            }

            if (g_cVerbosity > 3)
                VBoxServiceWritePropF(s_uGuestPropClientID, "/VirtualBox/GuestInfo/Debug/LSA",
                                      "#%RU32: cSessions=%RU32, cProcs=%RU32, cUniqueUsers=%RU32",
                                      s_uIter, cSessions, cProcs, cUniqueUsers);

            VBoxServiceVerbose(3, "Found %u unique logged-in user(s)\n",
                               cUniqueUsers);

            *pcUsersInList = 0;
            for (ULONG i = 0; i < cUniqueUsers; i++)
            {
                if (g_cVerbosity > 3)
                {
                    char szDebugUserPath[255]; RTStrPrintf(szDebugUserPath,  sizeof(szDebugUserPath), "/VirtualBox/GuestInfo/Debug/LSA/User/%RU32", i);
                    VBoxServiceWritePropF(s_uGuestPropClientID, szDebugUserPath,
                                          "#%RU32: szName=%ls, sessionID=%RU32, cProcs=%RU32",
                                          s_uIter, pUserInfo[i].wszUser, pUserInfo[i].ulLastSession, pUserInfo[i].ulNumProcs);
                }

                bool fAddUser = false;
                if (pUserInfo[i].ulNumProcs)
                    fAddUser = true;

                if (fAddUser)
                {
                    VBoxServiceVerbose(3, "User \"%ls\" has %RU32 interactive processes (session=%RU32)\n",
                                       pUserInfo[i].wszUser, pUserInfo[i].ulNumProcs, pUserInfo[i].ulLastSession);

                    if (*pcUsersInList > 0)
                    {
                        rc = RTStrAAppend(ppszUserList, ",");
                        AssertRCBreakStmt(rc, RTStrFree(*ppszUserList));
                    }

                    *pcUsersInList += 1;

                    char *pszTemp;
                    int rc2 = RTUtf16ToUtf8(pUserInfo[i].wszUser, &pszTemp);
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

            RTMemFree(pUserInfo);
        }
        VBoxServiceVMInfoWinProcessesFree(cProcs, paProcs);
    }
    LsaFreeReturnBuffer(paSessions);

    s_uIter++;
    VbglR3GuestPropDisconnect(s_uGuestPropClientID);

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
# ifdef VBOX_WITH_MMR
        { szSysDir, "VBoxMMR.exe" },
# endif /* VBOX_WITH_MMR */

 /* On 64-bit we don't yet have the OpenGL DLLs in native format.
    So just enumerate the 32-bit files in the SYSWOW directory. */
# ifdef RT_ARCH_AMD64
#  ifdef VBOX_WITH_MMR
        { szSysWowDir, "VBoxMMRHook.dll" },
#  endif /* VBOX_WITH_MMR */
        { szSysWowDir, "VBoxOGLarrayspu.dll" },
        { szSysWowDir, "VBoxOGLcrutil.dll" },
        { szSysWowDir, "VBoxOGLerrorspu.dll" },
        { szSysWowDir, "VBoxOGLpackspu.dll" },
        { szSysWowDir, "VBoxOGLpassthroughspu.dll" },
        { szSysWowDir, "VBoxOGLfeedbackspu.dll" },
        { szSysWowDir, "VBoxOGL.dll" },
# else  /* !RT_ARCH_AMD64 */
#  ifdef VBOX_WITH_MMR
        { szSysDir, "VBoxMMRHook.dll" },
#  endif /* VBOX_WITH_MMR */
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

