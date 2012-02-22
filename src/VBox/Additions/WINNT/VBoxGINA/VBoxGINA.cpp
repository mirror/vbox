/** @file
 *
 * VBoxGINA -- Windows Logon DLL for VirtualBox
 *
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include <iprt/buildconfig.h>
#include <iprt/initterm.h>

#include <VBox/VBoxGuestLib.h>

#include "winwlx.h"
#include "VBoxGINA.h"
#include "Helper.h"
#include "Dialog.h"

/*
 * Global variables.
 */

/** DLL instance handle. */
HINSTANCE hDllInstance;

/** Version of Winlogon. */
DWORD wlxVersion;

/** Handle to Winlogon service. */
HANDLE hGinaWlx;
/** Winlog function dispatch table. */
PWLX_DISPATCH_VERSION_1_1 pWlxFuncs;

/**
 * Function pointers to MSGINA entry points.
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
/* GINA 1.1. */
PGWLXSTARTAPPLICATION GWlxStartApplication;
PGWLXSCREENSAVERNOTIFY GWlxScreenSaverNotify;
/* GINA 1.3. */
PGWLXNETWORKPROVIDERLOAD GWlxNetworkProviderLoad;
PGWLXDISPLAYSTATUSMESSAGE GWlxDisplayStatusMessage;
PGWLXGETSTATUSMESSAGE GWlxGetStatusMessage;
PGWLXREMOVESTATUSMESSAGE GWlxRemoveStatusMessage;
/* GINA 1.4. */
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
            RTR3InitDll(0 /* Flags */);
            VbglR3Init();

            VBoxGINALoadConfiguration();

            VBoxGINAVerbose(0, "VBoxGINA: v%s r%s (%s %s) loaded\n",
                            RTBldCfgVersion(), RTBldCfgRevisionStr(),
                            __DATE__, __TIME__);

            DisableThreadLibraryCalls(hInstance);
            hDllInstance = hInstance;
            break;
        }

        case DLL_PROCESS_DETACH:
        {
            VBoxGINAVerbose(0, "VBoxGINA: Unloaded\n");
            VbglR3Term();
            /// @todo RTR3Term();
            break;
        }

        default:
            break;
    }
    return TRUE;
}


BOOL WINAPI WlxNegotiate(DWORD dwWinlogonVersion,
                         DWORD *pdwDllVersion)
{
    HINSTANCE hDll;

    VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: dwWinlogonVersion: %ld\n", dwWinlogonVersion);

    /* Load the standard Microsoft GINA DLL. */
    if (!(hDll = LoadLibrary(TEXT("MSGINA.DLL"))))
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed loading MSGINA! Last error=%ld\n", GetLastError());
        return FALSE;
    }

    /*
     * Now get the entry points of the MSGINA
     */
    GWlxNegotiate = (PGWLXNEGOTIATE)GetProcAddress(hDll, "WlxNegotiate");
    if (!GWlxNegotiate)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxNegotiate\n");
        return FALSE;
    }
    GWlxInitialize = (PGWLXINITIALIZE)GetProcAddress(hDll, "WlxInitialize");
    if (!GWlxInitialize)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxInitialize\n");
        return FALSE;
    }
    GWlxDisplaySASNotice =
        (PGWLXDISPLAYSASNOTICE)GetProcAddress(hDll, "WlxDisplaySASNotice");
    if (!GWlxDisplaySASNotice)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxDisplaySASNotice\n");
        return FALSE;
    }
    GWlxLoggedOutSAS =
        (PGWLXLOGGEDOUTSAS)GetProcAddress(hDll, "WlxLoggedOutSAS");
    if (!GWlxLoggedOutSAS)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxLoggedOutSAS\n");
        return FALSE;
    }
    GWlxActivateUserShell =
        (PGWLXACTIVATEUSERSHELL)GetProcAddress(hDll, "WlxActivateUserShell");
    if (!GWlxActivateUserShell)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxActivateUserShell\n");
        return FALSE;
    }
    GWlxLoggedOnSAS =
        (PGWLXLOGGEDONSAS)GetProcAddress(hDll, "WlxLoggedOnSAS");
    if (!GWlxLoggedOnSAS)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxLoggedOnSAS\n");
        return FALSE;
    }
    GWlxDisplayLockedNotice =
        (PGWLXDISPLAYLOCKEDNOTICE)GetProcAddress(hDll, "WlxDisplayLockedNotice");
    if (!GWlxDisplayLockedNotice)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxDisplayLockedNotice\n");
        return FALSE;
    }
    GWlxIsLockOk = (PGWLXISLOCKOK)GetProcAddress(hDll, "WlxIsLockOk");
    if (!GWlxIsLockOk)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxIsLockOk\n");
        return FALSE;
    }
    GWlxWkstaLockedSAS =
        (PGWLXWKSTALOCKEDSAS)GetProcAddress(hDll, "WlxWkstaLockedSAS");
    if (!GWlxWkstaLockedSAS)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxWkstaLockedSAS\n");
        return FALSE;
    }
    GWlxIsLogoffOk = (PGWLXISLOGOFFOK)GetProcAddress(hDll, "WlxIsLogoffOk");
    if (!GWlxIsLogoffOk)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxIsLogoffOk\n");
        return FALSE;
    }
    GWlxLogoff = (PGWLXLOGOFF)GetProcAddress(hDll, "WlxLogoff");
    if (!GWlxLogoff)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxLogoff\n");
        return FALSE;
    }
    GWlxShutdown = (PGWLXSHUTDOWN)GetProcAddress(hDll, "WlxShutdown");
    if (!GWlxShutdown)
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: failed resolving WlxShutdown\n");
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
    VBoxGINAVerbose(0, "VBoxGINA::WlxNegotiate: optional function pointers:\n"
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
                    GWlxGetConsoleSwitchCredentials, GWlxReconnectNotify, GWlxDisconnectNotify);

    wlxVersion = dwWinlogonVersion;

    /* Acknowledge interface version. */
    if (pdwDllVersion)
        *pdwDllVersion = dwWinlogonVersion;

    return TRUE; /* We're ready to rumble! */
}


