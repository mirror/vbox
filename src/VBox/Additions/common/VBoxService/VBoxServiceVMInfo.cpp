/* $Id$ */
/** @file
 * VBoxService - Virtual Machine Information for the Host.
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
#ifdef RT_OS_WINDOWS
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <Ntsecapi.h>
#else
# define __STDC_LIMIT_MACROS
# include <arpa/inet.h>
# include <errno.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
# include <unistd.h>
# ifndef RT_OS_FREEBSD /* The header does not exist anymore since FreeBSD 9-current */
#  include <utmp.h>
# endif
# ifdef RT_OS_SOLARIS
#  include <sys/sockio.h>
# endif
#endif

#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServicePropCache.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The vminfo interval (millseconds). */
static uint32_t                 g_cMsVMInfoInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI          g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t                 g_uVMInfoGuestPropSvcClientID = 0;
/** Number of logged in users in OS. */
static uint32_t                 g_cVMInfoLoggedInUsers = UINT32_MAX;
/** The guest property cache. */
static VBOXSERVICEVEPROPCACHE   g_VMInfoPropCache;


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceVMInfoPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceVMInfoOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    int rc = -1;
    if (ppszShort)
        /* no short options */;
    else if (!strcmp(argv[*pi], "--vminfo-interval"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_cMsVMInfoInterval, 1, UINT32_MAX - 1);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceVMInfoInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_cMsVMInfoInterval)
        g_cMsVMInfoInterval = g_DefaultInterval * 1000;
    if (!g_cMsVMInfoInterval)
        g_cMsVMInfoInterval = 10 * 1000;

    int rc = RTSemEventMultiCreate(&g_hVMInfoEvent);
    AssertRCReturn(rc, rc);

#ifdef RT_OS_WINDOWS
    /** @todo r=bird: call a windows specific init function and move
     *        g_pfnWTSGetActiveConsoleSessionId out of the global scope.  */
    /* Get function pointers. */
    HMODULE hKernel32 = LoadLibrary("kernel32");
    if (hKernel32 != NULL)
    {
        g_pfnWTSGetActiveConsoleSessionId = (PFNWTSGETACTIVECONSOLESESSIONID)GetProcAddress(hKernel32, "WTSGetActiveConsoleSessionId");
        FreeLibrary(hKernel32);
    }
#endif

    rc = VbglR3GuestPropConnect(&g_uVMInfoGuestPropSvcClientID);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "VMInfo: Property Service Client ID: %#x\n", g_uVMInfoGuestPropSvcClientID);
    else
    {
        /* If the service was not found, we disable this service without
           causing VBoxService to fail. */
        if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
        {
            VBoxServiceVerbose(0, "VMInfo: Guest property service is not available, disabling the service\n");
            rc = VERR_SERVICE_DISABLED;
        }
        else
            VBoxServiceError("VMInfo: Failed to connect to the guest property service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_hVMInfoEvent);
        g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
    }

    if (RT_SUCCESS(rc))
    {
        VBoxServicePropCacheCreate(&g_VMInfoPropCache, g_uVMInfoGuestPropSvcClientID);

        /*
         * Declare some guest properties with flags and reset values.
         */
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsersList",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY, NULL /* Delete on exit */);
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsers",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY, "0");
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/NoLoggedInUsers",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY, "true");
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/Net/Count",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY | VBOXSERVICEPROPCACHEFLAG_ALWAYS_UPDATE, NULL /* Delete on exit */);
    }
    return rc;
}


/**
 * Writes the properties that won't change while the service is running.
 *
 * Errors are ignored.
 */
static void VBoxServiceVMInfoWriteFixedProperties(void)
{
    /*
     * First get OS information that won't change.
     */
    char szInfo[256];
    int rc = RTSystemQueryOSInfo(RTSYSOSINFO_PRODUCT, szInfo, sizeof(szInfo));
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestInfo/OS/Product",
                          "%s", RT_FAILURE(rc) ? "" : szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_RELEASE, szInfo, sizeof(szInfo));
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestInfo/OS/Release",
                          "%s", RT_FAILURE(rc) ? "" : szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_VERSION, szInfo, sizeof(szInfo));
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestInfo/OS/Version",
                          "%s", RT_FAILURE(rc) ? "" : szInfo);

    rc = RTSystemQueryOSInfo(RTSYSOSINFO_SERVICE_PACK, szInfo, sizeof(szInfo));
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestInfo/OS/ServicePack",
                          "%s", RT_FAILURE(rc) ? "" : szInfo);

    /*
     * Retrieve version information about Guest Additions and installed files (components).
     */
    char *pszAddVer;
    char *pszAddRev;
    rc = VbglR3GetAdditionsVersion(&pszAddVer, &pszAddRev);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/Version",
                          "%s", RT_FAILURE(rc) ? "" : pszAddVer);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/Revision",
                          "%s", RT_FAILURE(rc) ? "" : pszAddRev);
    if (RT_SUCCESS(rc))
    {
        RTStrFree(pszAddVer);
        RTStrFree(pszAddRev);
    }

