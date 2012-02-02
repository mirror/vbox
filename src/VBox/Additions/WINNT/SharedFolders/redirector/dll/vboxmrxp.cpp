/*++

 Copyright (c) 1989-1999  Microsoft Corporation

 Module Name:

 ifsmrxnp.c

 Abstract:

 This module implements the routines required for interaction with network
 provider router interface in NT

 Notes:

 This module has been built and tested only in UNICODE environment

 --*/

#include <windows.h>
#include <windef.h>
#include <winbase.h>
#include <winsvc.h>
#include <winnetwk.h>
#include <npapi.h>
#include <devioctl.h>
#include <stdio.h>

#include "vboxmrxp.h"

#define WNNC_DRIVER( major, minor ) ( major * 0x00010000 + minor )

ULONG SendToMiniRdr (IN ULONG IoctlCode, IN PVOID InputDataBuf, IN ULONG InputDataLen, IN PVOID OutputDataBuf, IN PULONG pOutputDataLen)
/*++

 Routine Description:

 This routine sends a device ioctl to the Mini Rdr.

 Arguments:

 IoctlCode      - Function code for the Mini Rdr driver

 InputDataBuf   - Input buffer pointer

 InputDataLen   - Lenth of the input buffer

 OutputDataBuf  - Output buffer pointer

 pOutputDataLen - Pointer to the length of the output buffer

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 --*/
{
    HANDLE DeviceHandle; // The mini rdr device handle
    BOOL rc;
    ULONG Status;
    DWORD cbOut = 0;

    if (!pOutputDataLen)
        pOutputDataLen = &cbOut;

    Status = WN_SUCCESS;

    // Grab a handle to the redirector device object

    DeviceHandle = CreateFile(DD_MRX_VBOX_USERMODE_DEV_NAME_U, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, (LPSECURITY_ATTRIBUTES)NULL, OPEN_EXISTING, 0, (HANDLE)NULL );

    if (INVALID_HANDLE_VALUE != DeviceHandle)
    {
        rc = DeviceIoControl(DeviceHandle, IoctlCode, InputDataBuf, InputDataLen, OutputDataBuf, *pOutputDataLen, pOutputDataLen, NULL );

        if (!rc)
        {
            Status = GetLastError();
            Log(("VBOXNP: SendToMiniRdr: Returning error from DeviceIoctl! Status = %d \n", Status));
        }
        else
        {
            Log(("VBOXNP: SendToMiniRdr: The DeviceIoctl call succeeded\n"));
        }
        CloseHandle(DeviceHandle);
    }
    else
    {
        Status = GetLastError();
        static int sLogged = 0;
        if (!sLogged)
        {
            LogRel(("VBOXNP: SendToMiniRdr: Error opening device! Status = %lx\n", Status));
            sLogged++;
        }
    }

    return Status;
}

DWORD APIENTRY
NPGetCaps (DWORD nIndex)
/*++

 Routine Description:

 This routine returns the capabilities of the Null Mini redirector
 network provider implementation

 Arguments:

 nIndex - category of capabilities desired

 Return Value:

 the appropriate capabilities

 --*/
{
    DWORD rc = 0;

    Log(("VBOXNP: GetNetCaps: Index = 0x%lx\n", nIndex));

    switch (nIndex)
    {
    case WNNC_SPEC_VERSION:
        rc = WNNC_SPEC_VERSION51;
        break;

    case WNNC_NET_TYPE:
        rc = WNNC_NET_RDR2SAMPLE;
        break;

    case WNNC_DRIVER_VERSION:
        rc = WNNC_DRIVER(1, 0);
        break;

    case WNNC_CONNECTION:
        SendToMiniRdr(IOCTL_MRX_VBOX_START, NULL, 0, NULL, NULL);
        rc = WNNC_CON_GETCONNECTIONS | WNNC_CON_CANCELCONNECTION | WNNC_CON_ADDCONNECTION | WNNC_CON_ADDCONNECTION3;
        break;

    case WNNC_ENUMERATION:
        rc = WNNC_ENUM_LOCAL | WNNC_ENUM_GLOBAL | WNNC_ENUM_SHAREABLE;
        break;

    case WNNC_START:
        rc = 1;
        break;

    case WNNC_DIALOG:
        rc = WNNC_DLG_GETRESOURCEPARENT | WNNC_DLG_GETRESOURCEINFORMATION;
        break;

    case WNNC_USER:
    case WNNC_ADMIN:
    default:
        rc = 0;
        break;
    }

    return rc;
}

DWORD APIENTRY
NPLogonNotify (PLUID lpLogonId,
               LPCWSTR lpAuthentInfoType,
               LPVOID lpAuthentInfo,
               LPCWSTR lpPreviousAuthentInfoType,
               LPVOID lpPreviousAuthentInfo,
               LPWSTR lpStationName,
               LPVOID StationHandle,
               LPWSTR *lpLogonScript)
/*++

 Routine Description:

 This routine handles the logon notifications

 Arguments:

 lpLogonId -- the associated LUID

 lpAuthenInfoType - the authentication information type

 lpAuthenInfo  - the authentication Information

 lpPreviousAuthentInfoType - the previous authentication information type

 lpPreviousAuthentInfo - the previous authentication information

 lpStationName - the logon station name

 LPVOID - logon station handle

 lpLogonScript - the logon script to be executed.

 Return Value:

 WN_SUCCESS

 Notes:

 This capability has not been implemented in the sample.

 --*/
{
    Log(("VBOXNP: NPLogonNotify: Called.\n"));
    *lpLogonScript = NULL;

    return WN_SUCCESS;
}

DWORD APIENTRY
NPPasswordChangeNotify (LPCWSTR lpAuthentInfoType,
                        LPVOID lpAuthentInfo,
                        LPCWSTR lpPreviousAuthentInfoType,
                        LPVOID lpPreviousAuthentInfo,
                        LPWSTR lpStationName,
                        LPVOID StationHandle,
                        DWORD dwChangeInfo)
/*++

 Routine Description:

 This routine handles the password change notifications

 Arguments:

 lpAuthenInfoType - the authentication information type

 lpAuthenInfo  - the authentication Information

 lpPreviousAuthentInfoType - the previous authentication information type

 lpPreviousAuthentInfo - the previous authentication information

 lpStationName - the logon station name

 LPVOID - logon station handle

 dwChangeInfo - the password change information.

 Return Value:

 WN_NOT_SUPPORTED

 Notes:

 This capability has not been implemented in the sample.

 --*/
{
    Log(("VBOXNP: NPPasswordChangeNotify: Called.\n"));
    SetLastError(WN_NOT_SUPPORTED);

    return WN_NOT_SUPPORTED;
}

DWORD APIENTRY
NPAddConnection (LPNETRESOURCE lpNetResource, LPWSTR lpPassword, LPWSTR lpUserName)
/*++

 Routine Description:

 This routine adds a connection to the list of connections associated
 with this network provider

 Arguments:

 lpNetResource - the NETRESOURCE struct

 lpPassword  - the password

 lpUserName - the user name

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 --*/
{
    Log(("VBOXNP: NPAddConnection: Called.\n"));
    return NPAddConnection3(NULL, lpNetResource, lpPassword, lpUserName, 0);
}

