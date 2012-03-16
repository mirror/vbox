/* $Id$ */
/** @file
 * VBoxLA - VBox Location Awareness notifications.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

// #define LOG_ENABLED

#define _WIN32_WINNT 0x0501
#include <windows.h>

#include "VBoxTray.h"
#include "VBoxLA.h"

#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/list.h>

#define LALOG Log

#define REG_KEY_LEN 1024
#define MAX_CLIENT_NAME_CHARS 1024

#define LA_DO_NOTHING           0
#define LA_DO_ATTACH            1
#define LA_DO_DETACH            2
#define LA_DO_DETACH_AND_ATTACH 3
#define LA_DO_ATTACH_AND_DETACH 4

struct VBOXLACONTEXT
{
    const VBOXSERVICEENV *pEnv;

    uint32_t u32GuestPropHandle;  /* The client identifier of the guest property system. */

    RTLISTANCHOR listAttachActions;
    RTLISTANCHOR listDetachActions;

    uint64_t u64LastQuery;        /* The timestamp of the last query of the properties. */

    uint32_t u32Action;           /* Which action to do: LA_DO_*. */
    uint32_t u32PrevAction;       /* Which action were done last time. */

    struct                        /* Information about the client, which properties are monitored. */
    {
        uint32_t u32ClientId;     /* The RDP client identifier. 0 if none. */

        uint32_t u32LastAttach;
        uint64_t u64LastAttachTimestamp;

        char *pszLastName;
        uint64_t u64LastNameTimestamp;

        char *pszPropName;        /* The actual Client/%ID%/Name property name with client id. */
        char *pszPropAttach;      /* The actual Client/%ID%/Attach property name with client id. */

        char *pszPropWaitPattern; /* Which properties are monitored. */
    } activeClient;

    HMODULE hModuleKernel32;

    BOOL (WINAPI * pfnProcessIdToSessionId)(DWORD dwProcessId, DWORD *pSessionId);
};

typedef struct ACTIONENTRY
{
    RTLISTNODE nodeActionEntry;
    uint32_t u32Index;
    WCHAR wszCommandLine[1];
} ACTIONENTRY;


static VBOXLACONTEXT gCtx = {0};

static const char *g_pszPropActiveClient = "/VirtualBox/HostInfo/VRDP/ActiveClient";

static const char *g_pszPropNameTemplate = "/VirtualBox/HostInfo/VRDP/Client/%u/Name";
static const char *g_pszPropAttachTemplate = "/VirtualBox/HostInfo/VRDP/Client/%u/Attach";

static const char *g_pszVolatileEnvironment = "Volatile Environment";

static const WCHAR *g_pwszUTCINFOClientName = L"UTCINFO_CLIENTNAME";
static const WCHAR *g_pwszClientName = L"CLIENTNAME";

const WCHAR *g_pwszRegKeyDisconnectActions = L"Software\\Oracle\\Sun Ray\\ClientInfoAgent\\DisconnectActions";
const WCHAR *g_pwszRegKeyReconnectActions = L"Software\\Oracle\\Sun Ray\\ClientInfoAgent\\ReconnectActions";

const char g_szCommandPrefix[] = "Command";

static void ActionExecutorDeleteActions(RTLISTANCHOR *listActions)
{
    ACTIONENTRY *pIter = NULL;
    ACTIONENTRY *pIterNext = NULL;
    RTListForEachSafe(listActions, pIter, pIterNext, ACTIONENTRY, nodeActionEntry)
    {
        RTListNodeRemove(&pIter->nodeActionEntry);
        RTMemFree(pIter);
    }
}