#ifdef RT_OS_WINDOWS
    /*
     * Do windows specific properties.
     */
    char *pszInstDir;
    rc = VbglR3GetAdditionsInstallationPath(&pszInstDir);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/InstallDir",
                          "%s", RT_FAILURE(rc) ? "" :  pszInstDir);
    if (RT_SUCCESS(rc))
        RTStrFree(pszInstDir);

    VBoxServiceWinGetComponentVersions(g_uVMInfoGuestPropSvcClientID);
#endif
}


/**
 * Provide information about active users.
 */
int VBoxServiceVMInfoWriteUsers()
{
    int rc;
    char szUserList[4096] = {0};
    uint32_t cUsersInList = 0;

#ifdef RT_OS_WINDOWS
# ifndef TARGET_NT4
    PLUID       paSessions = NULL;
    ULONG       cSession = 0;
    NTSTATUS    r = 0;

    /* This function can report stale or orphaned interactive logon sessions
       of already logged off users (especially in Windows 2000). */
    r = ::LsaEnumerateLogonSessions(&cSession, &paSessions);
    VBoxServiceVerbose(3, "Users: Found %ld users.\n", cSession);
    if (r != STATUS_SUCCESS)
    {
        VBoxServiceError("LsaEnumerate failed %lu\n", LsaNtStatusToWinError(r));
        return RTErrConvertFromWin32(LsaNtStatusToWinError(r));
    }

    PVBOXSERVICEVMINFOPROC  paProcs;
    DWORD                   cProcs;
    rc = VBoxServiceVMInfoWinProcessesEnumerate(&paProcs, &cProcs);
    if (RT_SUCCESS(rc))
    {
        for (ULONG i = 0; i < cSession; i++)
        {
            VBOXSERVICEVMINFOUSER UserInfo;
            if (   VBoxServiceVMInfoWinIsLoggedIn(&UserInfo, &paSessions[i])
                && VBoxServiceVMInfoWinSessionHasProcesses(&paSessions[i], paProcs, cProcs))
            {
                if (cUsersInList > 0)
                    strcat(szUserList, ",");

                cUsersInList++;

                char *pszTemp;
                int rc2 = RTUtf16ToUtf8(UserInfo.wszUser, &pszTemp);
                if (RT_SUCCESS(rc2))
                {
                    strcat(szUserList, pszTemp);
                    RTMemFree(pszTemp);
                }
                else
                    strcat(szUserList, "<string-convertion-error>");
            }
        }
        VBoxServiceVMInfoWinProcessesFree(paProcs);
    }

    ::LsaFreeReturnBuffer(paSessions);
# endif /* TARGET_NT4 */
#elif defined(RT_OS_FREEBSD)
        /** @todo FreeBSD: Port logged on user info retrival. */
#elif defined(RT_OS_OS2)
        /** @todo OS/2: Port logged on (LAN/local/whatever) user info retrival. */
#else
    rc = utmpname(UTMP_FILE);
# ifdef RT_OS_SOLARIS
    if (rc != 1)
# else
    if (rc != 0)
# endif
    {
        VBoxServiceError("Could not set UTMP file! Error: %ld\n", errno);
    }
    setutent();
    utmp *ut_user;
    while ((ut_user = getutent()))
    {
        /* Make sure we don't add user names which are not
         * part of type USER_PROCESS and don't add same users twice. */
        if (   ut_user->ut_type == USER_PROCESS
            && strstr(szUserList, ut_user->ut_user) == NULL)
        {
            /** @todo Do we really want to filter out double user names? (Same user logged in twice)
             *  bird: If we do, then we must add checks for buffer overflows here!  */
            /** @todo r=bird: strstr will filtering out users with similar names. For
             *        example: smith, smithson, joesmith and bobsmith */
            if (cUsersInList > 0)
                strcat(szUserList, ",");
            strcat(szUserList, ut_user->ut_user);
            cUsersInList++;
        }
    }
    endutent();
#endif /* !RT_OS_WINDOWS */

    if (cUsersInList > 0)
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsersList", "%s", szUserList);
    else
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsersList", NULL);
    VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsers", "%u", cUsersInList);
    if (   g_cVMInfoLoggedInUsers != cUsersInList
        || g_cVMInfoLoggedInUsers == UINT32_MAX)
    {
        /*
         * Update this property ONLY if there is a real change from no users to
         * users or vice versa. The only exception is that the initialization
         * forces an update, but only once. This ensures consistent property
         * settings even if the VM aborted previously.
         */
        if (cUsersInList == 0)
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/NoLoggedInUsers", "true");
        else if (g_cVMInfoLoggedInUsers == 0)
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/NoLoggedInUsers", "false");
    }
    g_cVMInfoLoggedInUsers = cUsersInList;

    return VINF_SUCCESS;
}