DWORD APIENTRY
NPAddConnection3 (HWND hwndOwner, LPNETRESOURCE lpNetResource, LPWSTR lpPassword, LPWSTR lpUserName, DWORD dwFlags)
/*++

 Routine Description:

 This routine adds a connection to the list of connections associated
 with this network provider

 Arguments:

 hwndOwner - the owner handle

 lpNetResource - the NETRESOURCE struct

 lpPassword  - the password

 lpUserName - the user name

 dwFlags - flags for the connection

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 --*/
{
    DWORD Status;
    WCHAR ConnectionName[128];
    WCHAR wszScratch[128];
    WCHAR LocalName[3];
    DWORD CopyBytes = 0;
    BOOLEAN fLocalName = TRUE;

    Log(("VBOXNP: NPAddConnection3: dwFlags = 0x%x\n", dwFlags));
    Log(("VBOXNP: NPAddConnection3: Local Name:  %ls\n", lpNetResource->lpLocalName ));
    Log(("VBOXNP: NPAddConnection3: Remote Name: %ls\n", lpNetResource->lpRemoteName ));

    Status = WN_SUCCESS;

    if (lpNetResource->dwType != RESOURCETYPE_DISK && lpNetResource->dwType != RESOURCETYPE_ANY)
    {
        Log(("VBOXNP: NPAddConnection3: Incorrect net resource type %d\n", lpNetResource->dwType));
        return WN_BAD_NETNAME;
    }

    //  \device\miniredirector\;<DriveLetter>:\Server\Share

    if (lpNetResource->lpLocalName == NULL)
    {
        fLocalName = FALSE;
        lstrcpy(ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U );
        lstrcat(ConnectionName,L"\\;" );
    }
    else
        if ( lstrlen( lpNetResource->lpLocalName )> 1 )
        {
            if ( lpNetResource->lpLocalName[1] == L':' )
            {
                // LocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpNetResource->lpLocalName[0], 0 ) );
                LocalName[0] = (WCHAR) toupper(lpNetResource->lpLocalName[0]);
                LocalName[1] = L':';
                LocalName[2] = L'\0';
                lstrcpy( ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U );
                lstrcat( ConnectionName, L"\\;" );
                lstrcat( ConnectionName, LocalName );
            }
            else
                Status = WN_BAD_LOCALNAME;
        }
        else
            Status = WN_BAD_LOCALNAME;

    // format proper server name
    if ( lpNetResource->lpRemoteName[0] == L'\\' && lpNetResource->lpRemoteName[1] == L'\\' )
    {
        if (lstrlen (ConnectionName) + lstrlen (lpNetResource->lpRemoteName) <= sizeof (ConnectionName) / sizeof (WCHAR))
        {
            /* No need for (lstrlen + 1), because 'lpNetResource->lpRemoteName' leading \ is not copied. */
            lstrcat( ConnectionName, lpNetResource->lpRemoteName + 1 );
        }
        else
        {
            Status = WN_BAD_NETNAME;
        }
    }
    else
    {
        Status = WN_BAD_NETNAME;
    }

    Log(("VBOXNP: NPAddConnection3: Full Connect Name: %ls\n", ConnectionName ));
    Log(("VBOXNP: NPAddConnection3: Full Connect Name Length: %d\n", ( lstrlen( ConnectionName ) + 1 ) * sizeof( WCHAR ) ));

    if ( Status == WN_SUCCESS )
    {
        if ( fLocalName
                && QueryDosDevice( LocalName, wszScratch, sizeof (wszScratch) / sizeof (WCHAR) ))
        {
            Log(("VBOXNP: NPAddConnection3: Connection \"%ls\" already connected.\n", ConnectionName));
            Status = WN_ALREADY_CONNECTED;
        }
        else
            if (   !fLocalName
                || GetLastError() == ERROR_FILE_NOT_FOUND)
            {
                Status = SendToMiniRdr( IOCTL_MRX_VBOX_ADDCONN, ConnectionName,
                                        ( lstrlen( ConnectionName ) + 1 ) * sizeof( WCHAR ),
                                        NULL, &CopyBytes );

                if (Status == WN_SUCCESS)
                {
                    if ( fLocalName
                            && !DefineDosDevice( DDD_RAW_TARGET_PATH | DDD_NO_BROADCAST_SYSTEM,
                                                 lpNetResource->lpLocalName,
                                                 ConnectionName ) )
                    {
                        Status = GetLastError();
                    }
                }
                else Status = WN_BAD_NETNAME;
            }
            else Status = WN_ALREADY_CONNECTED;
    }

    Log(("VBOXNP: NPAddConnection3: Returned 0x%lx\n", Status));
    return Status;
}

DWORD APIENTRY
NPCancelConnection (LPWSTR lpName, BOOL fForce)
/*++

 Routine Description:

 This routine cancels ( deletes ) a connection from the list of connections
 associated with this network provider

 Arguments:

 lpName - name of the connection

 fForce - forcefully delete the connection

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 --*/

{
    WCHAR LocalName[3];
    WCHAR RemoteName[128];
    WCHAR ConnectionName[128];
    ULONG CopyBytes;
    DWORD Status = WN_NOT_CONNECTED;

    Log(("VBOXNP: NPCancelConnection: Name = %ls\n", lpName ));

    if (lstrlen(lpName) > 1)
    {
        if (lpName[1] == L':')
        {
            // LocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpName[0], 0 ) );
            LocalName[0] = (WCHAR)toupper(lpName[0]);
            LocalName[1] = L':';
            LocalName[2] = L'\0';

            CopyBytes = sizeof (RemoteName) - sizeof (WCHAR); /* Trailing NULL. */
            Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETCONN, LocalName, sizeof(LocalName), (PVOID)RemoteName, &CopyBytes);
            if (Status == WN_SUCCESS && CopyBytes > 0)
            {
                RemoteName[CopyBytes / sizeof(WCHAR)] = L'\0';

                if (lstrlen (DD_MRX_VBOX_FS_DEVICE_NAME_U) + 2 + lstrlen (LocalName) + lstrlen (RemoteName) + 1 > sizeof (ConnectionName) / sizeof (WCHAR))
                {
                    return WN_BAD_NETNAME;
                }

                lstrcpy(ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U );
                lstrcat(ConnectionName,L"\\;" );
                lstrcat( ConnectionName, LocalName );
                lstrcat( ConnectionName, RemoteName );
                CopyBytes = 0;
                Status = SendToMiniRdr( IOCTL_MRX_VBOX_DELCONN, ConnectionName,
                        ( lstrlen( ConnectionName ) + 1 ) * sizeof( WCHAR ),
                        NULL, &CopyBytes );
                if ( Status == WN_SUCCESS )
                {
                    if ( !DefineDosDevice( DDD_REMOVE_DEFINITION | DDD_RAW_TARGET_PATH | DDD_EXACT_MATCH_ON_REMOVE,
                                    LocalName,
                                    ConnectionName ) )
                    {
                        Status = GetLastError( );
                    }
                }
            }
            else
            {
                Status = WN_NOT_CONNECTED;
            }
        }
        else
        {
            BOOLEAN Verifier;

            Verifier = ( lpName[0] == L'\\' );
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
                /* full remote path */
                if (lstrlen (DD_MRX_VBOX_FS_DEVICE_NAME_U) + 2 + lstrlen (lpName) + 1 > sizeof (ConnectionName) / sizeof (WCHAR))
                {
                    return WN_BAD_NETNAME;
                }

                lstrcpy( ConnectionName, DD_MRX_VBOX_FS_DEVICE_NAME_U );
                lstrcat( ConnectionName, L"\\;" );
                lstrcat( ConnectionName, lpName );
                CopyBytes = 0;
                Status = SendToMiniRdr( IOCTL_MRX_VBOX_DELCONN, ConnectionName,
                        ( lstrlen( ConnectionName ) + 1 ) * sizeof( WCHAR ),
                        NULL, &CopyBytes );
            }
            else
            Status = WN_NOT_CONNECTED;
        }
    }

    Log(("VBOXNP: NPCancelConnection: Returned 0x%lx\n", Status));
    return Status;
}

