/* $Id$ */
/** @file
 *
 * VirtualBox Windows Guest Shared Folders
 *
 * Network provider dll
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <winsvc.h>
#include <winnetwk.h>
#include <npapi.h>
#include <devioctl.h>
#include <stdio.h>

#include "..\driver\vbsfshared.h"

#include <iprt/alloc.h>
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/log.h>
#include <VBox/version.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/Log.h>

#define MRX_VBOX_SERVER_NAME_U     L"VBOXSVR"
#define MRX_VBOX_SERVER_NAME_ALT_U L"VBOXSRV"

#define WNNC_DRIVER(major, minor) (major * 0x00010000 + minor)

static WCHAR vboxToUpper(WCHAR wc)
{
    /* The CharUpper parameter is a pointer to a null-terminated string,
     * or specifies a single character. If the high-order word of this
     * parameter is zero, the low-order word must contain a single character to be converted.
     */
    return (WCHAR)CharUpper((LPTSTR)wc);
}

static DWORD vbsfIOCTL(ULONG IoctlCode,
                       PVOID InputDataBuf,
                       ULONG InputDataLen,
                       PVOID OutputDataBuf,
                       PULONG pOutputDataLen)
{
    ULONG cbOut = 0;

    if (!pOutputDataLen)
    {
        pOutputDataLen = &cbOut;
    }

    ULONG dwStatus = WN_SUCCESS;

    HANDLE DeviceHandle = CreateFile(DD_MRX_VBOX_USERMODE_DEV_NAME_U,
                                     GENERIC_READ | GENERIC_WRITE,
                                     FILE_SHARE_READ | FILE_SHARE_WRITE,
                                     (LPSECURITY_ATTRIBUTES)NULL,
                                     OPEN_EXISTING,
                                     0,
                                     (HANDLE)NULL);

    if (INVALID_HANDLE_VALUE != DeviceHandle)
    {
        BOOL fSuccess = DeviceIoControl(DeviceHandle,
                                        IoctlCode,
                                        InputDataBuf,
                                        InputDataLen,
                                        OutputDataBuf,
                                        *pOutputDataLen,
                                        pOutputDataLen,
                                        NULL);

        if (!fSuccess)
        {
            dwStatus = GetLastError();

            Log(("VBOXNP: vbsfIOCTL: DeviceIoctl last error = %d\n", dwStatus));
        }

        CloseHandle(DeviceHandle);
    }
    else
    {
        dwStatus = GetLastError();

        static int sLogged = 0;
        if (!sLogged)
        {
            LogRel(("VBOXNP: vbsfIOCTL: Error opening device, last error = %d\n",
                    dwStatus));
            sLogged++;
        }
    }

    return dwStatus;
}

DWORD APIENTRY NPGetCaps(DWORD nIndex)
{
    DWORD rc = 0;

    Log(("VBOXNP: GetNetCaps: Index = 0x%x\n", nIndex));

    switch (nIndex)
    {
        case WNNC_SPEC_VERSION:
        {
            rc = WNNC_SPEC_VERSION51;
        } break;

        case WNNC_NET_TYPE:
        {
            rc = WNNC_NET_RDR2SAMPLE;
        } break;

        case WNNC_DRIVER_VERSION:
        {
            rc = WNNC_DRIVER(1, 0);
        } break;

        case WNNC_CONNECTION:
        {
            vbsfIOCTL(IOCTL_MRX_VBOX_START, NULL, 0, NULL, NULL);

            rc = WNNC_CON_GETCONNECTIONS |
                 WNNC_CON_CANCELCONNECTION |
                 WNNC_CON_ADDCONNECTION |
                 WNNC_CON_ADDCONNECTION3;
        } break;

        case WNNC_ENUMERATION:
        {
            rc = WNNC_ENUM_LOCAL |
                 WNNC_ENUM_GLOBAL |
                 WNNC_ENUM_SHAREABLE;
        } break;

        case WNNC_START:
        {
            rc = WNNC_WAIT_FOR_START;
            break;
        }

        case WNNC_DIALOG:
        {
            rc = WNNC_DLG_GETRESOURCEPARENT |
                 WNNC_DLG_GETRESOURCEINFORMATION;
        } break;

        case WNNC_USER:
        case WNNC_ADMIN:
        default:
        {
            rc = 0;
        } break;
    }

    return rc;
}

DWORD APIENTRY NPLogonNotify(PLUID lpLogonId,
                             LPCWSTR lpAuthentInfoType,
                             LPVOID lpAuthentInfo,
                             LPCWSTR lpPreviousAuthentInfoType,
                             LPVOID lpPreviousAuthentInfo,
                             LPWSTR lpStationName,
                             LPVOID StationHandle,
                             LPWSTR *lpLogonScript)
{
    Log(("VBOXNP: NPLogonNotify\n"));
    *lpLogonScript = NULL;
    return WN_SUCCESS;
}