/**
 * Provide information about the guest network.
 */
int VBoxServiceVMInfoWriteNetwork()
{
    int cIfacesReport = 0;
    char szPropPath [FILENAME_MAX];

#ifdef RT_OS_WINDOWS
    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) /* Socket invalid. */
    {
        VBoxServiceError("Failed to get a socket: Error %d\n", WSAGetLastError());
        return RTErrConvertFromWin32(WSAGetLastError());
    }

    INTERFACE_INFO InterfaceList[20] = {0};
    unsigned long nBytesReturned = 0;
    if (WSAIoctl(sd,
                 SIO_GET_INTERFACE_LIST,
                 0,
                 0,
                 &InterfaceList,
                 sizeof(InterfaceList),
                 &nBytesReturned,
                 0,
                 0) ==  SOCKET_ERROR)
    {
        VBoxServiceError("Failed to WSAIoctl() on socket: Error: %d\n", WSAGetLastError());
        return RTErrConvertFromWin32(WSAGetLastError());
    }
    int cIfacesSystem = nBytesReturned / sizeof(INTERFACE_INFO);

    /** @todo Use GetAdaptersInfo() and GetAdapterAddresses (IPv4 + IPv6) for more information. */
    for (int i = 0; i < cIfacesSystem; ++i)
    {
        sockaddr_in *pAddress;
        u_long nFlags = 0;
        if (InterfaceList[i].iiFlags & IFF_LOOPBACK) /* Skip loopback device. */
            continue;
        nFlags = InterfaceList[i].iiFlags;
        pAddress = (sockaddr_in *)&(InterfaceList[i].iiAddress);
        Assert(pAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/IP", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/Broadcast", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *)&(InterfaceList[i].iiNetmask);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/Netmask", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/Status", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, nFlags & IFF_UP ? "Up" : "Down");
        cIfacesReport++;
    }
    if (sd >= 0)
        closesocket(sd);
#else /* !RT_OS_WINDOWS */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        VBoxServiceError("Failed to get a socket: Error %d\n", errno);
        return RTErrConvertFromErrno(errno);
    }

    ifconf ifcfg;
    char buffer[1024] = {0};
    ifcfg.ifc_len = sizeof(buffer);
    ifcfg.ifc_buf = buffer;
    if (ioctl(sd, SIOCGIFCONF, &ifcfg) < 0)
    {
        VBoxServiceError("Failed to ioctl(SIOCGIFCONF) on socket: Error %d\n", errno);
        return RTErrConvertFromErrno(errno);
    }

    ifreq* ifrequest = ifcfg.ifc_req;
    int cIfacesSystem = ifcfg.ifc_len / sizeof(ifreq);

    for (int i = 0; i < cIfacesSystem; ++i)
    {
        sockaddr_in *pAddress;
        if (ioctl(sd, SIOCGIFFLAGS, &ifrequest[i]) < 0)
        {
            VBoxServiceError("Failed to ioctl(SIOCGIFFLAGS) on socket: Error %d\n", errno);
            close(sd);
            return RTErrConvertFromErrno(errno);
        }
        if (ifrequest[i].ifr_flags & IFF_LOOPBACK) /* Skip the loopback device. */
            continue;

        bool fIfUp = !!(ifrequest[i].ifr_flags & IFF_UP);
        pAddress = ((sockaddr_in *)&ifrequest[i].ifr_addr);
        Assert(pAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/IP", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        if (ioctl(sd, SIOCGIFBRDADDR, &ifrequest[i]) < 0)
        {
            VBoxServiceError("Failed to ioctl(SIOCGIFBRDADDR) on socket: Error %d\n", errno);
            close(sd);
            return RTErrConvertFromErrno(errno);
        }
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_broadaddr;
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/Broadcast", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        if (ioctl(sd, SIOCGIFNETMASK, &ifrequest[i]) < 0)
        {
            VBoxServiceError("Failed to ioctl(SIOCGIFBRDADDR) on socket: Error %d\n", errno);
            close(sd);
            return RTErrConvertFromErrno(errno);
        }
 #if defined(RT_OS_FREEBSD) || defined(RT_OS_OS2) || defined(RT_OS_SOLARIS)
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_addr;
 #else
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_netmask;
 #endif

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/V4/Netmask", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

 #if defined(RT_OS_SOLARIS)
        if (ioctl(sd, SIOCGENADDR, &ifrequest[i]) < 0)
 #else
        if (ioctl(sd, SIOCGIFHWADDR, &ifrequest[i]) < 0)
 #endif
        {
            VBoxServiceError("Failed to ioctl(SIOCGIFHWADDR) on socket: Error %d\n", errno);
            close(sd);
            return RTErrConvertFromErrno(errno);
        }

        char szMac[32];
 #if defined(RT_OS_SOLARIS)
        char *pu8Mac = &ifrequest[i].ifr_enaddr[0];
 #else
        char *pu8Mac = &ifrequest[i].ifr_hwaddr.sa_data[0];
 #endif
        RTStrPrintf(szMac, sizeof(szMac), "%02x:%02x:%02x:%02x:%02x:%02x",
                    pu8Mac[0], pu8Mac[1], pu8Mac[2], pu8Mac[3],  pu8Mac[4], pu8Mac[5]);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/MAC", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szMac);

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%d/Status", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, fIfUp ? "Up" : "Down");
        cIfacesReport++;
    }

    close(sd);

#endif /* !RT_OS_WINDOWS */

    /*
     * This property is a beacon which is _always_ written, even if the network configuration
     * does not change. If this property is missing, the host assumes that all other GuestInfo
     * properties are no longer valid.
     */
    VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/Net/Count", "%d",
                               cIfacesReport);

    /** @todo r=bird: if cIfacesReport decreased compared to the previous run, zap
     *        the stale data.  This can probably be encorporated into the cache.  */

    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceVMInfoWorker(bool volatile *pfShutdown)
{
    int rc;

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

#ifdef RT_OS_WINDOWS
    /* Required for network information (must be called per thread). */
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData))
        VBoxServiceError("WSAStartup failed! Error: %Rrc\n", RTErrConvertFromWin32(WSAGetLastError()));