DWORD APIENTRY
NPGetConnection (LPWSTR lpLocalName, LPWSTR lpRemoteName, LPDWORD lpBufferSize)
/*++

 Routine Description:

 This routine returns the information associated with a connection

 Arguments:

 lpLocalName - local name associated with the connection

 lpRemoteName - the remote name associated with the connection

 lpBufferSize - the remote name buffer size

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 --*/
{
    DWORD Status, len;
    ULONG CopyBytes;
    WCHAR RemoteName[128];
    WCHAR LocalName[3];

    Status = WN_NOT_CONNECTED;

    Log(("VBOXNP: NPGetConnection: Called\n" ));
    Log(("VBOXNP: NPGetConnection: lpLocalName = %ls\n", lpLocalName));

    if (lstrlen(lpLocalName) > 1)
    {
        if (lpLocalName[1] == L':')
        {
            CopyBytes = sizeof (RemoteName) - sizeof (WCHAR);
            RemoteName[CopyBytes / sizeof (WCHAR)] = 0;
            // LocalName[0] = (WCHAR) CharUpper( (PWCHAR) MAKELONG( (USHORT) lpLocalName[0], 0 ) );
            LocalName[0] = (WCHAR)toupper(lpLocalName[0]);
            LocalName[1] = L':';
            LocalName[2] = L'\0';
            Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETCONN, LocalName, 3 * sizeof(WCHAR), (PVOID)RemoteName, &CopyBytes);
            if (Status != NO_ERROR)
            {
                /* The device specified by lpLocalName is not redirected by this provider. */
                Status = WN_NOT_CONNECTED;
            }
        }
    }
    if (Status == WN_SUCCESS)
    {
        if (CopyBytes > 0)
        {
            /* Return the local name (X:) instead of UNC path \\VBOXSVR\folder to prevent
             * VBOXSVR name resolutions and therefore large delays.
             */

            Log(("VBOXNP: NPGetConnection: CopyBytes: %d, RemoteName: %ls\n", CopyBytes, RemoteName));

            CopyBytes = (lstrlen (RemoteName) + 1) * sizeof (WCHAR); /* Including the trailing 0. */

            len = sizeof(WCHAR) + CopyBytes; /* Including the leading '\'. */

            if (*lpBufferSize >= len)
            {
                lpRemoteName[0] = L'\\';
                CopyMemory( &lpRemoteName[1], RemoteName, CopyBytes );

                Log(("VBOXNP: NPGetConnection: returning lpRemoteName: %ls\n", lpRemoteName));
            }
            else
            {
                if (*lpBufferSize != 0)
                {
                    /* Log only real errors. Do not log a 0 bytes try. */
                    Log(("VBOXNP: NPGetConnection: Buffer overflow: *lpBufferSize = %d, len = %d\n", *lpBufferSize, len));
                }

                Status = WN_MORE_DATA;
            }

            *lpBufferSize = len;
        }
        else
        {
            Status = WN_NO_NETWORK;
        }
    }

    if ((Status != WN_SUCCESS) &&
        (Status != WN_MORE_DATA))
    {
        Log(("VBOXNP: NPGetConnection: Returned error 0x%lx\n", Status));
    }

    return Status;
}

static WCHAR vboxToUpper(WCHAR wc)
{
    /* The CharUpper parameter is a pointer to a null-terminated string,
     * or specifies a single character. If the high-order word of this
     * parameter is zero, the low-order word must contain a single character to be converted.
     */
    return (WCHAR)CharUpper((LPTSTR)wc);
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

#define NEWENUM

#ifdef NEWENUM
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

DWORD APIENTRY
NPOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage, LPNETRESOURCE lpNetResource, LPHANDLE lphEnum)
/*++

 Routine Description:

 This routine opens a handle for enumeration of resources.

 Arguments:

 dwScope - the scope of enumeration (RESOURCE_CONNECTED, RESOURCE_GLOBALNET or RESOURCE_CONTEXT)

 dwType  - the type of resources to be enumerated (RESOURCETYPE_DISK only supported)

 dwUsage - the usage parameter (RESOURCEUSAGE_CONNECTABLE | RESOURCEUSAGE_CONTAINER, or zero to match all the flags)

 lpNetResource - pointer to the container to perform the enumeration.
                 If dwScope is RESOURCE_CONNECTED or RESOURCE_CONTEXT, this parameter will be NULL.

 lphEnum - aptr. for passing nack the enumeration handle

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 --*/
{
    DWORD Status;

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
        Status = WN_OUT_OF_MEMORY;
    }
    else
    {
        if (lpNetResource && lpNetResource->lpRemoteName)
        {
            Log(("VBOXNP: NPOpenEnum: lpRemoteName %ls\n", lpNetResource->lpRemoteName));
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
                    Status = WN_NOT_CONTAINER;
                    break;
                }

                const WCHAR *lpAfterName = vboxSkipServerName(lpNetResource->lpRemoteName);
                if (   lpAfterName == NULL
                    || (*lpAfterName != L'\\' && *lpAfterName != 0))
                {
                    Status = WN_NOT_CONTAINER;
                    break;
                }

                /* Valid server name. */

                pCtx->index = 0;
                pCtx->dwScope = 6;
                pCtx->dwOriginalScope = dwScope;
                pCtx->dwType = dwType;
                pCtx->dwUsage = dwUsage;

                Status = WN_SUCCESS;
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
                        Status = WN_NOT_CONTAINER;
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

                Status = WN_SUCCESS;
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

                Status = WN_SUCCESS;
                break;
            }

            default:
                Log(("VBOXNP: NPOpenEnum: unsupported scope 0x%lx\n", dwScope));
                Status = WN_NOT_SUPPORTED;
                break;
        }
    }

    if (Status != WN_SUCCESS)
    {
        Log(("VBOXNP: NPOpenEnum: Returned error 0x%lx\n", Status));
        if (pCtx)
        {
            HeapFree(GetProcessHeap(), 0, pCtx);
        }
    }
    else
    {
        Log(("VBOXNP: NPOpenEnum: pCtx %p\n", pCtx));
        *lphEnum = pCtx;
    }

    return (Status);
}