static BOOL ActionExecutorEnumerateRegistryKey(const WCHAR *pwszRegKey,
                                               RTLISTANCHOR *listActions)
{
    BOOL bRet = TRUE;
    HKEY hKey;
    DWORD dwErr;

    dwErr = RegOpenKeyExW(HKEY_LOCAL_MACHINE,
                          pwszRegKey,
                          0,
                          KEY_QUERY_VALUE,
                          &hKey);

    if (dwErr != ERROR_SUCCESS)
    {
        LALOG(("LA: Can't open registry key [%ls], error %d\n",
               pwszRegKey, dwErr));
        return FALSE;
    }

    DWORD dwIndex = 0;

    for (;;)
    {
        DWORD dwRet;

        WCHAR wszValueName[256];
        DWORD cchValueName = RT_ELEMENTS(wszValueName);
        DWORD type;
        BYTE abData[1024];
        DWORD cbData = sizeof(abData);

        dwRet = RegEnumValueW(hKey,
                              dwIndex++,
                              wszValueName,
                              &cchValueName,
                              NULL,
                              &type,
                              abData,
                              &cbData);

        if (dwRet == ERROR_NO_MORE_ITEMS)
        {
            LALOG(("LA: Enumeration exhausted\n"));
            bRet = TRUE;
            break;
        }
        else if (dwRet != ERROR_SUCCESS)
        {
            LALOG(("LA: Enumeration failed, error %d\n",
                   dwRet));
            bRet = FALSE;
            break;
        }

        if ((type != REG_SZ) && (type != REG_EXPAND_SZ))
        {
            LALOG(("LA: skipped type %d\n",
                   type));
            continue;
        }

        char szName[256];
        char *pszName = &szName[0];
        int rc = RTUtf16ToUtf8Ex(wszValueName,
                                 RT_ELEMENTS(wszValueName),
                                 &pszName, sizeof(szName), NULL);
        if (RT_FAILURE(rc))
        {
            LALOG(("LA: RTUtf16ToUtf8Ex for [%ls] rc %Rrc\n",
                    wszValueName, rc));
            continue;
        }

        /* Check if the name starts with "Command" */
        if (RTStrNICmp(szName, g_szCommandPrefix, RT_ELEMENTS(g_szCommandPrefix) - 1) != 0)
        {
            LALOG(("LA: skipped prefix %s\n",
                   szName));
            continue;
        }

        char *pszIndex = &szName[RT_ELEMENTS(g_szCommandPrefix) - 1];

        uint32_t nIndex = RTStrToUInt32(pszIndex);
        if (nIndex == 0)
        {
            LALOG(("LA: skipped index %s\n",
                   szName));
            continue;
        }

        /* Allocate with terminating nul after data. */
        ACTIONENTRY *pEntry = (ACTIONENTRY *)RTMemAlloc(sizeof(ACTIONENTRY) + cbData);
        if (!pEntry)
        {
            LALOG(("LA: RTMemAlloc failed\n"));
            bRet = FALSE;
            break;
        }

        RT_ZERO(pEntry->nodeActionEntry);
        pEntry->u32Index = nIndex;
        memcpy(pEntry->wszCommandLine, abData, cbData);
        pEntry->wszCommandLine[cbData / sizeof(WCHAR)] = 0;

        /* Insert the new entry to the list. Sort by index. */
        if (RTListIsEmpty(listActions))
        {
            RTListAppend(listActions, &pEntry->nodeActionEntry);
        }
        else
        {
            bool fAdded = false;
            ACTIONENTRY *pIter = NULL;
            RTListForEach(listActions, pIter, ACTIONENTRY, nodeActionEntry)
            {
                if (pIter->u32Index > nIndex)
                {
                    RTListNodeInsertBefore(&pIter->nodeActionEntry, &pEntry->nodeActionEntry);
                    fAdded = true;
                    break;
                }
            }
            if (!fAdded)
            {
                RTListAppend(listActions, &pEntry->nodeActionEntry);
            }
        }

        LALOG(("LA: added %d %ls\n",
               pEntry->u32Index, pEntry->wszCommandLine));
    }

    RegCloseKey(hKey);

#ifdef LOG_ENABLED
    ACTIONENTRY *pIter = NULL;
    RTListForEach(listActions, pIter, ACTIONENTRY, nodeActionEntry)
    {
        LALOG(("LA: [%u]: [%ls]\n",
               pIter->u32Index, pIter->wszCommandLine));
    }
#endif

    if (!bRet)
    {
        ActionExecutorDeleteActions(listActions);
    }

    return bRet;
}