#endif /* RT_OS_WINDOWS */

    /*
     * Write the fixed properties first.
     */
    VBoxServiceVMInfoWriteFixedProperties();

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        rc = VBoxServiceVMInfoWriteUsers();
        if (RT_FAILURE(rc))
            break;

        rc = VBoxServiceVMInfoWriteNetwork();
        if (RT_FAILURE(rc))
            break;

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_hVMInfoEvent, g_cMsVMInfoInterval);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

#ifdef RT_OS_WINDOWS
    WSACleanup();
#endif

    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceVMInfoStop(void)
{
    RTSemEventMultiSignal(g_hVMInfoEvent);
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceVMInfoTerm(void)
{
    int rc;

    if (g_hVMInfoEvent != NIL_RTSEMEVENTMULTI)
    {
        /** @todo temporary solution: Zap all values which are not valid
         *        anymore when VM goes down (reboot/shutdown ). Needs to
         *        be replaced with "temporary properties" later.
         *
         *        One idea is to introduce a (HGCM-)session guest property
         *        flag meaning that a guest property is only valid as long
         *        as the HGCM session isn't closed (e.g. guest application
         *        terminates). [don't remove till implemented]
         */
        /** @todo r=bird: Drop the VbglR3GuestPropDelSet call here and use the cache
         *        since it remembers what we've written. */
        /* Delete the "../Net" branch. */
        const char *apszPat[1] = { "/VirtualBox/GuestInfo/Net/*" };
        rc = VbglR3GuestPropDelSet(g_uVMInfoGuestPropSvcClientID, &apszPat[0], RT_ELEMENTS(apszPat));

        /* Destroy property cache. */
        VBoxServicePropCacheDestroy(&g_VMInfoPropCache);

        /* Disconnect from guest properties service. */
        rc = VbglR3GuestPropDisconnect(g_uVMInfoGuestPropSvcClientID);
        if (RT_FAILURE(rc))
            VBoxServiceError("Failed to disconnect from guest property service! Error: %Rrc\n", rc);
        g_uVMInfoGuestPropSvcClientID = 0;

        RTSemEventMultiDestroy(g_hVMInfoEvent);
        g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_VMInfo =
{
    /* pszName. */
    "vminfo",
    /* pszDescription. */
    "Virtual Machine Information",
    /* pszUsage. */
    "[--vminfo-interval <ms>]"
    ,
    /* pszOptions. */
    "    --vminfo-interval   Specifies the interval at which to retrieve the\n"
    "                        VM information. The default is 10000 ms.\n"
    ,
    /* methods */
    VBoxServiceVMInfoPreInit,
    VBoxServiceVMInfoOption,
    VBoxServiceVMInfoInit,
    VBoxServiceVMInfoWorker,
    VBoxServiceVMInfoStop,
    VBoxServiceVMInfoTerm
};