DWORD APIENTRY
NPEnumResource (HANDLE hEnum, LPDWORD lpcCount, LPVOID lpBuffer, LPDWORD lpBufferSize)
/*++

 Routine Description:

 This routine uses the handle obtained by a call to NPOpenEnum for
 enuerating the connected shares

 Arguments:

 hEnum  - the enumeration handle

 lpcCount - the number of resources requested/returned

 lpBuffer - the buffer for passing back the NETRESOURCE entries

 lpBufferSize - the size of the buffer

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 WN_NO_MORE_ENTRIES - if the enumeration has exhausted the entries

 WN_MORE_DATA - if more data is available

 --*/
{
    DWORD Status = WN_SUCCESS;
    NPENUMCTX *pCtx = (NPENUMCTX *)hEnum;

    BYTE ConnectionList[26];
    ULONG CopyBytes;
    WCHAR LocalName[3];
    WCHAR RemoteName[128];

    ULONG SpaceNeeded;

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
    ULONG SpaceAvailable = *lpBufferSize;
    ULONG EntriesCopied = 0;
    PWCHAR StringZone = (PWCHAR)((PBYTE)lpBuffer + *lpBufferSize);

    if (pCtx->dwScope == RESOURCE_CONNECTED)
    {
        Log(("VBOXNP: NPEnumResource: RESOURCE_CONNECTED\n"));

        memset (ConnectionList, 0, sizeof (ConnectionList));
        CopyBytes = sizeof(ConnectionList);
        Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETLIST, NULL, 0, ConnectionList, &CopyBytes);

        if (Status == WN_SUCCESS && CopyBytes > 0)
        {
            while (EntriesCopied < *lpcCount && pCtx->index < RTL_NUMBER_OF(ConnectionList))
            {
                if (ConnectionList[pCtx->index])
                {
                    LocalName[0] = L'A' + (WCHAR)pCtx->index;
                    LocalName[1] = L':';
                    LocalName[2] = L'\0';
                    memset (RemoteName, 0, sizeof (RemoteName));
                    CopyBytes = sizeof(RemoteName);
                    Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETCONN, LocalName, sizeof(LocalName), RemoteName, &CopyBytes);

                    if (Status != WN_SUCCESS || CopyBytes == 0)
                    {
                        Status = WN_NO_MORE_ENTRIES;
                        break;
                    }

                    // Determine the space needed for this entry...
                    SpaceNeeded = sizeof(NETRESOURCE); // resource struct
                    SpaceNeeded += 3 * sizeof(WCHAR); // local name
                    SpaceNeeded += 2 * sizeof(WCHAR) + CopyBytes; // remote name: \ + name + 0
                    SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); // provider name

                    if (SpaceNeeded > SpaceAvailable)
                    {
                        break;
                    }

                    SpaceAvailable -= SpaceNeeded;

                    pNetResource->dwScope = RESOURCE_CONNECTED;
                    pNetResource->dwType = RESOURCETYPE_DISK;
                    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                    pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

                    // setup string area at opposite end of buffer
                    SpaceNeeded -= sizeof(NETRESOURCE);
                    StringZone = (PWCHAR)((PBYTE)StringZone - SpaceNeeded);
                    PWCHAR NewStringZone = StringZone;
                    // copy local name
                    pNetResource->lpLocalName = StringZone;
                    *StringZone++ = L'A' + (WCHAR)pCtx->index;
                    *StringZone++ = L':';
                    *StringZone++ = L'\0';
                    // copy remote name
                    pNetResource->lpRemoteName = StringZone;
                    *StringZone++ = L'\\';
                    CopyMemory( StringZone, RemoteName, CopyBytes );
                    StringZone += CopyBytes / sizeof(WCHAR);
                    *StringZone++ = L'\0';
                    // copy comment
                    pNetResource->lpComment = 0;
                    // copy provider name
                    pNetResource->lpProvider = StringZone;
                    CopyMemory(StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof (MRX_VBOX_PROVIDER_NAME_U));

                    EntriesCopied++;

                    // set new bottom of string zone
                    StringZone = NewStringZone;
                    Log(("VBOXNP: NPEnumResource: lpRemoteName: %ls\n", pNetResource->lpRemoteName));

                    pNetResource++;
                }

                pCtx->index++;
            }
        }
        else
        {
            Status = WN_NO_MORE_ENTRIES;
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
                Status = WN_NO_MORE_ENTRIES;
            }
            else
            {
                /* Return VBOXSVR server.
                 * Determine the space needed for this entry.
                 */
                SpaceNeeded = sizeof(NETRESOURCE);
                SpaceNeeded += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name */
                SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

                if (SpaceNeeded > SpaceAvailable)
                {
                    /* Do nothing. */
                }
                else
                {
                    SpaceAvailable -= SpaceNeeded;

                    memset (pNetResource, 0, sizeof (*pNetResource));

                    pNetResource->dwScope = RESOURCE_GLOBALNET;
                    pNetResource->dwType = RESOURCETYPE_ANY;
                    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
                    pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

                    // setup string area at opposite end of buffer
                    SpaceNeeded -= sizeof(NETRESOURCE);
                    StringZone = (PWCHAR)((PBYTE)StringZone - SpaceNeeded);
                    // local name
                    pNetResource->lpLocalName = 0;
                    // remote name
                    pNetResource->lpRemoteName = StringZone;
                    *StringZone++ = L'\\';
                    *StringZone++ = L'\\';
                    CopyMemory( StringZone, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) );
                    StringZone += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR);
                    // comment
                    pNetResource->lpComment = 0;
                    // provider name
                    pNetResource->lpProvider = StringZone;
                    CopyMemory(StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof (MRX_VBOX_PROVIDER_NAME_U));

                    EntriesCopied++;

                    pCtx->index++;
                }
            }
        }
        else
        {
            /* Shares of VBOXSVR. */
            memset (ConnectionList, 0, sizeof (ConnectionList));
            CopyBytes = sizeof(ConnectionList);
            Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETGLOBALLIST, NULL, 0, ConnectionList, &CopyBytes);

            if (Status == WN_SUCCESS && CopyBytes > 0)
            {
                while (EntriesCopied < *lpcCount && pCtx->index < RTL_NUMBER_OF(ConnectionList))
                {
                    if (ConnectionList[pCtx->index])
                    {
                        memset (RemoteName, 0, sizeof (RemoteName));
                        CopyBytes = sizeof(RemoteName);
                        Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETGLOBALCONN, &ConnectionList[pCtx->index], sizeof(ConnectionList[pCtx->index]), RemoteName, &CopyBytes);

                        if (Status != WN_SUCCESS || CopyBytes == 0)
                        {
                            Status = WN_NO_MORE_ENTRIES;
                            break;
                        }

                        // Determine the space needed for this entry...
                        SpaceNeeded = sizeof(NETRESOURCE); // resource struct
                        SpaceNeeded += 3 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U) + CopyBytes; /* remote name: \\ + vboxsvr + \ + name + 0. */
                        SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); // provider name

                        if (SpaceNeeded > SpaceAvailable)
                        {
                            break;
                        }

                        SpaceAvailable -= SpaceNeeded;

                        pNetResource->dwScope = pCtx->dwOriginalScope;
                        pNetResource->dwType = RESOURCETYPE_DISK;
                        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                        pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

                        // setup string area at opposite end of buffer
                        SpaceNeeded -= sizeof(NETRESOURCE);
                        StringZone = (PWCHAR)((PBYTE)StringZone - SpaceNeeded);
                        PWCHAR NewStringZone = StringZone;
                        // copy local name
                        pNetResource->lpLocalName = 0;
                        // copy remote name
                        pNetResource->lpRemoteName = StringZone;
                        *StringZone++ = L'\\';
                        *StringZone++ = L'\\';
                        lstrcpy(StringZone, MRX_VBOX_SERVER_NAME_U );
                        StringZone += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
                        *StringZone++ = L'\\';
                        CopyMemory( StringZone, RemoteName, CopyBytes );
                        StringZone += CopyBytes / sizeof(WCHAR);
                        *StringZone++ = L'\0';
                        // copy comment
                        pNetResource->lpComment = 0;
                        // copy provider name
                        pNetResource->lpProvider = StringZone;
                        CopyMemory(StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof (MRX_VBOX_PROVIDER_NAME_U));

                        EntriesCopied++;

                        // set new bottom of string zone
                        StringZone = NewStringZone;
                        Log(("VBOXNP: NPEnumResource: lpRemoteName: %ls\n", pNetResource->lpRemoteName));

                        pNetResource++;
                    }

                    pCtx->index++;
                }
            }
            else
            {
                Status = WN_NO_MORE_ENTRIES;
            }
        }
    }
    else if (pCtx->dwScope == 6)
    {
        Log(("VBOXNP: NPEnumResource: dwScope 6\n"));
        Status = WN_NO_MORE_ENTRIES;
    }
    else
    {
        Log(("VBOXNP: NPEnumResource: invalid dwScope 0x%x\n", pCtx->dwScope));
        return WN_BAD_HANDLE;
    }

    *lpcCount = EntriesCopied;

    if (EntriesCopied == 0 && Status == WN_SUCCESS)
    {
        if (pCtx->index >= RTL_NUMBER_OF(ConnectionList))
        {
            Status = WN_NO_MORE_ENTRIES;
        }
        else
        {
            Log(("VBOXNP: NPEnumResource: More Data Needed - %d\n", SpaceNeeded));
            *lpBufferSize = SpaceNeeded;
            Status = WN_MORE_DATA;
        }
    }

    Log(("VBOXNP: NPEnumResource: Entries returned - %d, Status %d\n", EntriesCopied, Status));
    return Status;
}
DWORD APIENTRY
NPCloseEnum (HANDLE hEnum)
/*++

 Routine Description:

 This routine closes the handle for enumeration of resources.

 Arguments:

 hEnum  - the enumeration handle

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 --*/
{
    DWORD Status = WN_SUCCESS;
    NPENUMCTX *pCtx = (NPENUMCTX *)hEnum;

    Log(("VBOXNP: NPCloseEnum: hEnum %p\n", hEnum));

    if (pCtx)
    {
        HeapFree(GetProcessHeap(), 0, pCtx);
    }

    Log(("VBOXNP: NPCloseEnum: returns\n"));
    return WN_SUCCESS;
}
#else
DWORD APIENTRY
NPOpenEnum (DWORD dwScope, DWORD dwType, DWORD dwUsage, LPNETRESOURCE lpNetResource, LPHANDLE lphEnum)
/*++

 Routine Description:

 This routine opens a handle for enumeration of resources. The only capability
 implemented in the sample is for enumerating connected shares

 Arguments:

 dwScope - the scope of enumeration

 dwType  - the type of resources to be enumerated

 dwUsage - the usage parameter

 lpNetResource - a pointer to the desired NETRESOURCE struct.

 lphEnum - aptr. for passing nack the enumeration handle

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 The sample only supports the notion of enumerating connected shares

 The handle passed back is merely the index of the last entry returned

 --*/
{
    DWORD Status;

    Log(("VBOXNP: NPOpenEnum: Called.\n"));

    *lphEnum = NULL;

    switch (dwScope)
    {
    case RESOURCE_GLOBALNET:
    case RESOURCE_CONNECTED:
    {
        DWORD *lpdwEnum = NULL;
        lpdwEnum = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(DWORD) * 2);

        if (lpdwEnum != NULL)
        {
            lpdwEnum[0] = 0;
            lpdwEnum[1] = dwScope;
            *lphEnum = lpdwEnum;

            Status = WN_SUCCESS;
        }
        else
        {
            Status = WN_OUT_OF_MEMORY;
        }
        break;
    }
        break;

    case RESOURCE_CONTEXT:
    default:
        Status = WN_NOT_SUPPORTED;
        break;
    }

    if (Status != WN_SUCCESS)
        Log(("VBOXNP: NPOpenEnum: Returned error 0x%lx\n", Status));

    return (Status);
}

