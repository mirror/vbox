/** @file
 *
 * VBoxGINA -- Windows Logon DLL for VirtualBox
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

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "winwlx.h"
#include "VBoxGINA.h"
#include "Helper.h"
#include "Dialog.h"

/*
 * Global variables
 */


/** DLL instance handle */
HINSTANCE hDllInstance;

/** Version of Winlogon */
DWORD wlxVersion;

/** Handle to Winlogon service */
HANDLE hGinaWlx;
/** Winlog function dispatch table */
PWLX_DISPATCH_VERSION_1_1 pWlxFuncs;

/**
 * Function pointers to MSGINA entry points
 */
PGWLXNEGOTIATE GWlxNegotiate;
PGWLXINITIALIZE GWlxInitialize;
PGWLXDISPLAYSASNOTICE GWlxDisplaySASNotice;
PGWLXLOGGEDOUTSAS GWlxLoggedOutSAS;
PGWLXACTIVATEUSERSHELL GWlxActivateUserShell;
PGWLXLOGGEDONSAS GWlxLoggedOnSAS;
PGWLXDISPLAYLOCKEDNOTICE GWlxDisplayLockedNotice;
PGWLXWKSTALOCKEDSAS GWlxWkstaLockedSAS;
PGWLXISLOCKOK GWlxIsLockOk;
PGWLXISLOGOFFOK GWlxIsLogoffOk;
PGWLXLOGOFF GWlxLogoff;
PGWLXSHUTDOWN GWlxShutdown;
/* GINA 1.1 */
PGWLXSTARTAPPLICATION GWlxStartApplication;
PGWLXSCREENSAVERNOTIFY GWlxScreenSaverNotify;
/* GINA 1.3 */
PGWLXNETWORKPROVIDERLOAD GWlxNetworkProviderLoad;
PGWLXDISPLAYSTATUSMESSAGE GWlxDisplayStatusMessage;
PGWLXGETSTATUSMESSAGE GWlxGetStatusMessage;
PGWLXREMOVESTATUSMESSAGE GWlxRemoveStatusMessage;
/* GINA 1.4 */
PGWLXGETCONSOLESWITCHCREDENTIALS GWlxGetConsoleSwitchCredentials;
PGWLXRECONNECTNOTIFY GWlxReconnectNotify;
PGWLXDISCONNECTNOTIFY GWlxDisconnectNotify;



/**
 * DLL entry point.
 */
BOOL WINAPI DllMain(HINSTANCE hInstance,
                    DWORD     dwReason,
                    LPVOID    lpReserved)
{
    switch (dwReason)
    {
        case DLL_PROCESS_ATTACH:
        {
            DisableThreadLibraryCalls(hInstance);
            hDllInstance = hInstance;
            break;
        }

        case DLL_PROCESS_DETACH:
        default:
            break;
    }
    return TRUE;
}