BOOL WINAPI WlxInitialize(LPWSTR lpWinsta, HANDLE hWlx, PVOID pvReserved,
                          PVOID pWinlogonFunctions, PVOID *pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxInitialize\n");

    /* Store Winlogon function table */
    pWlxFuncs = (PWLX_DISPATCH_VERSION_1_1)pWinlogonFunctions;

    /* Store handle to Winlogon service*/
    hGinaWlx = hWlx;

    VBoxGINAReportStatus(VBoxGuestFacilityStatus_Init);

    /* Hook the dialogs */
    hookDialogBoxes(pWlxFuncs, wlxVersion);

    /* Forward call */
    return GWlxInitialize(lpWinsta, hWlx, pvReserved, pWinlogonFunctions, pWlxContext);
}


VOID WINAPI WlxDisplaySASNotice(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxDisplaySASNotice\n");

    /* Check if there are credentials for us, if so simulate C-A-D */
    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_SUCCESS(rc))
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxDisplaySASNotice: simulating C-A-D\n");
        /* Wutomatic C-A-D */
        pWlxFuncs->WlxSasNotify(hGinaWlx, WLX_SAS_TYPE_CTRL_ALT_DEL);
    }
    else
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxDisplaySASNotice: starting credentials poller\n");
        /* start the credentials poller thread */
        VBoxGINACredentialsPollerCreate();
        /* Forward call to MSGINA. */
        GWlxDisplaySASNotice(pWlxContext);
    }
}


int WINAPI WlxLoggedOutSAS(PVOID pWlxContext, DWORD dwSasType, PLUID pAuthenticationId,
                           PSID pLogonSid, PDWORD pdwOptions, PHANDLE phToken,
                           PWLX_MPR_NOTIFY_INFO pMprNotifyInfo, PVOID *pProfile)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxLoggedOutSAS\n");

    /* When performing a direct logon without C-A-D, our poller might not be running */
    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_FAILURE(rc))
        VBoxGINACredentialsPollerCreate();

    int iRet;
    iRet = GWlxLoggedOutSAS(pWlxContext, dwSasType, pAuthenticationId, pLogonSid,
                            pdwOptions, phToken, pMprNotifyInfo, pProfile);

    if (iRet == WLX_SAS_ACTION_LOGON)
    {
        //
        // Copy pMprNotifyInfo and pLogonSid for later use
        //

        // pMprNotifyInfo->pszUserName
        // pMprNotifyInfo->pszDomain
        // pMprNotifyInfo->pszPassword
        // pMprNotifyInfo->pszOldPassword
    }

    return iRet;
}


/**
 * WinLogon calls this function following a successful logon to request that the GINA activate the user's shell program.
 */