DWORD APIENTRY
NPEnumResource (HANDLE hEnum, LPDWORD lpcCount, LPVOID lpBuffer, LPDWORD lpBufferSize)
/*++

 Routine Description:

 This routine uses the handle obtained by a call to NPOpenEnum for
 enuerating the connected shares

 Arguments:

 hEnum  - the enumeration handle

 lpcCount - the number of resources returned

 lpBuffer - the buffere for passing back the entries

 lpBufferSize - the size of the buffer

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 WN_NO_MORE_ENTRIES - if the enumeration has exhausted the entries

 WN_MORE_DATA - if nmore data is available

 Notes:

 The sample only supports the notion of enumerating connected shares

 The handle passed back is merely the index of the last entry returned

 --*/
{
    DWORD Status = WN_SUCCESS;
    BYTE byConnectionList[26];
    ULONG CopyBytes;
    ULONG EntriesCopied;
    ULONG i;
    LPNETRESOURCE pNetResource;
    ULONG SpaceNeeded;
    ULONG SpaceAvailable;
    WCHAR wzLocalName[3];
    WCHAR wzRemoteName[128];
    PWCHAR pwcStringZone;
    DWORD dwScope = 0;

    Log(("VBOXNP: NPEnumResource: Called.\n"));

    if ((lpcCount == NULL) || (lpBufferSize == NULL) || (lpBuffer == NULL))
    {
        Log(("VBOXNP: NPEnumResource: Has invalid parameter(s)!\n"));
        return WN_NO_MORE_ENTRIES;
    }

    if (hEnum != NULL)
        dwScope = ((DWORD*)hEnum)[1];

    Log(("VBOXNP: NPEnumResource: Count Requested %d\n", *lpcCount));
    Log(("VBOXNP: NPEnumResource: Scope %ld\n", dwScope));

    pNetResource = (LPNETRESOURCE)lpBuffer;
    SpaceAvailable = *lpBufferSize;
    EntriesCopied = 0;
    pwcStringZone = (PWCHAR)((PBYTE)lpBuffer + *lpBufferSize);

    /*
     * Returns the already connected resources of each driver letter (A-Z).
     */
    if (dwScope == RESOURCE_CONNECTED)
    {
        Log(("VBOXNP: NPEnumResource: Scope = Resource connected.\n"));

        CopyBytes = 26;
        Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETLIST, NULL, 0, (PVOID)byConnectionList, &CopyBytes);

        if (Status == WN_SUCCESS && CopyBytes > 0)
        {
            for (i = *((PULONG)hEnum); EntriesCopied < *lpcCount, i < 26; i++)
            {
                if (byConnectionList[i])
                {
                    CopyBytes = 128;
                    wzLocalName[0] = L'A' + (WCHAR)i;
                    wzLocalName[1] = L':';
                    wzLocalName[2] = L'\0';
                    Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETCONN, wzLocalName, 3 * sizeof(WCHAR), (PVOID)wzRemoteName, &CopyBytes);

                    // if something strange happened then just say there are no more entries
                    if (Status != WN_SUCCESS || CopyBytes == 0)
                    {
                        Status = WN_NO_MORE_ENTRIES;
                        break;
                    }
                    // Determine the space needed for this entry...

                    SpaceNeeded = sizeof(NETRESOURCE); // resource struct
                    SpaceNeeded += 3 * sizeof(WCHAR); // local name
                    SpaceNeeded += 2 * sizeof(WCHAR) + CopyBytes; // remote name
                    SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); // provider name

                    if (SpaceNeeded > SpaceAvailable)
                    {
                        break;
                    }
                    else
                    {
                        SpaceAvailable -= SpaceNeeded;

                        pNetResource->dwScope = RESOURCE_CONNECTED;
                        pNetResource->dwType = RESOURCETYPE_DISK;
                        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                        pNetResource->dwUsage = 0;

                        // setup string area at opposite end of buffer
                        SpaceNeeded -= sizeof(NETRESOURCE);
                        pwcStringZone = (PWCHAR)((PBYTE)pwcStringZone - SpaceNeeded);
                        // copy local name
                        pNetResource->lpLocalName = pwcStringZone;
                        *pwcStringZone++ = L'A' + (WCHAR)i;
                        *pwcStringZone++ = L':';
                        *pwcStringZone++ = L'\0';
                        // copy remote name
                        pNetResource->lpRemoteName = pwcStringZone;
                        *pwcStringZone++ = L'\\';
                        CopyMemory( pwcStringZone, wzRemoteName, CopyBytes );
                        pwcStringZone += CopyBytes / sizeof(WCHAR);
                        *pwcStringZone++ = L'\0';
                        // copy comment
                        pNetResource->lpComment = 0;
                        // copy provider name
                        pNetResource->lpProvider = pwcStringZone;
                        lstrcpy(pwcStringZone, MRX_VBOX_PROVIDER_NAME_U );

                        EntriesCopied++;
                        // set new bottom of string zone
                        pwcStringZone = (PWCHAR)((PBYTE)pwcStringZone - SpaceNeeded);
                    }
                    pNetResource++;
                }
            }
        }
        else
        {
            Status = WN_NO_MORE_ENTRIES;
        }
    }
    else if (dwScope == RESOURCE_GLOBALNET)
    {
        Log(("VBOXNP: NPEnumResource: Scope =  Resource global net.\n"));

        CopyBytes = 26;
        Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETGLOBALLIST, NULL, 0, (PVOID)byConnectionList, &CopyBytes);

        if (Status == WN_SUCCESS && CopyBytes > 0)
        {
            for (i = *((PULONG)hEnum); EntriesCopied < *lpcCount, i < 26; i++)
            {
                if (byConnectionList[i])
                {
                    CopyBytes = 128;

                    Status = SendToMiniRdr(IOCTL_MRX_VBOX_GETGLOBALCONN, &byConnectionList[i], sizeof(byConnectionList[i]), (PVOID)wzRemoteName, &CopyBytes);

                    // if something strange happened then just say there are no more entries
                    if (Status != WN_SUCCESS || CopyBytes == 0)
                    {
                        Status = WN_NO_MORE_ENTRIES;
                        break;
                    }
                    // Determine the space needed for this entry...

                    SpaceNeeded = sizeof(NETRESOURCE); // resource struct
                    SpaceNeeded += 11 * sizeof(WCHAR) + CopyBytes; // remote name
                    SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); // provider name

                    if (SpaceNeeded > SpaceAvailable)
                    {
                        break;
                    }
                    else
                    {
                        SpaceAvailable -= SpaceNeeded;

                        pNetResource->dwScope = RESOURCE_GLOBALNET;
                        pNetResource->dwType = RESOURCETYPE_DISK;
                        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
                        pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

                        // setup string area at opposite end of buffer
                        SpaceNeeded -= sizeof(NETRESOURCE);
                        pwcStringZone = (PWCHAR)((PBYTE)pwcStringZone - SpaceNeeded);
                        // copy local name
                        pNetResource->lpLocalName = 0;
                        // copy remote name
                        pNetResource->lpRemoteName = pwcStringZone;
                        *pwcStringZone++ = L'\\';
                        *pwcStringZone++ = L'\\';
                        *pwcStringZone++ = L'V';
                        *pwcStringZone++ = L'B';
                        *pwcStringZone++ = L'O';
                        *pwcStringZone++ = L'X';
                        *pwcStringZone++ = L'S';
                        *pwcStringZone++ = L'V';
                        *pwcStringZone++ = L'R';
                        *pwcStringZone++ = L'\\';
                        CopyMemory( pwcStringZone, wzRemoteName, CopyBytes );
                        pwcStringZone += CopyBytes / sizeof(WCHAR);
                        *pwcStringZone++ = L'\0';
                        // copy comment
                        pNetResource->lpComment = 0;
                        // copy provider name
                        pNetResource->lpProvider = pwcStringZone;
                        lstrcpy(pwcStringZone, MRX_VBOX_PROVIDER_NAME_U );

                        EntriesCopied++;
                        // set new bottom of string zone
                        pwcStringZone = (PWCHAR)((PBYTE)pwcStringZone - SpaceNeeded);
                        Log(("VBOXNP: NPEnumResource: StringZone: %ls\n", pwcStringZone));
                    }
                    pNetResource++;
                }
            }
        }
        else
        {
            Status = WN_NO_MORE_ENTRIES;
        }
    }
    else return WN_NO_MORE_ENTRIES;

    *lpcCount = EntriesCopied;
    if (EntriesCopied == 0 && Status == WN_SUCCESS)
    {
        if (i > 25)
        {
            Status = WN_NO_MORE_ENTRIES;
        }
        else
        {
            Log(("VBOXNP: NPEnumResource: More Data Needed - %d\n", SpaceNeeded));
            Status = WN_MORE_DATA;
            *lpBufferSize = SpaceNeeded;
        }
    }
    // update entry index
    *(PULONG)hEnum = i;

    Log(("VBOXNP: NPEnumResource: Entries returned - %d\n", EntriesCopied));

    return Status;
}