BOOL WINAPI WlxNegotiate(DWORD dwWinlogonVersion,
                         DWORD *pdwDllVersion)
{
    HINSTANCE hDll;

#ifdef DEBUG
    /* enable full log output */
    RTLogGroupSettings(RTLogDefaultInstance(), "all=~0");
#endif

    Log(("VBoxGINA::WlxNegotiate: dwWinlogonVersion: %d\n", dwWinlogonVersion));

    /* load the standard Microsoft GINA DLL */
    if (!(hDll = LoadLibrary(TEXT("MSGINA.DLL"))))
    {
        Log(("VBoxGINA::WlxNegotiate: failed loading MSGINA! last error = %d\n", GetLastError()));
        return FALSE;
    }

    /*
     * Now get the entry points of the MSGINA
     */
    GWlxNegotiate = (PGWLXNEGOTIATE)GetProcAddress(hDll, "WlxNegotiate");
    if (!GWlxNegotiate)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxNegotiate\n"));
        return FALSE;
    }
    GWlxInitialize = (PGWLXINITIALIZE)GetProcAddress(hDll, "WlxInitialize");
    if (!GWlxInitialize)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxInitialize\n"));
        return FALSE;
    }
    GWlxDisplaySASNotice =
        (PGWLXDISPLAYSASNOTICE)GetProcAddress(hDll, "WlxDisplaySASNotice");
    if (!GWlxDisplaySASNotice)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxDisplaySASNotice\n"));
        return FALSE;
    }
    GWlxLoggedOutSAS =
        (PGWLXLOGGEDOUTSAS)GetProcAddress(hDll, "WlxLoggedOutSAS");
    if (!GWlxLoggedOutSAS)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxLoggedOutSAS\n"));
        return FALSE;
    }
    GWlxActivateUserShell =
        (PGWLXACTIVATEUSERSHELL)GetProcAddress(hDll, "WlxActivateUserShell");
    if (!GWlxActivateUserShell)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxActivateUserShell\n"));
        return FALSE;
    }
    GWlxLoggedOnSAS =
        (PGWLXLOGGEDONSAS)GetProcAddress(hDll, "WlxLoggedOnSAS");
    if (!GWlxLoggedOnSAS)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxLoggedOnSAS\n"));
        return FALSE;
    }
    GWlxDisplayLockedNotice =
        (PGWLXDISPLAYLOCKEDNOTICE)GetProcAddress(hDll, "WlxDisplayLockedNotice");
    if (!GWlxDisplayLockedNotice)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxDisplayLockedNotice\n"));
        return FALSE;
    }
    GWlxIsLockOk = (PGWLXISLOCKOK)GetProcAddress(hDll, "WlxIsLockOk");
    if (!GWlxIsLockOk)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxIsLockOk\n"));
        return FALSE;
    }
    GWlxWkstaLockedSAS =
        (PGWLXWKSTALOCKEDSAS)GetProcAddress(hDll, "WlxWkstaLockedSAS");
    if (!GWlxWkstaLockedSAS)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxWkstaLockedSAS\n"));
        return FALSE;
    }
    GWlxIsLogoffOk = (PGWLXISLOGOFFOK)GetProcAddress(hDll, "WlxIsLogoffOk");
    if (!GWlxIsLogoffOk)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxIsLogoffOk\n"));
        return FALSE;
    }
    GWlxLogoff = (PGWLXLOGOFF)GetProcAddress(hDll, "WlxLogoff");
    if (!GWlxLogoff)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxLogoff\n"));
        return FALSE;
    }
    GWlxShutdown = (PGWLXSHUTDOWN)GetProcAddress(hDll, "WlxShutdown");
    if (!GWlxShutdown)
    {
        Log(("VBoxGINA::WlxNegotiate: failed resolving WlxShutdown\n"));
        return FALSE;
    }
    /* GINA 1.1, optional */
    GWlxStartApplication = (PGWLXSTARTAPPLICATION)GetProcAddress(hDll, "WlxStartApplication");
    GWlxScreenSaverNotify = (PGWLXSCREENSAVERNOTIFY)GetProcAddress(hDll, "WlxScreenSaverNotify");
    /* GINA 1.3, optional */
    GWlxNetworkProviderLoad = (PGWLXNETWORKPROVIDERLOAD)GetProcAddress( hDll, "WlxNetworkProviderLoad");
    GWlxDisplayStatusMessage = (PGWLXDISPLAYSTATUSMESSAGE)GetProcAddress( hDll, "WlxDisplayStatusMessage");
    GWlxGetStatusMessage = (PGWLXGETSTATUSMESSAGE)GetProcAddress( hDll, "WlxGetStatusMessage");
    GWlxRemoveStatusMessage = (PGWLXREMOVESTATUSMESSAGE)GetProcAddress( hDll, "WlxRemoveStatusMessage");
    /* GINA 1.4, optional */
    GWlxGetConsoleSwitchCredentials =
        (PGWLXGETCONSOLESWITCHCREDENTIALS)GetProcAddress(hDll, "WlxGetConsoleSwitchCredentials");
    GWlxReconnectNotify = (PGWLXRECONNECTNOTIFY)GetProcAddress(hDll, "WlxReconnectNotify");
    GWlxDisconnectNotify = (PGWLXDISCONNECTNOTIFY)GetProcAddress(hDll, "WlxDisconnectNotify");
    Log(("VBoxGINA::WlxNegotiate: optional function pointers:\n"
             "  WlxStartApplication: %p\n"
             "  WlxScreenSaverNotify: %p\n"
             "  WlxNetworkProviderLoad: %p\n"
             "  WlxDisplayStatusMessage: %p\n"
             "  WlxGetStatusMessage: %p\n"
             "  WlxRemoveStatusMessage: %p\n"
             "  WlxGetConsoleSwitchCredentials: %p\n"
             "  WlxReconnectNotify: %p\n"
             "  WlxDisconnectNotify: %p\n",
             GWlxStartApplication, GWlxScreenSaverNotify, GWlxNetworkProviderLoad,
             GWlxDisplayStatusMessage, GWlxGetStatusMessage, GWlxRemoveStatusMessage,
             GWlxGetConsoleSwitchCredentials, GWlxReconnectNotify, GWlxDisconnectNotify));

    wlxVersion = dwWinlogonVersion;

    /* forward call */
    return GWlxNegotiate(dwWinlogonVersion, pdwDllVersion);
}