DWORD APIENTRY NPPasswordChangeNotify(LPCWSTR lpAuthentInfoType,
                                      LPVOID lpAuthentInfo,
                                      LPCWSTR lpPreviousAuthentInfoType,
                                      LPVOID lpPreviousAuthentInfo,
                                      LPWSTR lpStationName,
                                      LPVOID StationHandle,
                                      DWORD dwChangeInfo)
{
    Log(("VBOXNP: NPPasswordChangeNotify\n"));

    SetLastError(WN_NOT_SUPPORTED);
    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY NPAddConnection(LPNETRESOURCE lpNetResource,
                               LPWSTR lpPassword,
                               LPWSTR lpUserName)
{
    Log(("VBOXNP: NPAddConnection\n"));
    return NPAddConnection3(NULL, lpNetResource, lpPassword, lpUserName, 0);
}

DWORD APIENTRY NPAddConnection3(HWND hwndOwner,
                                LPNETRESOURCE lpNetResource,
                                LPWSTR lpPassword,
                                LPWSTR lpUserName,
                                DWORD dwFlags)
{
    DWORD dwStatus = WN_SUCCESS;
    WCHAR ConnectionName[256];
    WCHAR LocalName[3];
    BOOLEAN fLocalName = TRUE;

    Log(("VBOXNP: NPAddConnection3: dwFlags = 0x%x\n", dwFlags));
    Log(("VBOXNP: NPAddConnection3: Local Name:  %ls\n", lpNetResource->lpLocalName ));
    Log(("VBOXNP: NPAddConnection3: Remote Name: %ls\n", lpNetResource->lpRemoteName ));

    if (   lpNetResource->dwType != RESOURCETYPE_DISK
        && lpNetResource->dwType != RESOURCETYPE_ANY)
    {
        Log(("VBOXNP: NPAddConnection3: Incorrect net resource type %d\n", lpNetResource->dwType));
        return WN_BAD_NETNAME;
    }

    /* Build connection name: \Device\VBoxMiniRdr\;%DriveLetter%:\vboxsvr\share */

    lstrcpy(ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U);
    lstrcat(ConnectionName, L"\\;");

    if (lpNetResource->lpLocalName == NULL)
    {
        LocalName[0] = L'\0';
        fLocalName = FALSE;
    }
    else
    {
        if (   lpNetResource->lpLocalName[0]
            && lpNetResource->lpLocalName[1] == L':')
        {
            LocalName[0] = vboxToUpper(lpNetResource->lpLocalName[0]);
            LocalName[1] = L':';
            LocalName[2] = L'\0';

            lstrcat(ConnectionName, LocalName);
        }
        else
        {
            dwStatus = WN_BAD_LOCALNAME;
        }
    }


    if (dwStatus == WN_SUCCESS)
    {
        /* Append the remote name. */
        if (   lpNetResource->lpRemoteName
            && lpNetResource->lpRemoteName[0] == L'\\'
            && lpNetResource->lpRemoteName[1] == L'\\' )
        {
            /* No need for (lstrlen + 1), because 'lpNetResource->lpRemoteName' leading \ is not copied. */
            if (lstrlen(ConnectionName) + lstrlen(lpNetResource->lpRemoteName) <= sizeof(ConnectionName) / sizeof(WCHAR))
            {
                lstrcat(ConnectionName, &lpNetResource->lpRemoteName[1]);
            }
            else
            {
                dwStatus = WN_BAD_NETNAME;
            }
        }
        else
        {
            dwStatus = WN_BAD_NETNAME;
        }
    }

    Log(("VBOXNP: NPAddConnection3: ConnectionName: [%ls], len %d, dwStatus 0x%08X\n",
         ConnectionName, (lstrlen(ConnectionName) + 1) * sizeof(WCHAR), dwStatus));

    if (dwStatus == WN_SUCCESS)
    {
        WCHAR wszTmp[128];

        SetLastError(NO_ERROR);

        if (   fLocalName
            && QueryDosDevice(LocalName, wszTmp, sizeof(wszTmp) / sizeof(WCHAR)))
        {
            Log(("VBOXNP: NPAddConnection3: Connection [%ls] already connected.\n",
                 ConnectionName));
            dwStatus = WN_ALREADY_CONNECTED;
        }
        else
        {
            if (   !fLocalName
                || GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_ADDCONN,
                                     ConnectionName,
                                     (lstrlen(ConnectionName) + 1) * sizeof(WCHAR),
                                     NULL,
                                     NULL);

                if (dwStatus == WN_SUCCESS)
                {
                    if (   fLocalName
                        && !DefineDosDevice(DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM,
                                            lpNetResource->lpLocalName,
                                            ConnectionName))
                    {
                        dwStatus = GetLastError();
                    }
                }
                else
                {
                    dwStatus = WN_BAD_NETNAME;
                }
            }
            else
            {
                dwStatus = WN_ALREADY_CONNECTED;
            }
        }
    }

    Log(("VBOXNP: NPAddConnection3: Returned 0x%08X\n",
         dwStatus));
    return dwStatus;
}

DWORD APIENTRY NPCancelConnection(LPWSTR lpName,
                                  BOOL fForce)
{
    DWORD dwStatus = WN_NOT_CONNECTED;

    Log(("VBOXNP: NPCancelConnection: Name = %ls\n",
         lpName));

    if (lpName && lpName[0] != 0)
    {
        WCHAR ConnectionName[256];

        if (lpName[1] == L':')
        {
            WCHAR RemoteName[128];
            WCHAR LocalName[3];

            LocalName[0] = vboxToUpper(lpName[0]);
            LocalName[1] = L':';
            LocalName[2] = L'\0';

            ULONG cbOut = sizeof(RemoteName) - sizeof(WCHAR); /* Trailing NULL. */

            dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETCONN,
                                 LocalName,
                                 sizeof(LocalName),
                                 (PVOID)RemoteName,
                                 &cbOut);

            if (   dwStatus == WN_SUCCESS
                && cbOut > 0)
            {
                RemoteName[cbOut / sizeof(WCHAR)] = L'\0';

                if (lstrlen(DD_MRX_VBOX_FS_DEVICE_NAME_U) + 2 + lstrlen(LocalName) + lstrlen(RemoteName) + 1 > sizeof(ConnectionName) / sizeof(WCHAR))
                {
                    dwStatus = WN_BAD_NETNAME;
                }
                else
                {
                    lstrcpy(ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U);
                    lstrcat(ConnectionName, L"\\;");
                    lstrcat(ConnectionName, LocalName);
                    lstrcat(ConnectionName, RemoteName);

                    dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_DELCONN,
                                         ConnectionName,
                                         (lstrlen(ConnectionName) + 1) * sizeof(WCHAR),
                                         NULL,
                                         NULL);

                    if (dwStatus == WN_SUCCESS)
                    {
                        if (!DefineDosDevice(DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE,
                                             LocalName,
                                             ConnectionName))
                        {
                            dwStatus = GetLastError();
                        }
                    }
                }
            }
            else
            {
                dwStatus = WN_NOT_CONNECTED;
            }
        }
        else
        {
            BOOLEAN Verifier;

            Verifier  = ( lpName[0] == L'\\' );
            Verifier &= ( lpName[1] == L'V' ) || ( lpName[1] == L'v' );
            Verifier &= ( lpName[2] == L'B' ) || ( lpName[2] == L'b' );
            Verifier &= ( lpName[3] == L'O' ) || ( lpName[3] == L'o' );
            Verifier &= ( lpName[4] == L'X' ) || ( lpName[4] == L'x' );
            Verifier &= ( lpName[5] == L'S' ) || ( lpName[5] == L's' );
            /* Both vboxsvr & vboxsrv are now accepted */
            if (( lpName[6] == L'V' ) || ( lpName[6] == L'v'))
            {
                Verifier &= ( lpName[6] == L'V' ) || ( lpName[6] == L'v' );
                Verifier &= ( lpName[7] == L'R' ) || ( lpName[7] == L'r' );
            }
            else
            {
                Verifier &= ( lpName[6] == L'R' ) || ( lpName[6] == L'r' );
                Verifier &= ( lpName[7] == L'V' ) || ( lpName[7] == L'v' );
            }
            Verifier &= ( lpName[8] == L'\\') || ( lpName[8] == 0 );

            if (Verifier)
            {
                /* Full remote path */
                if (lstrlen(DD_MRX_VBOX_FS_DEVICE_NAME_U) + 2 + lstrlen(lpName) + 1 > sizeof(ConnectionName) / sizeof(WCHAR))
                {
                    dwStatus = WN_BAD_NETNAME;
                }
                else
                {
                    lstrcpy(ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U);
                    lstrcat(ConnectionName, L"\\;");
                    lstrcat(ConnectionName, lpName);

                    dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_DELCONN,
                                         ConnectionName,
                                         (lstrlen(ConnectionName) + 1) * sizeof(WCHAR),
                                         NULL,
                                         NULL);
                }
            }
            else
            {
                dwStatus = WN_NOT_CONNECTED;
            }
        }
    }

    Log(("VBOXNP: NPCancelConnection: Returned 0x%08X\n",
         dwStatus));
    return dwStatus;
}