DWORD APIENTRY
NPCloseEnum (HANDLE hEnum)
/*++

 Routine Description:

 This routine closes the handle for enumeration of resources.

 Arguments:

 hEnum  - the enumeration handle

 Return Value:

 WN_SUCCESS if successful, otherwise the appropriate error

 Notes:

 The sample only supports the notion of enumerating connected shares

 --*/
{
    Log(("VBOXNP: NPCloseEnum: Called.\n"));

    if (NULL != (PVOID)hEnum)
        HeapFree(GetProcessHeap(), 0, (PVOID)hEnum);

    Log(("VBOXNP: NPCloseEnum: Memory freed.\n"));
    return WN_SUCCESS;
}
#endif

DWORD APIENTRY
NPGetResourceParent (LPNETRESOURCE lpNetResource, LPVOID lpBuffer, LPDWORD lpBufferSize)
/*++

 Routine Description:

 This routine returns the information about net resource parent

 Arguments:

 lpNetResource - the NETRESOURCE struct

 lpBuffer - the buffer for passing back the parent information

 lpBufferSize - the buffer size

 Return Value:

 Notes:

 --*/
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

    DWORD SpaceNeeded = sizeof (NETRESOURCE);
    SpaceNeeded += (RemoteNameLength + 1) * sizeof (WCHAR);

    NETRESOURCE *pParent = (NETRESOURCE *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, SpaceNeeded);

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

    DWORD Status = WN_SUCCESS;

    if (   pLastSlash == pParent->lpRemoteName
        || pLastSlash == pParent->lpRemoteName + 1)
    {
        /* It is a leading backslash. Construct "no parent" NETRESOURCE. */
        NETRESOURCE *pNetResource = (NETRESOURCE *)lpBuffer;

        SpaceNeeded = sizeof(NETRESOURCE);
        SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* remote name */
        SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (SpaceNeeded > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceParent: WN_MORE_DATA 0x%x\n", SpaceNeeded));
            *lpBufferSize = SpaceNeeded;
            Status = WN_MORE_DATA;
        }
        else
        {
            memset (pNetResource, 0, sizeof (*pNetResource));

            pNetResource->dwType = RESOURCETYPE_ANY;
            pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_NETWORK;
            pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

            /* Setup string area at opposite end of buffer. */
            WCHAR *StringZone = (WCHAR *)((PBYTE)lpBuffer + *lpBufferSize);
            StringZone = (PWCHAR)((PBYTE)StringZone - (SpaceNeeded - sizeof(NETRESOURCE)));

            pNetResource->lpRemoteName = StringZone;
            CopyMemory (StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
            StringZone += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

            /* Provider. */
            pNetResource->lpProvider = StringZone;
            CopyMemory (StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
            StringZone += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

            Log(("VBOXNP: NPGetResourceParent: no parent, strings %p/%p\n",
                 StringZone, (PBYTE)lpBuffer + *lpBufferSize));
        }
    }
    else
    {
        /* Make the parent remote name and get its information. */
        *pLastSlash = 0;

        LPWSTR lpSystem = NULL;
        Status = NPGetResourceInformation (pParent, lpBuffer, lpBufferSize, &lpSystem);
    }

    if (pParent)
    {
        HeapFree(GetProcessHeap(), 0, pParent);
    }
    return Status;
}

DWORD APIENTRY
NPGetResourceInformation (LPNETRESOURCE lpNetResource, LPVOID lpBuffer, LPDWORD lpBufferSize, LPWSTR *lplpSystem)
/*

 Routine Description:

 Separates the part of a network resource accessed through the WNet API from the part accessed
 through APIs specific to the resource type.

 Arguments:

 lpNetResource - network resource for which information is required.
                 The lpRemoteName field specifies the remote name of the resource.

 lpBuffer - a single NETRESOURCE structure and associated strings. The lpRemoteName field should be
            returned in the same format as that returned from an enumeration by the NPEnumResource
            function, so that the caller can perform a case-sensitive string comparison.

 lpBufferSize - the buffer size. If the buffer is too small for the result, the function places the
                required buffer size at this location and returns the error WN_MORE_DATA.

 lplpSystem - On a successful return, a pointer to a null-terminated string in the output buffer
              specifying that part of the resource that is accessed through system APIs specific
              to the resource type, rather than through the WNet API. If there is no such part,
              lplpSystem is set to NULL.

 */
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
    DWORD SpaceNeeded;
    WCHAR *StringZone = (WCHAR *)((PBYTE)lpBuffer + *lpBufferSize);
    NETRESOURCE *pNetResource = (NETRESOURCE *)lpBuffer;

    /* Check what kind of the resource is that by parsing path components.
     * lpAfterName points to first WCHAR after a valid server name.
     */

    if (lpAfterName[0] == 0 || lpAfterName[1] == 0)
    {
        /* "\\VBOXSVR" or "\\VBOXSVR\" */
        SpaceNeeded = sizeof(NETRESOURCE);
        SpaceNeeded += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name */
        SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (SpaceNeeded > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", SpaceNeeded));
            *lpBufferSize = SpaceNeeded;
            return WN_MORE_DATA;
        }

        memset (pNetResource, 0, sizeof (*pNetResource));

        pNetResource->dwType = RESOURCETYPE_ANY;
        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SERVER;
        pNetResource->dwUsage = RESOURCEUSAGE_CONTAINER;

        /* Setup string area at opposite end of buffer. */
        StringZone = (PWCHAR)((PBYTE)StringZone - (SpaceNeeded - sizeof(NETRESOURCE)));

        /* The remote name is the server. */
        pNetResource->lpRemoteName = StringZone;
        *StringZone++ = L'\\';
        *StringZone++ = L'\\';
        CopyMemory (StringZone, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U));
        StringZone += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR);

        /* Provider. */
        pNetResource->lpProvider = StringZone;
        CopyMemory (StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
        StringZone += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

        Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
             pNetResource->lpRemoteName, StringZone, (PBYTE)lpBuffer + *lpBufferSize));

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
        SpaceNeeded = sizeof(NETRESOURCE);
        SpaceNeeded += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name with trailing nul */
        SpaceNeeded += (lp - lpAfterName) * sizeof(WCHAR); /* The share name with leading \\ */
        SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */

        if (SpaceNeeded > *lpBufferSize)
        {
            Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", SpaceNeeded));
            *lpBufferSize = SpaceNeeded;
            return WN_MORE_DATA;
        }

        memset (pNetResource, 0, sizeof (*pNetResource));

        pNetResource->dwType = RESOURCETYPE_DISK;
        pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
        pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

        /* Setup string area at opposite end of buffer. */
        StringZone = (PWCHAR)((PBYTE)StringZone - (SpaceNeeded - sizeof(NETRESOURCE)));

        /* The remote name is the server + share. */
        pNetResource->lpRemoteName = StringZone;
        *StringZone++ = L'\\';
        *StringZone++ = L'\\';
        CopyMemory (StringZone, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) - sizeof (WCHAR));
        StringZone += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
        CopyMemory (StringZone, lpAfterName, (lp - lpAfterName + 1) * sizeof(WCHAR));
        StringZone += lp - lpAfterName + 1;

        /* Provider. */
        pNetResource->lpProvider = StringZone;
        CopyMemory (StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
        StringZone += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

        Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
             pNetResource->lpRemoteName, StringZone, (PBYTE)lpBuffer + *lpBufferSize));

        if (lplpSystem)
        {
            *lplpSystem = NULL;
        }
        return WN_SUCCESS;
    }

    /* \\vboxsvr\share\path */
    SpaceNeeded = sizeof(NETRESOURCE);
    SpaceNeeded += 2 * sizeof(WCHAR) + sizeof(MRX_VBOX_SERVER_NAME_U); /* \\ + server name with trailing nul */
    SpaceNeeded += (lp - lpAfterName) * sizeof(WCHAR); /* The share name with leading \\ */
    SpaceNeeded += sizeof(MRX_VBOX_PROVIDER_NAME_U); /* provider name */
    SpaceNeeded += (lstrlen(lp) + 1) * sizeof (WCHAR); /* path string for lplpSystem */

    if (SpaceNeeded > *lpBufferSize)
    {
        Log(("VBOXNP: NPGetResourceInformation: WN_MORE_DATA 0x%x\n", SpaceNeeded));
        *lpBufferSize = SpaceNeeded;
        return WN_MORE_DATA;
    }

    memset (pNetResource, 0, sizeof (*pNetResource));

    pNetResource->dwType = RESOURCETYPE_DISK;
    pNetResource->dwDisplayType = RESOURCEDISPLAYTYPE_SHARE;
    pNetResource->dwUsage = RESOURCEUSAGE_CONNECTABLE;

    /* Setup string area at opposite end of buffer. */
    StringZone = (PWCHAR)((PBYTE)StringZone - (SpaceNeeded - sizeof(NETRESOURCE)));

    /* The remote name is the server + share. */
    pNetResource->lpRemoteName = StringZone;
    *StringZone++ = L'\\';
    *StringZone++ = L'\\';
    CopyMemory (StringZone, MRX_VBOX_SERVER_NAME_U, sizeof(MRX_VBOX_SERVER_NAME_U) - sizeof (WCHAR));
    StringZone += sizeof(MRX_VBOX_SERVER_NAME_U) / sizeof(WCHAR) - 1;
    CopyMemory (StringZone, lpAfterName, (lp - lpAfterName) * sizeof(WCHAR));
    StringZone += lp - lpAfterName;
    *StringZone++ = 0;

    /* Provider. */
    pNetResource->lpProvider = StringZone;
    CopyMemory (StringZone, MRX_VBOX_PROVIDER_NAME_U, sizeof(MRX_VBOX_PROVIDER_NAME_U));
    StringZone += sizeof(MRX_VBOX_PROVIDER_NAME_U) / sizeof(WCHAR);

    if (lplpSystem)
    {
        *lplpSystem = StringZone;
    }

    lstrcpy(StringZone, lp);
    StringZone += lstrlen(lp) + 1;

    Log(("VBOXNP: NPGetResourceInformation: lpRemoteName: %ls, strings %p/%p\n",
         pNetResource->lpRemoteName, StringZone, (PBYTE)lpBuffer + *lpBufferSize));
    Log(("VBOXNP: NPGetResourceInformation: *lplpSystem: %ls\n", *lplpSystem));

    return WN_SUCCESS;
}