BOOL WINAPI WlxActivateUserShell(PVOID pWlxContext, PWSTR pszDesktopName,
                                 PWSTR pszMprLogonScript, PVOID pEnvironment)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxActivateUserShell\n");

    /*
     * Report status "terminated" to the host -- this means that a user
     * got logged in (either manually or automatically using the provided credentials).
     */
    VBoxGINAReportStatus(VBoxGuestFacilityStatus_Terminated);

    /* Forward call to MSGINA. */
    return GWlxActivateUserShell(pWlxContext, pszDesktopName, pszMprLogonScript, pEnvironment);
}


int WINAPI WlxLoggedOnSAS(PVOID pWlxContext, DWORD dwSasType, PVOID pReserved)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxLoggedOnSAS: dwSasType=%ld\n", dwSasType);

    /*
     * We don't want to do anything special here since the OS should behave
     * as VBoxGINA wouldn't have been installed. So pass all calls down
     * to the original MSGINA.
     */

    /* Forward call to MSGINA. */
    VBoxGINAVerbose(0, "VBoxGINA::WlxLoggedOnSAS: Forwarding call to MSGINA ...\n");
    return GWlxLoggedOnSAS(pWlxContext, dwSasType, pReserved);
}

VOID WINAPI WlxDisplayLockedNotice(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxDisplayLockedNotice\n");

    /* Check if there are credentials for us, if so simulate C-A-D */
    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_SUCCESS(rc))
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxDisplayLockedNotice: simulating C-A-D\n");
        /* Automatic C-A-D */
        pWlxFuncs->WlxSasNotify(hGinaWlx, WLX_SAS_TYPE_CTRL_ALT_DEL);
    }
    else
    {
        VBoxGINAVerbose(0, "VBoxGINA::WlxDisplayLockedNotice: starting credentials poller\n");
        /* start the credentials poller thread */
        VBoxGINACredentialsPollerCreate();
        /* Forward call to MSGINA. */
        GWlxDisplayLockedNotice(pWlxContext);
    }
}


/*
 * Winlogon calls this function before it attempts to lock the workstation.
 */
BOOL WINAPI WlxIsLockOk(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxIsLockOk\n");
    /* Forward call to MSGINA. */
    return GWlxIsLockOk(pWlxContext);
}


int WINAPI WlxWkstaLockedSAS(PVOID pWlxContext, DWORD dwSasType)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxWkstaLockedSAS, dwSasType=%ld\n", dwSasType);

    /* When performing a direct logon without C-A-D, our poller might not be running */
    int rc = VbglR3CredentialsQueryAvailability();
    if (RT_FAILURE(rc))
        VBoxGINACredentialsPollerCreate();

    /* Forward call to MSGINA. */
    return GWlxWkstaLockedSAS(pWlxContext, dwSasType);
}


BOOL WINAPI WlxIsLogoffOk(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxIsLogoffOk\n");

    return GWlxIsLogoffOk(pWlxContext);
}


/*
 * Winlogon calls this function to notify the GINA of a logoff operation on this
 * workstation. This allows the GINA to perform any logoff operations that may be required.
 */
VOID WINAPI WlxLogoff(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxLogoff\n");

    /* No need to report the "active" status to the host here -- this will be done
     * when VBoxGINA gets the chance to hook the dialogs (again). */

    /* Forward call to MSGINA. */
    GWlxLogoff(pWlxContext);
}


/*
 * Winlogon calls this function just before shutting down.
 * This allows the GINA to perform any necessary shutdown tasks.
 * Will be called *after* WlxLogoff!
 */
VOID WINAPI WlxShutdown(PVOID pWlxContext, DWORD ShutdownType)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxShutdown\n");

    /*
     * Report status "inactive" to the host -- this means the
     * auto-logon feature won't be active anymore at this point
     * (until it maybe gets loaded again after a reboot).
     */
    VBoxGINAReportStatus(VBoxGuestFacilityStatus_Inactive);

    /* Forward call to MSGINA. */
    GWlxShutdown(pWlxContext, ShutdownType);
}


/*
 * GINA 1.1 entry points
 */
BOOL WINAPI WlxScreenSaverNotify(PVOID pWlxContext, BOOL *pSecure)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxScreenSaverNotify\n");

    /* Forward to MSGINA if present. */
    if (GWlxScreenSaverNotify)
        return GWlxScreenSaverNotify(pWlxContext, pSecure);
    /* Return something intelligent */
    *pSecure = TRUE;
    return TRUE;
}