DWORD APIENTRY NPGetConnection(LPWSTR lpLocalName,
                               LPWSTR lpRemoteName,
                               LPDWORD lpBufferSize)
{
    DWORD dwStatus = WN_NOT_CONNECTED;

    WCHAR RemoteName[128];
    ULONG cbOut = 0;

    Log(("VBOXNP: NPGetConnection: lpLocalName = %ls\n",
         lpLocalName));

    if (lpLocalName && lpLocalName[0] != 0)
    {
        if (lpLocalName[1] == L':')
        {
            WCHAR LocalName[3];

            cbOut = sizeof(RemoteName) - sizeof(WCHAR);
            RemoteName[cbOut / sizeof(WCHAR)] = 0;

            LocalName[0] = vboxToUpper(lpLocalName[0]);
            LocalName[1] = L':';
            LocalName[2] = L'\0';

            dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETCONN,
                                 LocalName,
                                 sizeof(LocalName),
                                 (PVOID)RemoteName,
                                 &cbOut);

            if (dwStatus != NO_ERROR)
            {
                /* The device specified by lpLocalName is not redirected by this provider. */
                dwStatus = WN_NOT_CONNECTED;
            }
            else
            {
                RemoteName[cbOut / sizeof(WCHAR)] = 0;

                if (cbOut == 0)
                {
                    dwStatus = WN_NO_NETWORK;
                }
            }
        }
    }

    if (dwStatus == WN_SUCCESS)
    {
        ULONG cbRemoteName = (lstrlen(RemoteName) + 1) * sizeof (WCHAR); /* Including the trailing 0. */

        Log(("VBOXNP: NPGetConnection: RemoteName: %ls, cb %d\n",
             RemoteName, cbRemoteName));

        DWORD len = sizeof(WCHAR) + cbRemoteName; /* Including the leading '\'. */

        if (*lpBufferSize >= len)
        {
            lpRemoteName[0] = L'\\';
            CopyMemory(&lpRemoteName[1], RemoteName, cbRemoteName);

            Log(("VBOXNP: NPGetConnection: returning lpRemoteName: %ls\n",
                 lpRemoteName));
        }
        else
        {
            if (*lpBufferSize != 0)
            {
                /* Log only real errors. Do not log a 0 bytes try. */
                Log(("VBOXNP: NPGetConnection: Buffer overflow: *lpBufferSize = %d, len = %d\n",
                     *lpBufferSize, len));
            }

            dwStatus = WN_MORE_DATA;
        }

        *lpBufferSize = len;
    }

    if ((dwStatus != WN_SUCCESS) &&
        (dwStatus != WN_MORE_DATA))
    {
        Log(("VBOXNP: NPGetConnection: Returned error 0x%08X\n",
             dwStatus));
    }

    return dwStatus;
}

static const WCHAR *vboxSkipServerPrefix(const WCHAR *lpRemoteName, const WCHAR *lpPrefix)
{
    while (*lpPrefix)
    {
        if (vboxToUpper(*lpPrefix) != vboxToUpper(*lpRemoteName))
        {
            /* Not a prefix */
            return NULL;
        }

        lpPrefix++;
        lpRemoteName++;
    }

    return lpRemoteName;
}

static const WCHAR *vboxSkipServerName(const WCHAR *lpRemoteName)
{
    int cLeadingBackslashes = 0;
    while (*lpRemoteName == L'\\')
    {
        lpRemoteName++;
        cLeadingBackslashes++;
    }

    if (cLeadingBackslashes == 0 || cLeadingBackslashes == 2)
    {
        const WCHAR *lpAfterPrefix = vboxSkipServerPrefix(lpRemoteName, MRX_VBOX_SERVER_NAME_U);

        if (!lpAfterPrefix)
        {
            lpAfterPrefix = vboxSkipServerPrefix(lpRemoteName, MRX_VBOX_SERVER_NAME_ALT_U);
        }

        return lpAfterPrefix;
    }

    return NULL;
}

/* Enumerate shared folders as hierarchy:
 * VBOXSVR(container)
 * +--------------------+
 * |                     \
 * Folder1(connectable)  FolderN(connectable)
 */
typedef struct _NPENUMCTX
{
    ULONG index; /* Index of last entry returned. */
    DWORD dwScope;
    DWORD dwOriginalScope;
    DWORD dwType;
    DWORD dwUsage;
    bool fRoot;
} NPENUMCTX;