DWORD APIENTRY
NPGetUniversalName (LPCWSTR lpLocalPath, DWORD dwInfoLevel, LPVOID lpBuffer, LPDWORD lpBufferSize)
/*++

 Routine Description:

 This routine returns the information associated net resource

 Arguments:

 lpLocalPath - the local path name

 dwInfoLevel  - the desired info level

 lpBuffer - the buffer for the universal name

 lpBufferSize - the buffer size

 Return Value:

 WN_SUCCESS if successful

 Notes:

 --*/
{
    DWORD dwStatus;

    DWORD BufferRequired = 0;
    DWORD RemoteNameLength = 0;
    DWORD RemainingPathLength = 0;

    WCHAR LocalDrive[3];

    const WCHAR *lpRemainingPath;
    WCHAR *lpString;

    Log(("VBOXNP: NPGetUniversalName: lpLocalPath = %ls, InfoLevel = %d, *lpBufferSize = %d\n", lpLocalPath, dwInfoLevel, *lpBufferSize));

    /* Check is input parameter is OK. */
    if (   dwInfoLevel != UNIVERSAL_NAME_INFO_LEVEL
        && dwInfoLevel != REMOTE_NAME_INFO_LEVEL)
    {
        Log(("VBOXNP: NPGetUniversalName: Bad dwInfoLevel value: %d\n", dwInfoLevel));
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
    RemainingPathLength = (DWORD)((wcslen (lpRemainingPath) + 1) * sizeof(WCHAR));

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
        dwStatus = NPGetConnection (LocalDrive, RemoteNameLength? pUniversalNameInfo->lpUniversalName: NULL, &RemoteNameLength);

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

        /* Adjust for actual remote name length. */
        BufferRequired += RemoteNameLength;

        /* And for required place for remaining path. */
        BufferRequired += RemainingPathLength;

        if (*lpBufferSize < BufferRequired)
        {
            Log(("VBOXNP: NPGetUniversalName: WN_MORE_DATA BufferRequired: %d\n", BufferRequired));
            *lpBufferSize = BufferRequired;
            return WN_MORE_DATA;
        }

        /* Enough memory in the buffer. Add '\' and remaining path to the remote name. */
        lpString = &pUniversalNameInfo->lpUniversalName[RemoteNameLength / sizeof (WCHAR)];
        lpString--; /* Trailing NULL */

        CopyMemory( lpString, lpRemainingPath, RemainingPathLength);
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
        dwStatus = NPGetConnection (LocalDrive, RemoteNameLength? pRemoteNameInfo->lpUniversalName: NULL, &RemoteNameLength);

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

        /* Adjust for actual remote name length. */
        BufferRequired += RemoteNameLength;

        /* And for required place for remaining path. */
        BufferRequired += RemainingPathLength;

        /* lpConnectionName, which is the remote name. */
        BufferRequired += RemoteNameLength;

        /* lpRemainingPath. */
        BufferRequired += RemainingPathLength;

        if (*lpBufferSize < BufferRequired)
        {
            Log(("VBOXNP: NPGetUniversalName: WN_MORE_DATA BufferRequired: %d\n", BufferRequired));
            *lpBufferSize = BufferRequired;
            return WN_MORE_DATA;
        }

        /* Enough memory in the buffer. Add \ and remaining path to the remote name. */
        lpString = &pRemoteNameInfo->lpUniversalName[RemoteNameLength / sizeof (WCHAR)];
        lpString--; /* Trailing NULL */

        lpDelimiter = lpString; /* Delimiter will be inserted later. For now keep the NULL terminated string. */

        CopyMemory( lpString, lpRemainingPath, RemainingPathLength);
        lpString += RemainingPathLength / sizeof (WCHAR);

        *lpDelimiter = 0;

        pRemoteNameInfo->lpConnectionName = lpString;
        CopyMemory( lpString, pRemoteNameInfo->lpUniversalName, RemoteNameLength);
        lpString += RemoteNameLength / sizeof (WCHAR);

        pRemoteNameInfo->lpRemainingPath = lpString;
        CopyMemory( lpString, lpRemainingPath, RemainingPathLength);

        *lpDelimiter = L'\\';
    }

    Log(("VBOXNP: NPGetUniversalName: WN_SUCCESS\n"));
    return WN_SUCCESS;
}