BOOL WINAPI WlxInitialize(LPWSTR lpWinsta, HANDLE hWlx, PVOID pvReserved,
                          PVOID pWinlogonFunctions, PVOID *pWlxContext)
{
    Log(("VBoxGINA::WlxInitialize\n"));

    /* store Winlogon function table */
    pWlxFuncs = (PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions;

    /* store handle to Winlogon service*/
    hGinaWlx = hWlx;

    /* hook the dialogs */
    hookDialogBoxes(pWlxFuncs, wlxVersion);

    /* forward call */
    return GWlxInitialize(lpWinsta, hWlx, pvReserved, pWinlogonFunctions, pWlxContext);
}


VOID WINAPI WlxDisplaySASNotice(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxDisplaySASNotice\n"));

    /* check if there are credentials for us, if so simulat C-A-D */
    if (credentialsAvailable())
    {
        Log(("VBoxGINA::WlxDisplaySASNotice: simulating C-A-D\n"));
        /* automatic C-A-D */
        pWlxFuncs->WlxSasNotify(hGinaWlx, WLX_SAS_TYPE_CTRL_ALT_DEL);
    }
    else
    {
        Log(("VBoxGINA::WlxDisplaySASNotice: starting credentials poller\n"));
        /* start the credentials poller thread */
        credentialsPollerCreate();
        /* forward call to MSGINA */
        GWlxDisplaySASNotice(pWlxContext);
    }
}


int WINAPI WlxLoggedOutSAS(PVOID pWlxContext, DWORD dwSasType, PLUID pAuthenticationId,
                           PSID pLogonSid, PDWORD pdwOptions, PHANDLE phToken,
                           PWLX_MPR_NOTIFY_INFO pMprNotifyInfo, PVOID *pProfile)
{
    Log(("VBoxGINA::WlxLoggedOutSAS\n"));

    /* when performing a direct logon without C-A-D, our poller might not be running */
    if (!credentialsAvailable())
    {
        credentialsPollerCreate();
    }

    int iRet;
    iRet = GWlxLoggedOutSAS(pWlxContext, dwSasType, pAuthenticationId, pLogonSid,
                            pdwOptions, phToken, pMprNotifyInfo, pProfile);

    if (iRet == WLX_SAS_ACTION_LOGON)
    {
        //
        // copy pMprNotifyInfo and pLogonSid for later use
        //

        // pMprNotifyInfo->pszUserName
        // pMprNotifyInfo->pszDomain
        // pMprNotifyInfo->pszPassword
        // pMprNotifyInfo->pszOldPassword

    }

    return iRet;
}


BOOL WINAPI WlxActivateUserShell(PVOID pWlxContext, PWSTR pszDesktopName,
                                 PWSTR pszMprLogonScript, PVOID pEnvironment)
{
    Log(("VBoxGINA::WlxActivateUserShell\n"));

    /* forward call to MSGINA */
    return GWlxActivateUserShell(pWlxContext, pszDesktopName, pszMprLogonScript, pEnvironment);
}


int WINAPI WlxLoggedOnSAS(PVOID pWlxContext, DWORD dwSasType, PVOID pReserved)
{
    HKEY hKey;
    DWORD dwValue = 1;
    DWORD dwSize;
    DWORD dwType;

    Log(("VBoxGINA::WlxLoggedOnSAS\n"));

    /* Winlogon registry path */
    static TCHAR szPath[] = TEXT("Software\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon");

    if (!RegOpenKey(HKEY_LOCAL_MACHINE, szPath, &hKey))
    {
        dwSize = sizeof(DWORD);
        RegQueryValueEx(hKey, TEXT("SAS_S"), 0, &dwType, (PBYTE)&dwValue, &dwSize);
        RegCloseKey(hKey);
    }
    else
        Log(("VBoxGINA::WlxLoggedOnSAS: could not open registry key! last error: %d\n", GetLastError()));

    if (dwValue)
        return WLX_SAS_ACTION_NONE;
    else
        /* forward call to MSGINA */
        return GWlxLoggedOnSAS(pWlxContext, dwSasType, pReserved);
}

VOID WINAPI WlxDisplayLockedNotice(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxDisplayLockedNotice\n"));
    /* forward call to MSGINA */
    GWlxDisplayLockedNotice(pWlxContext);
}


BOOL WINAPI WlxIsLockOk(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxIsLockOk\n"));
    /* forward call to MSGINA */
    return GWlxIsLockOk(pWlxContext);
}

int WINAPI WlxWkstaLockedSAS(PVOID pWlxContext, DWORD dwSasType)
{
    Log(("VBoxGINA::WlxWkstaLockedSAS\n"));
    /* forward call to MSGINA */
    return GWlxWkstaLockedSAS(pWlxContext, dwSasType);
}

BOOL WINAPI WlxIsLogoffOk(PVOID pWlxContext)
{
    BOOL bSuccess;

    Log(("VBoxGINA::WlxIsLogoffOk\n"));

    bSuccess = GWlxIsLogoffOk(pWlxContext);

    if (bSuccess)
    {
        //
        // if it's ok to logoff, finish with the stored credentials
        // and scrub the buffers
        //

    }
    return bSuccess;
}


VOID WINAPI WlxLogoff(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxLogoff\n"));

    /* forward call to MSGINA */
    GWlxLogoff(pWlxContext);
}


VOID WINAPI WlxShutdown(PVOID pWlxContext, DWORD ShutdownType)
{
    Log(("VBoxGINA::WlxShutdown\n"));

    /* forward call to MSGINA */
    GWlxShutdown(pWlxContext, ShutdownType);
}


/*
 * GINA 1.1 entry points
 */

BOOL WINAPI WlxScreenSaverNotify(PVOID pWlxContext, BOOL *pSecure)
{
    Log(("VBoxGINA::WlxScreenSaverNotify\n"));

    /* forward to MSGINA if present */
    if (GWlxScreenSaverNotify)
        return GWlxScreenSaverNotify(pWlxContext, pSecure);
    /* return something intelligent */
    *pSecure = TRUE;
    return TRUE;
}

BOOL WINAPI WlxStartApplication(PVOID pWlxContext, PWSTR pszDesktopName,
                                PVOID pEnvironment, PWSTR pszCmdLine)
{
    Log(("VBoxGINA::WlxStartApplication\n"));

    /* forward to MSGINA if present */
    if (GWlxStartApplication)
        return GWlxStartApplication(pWlxContext, pszDesktopName, pEnvironment, pszCmdLine);
    return FALSE;
}

/*
 * GINA 1.3 entry points
 */
BOOL WINAPI WlxNetworkProviderLoad (PVOID pWlxContext, PWLX_MPR_NOTIFY_INFO pNprNotifyInfo)
{
    Log(("VBoxGINA::WlxNetworkProviderLoad\n"));

    /* forward to MSGINA if present */
    if (GWlxNetworkProviderLoad)
        return GWlxNetworkProviderLoad(pWlxContext, pNprNotifyInfo);
    return FALSE;
}


BOOL WINAPI WlxDisplayStatusMessage(PVOID pWlxContext, HDESK hDesktop, DWORD dwOptions,
                                    PWSTR pTitle, PWSTR pMessage)
{
    Log(("VBoxGINA::WlxDisplayStatusMessage\n"));

    /* forward to MSGINA if present */
    if (GWlxDisplayStatusMessage)
        return GWlxDisplayStatusMessage(pWlxContext, hDesktop, dwOptions, pTitle, pMessage);
    return FALSE;
}


BOOL WINAPI WlxGetStatusMessage(PVOID pWlxContext, DWORD *pdwOptions,
                                PWSTR pMessage, DWORD dwBufferSize)
{
    Log(("VBoxGINA::WlxGetStatusMessage\n"));

    /* forward to MSGINA if present */
    if (GWlxGetStatusMessage)
        return GWlxGetStatusMessage(pWlxContext, pdwOptions, pMessage, dwBufferSize);
    return FALSE;
}


BOOL WINAPI WlxRemoveStatusMessage(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxRemoveStatusMessage\n"));

    /* forward to MSGINA if present */
    if (GWlxRemoveStatusMessage)
        return GWlxRemoveStatusMessage(pWlxContext);
    return FALSE;
}


/*
 * GINA 1.4 entry points
 */

BOOL WINAPI WlxGetConsoleSwitchCredentials(PVOID pWlxContext,PVOID pCredInfo)
{
    Log(("VBoxGINA::WlxGetConsoleSwitchCredentials\n"));

    /* forward call to MSGINA if present */
    if (GWlxGetConsoleSwitchCredentials)
        return GWlxGetConsoleSwitchCredentials(pWlxContext,pCredInfo);
    return FALSE;
}

VOID WINAPI WlxReconnectNotify(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxReconnectNotify\n"));

    /* forward to MSGINA if present */
    if (GWlxReconnectNotify)
        GWlxReconnectNotify(pWlxContext);
}

VOID WINAPI WlxDisconnectNotify(PVOID pWlxContext)
{
    Log(("VBoxGINA::WlxDisconnectNotify\n"));

    /* forward to MSGINA if present */
    if (GWlxDisconnectNotify)
        GWlxDisconnectNotify(pWlxContext);
}