DWORD APIENTRY NPOpenEnum(DWORD dwScope,
                          DWORD dwType,
                          DWORD dwUsage,
                          LPNETRESOURCE lpNetResource,
                          LPHANDLE lphEnum)
{
    DWORD dwStatus;

    Log(("VBOXNP: NPOpenEnum: dwScope 0x%08X, dwType 0x%08X, dwUsage 0x%08X, lpNetResource %p\n",
         dwScope, dwType, dwUsage, lpNetResource));

    if (dwUsage == 0)
    {
        /* The bitmask may be zero to match all of the flags. */
        dwUsage = RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_CONTAINER;
    }

    *lphEnum = NULL;

    /* Allocate the context structure. */
    NPENUMCTX *pCtx = (NPENUMCTX *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(NPENUMCTX));

    if (pCtx == NULL)
    {
        dwStatus = WN_OUT_OF_MEMORY;
    }
    else
    {
        if (lpNetResource && lpNetResource->lpRemoteName)
        {
            Log(("VBOXNP: NPOpenEnum: lpRemoteName %ls\n",
                 lpNetResource->lpRemoteName));
        }

        switch (dwScope)
        {
            case 6: /* Advertised as WNNC_ENUM_SHAREABLE. This returns C$ system shares.
                     * NpEnumResource will return NO_MORE_ENTRIES.
                     */
            {
                if (lpNetResource == NULL || lpNetResource->lpRemoteName == NULL)
                {
                    /* If it is NULL or if the lpRemoteName field of the NETRESOURCE is NULL,
                     * the provider should enumerate the top level of its network.
                     * But system shares can't be on top level.
                     */
                    dwStatus = WN_NOT_CONTAINER;
                    break;
                }

                const WCHAR *lpAfterName = vboxSkipServerName(lpNetResource->lpRemoteName);
                if (   lpAfterName == NULL
                    || (*lpAfterName != L'\\' && *lpAfterName != 0))
                {
                    dwStatus = WN_NOT_CONTAINER;
                    break;
                }

                /* Valid server name. */

                pCtx->index = 0;
                pCtx->dwScope = 6;
                pCtx->dwOriginalScope = dwScope;
                pCtx->dwType = dwType;
                pCtx->dwUsage = dwUsage;

                dwStatus = WN_SUCCESS;
                break;
            }
            case RESOURCE_GLOBALNET: /* All resources on the network. */
            {
                if (lpNetResource == NULL || lpNetResource->lpRemoteName == NULL)
                {
                    /* If it is NULL or if the lpRemoteName field of the NETRESOURCE is NULL,
                     * the provider should enumerate the top level of its network.
                     */
                    pCtx->fRoot = true;
                }
                else
                {
                    /* Enumerate lpNetResource->lpRemoteName container, which can be only the VBOXSVR container. */
                    const WCHAR *lpAfterName = vboxSkipServerName(lpNetResource->lpRemoteName);
                    if (   lpAfterName == NULL
                        || (*lpAfterName != L'\\' && *lpAfterName != 0))
                    {
                        dwStatus = WN_NOT_CONTAINER;
                        break;
                    }

                    /* Valid server name. */
                    pCtx->fRoot = false;
                }

                pCtx->index = 0;
                pCtx->dwScope = RESOURCE_GLOBALNET;
                pCtx->dwOriginalScope = dwScope;
                pCtx->dwType = dwType;
                pCtx->dwUsage = dwUsage;

                dwStatus = WN_SUCCESS;
                break;
            }

            case RESOURCE_CONNECTED: /* All currently connected resources. */
            case RESOURCE_CONTEXT: /* The interpretation of this is left to the provider. Treat this as RESOURCE_GLOBALNET. */
            {
                pCtx->index = 0;
                pCtx->dwScope = RESOURCE_CONNECTED;
                pCtx->dwOriginalScope = dwScope;
                pCtx->dwType = dwType;
                pCtx->dwUsage = dwUsage;
                pCtx->fRoot = false; /* Actually ignored for RESOURCE_CONNECTED. */

                dwStatus = WN_SUCCESS;
                break;
            }

            default:
                Log(("VBOXNP: NPOpenEnum: unsupported scope 0x%lx\n",
                     dwScope));
                dwStatus = WN_NOT_SUPPORTED;
                break;
        }
    }

    if (dwStatus != WN_SUCCESS)
    {
        Log(("VBOXNP: NPOpenEnum: Returned error 0x%08X\n",
             dwStatus));
        if (pCtx)
        {
            HeapFree(GetProcessHeap(), 0, pCtx);
        }
    }
    else
    {
        Log(("VBOXNP: NPOpenEnum: pCtx %p\n",
             pCtx));
        *lphEnum = pCtx;
    }

    return dwStatus;
}