BOOL WINAPI WlxStartApplication(PVOID pWlxContext, PWSTR pszDesktopName,
                                PVOID pEnvironment, PWSTR pszCmdLine)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxStartApplication: pWlxCtx=%p, pszDesktopName=%ls, pEnvironment=%p, pszCmdLine=%ls\n",
                    pWlxContext, pszDesktopName, pEnvironment, pszCmdLine);

    /* Forward to MSGINA if present. */
    if (GWlxStartApplication)
        return GWlxStartApplication(pWlxContext, pszDesktopName, pEnvironment, pszCmdLine);
    return FALSE;
}


/*
 * GINA 1.3 entry points
 */
BOOL WINAPI WlxNetworkProviderLoad(PVOID pWlxContext, PWLX_MPR_NOTIFY_INFO pNprNotifyInfo)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxNetworkProviderLoad\n");

    /* Forward to MSGINA if present. */
    if (GWlxNetworkProviderLoad)
        return GWlxNetworkProviderLoad(pWlxContext, pNprNotifyInfo);
    return FALSE;
}


BOOL WINAPI WlxDisplayStatusMessage(PVOID pWlxContext, HDESK hDesktop, DWORD dwOptions,
                                    PWSTR pTitle, PWSTR pMessage)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxDisplayStatusMessage\n");

    /* Forward to MSGINA if present. */
    if (GWlxDisplayStatusMessage)
        return GWlxDisplayStatusMessage(pWlxContext, hDesktop, dwOptions, pTitle, pMessage);
    return FALSE;
}


BOOL WINAPI WlxGetStatusMessage(PVOID pWlxContext, DWORD *pdwOptions,
                                PWSTR pMessage, DWORD dwBufferSize)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxGetStatusMessage\n");

    /* Forward to MSGINA if present. */
    if (GWlxGetStatusMessage)
        return GWlxGetStatusMessage(pWlxContext, pdwOptions, pMessage, dwBufferSize);
    return FALSE;
}


BOOL WINAPI WlxRemoveStatusMessage(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxRemoveStatusMessage\n");

    /* Forward to MSGINA if present. */
    if (GWlxRemoveStatusMessage)
        return GWlxRemoveStatusMessage(pWlxContext);
    return FALSE;
}


/*
 * GINA 1.4 entry points
 */
BOOL WINAPI WlxGetConsoleSwitchCredentials(PVOID pWlxContext,PVOID pCredInfo)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxGetConsoleSwitchCredentials\n");

    /* Forward call to MSGINA if present */
    if (GWlxGetConsoleSwitchCredentials)
        return GWlxGetConsoleSwitchCredentials(pWlxContext,pCredInfo);
    return FALSE;
}


VOID WINAPI WlxReconnectNotify(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxReconnectNotify\n");

    /* Forward to MSGINA if present. */
    if (GWlxReconnectNotify)
        GWlxReconnectNotify(pWlxContext);
}


VOID WINAPI WlxDisconnectNotify(PVOID pWlxContext)
{
    VBoxGINAVerbose(0, "VBoxGINA::WlxDisconnectNotify\n");

    /* Forward to MSGINA if present. */
    if (GWlxDisconnectNotify)
        GWlxDisconnectNotify(pWlxContext);
}


DWORD WINAPI VBoxGINADebug(void)
{
#ifdef DEBUG
    DWORD dwVersion;
    BOOL fRes = WlxNegotiate(WLX_VERSION_1_4, &dwVersion);
    if (!fRes)
        return 1;

    void* pWlxContext = NULL;
    WLX_DISPATCH_VERSION_1_4 wlxDispatch;
    ZeroMemory(&wlxDispatch, sizeof(WLX_DISPATCH_VERSION_1_4));

    fRes = WlxInitialize(0, 0,
                         NULL /* Reserved */,
                         NULL /* Winlogon functions */,
                         &pWlxContext);
    if (!fRes)
        return 2;

    WlxDisplaySASNotice(pWlxContext);

    char szSID[MAX_PATH];
    LUID luidAuth;
    DWORD dwOpts;
    WLX_MPR_NOTIFY_INFO wlxNotifyInfo;
    void* pvProfile;
    HANDLE hToken;
    int iRes = WlxLoggedOutSAS(pWlxContext, WLX_SAS_TYPE_CTRL_ALT_DEL,
                               &luidAuth, szSID,
                               &dwOpts, &hToken, &wlxNotifyInfo, &pvProfile);
    return iRes;
#else
    return 0;
#endif
}