static void ActionExecutorExecuteActions(RTLISTANCHOR *listActions)
{
    LALOG(("LA: ExecuteActions\n"));

    ACTIONENTRY *pIter = NULL;
    RTListForEach(listActions, pIter, ACTIONENTRY, nodeActionEntry)
    {
        LALOG(("LA: [%u]: [%ls]\n",
               pIter->u32Index, pIter->wszCommandLine));

        STARTUPINFOW si;
        PROCESS_INFORMATION pi;

        GetStartupInfoW(&si);

        if (!CreateProcessW(NULL,                  // lpApplicationName
                            pIter->wszCommandLine, // lpCommandLine
                            NULL,                  // lpProcessAttributes
                            NULL,                  // lpThreadAttributes
                            FALSE,                 // bInheritHandles
                            0,                     // dwCreationFlags
                            NULL,                  // lpEnvironment
                            NULL,                  // lpCurrentDirectory
                            &si,                   // lpStartupInfo
                            &pi))                  // lpProcessInformation
        {
            LALOG(("LA: Executing [%ls] failed, error %d\n",
                   pIter->wszCommandLine, GetLastError()));
        }
        else
        {
            LALOG(("LA: Executing [%ls] succeeded\n",
                   pIter->wszCommandLine));

            /* Don't care about waiting on the new process, so close these. */
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }

    LALOG(("LA: ExecuteActions leave\n"));
}
	
static BOOL GetVolatileEnvironmentKey(WCHAR *pwszRegKey, DWORD cbRegKey)
{
    BOOL fFound = FALSE;

    DWORD nSessionID;
    LONG lErr;
    HKEY hKey;
    char szRegKey[REG_KEY_LEN];

    /* Attempt to open HKCU\Volatile Environment\<session ID> first. */
    if (   gCtx.pfnProcessIdToSessionId != NULL
        && gCtx.pfnProcessIdToSessionId(GetCurrentProcessId(), &nSessionID))
    {
        RTStrPrintf(szRegKey, sizeof(szRegKey),
                    "%s\\%d",
                    g_pszVolatileEnvironment, nSessionID);

        lErr = RegOpenKeyExA(HKEY_CURRENT_USER,
                             szRegKey,
                             0,
                             KEY_SET_VALUE,
                             &hKey);

        if (lErr == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            fFound = TRUE;
        }
    }

    if (!fFound)
    {
        /* Fall back to HKCU\Volatile Environment. */
        RTStrPrintf(szRegKey, sizeof(szRegKey),
                    "%s",
                    g_pszVolatileEnvironment);

        lErr = RegOpenKeyExA(HKEY_CURRENT_USER,
                             szRegKey,
                             0,
                             KEY_SET_VALUE,
                             &hKey);

        if (lErr == ERROR_SUCCESS)
        {
            RegCloseKey(hKey);
            fFound = TRUE;
        }
    }

    if (fFound)
    {
        LALOG(("LA: GetVolatileEnvironmentKey: [%s]\n", szRegKey));

        /* Convert szRegKey to Utf16 string. */
        PRTUTF16 putf16Unicode = pwszRegKey;
        size_t cchUnicode = cbRegKey / sizeof(WCHAR);

        int rc = RTStrToUtf16Ex(szRegKey, RTSTR_MAX,
                                &putf16Unicode, cchUnicode, NULL);
        if (RT_FAILURE(rc))
        {
            LALOG(("LA: RTStrToUtf16Ex failed %Rrc\n", rc));
            fFound = FALSE;
        }
        else
        {
            LALOG(("LA: unicode [%ls]\n", putf16Unicode));
        }
    }
    else
    {
        LALOG(("LA: GetVolatileEnvironmentKey: not found\n"));
    }

    return fFound;
}

static BOOL GetUtcInfoClientName(WCHAR *pwszClientName, DWORD cbClientName)
{
    LONG lErr;

    WCHAR wszRegKey[REG_KEY_LEN];
    if (!GetVolatileEnvironmentKey(wszRegKey, sizeof(wszRegKey)))
    {
        return FALSE;
    }

    HKEY hKey;
    lErr = RegOpenKeyExW(HKEY_CURRENT_USER,
                         wszRegKey,
                         0,
                         KEY_QUERY_VALUE,
                         &hKey);

    if (lErr != ERROR_SUCCESS)
    {
        LALOG(("LA: RegOpenKeyExW: failed [%ls]\n",
               wszRegKey));
        return FALSE;
    }

    DWORD nRegData;
    DWORD dwType;
    lErr = RegQueryValueExW(hKey,
                            g_pwszUTCINFOClientName,
                            NULL,
                            &dwType,
                            NULL,
                            &nRegData);

    if (lErr != ERROR_SUCCESS)
    {
        LALOG(("LA: RegQueryValueExW: failed [%ls]\n",
               wszRegKey));
        RegCloseKey(hKey);
        return FALSE;
    }

    if (nRegData >= cbClientName)
    {
        LALOG(("LA: buffer overflow reg %d, buffer %d, [%ls]\n",
               nRegData, cbClientName, wszRegKey));
        RegCloseKey(hKey);
        return FALSE;
    }

    if (dwType != REG_SZ)
    {
        LALOG(("LA: wrong type %d, [%ls]\n",
               dwType, wszRegKey));
        RegCloseKey(hKey);
        return FALSE;
    }

    ZeroMemory(pwszClientName, cbClientName);

    lErr = RegQueryValueExW(hKey,
                            g_pwszUTCINFOClientName,
                            NULL,
                            NULL,
                            (BYTE *)pwszClientName,
                            &nRegData);

    RegCloseKey(hKey);

    if (lErr != ERROR_SUCCESS)
    {
        return FALSE;
    }

    return TRUE;
}

static BOOL SetClientName(const WCHAR *pwszClientName)
{
    LONG lErr;

    WCHAR wszRegKey[REG_KEY_LEN];
    if (!GetVolatileEnvironmentKey(wszRegKey, sizeof(wszRegKey)))
    {
        return FALSE;
    }

    HKEY hKey;
    lErr = RegOpenKeyExW(HKEY_CURRENT_USER,
                         wszRegKey,
                         0,
                         KEY_SET_VALUE,
                         &hKey);

    if (lErr != ERROR_SUCCESS)
    {
        return FALSE;
    }

    DWORD nClientName = (lstrlenW(pwszClientName) + 1) * sizeof(WCHAR);
    lErr = RegSetValueExW(hKey,
                          g_pwszClientName,
                          0,
                          REG_SZ,
                          (BYTE*)pwszClientName,
                          nClientName);

    RegCloseKey(hKey);

    if (lErr != ERROR_SUCCESS)
    {
        return FALSE;
    }

    return TRUE;
}

static void laBroadcastSettingChange(void)
{
    DWORD_PTR dwResult;

    if (SendMessageTimeoutA(HWND_BROADCAST,
                            WM_SETTINGCHANGE,
                            NULL,
                            (LPARAM)"Environment",
                            SMTO_ABORTIFHUNG,
                            5000,
                            &dwResult) == 0)
    {
        LALOG(("LA: SendMessageTimeout failed, error %d\n", GetLastError()));
    }
}

static void laUpdateClientName(VBOXLACONTEXT *pCtx)
{
    WCHAR wszUtcInfoClientName[MAX_CLIENT_NAME_CHARS];

    if (GetUtcInfoClientName(wszUtcInfoClientName, sizeof(wszUtcInfoClientName)))
    {
        if (SetClientName(wszUtcInfoClientName))
        {
            laBroadcastSettingChange();
        }
    }
}

static void laOnClientName(const char *pszClientName)
{
    /* pszClientName is UTF8, make an Unicode copy for registry. */
    PRTUTF16 putf16UnicodeName = NULL;
    size_t cchUnicodeName = 0;
    int rc = RTStrToUtf16Ex(pszClientName, MAX_CLIENT_NAME_CHARS,
                            &putf16UnicodeName, 0, &cchUnicodeName);

    if (RT_FAILURE(rc))
    {
        LALOG(("LA: RTStrToUniEx failed %Rrc\n", rc));
        return;
    }

    LALOG(("LA: unicode %d chars [%ls]\n", cchUnicodeName, putf16UnicodeName));

    /*
     * Write the client name to:
     * HKCU\Volatile Environment\UTCINFO_CLIENTNAME or
     * HKCU\Volatile Environment\<SessionID>\UTCINFO_CLIENTNAME
     * depending on whether this is a Terminal Services or desktop session
     * respectively.
     */
    WCHAR wszRegKey[REG_KEY_LEN];
    if (!GetVolatileEnvironmentKey(wszRegKey, sizeof(wszRegKey)))
    {
        LALOG(("LA: Failed to get 'Volatile Environment' registry key\n"));
        RTUtf16Free(putf16UnicodeName);
        return;
    }

    /* Now write the client name under the appropriate key. */
    LONG lRet;
    HKEY hKey;

    lRet = RegOpenKeyExW(HKEY_CURRENT_USER,
                         wszRegKey,
                         0,
                         KEY_SET_VALUE,
                         &hKey);

    if (lRet != ERROR_SUCCESS)
    {
        LALOG(("LA: Failed to open key [%ls], error %lu\n",
               wszRegKey, lRet));
        RTUtf16Free(putf16UnicodeName);
        return;
    }

    DWORD nDataLength = (DWORD)((cchUnicodeName + 1) * sizeof(WCHAR));
    lRet = RegSetValueExW(hKey,
                          g_pwszUTCINFOClientName,
                          0,
                          REG_SZ,
                          (BYTE *)putf16UnicodeName,
                          nDataLength);

    if (lRet != ERROR_SUCCESS)
    {
        LALOG(("LA: RegSetValueExW failed error %lu\n", lRet));
    }

    RegCloseKey(hKey);

    laBroadcastSettingChange();

    /* Also, write the client name to the environment of this process, as it
     * doesn't listen for WM_SETTINGCHANGE messages.
     */
    SetEnvironmentVariableW(g_pwszUTCINFOClientName, putf16UnicodeName);

    RTUtf16Free(putf16UnicodeName);
}

static void laDoAttach(VBOXLACONTEXT *pCtx)
{
    LALOG(("LA: laDoAttach\n"));

    /* Hardcoded action. */
    laUpdateClientName(pCtx);

    /* Process configured actions. */
    ActionExecutorExecuteActions(&pCtx->listAttachActions);
}

static void laDoDetach(VBOXLACONTEXT *pCtx)
{
    LALOG(("LA: laDoDetach\n"));

    /* Process configured actions. */
    ActionExecutorExecuteActions(&pCtx->listDetachActions);
}

static int laGetProperty(uint32_t u32GuestPropHandle, const char *pszName, uint64_t *pu64Timestamp, char **ppszValue)
{
    int rc = VINF_SUCCESS;

    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised.
     */
    uint32_t cbBuf = 1024;
    void *pvBuf = NULL;

    /* Because there is a race condition between our reading the size of a
     * property and the guest updating it, we loop a few times here and
     * hope.  Actually this should never go wrong, as we are generous
     * enough with buffer space.
     */
    unsigned i;
    for (i = 0; i < 3; ++i)
    {
        void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
        if (pvTmpBuf == NULL)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pvBuf = pvTmpBuf;

        rc = VbglR3GuestPropRead(u32GuestPropHandle, pszName, pvBuf, cbBuf,
                                 NULL, pu64Timestamp, NULL,
                                 &cbBuf);
        if (rc != VERR_BUFFER_OVERFLOW)
        {
            break;
        }

        cbBuf += 1024;
    }

    if (RT_SUCCESS(rc))
    {
        LALOG(("LA: laGetProperty: [%s]\n"
               "            value: [%s]\n"
               "        timestamp: %lld ns\n",
               pszName, (char *)pvBuf, *pu64Timestamp));

        *ppszValue = (char *)pvBuf;
    }
    else if (rc == VERR_NOT_FOUND)
    {
        LALOG(("LA: laGetProperty: not found [%s]\n"));
    }
    else
    {
        LALOG(("LA: Failed to retrieve the property value, error %Rrc\n", rc));
    }

    return rc;
}

static int laWaitProperties(uint32_t u32GuestPropHandle,
                            const char *pszPatterns,
                            uint64_t u64LastTimestamp,
                            uint64_t *pu64Timestamp,
                            uint32_t u32Timeout)
{
    int rc = VINF_SUCCESS;

    /* The buffer for storing the data and its initial size.  We leave a bit
     * of space here in case the maximum values are raised.
     */
    void *pvBuf = NULL;
    uint32_t cbBuf = 4096;

    /* Because there is a race condition between our reading the size of a
     * property and the guest updating it, we loop a few times here and
     * hope.  Actually this should never go wrong, as we are generous
     * enough with buffer space.
     */
    unsigned i;
    for (i = 0; i < 3; ++i)
    {
        void *pvTmpBuf = RTMemRealloc(pvBuf, cbBuf);
        if (NULL == pvTmpBuf)
        {
            rc = VERR_NO_MEMORY;
            break;
        }

        pvBuf = pvTmpBuf;

        rc = VbglR3GuestPropWait(u32GuestPropHandle, pszPatterns, pvBuf, cbBuf,
                                 u64LastTimestamp, u32Timeout,
                                 NULL /* ppszName */,
                                 NULL /* ppszValue */,
                                 pu64Timestamp,
                                 NULL /* ppszFlags */,
                                 &cbBuf);

        if (rc != VERR_BUFFER_OVERFLOW)
        {
            break;
        }

        cbBuf += 1024;
    }

    RTMemFree(pvBuf);

    return rc;
}

static int laGetUint32(uint32_t u32GuestPropHandle, const char *pszName, uint64_t *pu64Timestamp, uint32_t *pu32Value)
{
    uint64_t u64Timestamp = 0;
    char *pszValue = NULL;

    int rc = laGetProperty(u32GuestPropHandle,
                           pszName,
                           &u64Timestamp,
                           &pszValue);
    if (RT_SUCCESS(rc))
    {
        if (pszValue && *pszValue)
        {
            uint32_t u32 = 0;
            rc = RTStrToUInt32Full(pszValue, 10, &u32);

            if (RT_SUCCESS(rc))
            {
                *pu64Timestamp = u64Timestamp;
                *pu32Value = u32;
            }
        }
        else
        {
            rc = VERR_NOT_SUPPORTED;
        }
    }

    if (pszValue)
    {
        RTMemFree(pszValue);
    }

    LALOG(("LA: laGetUint32: rc = %Rrc, [%s]\n",
           rc, pszName));

    return rc;
}

static int laGetString(uint32_t u32GuestPropHandle, const char *pszName, uint64_t *pu64Timestamp, char **ppszValue)
{
    int rc = laGetProperty(u32GuestPropHandle,
                           pszName,
                           pu64Timestamp,
                           ppszValue);

    LALOG(("LA: laGetString: rc = %Rrc, [%s]\n",
           rc, pszName));

    return rc;
}

static int laGetActiveClient(VBOXLACONTEXT *pCtx, uint64_t *pu64Timestamp, uint32_t *pu32Value)
{
    int rc = laGetUint32(pCtx->u32GuestPropHandle,
                         g_pszPropActiveClient,
                         pu64Timestamp,
                         pu32Value);

    LALOG(("LA: laGetActiveClient: rc %Rrc, %d, %lld\n", rc, *pu32Value, *pu64Timestamp));

    return rc;
}

static int laUpdateCurrentState(VBOXLACONTEXT *pCtx, uint32_t u32ActiveClientId, uint64_t u64ActiveClientTS)
{
    /* Prepare the current state for the active client.
     * If u32ActiveClientId is 0, then there is no connected clients.
     */
    LALOG(("LA: laUpdateCurrentState: %u %lld\n",
            u32ActiveClientId, u64ActiveClientTS));

    int rc = VINF_SUCCESS;

    int l;

    pCtx->activeClient.u32LastAttach = ~0;
    pCtx->activeClient.u64LastAttachTimestamp = u64ActiveClientTS;

    if (pCtx->activeClient.pszLastName)
    {
        RTMemFree(pCtx->activeClient.pszLastName);
    }
    pCtx->activeClient.pszLastName = NULL;
    pCtx->activeClient.u64LastNameTimestamp = u64ActiveClientTS;

    if (pCtx->activeClient.pszPropName)
    {
        RTMemFree(pCtx->activeClient.pszPropName);
        pCtx->activeClient.pszPropName = NULL;
    }
    if (u32ActiveClientId != 0)
    {
        l = RTStrAPrintf(&pCtx->activeClient.pszPropName,
                         g_pszPropNameTemplate,
                         u32ActiveClientId);
        if (l == -1)
        {
            pCtx->activeClient.pszPropName = NULL;
            rc = VERR_NO_MEMORY;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pCtx->activeClient.pszPropAttach)
        {
            RTMemFree(pCtx->activeClient.pszPropAttach);
            pCtx->activeClient.pszPropAttach = NULL;
        }
        if (u32ActiveClientId != 0)
        {
            l = RTStrAPrintf(&pCtx->activeClient.pszPropAttach,
                             g_pszPropAttachTemplate,
                             u32ActiveClientId);
            if (l == -1)
            {
                pCtx->activeClient.pszPropAttach = NULL;
                rc = VERR_NO_MEMORY;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pCtx->activeClient.pszPropWaitPattern)
        {
            RTMemFree(pCtx->activeClient.pszPropWaitPattern);
            pCtx->activeClient.pszPropWaitPattern = NULL;
        }
        if (u32ActiveClientId != 0)
        {
            l = RTStrAPrintf(&pCtx->activeClient.pszPropWaitPattern,
                             "%s|%s",
                             pCtx->activeClient.pszPropName,
                             pCtx->activeClient.pszPropAttach);
            if (l == -1)
            {
                pCtx->activeClient.pszPropWaitPattern = NULL;
                rc = VERR_NO_MEMORY;
            }
        }
    }

    if (RT_SUCCESS(rc))
    {
        pCtx->activeClient.u32ClientId = u32ActiveClientId;
    }
    else
    {
        pCtx->activeClient.u32ClientId = 0;
    }

    LALOG(("LA: laUpdateCurrentState rc = %Rrc\n",
           rc));

    return rc;
}

static int laWait(VBOXLACONTEXT *pCtx, uint64_t *pu64Timestamp, uint32_t u32Timeout)
{
    LALOG(("LA: laWait [%s]\n",
           pCtx->activeClient.pszPropWaitPattern));

    int rc = laWaitProperties(pCtx->u32GuestPropHandle,
                              pCtx->activeClient.pszPropWaitPattern,
                              pCtx->u64LastQuery,
                              pu64Timestamp,
                              u32Timeout);

    LALOG(("LA: laWait rc %Rrc\n",
           rc));

    return rc;
}

static void laProcessName(VBOXLACONTEXT *pCtx)
{
    /* Check if the name was changed. */
    /* Get the name string and check if it was changed since last time.
     * Write the name to the registry if changed.
     */
    uint64_t u64Timestamp = 0;
    char *pszName = NULL;

    int rc = laGetString(pCtx->u32GuestPropHandle,
                         pCtx->activeClient.pszPropName,
                         &u64Timestamp,
                         &pszName);
    if (RT_SUCCESS(rc))
    {
        LALOG(("LA: laProcessName: read [%s], at %lld\n",
               pszName, u64Timestamp));

        if (u64Timestamp != pCtx->activeClient.u64LastNameTimestamp)
        {
            laOnClientName(pszName);

            pCtx->activeClient.u64LastNameTimestamp = u64Timestamp;
        }

    }

    if (pszName)
    {
        RTMemFree(pszName);
    }
}

static void laProcessAttach(VBOXLACONTEXT *pCtx)
{
    /* Check if the attach was changed. */
    pCtx->u32Action = LA_DO_NOTHING;

    uint64_t u64Timestamp = 0;
    uint32_t u32Attach = ~0;

    int rc = laGetUint32(pCtx->u32GuestPropHandle,
                         pCtx->activeClient.pszPropAttach,
                         &u64Timestamp,
                         &u32Attach);

    if (RT_SUCCESS(rc))
    {
        LALOG(("LA: laProcessAttach: read %d, at %lld\n",
               u32Attach, u64Timestamp));

        if (u64Timestamp != pCtx->activeClient.u64LastAttachTimestamp)
        {
            if (u32Attach != pCtx->activeClient.u32LastAttach)
            {
                LALOG(("LA: laProcessAttach: changed\n"));

                /* Just do the last action. */
                pCtx->u32Action = u32Attach?
                                       LA_DO_ATTACH:
                                       LA_DO_DETACH;

                pCtx->activeClient.u32LastAttach = u32Attach;
            }
            else
            {
                LALOG(("LA: laProcessAttach: same\n"));

                /* The property has changed but the value is the same,
                 * which means that it was changed and restored.
                 */
                pCtx->u32Action = u32Attach?
                                       LA_DO_DETACH_AND_ATTACH:
                                       LA_DO_ATTACH_AND_DETACH;
            }

            pCtx->activeClient.u64LastAttachTimestamp = u64Timestamp;
        }

    }

    LALOG(("LA: laProcessAttach: action %d\n",
           pCtx->u32Action));
}

static void laDoActions(VBOXLACONTEXT *pCtx)
{
    /* Check if the attach was changed.
     *
     * Caller assumes that this function will filter double actions.
     * That is two or more LA_DO_ATTACH will do just one LA_DO_ATTACH.
     */
    LALOG(("LA: laDoActions: action %d\n",
           pCtx->u32Action));

    switch(pCtx->u32Action)
    {
        case LA_DO_ATTACH:
        {
            if (pCtx->u32PrevAction != LA_DO_ATTACH)
            {
                pCtx->u32PrevAction = LA_DO_ATTACH;
                laDoAttach(pCtx);
            }
        } break;

        case LA_DO_DETACH:
        {
            if (pCtx->u32PrevAction != LA_DO_DETACH)
            {
                pCtx->u32PrevAction = LA_DO_DETACH;
                laDoDetach(pCtx);
            }
        } break;

        case LA_DO_DETACH_AND_ATTACH:
        {
            if (pCtx->u32PrevAction != LA_DO_DETACH)
            {
                pCtx->u32PrevAction = LA_DO_DETACH;
                laDoDetach(pCtx);
            }
            pCtx->u32PrevAction = LA_DO_ATTACH;
            laDoAttach(pCtx);
        } break;

        case LA_DO_ATTACH_AND_DETACH:
        {
            if (pCtx->u32PrevAction != LA_DO_ATTACH)
            {
                pCtx->u32PrevAction = LA_DO_ATTACH;
                laDoAttach(pCtx);
            }
            pCtx->u32PrevAction = LA_DO_DETACH;
            laDoDetach(pCtx);
        } break;

        case LA_DO_NOTHING:
        default:
            break;
    }

    pCtx->u32Action = LA_DO_NOTHING;

    LALOG(("LA: laDoActions: leave\n"));
}

int VBoxLAInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    LALOG(("VBoxTray: VBoxLAInit\n"));

    gCtx.pEnv = pEnv;

    int rc = VbglR3GuestPropConnect(&gCtx.u32GuestPropHandle);
    if (RT_FAILURE(rc))
    {
        return rc;
    }

    RTListInit(&gCtx.listAttachActions);
    RTListInit(&gCtx.listDetachActions);

    RT_ZERO(gCtx.activeClient);

    gCtx.hModuleKernel32 = LoadLibrary("KERNEL32");

    if (gCtx.hModuleKernel32)
    {
        *(uintptr_t *)&gCtx.pfnProcessIdToSessionId = (uintptr_t)GetProcAddress(gCtx.hModuleKernel32, "ProcessIdToSessionId");
    }
    else
    {
        gCtx.pfnProcessIdToSessionId = NULL;
    }
    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxLADestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    LALOG(("VBoxTray: VBoxLADestroy\n"));

    VBOXLACONTEXT *pCtx = (VBOXLACONTEXT *)pInstance;

    if (pCtx->u32GuestPropHandle != 0)
    {
        VbglR3GuestPropDisconnect(pCtx->u32GuestPropHandle);
    }

    ActionExecutorDeleteActions(&pCtx->listAttachActions);
    ActionExecutorDeleteActions(&pCtx->listDetachActions);

    if (pCtx->hModuleKernel32)
    {
        FreeLibrary(pCtx->hModuleKernel32);
        pCtx->pfnProcessIdToSessionId = NULL;
    }
    pCtx->hModuleKernel32 = NULL;
}

/*
 * Thread function to wait for and process property changes
 */
unsigned __stdcall VBoxLAThread(void *pInstance)
{
    VBOXLACONTEXT *pCtx = (VBOXLACONTEXT *)pInstance;

    LALOG(("VBoxTray: VBoxLAThread: Started.\n"));

    /*
     * On name change event (/VirtualBox/HostInfo/VRDP/Client/%ID%/Name)
     *   Store the name in the registry (HKCU\Volatile Environment\UTCINFO_CLIENTNAME).
     * On a client attach event (/VirtualBox/HostInfo/VRDP/Client/%ID%/Attach -> 1):
     *   Execute ReconnectActions
     * On a client detach event (/VirtualBox/HostInfo/VRDP/Client/%ID%/Attach -> 0):
     *   Execute DisconnectActions
     *
     * The active connected client id is /VirtualBox/HostInfo/VRDP/ActiveClientClient.
     */

    if (!ActionExecutorEnumerateRegistryKey(g_pwszRegKeyReconnectActions, &gCtx.listAttachActions))
    {
        LALOG(("LA: Can't enumerate registry key %ls\n", g_pwszRegKeyReconnectActions));
    }
    if (!ActionExecutorEnumerateRegistryKey(g_pwszRegKeyDisconnectActions, &gCtx.listDetachActions))
    {
        LALOG(("LA: Can't enumerate registry key %ls\n", g_pwszRegKeyDisconnectActions));
    }

    /* A non zero timestamp in the past. */
    pCtx->u64LastQuery = 1;
    /* Start at Detached state. */
    pCtx->u32PrevAction = LA_DO_DETACH;

    for (;;)
    {
        /* Query current ActiveClient.
         * if it differs from the current active client
         *    rebuild the context;
         * wait with timeout for properties change since the active client was changed;
         * if 'Name' was changed
         *    update the name;
         * if 'Attach' was changed
         *    do respective actions.
         * remember the query timestamp;
         */
        uint64_t u64Timestamp = 0;
        uint32_t u32ActiveClientId = 0;
        int rc = laGetActiveClient(pCtx, &u64Timestamp, &u32ActiveClientId);

        if (RT_SUCCESS(rc))
        {
            if (pCtx->activeClient.u32ClientId != u32ActiveClientId)
            {
                rc = laUpdateCurrentState(pCtx, u32ActiveClientId, u64Timestamp);
            }

            if (RT_SUCCESS(rc))
            {
                if (pCtx->activeClient.u32ClientId != 0)
                {
                    rc = laWait(pCtx, &u64Timestamp, 1000);

                    if (RT_SUCCESS(rc))
                    {
                        laProcessAttach(pCtx);

                        laProcessName(pCtx);

                        laDoActions(pCtx);

                        pCtx->u64LastQuery = u64Timestamp;
                    }
                }
            }
        }

        /* Check if it is time to exit.
         * If the code above failed, wait a bit until repeating to avoid a loop.
         * Otherwise just check if the stop event was signalled.
         */
        uint32_t u32Wait;
        if (   rc == VERR_NOT_FOUND
            || pCtx->activeClient.u32ClientId == 0)
        {
            /* No connections, wait longer. */
            u32Wait = 10000;
        }
        else if (RT_FAILURE(rc))
        {
            u32Wait = 1000;
        }
        else
        {
            u32Wait = 0;
        }
        if (WaitForSingleObject(pCtx->pEnv->hStopEvent, u32Wait) == WAIT_OBJECT_0)
        {
            break;
        }
    }

    LALOG(("VBoxTray: VBoxLAThread: Finished.\n"));
    return 0;
}