DWORD APIENTRY NPEnumResource(HANDLE hEnum,
                              LPDWORD lpcCount,
                              LPVOID lpBuffer,
                              LPDWORD lpBufferSize)
{
    DWORD dwStatus = WN_SUCCESS;
    NPENUMCTX *pCtx = (NPENUMCTX *)hEnum;

    BYTE ConnectionList[26];
    ULONG cbOut;
    WCHAR LocalName[3];
    WCHAR RemoteName[128];
    int cbRemoteName;

    ULONG cbEntry = 0;

    Log(("VBOXNP: NPEnumResource: hEnum %p, lpcCount %p, lpBuffer %p, lpBufferSize %p.\n",
         hEnum, lpcCount, lpBuffer, lpBufferSize));

    if (pCtx == NULL)
    {
        Log(("VBOXNP: NPEnumResource: WN_BAD_HANDLE\n"));
        return WN_BAD_HANDLE;
    }

    if (lpcCount == NULL || lpBuffer == NULL)
    {
        Log(("VBOXNP: NPEnumResource: WN_BAD_VALUE\n"));
        return WN_BAD_VALUE;
    }

    Log(("VBOXNP: NPEnumResource: *lpcCount 0x%x, *lpBufferSize 0x%x, pCtx->index %d\n",
         *lpcCount, *lpBufferSize, pCtx->index));

    LPNETRESOURCE pNetResource = (LPNETRESOURCE)lpBuffer;
    ULONG cbRemaining = *lpBufferSize;
    ULONG cEntriesCopied = 0;
    PWCHAR pStrings = (PWCHAR)((PBYTE)lpBuffer + *lpBufferSize);
    PWCHAR pDst;

    if (pCtx->dwScope == RESOURCE_CONNECTED)
    {
        Log(("VBOXNP: NPEnumResource: RESOURCE_CONNECTED\n"));

        memset(ConnectionList, 0, sizeof(ConnectionList));
        cbOut = sizeof(ConnectionList);

        dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETLIST,
                             NULL, 0,
                             ConnectionList,
                             &cbOut);

        if (dwStatus == WN_SUCCESS && cbOut > 0)
        {
            while (cEntriesCopied < *lpcCount && pCtx->index < RTL_NUMBER_OF(ConnectionList))
            {
                if (ConnectionList[pCtx->index])
                {
                    LocalName[0] = L'A' + (WCHAR)pCtx->index;
                    LocalName[1] = L':';
                    LocalName[2] = L'\0';
                    memset(RemoteName, 0, sizeof(RemoteName));
                    cbOut = sizeof(RemoteName);

                    dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETCONN,
                                         LocalName,
                                         sizeof(LocalName),
                                         RemoteName,
                                         &cbOut);

                    if (dwStatus != WN_SUCCESS || cbOut == 0)
                    {
                        dwStatus = WN_NO_MORE_ENTRIES;
                        break;
                    }

                    /* How many bytes is needed for the current NETRESOURCE data. */
                    cbRemoteName = (lstrlen(RemoteName) + 1) * sizeof(WCHAR);
                    cbEntry = sizeof(NETRESOURCE);
                    cbEntry += sizeof(LocalName);
                    cbEntry += sizeof(WCHAR) + cbRemoteName; /* Leading \. */
                    cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U);

                    if (cbEntry > cbRemaining)
                    {
                        break;
                    }

                    cbRemaining -= cbEntry;

                    memset(pNetResource, 0, sizeof (*pNetResource));

                    pNetResource->dwScope = RESOURCE_CONNECTED;
                    pNetResource->dwType = RESOURCETYPE_DISK;
                    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                    pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

                    /* Reserve the space in the string area. */
                    pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));
                    pDst = pStrings;

                    pNetResource->lpLocalName = pDst;
                    *pDst++ = L'A' + (WCHAR)pCtx->index;
                    *pDst++ = L':';
                    *pDst++ = L'\0';

                    pNetResource->lpRemoteName = pDst;
                    *pDst++ = L'\\';
                    CopyMemory(pDst, RemoteName, cbRemoteName);
                    pDst += cbRemoteName / sizeof(WCHAR);

                    pNetResource->lpComment = NULL;

                    pNetResource->lpProvider = pDst;
                    CopyMemory(pDst, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));

                    Log(("VBOXNP: NPEnumResource: lpRemoteName: %ls\n",
                         pNetResource->lpRemoteName));

                    cEntriesCopied++;
                    pNetResource++;
                }

                pCtx->index++;
            }
        }
        else
        {
            dwStatus = WN_NO_MORE_ENTRIES;
        }
    }
    else if (pCtx->dwScope == RESOURCE_GLOBALNET)
    {
        Log(("VBOXNP: NPEnumResource: RESOURCE_GLOBALNET: root %d\n", pCtx->fRoot));

        if (pCtx->fRoot)
        {
            /* VBOXSVR container. */
            if (pCtx->index > 0)
            {
                dwStatus = WN_NO_MORE_ENTRIES;
            }
            else
            {
                /* Return VBOXSVR server.
                 * Determine the space needed for this entry.
                 */
                cbEntry = sizeof(NETRESOURCE);
                cbEntry += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + the server name */
                cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U);

                if (cbEntry > cbRemaining)
                {
                    /* Do nothing. */
                }
                else
                {
                    cbRemaining -= cbEntry;

                    memset(pNetResource, 0, sizeof (*pNetResource));

                    pNetResource->dwScope = RESOURCE_GLOBALNET;
                    pNetResource->dwType = RESOURCETYPE_ANY;
                    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
                    pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

                    pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));
                    pDst = pStrings;

                    pNetResource->lpLocalName = NULL;

                    pNetResource->lpRemoteName = pDst;
                    *pDst++ = L'\\';
                    *pDst++ = L'\\';
                    CopyMemory(pDst, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U));
                    pDst += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR);

                    pNetResource->lpComment = NULL;

                    pNetResource->lpProvider = pDst;
                    CopyMemory(pDst, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));

                    cEntriesCopied++;

                    pCtx->index++;
                }
            }
        }
        else
        {
            /* Shares of VBOXSVR. */
            memset(ConnectionList, 0, sizeof (ConnectionList));
            cbOut = sizeof(ConnectionList);

            dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETGLOBALLIST,
                                 NULL,
                                 0,
                                 ConnectionList,
                                 &cbOut);

            if (dwStatus == WN_SUCCESS && cbOut > 0)
            {
                while (cEntriesCopied < *lpcCount && pCtx->index < RTL_NUMBER_OF(ConnectionList))
                {
                    if (ConnectionList[pCtx->index])
                    {
                        memset(RemoteName, 0, sizeof(RemoteName));
                        cbOut = sizeof(RemoteName);

                        dwStatus = vbsfIOCTL(IOCTL_MRX_VBOX_GETGLOBALCONN,
                                             &ConnectionList[pCtx->index],
                                             sizeof(ConnectionList[pCtx->index]),
                                             RemoteName,
                                             &cbOut);

                        if (dwStatus != WN_SUCCESS || cbOut == 0)
                        {
                            dwStatus = WN_NO_MORE_ENTRIES;
                            break;
                        }

                        /* How many bytes is needed for the current NETRESOURCE data. */
                        cbRemoteName = (lstrlen(RemoteName) + 1) * sizeof(WCHAR);
                        cbEntry = sizeof(NETRESOURCE);
                        /* Remote name: \\ + vboxsvr + \ + name. */
                        cbEntry += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U) + cbRemoteName;
                        cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U);

                        if (cbEntry > cbRemaining)
                        {
                            break;
                        }

                        cbRemaining -= cbEntry;

                        memset(pNetResource, 0, sizeof (*pNetResource));

                        pNetResource->dwScope = pCtx->dwOriginalScope;
                        pNetResource->dwType = RESOURCETYPE_DISK;
                        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                        pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

                        pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));
                        pDst = pStrings;

                        pNetResource->lpLocalName = NULL;

                        pNetResource->lpRemoteName = pDst;
                        *pDst++ = L'\\';
                        *pDst++ = L'\\';
                        CopyMemory(pDst, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) - sizeof(WCHAR));
                        pDst += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
                        *pDst++ = L'\\';
                        CopyMemory(pDst, RemoteName, cbRemoteName);
                        pDst += cbRemoteName / sizeof(WCHAR);

                        pNetResource->lpComment = NULL;

                        pNetResource->lpProvider = pDst;
                        CopyMemory(pDst, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));

                        Log(("VBOXNP: NPEnumResource: lpRemoteName: %ls\n",
                             pNetResource->lpRemoteName));

                        cEntriesCopied++;
                        pNetResource++;
                    }

                    pCtx->index++;
                }
            }
            else
            {
                dwStatus = WN_NO_MORE_ENTRIES;
            }
        }
    }
    else if (pCtx->dwScope == 6)
    {
        Log(("VBOXNP: NPEnumResource: dwScope 6\n"));
        dwStatus = WN_NO_MORE_ENTRIES;
    }
    else
    {
        Log(("VBOXNP: NPEnumResource: invalid dwScope 0x%x\n",
             pCtx->dwScope));
        return WN_BAD_HANDLE;
    }

    *lpcCount = cEntriesCopied;

    if (cEntriesCopied == 0 && dwStatus == WN_SUCCESS)
    {
        if (pCtx->index >= RTL_NUMBER_OF(ConnectionList))
        {
            dwStatus = WN_NO_MORE_ENTRIES;
        }
        else
        {
            Log(("VBOXNP: NPEnumResource: More Data Needed - %d\n",
                 cbEntry));
            *lpBufferSize = cbEntry;
            dwStatus = WN_MORE_DATA;
        }
    }

    Log(("VBOXNP: NPEnumResource: Entries returned %d, dwStatus 0x%08X\n",
         cEntriesCopied, dwStatus));
    return dwStatus;
}

DWORD APIENTRY NPCloseEnum(HANDLE hEnum)
{
    DWORD dwStatus = WN_SUCCESS;
    NPENUMCTX *pCtx = (NPENUMCTX *)hEnum;

    Log(("VBOXNP: NPCloseEnum: hEnum %p\n",
         hEnum));

    if (pCtx)
    {
        HeapFree(GetProcessHeap(), 0, pCtx);
    }

    Log(("VBOXNP: NPCloseEnum: returns\n"));
    return WN_SUCCESS;
}

DWORD APIENTRY NPGetResourceParent(LPNETRESOURCE lpNetResource,
                                   LPVOID lpBuffer,
                                   LPDWORD lpBufferSize)
{
    Log(("VBOXNP: NPGetResourceParent: lpNetResource %p, lpBuffer %p, lpBufferSize %p\n",
         lpNetResource, lpBuffer, lpBufferSize));

    /* Construct a new NETRESOURCE which is syntactically a parent of lpNetResource,
     * then call NPGetResourceInformation to actually fill the buffer.
     */
    if (!lpNetResource || !lpNetResource->lpRemoteName || !lpBufferSize)
    {
        return WN_BAD_NETNAME;
    }

    const WCHAR *lpAfterName = vboxSkipServerName(lpNetResource->lpRemoteName);
    if (   lpAfterName == NULL
        || (*lpAfterName != L'\\' && *lpAfterName != 0))
    {
        Log(("VBOXNP: NPGetResourceParent: WN_BAD_NETNAME\n"));
        return WN_BAD_NETNAME;
    }

    DWORD RemoteNameLength = lstrlen(lpNetResource->lpRemoteName);

    DWORD cbEntry = sizeof (NETRESOURCE);
    cbEntry += (RemoteNameLength + 1) * sizeof (WCHAR);

    NETRESOURCE *pParent = (NETRESOURCE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cbEntry);

    if (!pParent)
    {
        return WN_OUT_OF_MEMORY;
    }

    pParent->lpRemoteName = (WCHAR *)((PBYTE)pParent + sizeof (NETRESOURCE));
    lstrcpy(pParent->lpRemoteName, lpNetResource->lpRemoteName);

    /* Remove last path component of the pParent->lpRemoteName. */
    WCHAR *pLastSlash = pParent->lpRemoteName + RemoteNameLength;
    if (*pLastSlash == L'\\')
    {
        /* \\server\share\path\, skip last slash immediately. */
        pLastSlash--;
    }

    while (pLastSlash != pParent->lpRemoteName)
    {
        if (*pLastSlash == L'\\')
        {
            break;
        }

        pLastSlash--;
    }

    DWORD dwStatus = WN_SUCCESS;

    if (   pLastSlash == pParent->lpRemoteName
        || pLastSlash == pParent->lpRemoteName + 1)
    {
        /* It is a leading backslash. Construct "no parent" NETRESOURCE. */
        NETRESOURCE *pNetResource = (NETRESOURCE *)lpBuffer;

        cbEntry = sizeof(NETRESOURCE);
        cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* remote name */
        cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (cbEntry > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceParent: WN_MORE_DATA 0x%x\n", cbEntry));
            *lpBufferSize = cbEntry;
            dwStatus = WN_MORE_DATA;
        }
        else
        {
            memset (pNetResource, 0, sizeof (*pNetResource));

            pNetResource->dwType = RESOURCETYPE_ANY;
            pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_NETWORK;
            pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

            WCHAR *pStrings = (WCHAR *)((PBYTE)lpBuffer + *lpBufferSize);
            pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));

            pNetResource->lpRemoteName = pStrings;
            CopyMemory (pStrings, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
            pStrings += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

            pNetResource->lpProvider = pStrings;
            CopyMemory (pStrings, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
            pStrings += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

            Log(("VBOXNP: NPGetResourceParent: no parent, strings %p/%p\n",
                 pStrings, (PBYTE)lpBuffer + *lpBufferSize));
        }
    }
    else
    {
        /* Make the parent remote name and get its information. */
        *pLastSlash = 0;

        LPWSTR lpSystem = NULL;
        dwStatus = NPGetResourceInformation (pParent, lpBuffer, lpBufferSize, &lpSystem);
    }

    if (pParent)
    {
        HeapFree(GetProcessHeap(), 0, pParent);
    }

    return dwStatus;
}

DWORD APIENTRY NPGetResourceInformation(LPNETRESOURCE lpNetResource,
                                        LPVOID lpBuffer,
                                        LPDWORD lpBufferSize,
                                        LPWSTR *lplpSystem)
{
    Log(("VBOXNP: NPGetResourceInformation: lpNetResource %p, lpBuffer %p, lpBufferSize %p, lplpSystem %p\n",
         lpNetResource, lpBuffer, lpBufferSize, lplpSystem));

    if (   lpNetResource == NULL
        || lpNetResource->lpRemoteName == NULL
        || lpBufferSize == NULL)
    {
        Log(("VBOXNP: NPGetResourceInformation: WN_BAD_VALUE\n"));
        return WN_BAD_VALUE;
    }

    Log(("VBOXNP: NPGetResourceInformation: lpRemoteName %ls, *lpBufferSize 0x%x\n",
         lpNetResource->lpRemoteName, *lpBufferSize));

    const WCHAR *lpAfterName = vboxSkipServerName(lpNetResource->lpRemoteName);
    if (   lpAfterName == NULL
        || (*lpAfterName != L'\\' && *lpAfterName != 0))
    {
        Log(("VBOXNP: NPGetResourceInformation: WN_BAD_NETNAME\n"));
        return WN_BAD_NETNAME;
    }

    if (lpNetResource->dwType != 0 && lpNetResource->dwType != RESOURCETYPE_DISK)
    {
        /* The caller passed in a nonzero dwType that does not match
         * the actual type of the network resource.
         */
        return WN_BAD_DEV_TYPE;
    }

    /*
     * If the input remote resource name was "\\server\share\dir1\dir2",
     * then the output NETRESOURCE contains information about the resource "\\server\share".
     * The lpRemoteName, lpProvider, dwType, dwDisplayType, and dwUsage fields are returned
     * containing values, all other fields being set to NULL.
     */
    DWORD cbEntry;
    WCHAR *pStrings = (WCHAR *)((PBYTE)lpBuffer + *lpBufferSize);
    NETRESOURCE *pNetResource = (NETRESOURCE *)lpBuffer;

    /* Check what kind of the resource is that by parsing path components.
     * lpAfterName points to first WCHAR after a valid server name.
     */

    if (lpAfterName[0] == 0 || lpAfterName[1] == 0)
    {
        /* "\\VBOXSVR" or "\\VBOXSVR\" */
        cbEntry = sizeof(NETRESOURCE);
        cbEntry += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name */
        cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (cbEntry > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", cbEntry));
            *lpBufferSize = cbEntry;
            return WN_MORE_DATA;
        }

        memset(pNetResource, 0, sizeof (*pNetResource));

        pNetResource->dwType = RESOURCETYPE_ANY;
        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
        pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

        pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));

        pNetResource->lpRemoteName = pStrings;
        *pStrings++ = L'\\';
        *pStrings++ = L'\\';
        CopyMemory (pStrings, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U));
        pStrings += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR);

        pNetResource->lpProvider = pStrings;
        CopyMemory (pStrings, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
        pStrings += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

        Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
             pNetResource->lpRemoteName, pStrings, (PBYTE)lpBuffer + *lpBufferSize));

        if (lplpSystem)
        {
            *lplpSystem = NULL;
        }

        return WN_SUCCESS;
    }

    /* *lpAfterName == L'\\', could be share or share + path.
     * Check if there are more path components after the share name.
     */
    const WCHAR *lp = lpAfterName + 1;
    while (*lp && *lp != L'\\')
    {
        lp++;
    }

    if (*lp == 0)
    {
        /* It is a share only: \\vboxsvr\share */
        cbEntry = sizeof(NETRESOURCE);
        cbEntry += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name with trailing nul */
        cbEntry += (DWORD)((lp - lpAfterName) * sizeof(WCHAR)); /* The share name with leading \\ */
        cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (cbEntry > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", cbEntry));
            *lpBufferSize = cbEntry;
            return WN_MORE_DATA;
        }

        memset(pNetResource, 0, sizeof (*pNetResource));

        pNetResource->dwType = RESOURCETYPE_DISK;
        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
        pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

        pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));

        pNetResource->lpRemoteName = pStrings;
        *pStrings++ = L'\\';
        *pStrings++ = L'\\';
        CopyMemory(pStrings, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) - sizeof (WCHAR));
        pStrings += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
        CopyMemory (pStrings, lpAfterName, (lp - lpAfterName + 1) * sizeof(WCHAR));
        pStrings += lp - lpAfterName + 1;

        pNetResource->lpProvider = pStrings;
        CopyMemory(pStrings, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
        pStrings += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

        Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
             pNetResource->lpRemoteName, pStrings, (PBYTE)lpBuffer + *lpBufferSize));

        if (lplpSystem)
        {
            *lplpSystem = NULL;
        }

        return WN_SUCCESS;
    }

    /* \\vboxsvr\share\path */
    cbEntry = sizeof(NETRESOURCE);
    cbEntry += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name with trailing nul */
    cbEntry += (DWORD)((lp - lpAfterName) * sizeof(WCHAR)); /* The share name with leading \\ */
    cbEntry += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */
    cbEntry += (lstrlen(lp) + 1) * sizeof (WCHAR); /* path string for lplpSystem */

    if (cbEntry > *lpBufferSize)
    {
        Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", cbEntry));
        *lpBufferSize = cbEntry;
        return WN_MORE_DATA;
    }

    memset(pNetResource, 0, sizeof (*pNetResource));

    pNetResource->dwType = RESOURCETYPE_DISK;
    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
    pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

    pStrings = (PWCHAR)((PBYTE)pStrings - (cbEntry - sizeof(NETRESOURCE)));

    /* The server + share. */
    pNetResource->lpRemoteName = pStrings;
    *pStrings++ = L'\\';
    *pStrings++ = L'\\';
    CopyMemory (pStrings, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) - sizeof (WCHAR));
    pStrings += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
    CopyMemory(pStrings, lpAfterName, (lp - lpAfterName) * sizeof(WCHAR));
    pStrings += lp - lpAfterName;
    *pStrings++ = 0;

    pNetResource->lpProvider = pStrings;
    CopyMemory(pStrings, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
    pStrings += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

    if (lplpSystem)
    {
        *lplpSystem = pStrings;
    }

    lstrcpy(pStrings, lp);
    pStrings += lstrlen(lp) + 1;

    Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
         pNetResource->lpRemoteName, pStrings, (PBYTE)lpBuffer + *lpBufferSize));
    Log(("VBOXNP: NPGetResourceInformation: *lplpSystem: %ls\n", *lplpSystem));

    return WN_SUCCESS;
}

DWORD APIENTRY NPGetUniversalName(LPCWSTR lpLocalPath,
                                  DWORD dwInfoLevel,
                                  LPVOID lpBuffer,
                                  LPDWORD lpBufferSize)
{
    DWORD dwStatus;

    DWORD BufferRequired = 0;
    DWORD RemoteNameLength = 0;
    DWORD RemainingPathLength = 0;

    WCHAR LocalDrive[3];

    const WCHAR *lpRemainingPath;
    WCHAR *lpString;

    Log(("VBOXNP: NPGetUniversalName: lpLocalPath = %ls, InfoLevel = %d, *lpBufferSize = %d\n",
         lpLocalPath, dwInfoLevel, *lpBufferSize));

    /* Check is input parameter is OK. */
    if (   dwInfoLevel != UNIVERSAL_NAME_INFO_LEVEL
        && dwInfoLevel != REMOTE_NAME_INFO_LEVEL)
    {
        Log(("VBOXNP: NPGetUniversalName: Bad dwInfoLevel value: %d\n",
             dwInfoLevel));
        return WN_BAD_LEVEL;
    }

    /* The 'lpLocalPath' is "X:\something". Extract the "X:" to pass to NPGetConnection. */
    if (   lpLocalPath == NULL
        || lpLocalPath[0] == 0
        || lpLocalPath[1] != L':')
    {
        Log(("VBOXNP: NPGetUniversalName: Bad lpLocalPath.\n"));
        return WN_BAD_LOCALNAME;
    }

    LocalDrive[0] = lpLocalPath[0];
    LocalDrive[1] = lpLocalPath[1];
    LocalDrive[2] = 0;

    /* Length of the original path without the driver letter, including trailing NULL. */
    lpRemainingPath = &lpLocalPath[2];
    RemainingPathLength = (DWORD)((wcslen(lpRemainingPath) + 1) * sizeof(WCHAR));

    /* Build the required structure in place of the supplied buffer. */
    if (dwInfoLevel == UNIVERSAL_NAME_INFO_LEVEL)
    {
        LPUNIVERSAL_NAME_INFOW pUniversalNameInfo = (LPUNIVERSAL_NAME_INFOW)lpBuffer;

        BufferRequired = sizeof (UNIVERSAL_NAME_INFOW);

        if (*lpBufferSize >= BufferRequired)
        {
            /* Enough place for the structure. */
            pUniversalNameInfo->lpUniversalName = (PWCHAR)((PBYTE)lpBuffer + sizeof(UNIVERSAL_NAME_INFOW));

            /* At least so many bytes are available for obtaining the remote name. */
            RemoteNameLength = *lpBufferSize - BufferRequired;
        }
        else
        {
            RemoteNameLength = 0;
        }

        /* Put the remote name directly to the buffer if possible and get the name length. */
        dwStatus = NPGetConnection(LocalDrive,
                                   RemoteNameLength? pUniversalNameInfo->lpUniversalName: NULL,
                                   &RemoteNameLength);

        if (   dwStatus != WN_SUCCESS
            && dwStatus != WN_MORE_DATA)
        {
            if (dwStatus != WN_NOT_CONNECTED)
            {
                Log(("VBOXNP: NPGetUniversalName: NPGetConnection returned error 0x%lx\n",
                     dwStatus));
            }
            return dwStatus;
        }

        if (RemoteNameLength < sizeof (WCHAR))
        {
            Log(("VBOXNP: NPGetUniversalName: Remote name is empty.\n"));
            return WN_NO_NETWORK;
        }

        /* Adjust for actual remote name length. */
        BufferRequired += RemoteNameLength;

        /* And for required place for remaining path. */
        BufferRequired += RemainingPathLength;

        if (*lpBufferSize < BufferRequired)
        {
            Log(("VBOXNP: NPGetUniversalName: WN_MORE_DATA BufferRequired: %d\n",
                 BufferRequired));
            *lpBufferSize = BufferRequired;
            return WN_MORE_DATA;
        }

        /* Enough memory in the buffer. Add '\' and remaining path to the remote name. */
        lpString = &pUniversalNameInfo->lpUniversalName[RemoteNameLength / sizeof (WCHAR)];
        lpString--; /* Trailing NULL */

        CopyMemory(lpString, lpRemainingPath, RemainingPathLength);
    }
    else
    {
        LPREMOTE_NAME_INFOW pRemoteNameInfo = (LPREMOTE_NAME_INFOW)lpBuffer;
        WCHAR *lpDelimiter;

        BufferRequired = sizeof (REMOTE_NAME_INFOW);

        if (*lpBufferSize >= BufferRequired)
        {
            /* Enough place for the structure. */
            pRemoteNameInfo->lpUniversalName = (PWCHAR)((PBYTE)lpBuffer + sizeof(REMOTE_NAME_INFOW));
            pRemoteNameInfo->lpConnectionName = NULL;
            pRemoteNameInfo->lpRemainingPath = NULL;

            /* At least so many bytes are available for obtaining the remote name. */
            RemoteNameLength = *lpBufferSize - BufferRequired;
        }
        else
        {
            RemoteNameLength = 0;
        }

        /* Put the remote name directly to the buffer if possible and get the name length. */
        dwStatus = NPGetConnection(LocalDrive, RemoteNameLength? pRemoteNameInfo->lpUniversalName: NULL, &RemoteNameLength);

        if (   dwStatus != WN_SUCCESS
            && dwStatus != WN_MORE_DATA)
        {
            if (dwStatus != WN_NOT_CONNECTED)
            {
                Log(("VBOXNP: NPGetUniversalName: NPGetConnection returned error 0x%lx\n", dwStatus));
            }
            return dwStatus;
        }

        if (RemoteNameLength < sizeof (WCHAR))
        {
            Log(("VBOXNP: NPGetUniversalName: Remote name is empty.\n"));
            return WN_NO_NETWORK;
        }

        /* Adjust for actual remote name length as a part of the universal name. */
        BufferRequired += RemoteNameLength;

        /* And for required place for remaining path as a part of the universal name. */
        BufferRequired += RemainingPathLength;

        /* lpConnectionName, which is the remote name. */
        BufferRequired += RemoteNameLength;

        /* lpRemainingPath. */
        BufferRequired += RemainingPathLength;

        if (*lpBufferSize < BufferRequired)
        {
            Log(("VBOXNP: NPGetUniversalName: WN_MORE_DATA BufferRequired: %d\n",
                 BufferRequired));
            *lpBufferSize = BufferRequired;
            return WN_MORE_DATA;
        }

        /* Enough memory in the buffer. Add \ and remaining path to the remote name. */
        lpString = &pRemoteNameInfo->lpUniversalName[RemoteNameLength / sizeof (WCHAR)];
        lpString--; /* Trailing NULL */

        lpDelimiter = lpString; /* Delimiter between the remote name and the remaining path.
                                 * May be 0 if the remaining path is empty.
                                 */

        CopyMemory( lpString, lpRemainingPath, RemainingPathLength);
        lpString += RemainingPathLength / sizeof (WCHAR);

        *lpDelimiter = 0; /* Keep NULL terminated remote name. */

        pRemoteNameInfo->lpConnectionName = lpString;
        CopyMemory( lpString, pRemoteNameInfo->lpUniversalName, RemoteNameLength);
        lpString += RemoteNameLength / sizeof (WCHAR);

        pRemoteNameInfo->lpRemainingPath = lpString;
        CopyMemory( lpString, lpRemainingPath, RemainingPathLength);

        /* If remaining path was not empty, restore the delimiter in the universal name. */
        if (RemainingPathLength > sizeof(WCHAR))
        {
           *lpDelimiter = L'\\';
        }
    }

    Log(("VBOXNP: NPGetUniversalName: WN_SUCCESS\n"));
    return WN_SUCCESS;
}

BOOL WINAPI DllMain(HINSTANCE hDLLInst,
                    DWORD fdwReason,
                    LPVOID lpvReserved)
{
    BOOL fReturn = TRUE;

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH:
            RTR3InitDll(RTR3INIT_FLAGS_UNOBTRUSIVE);
            VbglR3Init();
            LogRel(("VBOXNP: DLL loaded.\n"));
            break;

        case DLL_PROCESS_DETACH:
            LogRel(("VBOXNP: DLL unloaded.\n"));
            VbglR3Term();
            /// @todo RTR3Term();
            break;

        case DLL_THREAD_ATTACH:
            break;

        case DLL_THREAD_DETACH:
            break;

        default:
            break;
    }

    return fReturn;
}
