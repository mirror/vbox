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
# ifdef TARGET_NT4 /* HACK ALERT! PMIB_IPSTATS undefined if 0x0400 with newer SDKs. */
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x0500
# endif
# include <winsock2.h>
# include <iphlpapi.h>
# include <ws2tcpip.h>
# include <windows.h>
# include <Ntsecapi.h>
#else
# define __STDC_LIMIT_MACROS
# include <arpa/inet.h>
# include <errno.h>
# include <netinet/in.h>
# include <sys/ioctl.h>
# include <sys/socket.h>
# include <net/if.h>
# include <unistd.h>
# ifndef RT_OS_OS2
#  ifndef RT_OS_FREEBSD
#   include <utmpx.h> /* @todo FreeBSD 9 should have this. */
#  endif
# endif
# ifdef RT_OS_SOLARIS
#  include <sys/sockio.h>
#  include <net/if_arp.h>
# endif
# ifdef RT_OS_FREEBSD
#  include <ifaddrs.h> /* getifaddrs, freeifaddrs */
#  include <net/if_dl.h> /* LLADDR */
#  include <netdb.h> /* getnameinfo */
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
/** The vminfo interval (milliseconds). */
static uint32_t                 g_cMsVMInfoInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI          g_hVMInfoEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t                 g_uVMInfoGuestPropSvcClientID = 0;
/** Number of logged in users in OS. */
static uint32_t                 g_cVMInfoLoggedInUsers = UINT32_MAX;
/** The guest property cache. */
static VBOXSERVICEVEPROPCACHE   g_VMInfoPropCache;
/** The VM session ID. Changes whenever the VM is restored or reset. */
static uint64_t                 g_idVMInfoSession;



/**
 * Signals the event so that a re-enumeration of VM-specific
 * information (like logged in users) can happen.
 *
 * @return  IPRT status code.
 */
int VBoxServiceVMInfoSignal(void)
{
    /* Trigger a re-enumeration of all logged-in users by unblocking
     * the multi event semaphore of the VMInfo thread. */
    if (g_hVMInfoEvent)
        return RTSemEventMultiSignal(g_hVMInfoEvent);

    return VINF_SUCCESS;
}


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

    VbglR3GetSessionId(&g_idVMInfoSession);
    /* The status code is ignored as this information is not available with VBox < 3.2.10. */

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
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY | VBOXSERVICEPROPCACHEFLAG_TRANSIENT, NULL /* Delete on exit */);
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsers",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY | VBOXSERVICEPROPCACHEFLAG_TRANSIENT, "0");
        VBoxServicePropCacheUpdateEntry(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/NoLoggedInUsers",
                                        VBOXSERVICEPROPCACHEFLAG_TEMPORARY | VBOXSERVICEPROPCACHEFLAG_TRANSIENT, "true");
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
static void vboxserviceVMInfoWriteFixedProperties(void)
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
    char *pszAddVerExt;
    char *pszAddRev;
    rc = VbglR3GetAdditionsVersion(&pszAddVer, &pszAddVerExt, &pszAddRev);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/Version",
                          "%s", RT_FAILURE(rc) ? "" : pszAddVer);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/VersionExt",
                          "%s", RT_FAILURE(rc) ? "" : pszAddVerExt);
    VBoxServiceWritePropF(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestAdd/Revision",
                          "%s", RT_FAILURE(rc) ? "" : pszAddRev);
    if (RT_SUCCESS(rc))
    {
        RTStrFree(pszAddVer);
        RTStrFree(pszAddVerExt);
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
static int vboxserviceVMInfoWriteUsers(void)
{
    int rc = VINF_SUCCESS;
    char *pszUserList = NULL;
    uint32_t cUsersInList = 0;

#ifdef RT_OS_WINDOWS
# ifndef TARGET_NT4
    rc = VBoxServiceVMInfoWinWriteUsers(&pszUserList, &cUsersInList);
# else
    rc = VERR_NOT_IMPLEMENTED;
# endif

#elif defined(RT_OS_FREEBSD)
    /** @todo FreeBSD: Port logged on user info retrieval.
     *                 However, FreeBSD 9 supports utmpx, so we could use the code
     *                 block below (?). */
    rc = VERR_NOT_IMPLEMENTED;

#elif defined(RT_OS_OS2)
    /** @todo OS/2: Port logged on (LAN/local/whatever) user info retrieval. */
    rc = VERR_NOT_IMPLEMENTED;

#else
    setutxent();
    utmpx *ut_user;
    uint32_t cListSize = 32;

    /* Allocate a first array to hold 32 users max. */
    char **papszUsers = (char **)RTMemAllocZ(cListSize * sizeof(char *));
    if (papszUsers == NULL)
        rc = VERR_NO_MEMORY;

    /* Process all entries in the utmp file. */
    while (   (ut_user = getutxent())
           && RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(4, "Found logged in user \"%s\" (type: %d)\n",
                           ut_user->ut_user, ut_user->ut_type);
        if (cUsersInList > cListSize)
        {
            cListSize += 32;
            void *pvNew = RTMemRealloc(papszUsers, cListSize * sizeof(char*));
            AssertPtrBreakStmt(pvNew, cListSize -= 32);
            papszUsers = (char **)pvNew;
        }

        /* Make sure we don't add user names which are not
         * part of type USER_PROCESS. */
        if (ut_user->ut_type == USER_PROCESS)
        {
            bool fFound = false;
            for (uint32_t i = 0; i < cUsersInList && !fFound; i++)
                fFound = strcmp(papszUsers[i], ut_user->ut_user) == 0;

            if (!fFound)
            {
                VBoxServiceVerbose(4, "Adding user \"%s\" (type: %d) to list\n",
                                   ut_user->ut_user, ut_user->ut_type);

                rc = RTStrDupEx(&papszUsers[cUsersInList], (const char *)ut_user->ut_user);
                if (RT_FAILURE(rc))
                    break;
                cUsersInList++;
            }
        }
    }

    /* Calc the string length. */
    size_t cchUserList = 0;
    for (uint32_t i = 0; i < cUsersInList; i++)
        cchUserList += (i != 0) + strlen(papszUsers[i]);

    /* Build the user list. */
    rc = RTStrAllocEx(&pszUserList, cchUserList + 1);
    if (RT_SUCCESS(rc))
    {
        char *psz = pszUserList;
        for (uint32_t i = 0; i < cUsersInList; i++)
        {
            if (i != 0)
                *psz++ = ',';
            size_t cch = strlen(papszUsers[i]);
            memcpy(psz, papszUsers[i], cch);
            psz += cch;
        }
        *psz = '\0';
    }

    /* Cleanup. */
    for (uint32_t i = 0; i < cUsersInList; i++)
        RTStrFree(papszUsers[i]);
    RTMemFree(papszUsers);

    endutxent(); /* Close utmpx file. */
#endif
    Assert(RT_FAILURE(rc) || cUsersInList == 0 || (pszUserList && *pszUserList));

    /* If the user enumeration above failed, reset the user count to 0 except
     * we didn't have enough memory anymore. In that case we want to preserve
     * the previous user count in order to not confuse third party tools which
     * rely on that count. */
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NO_MEMORY)
        {
            static int s_iVMInfoBitchedOOM = 0;
            if (s_iVMInfoBitchedOOM++ < 3)
                VBoxServiceVerbose(0, "Warning: Not enough memory available to enumerate users! Keeping old value (%u)\n",
                                   g_cVMInfoLoggedInUsers);
            cUsersInList = g_cVMInfoLoggedInUsers;
        }
        else
            cUsersInList = 0;
    }

    VBoxServiceVerbose(4, "cUsersInList=%RU32, pszUserList=%s, rc=%Rrc\n",
                       cUsersInList, pszUserList ? pszUserList : "<NULL>", rc);

    if (pszUserList && cUsersInList > 0)
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsersList", "%s", pszUserList);
    else
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsersList", NULL);
    VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/LoggedInUsers", "%u", cUsersInList);
    if (g_cVMInfoLoggedInUsers != cUsersInList)
    {
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/OS/NoLoggedInUsers",
                                   cUsersInList == 0 ? "true" : "false");
        g_cVMInfoLoggedInUsers = cUsersInList;
    }
    if (RT_SUCCESS(rc) && pszUserList)
        RTStrFree(pszUserList);
    return rc;
}


/**
 * Provide information about the guest network.
 */
static int vboxserviceVMInfoWriteNetwork(void)
{
    int rc = VINF_SUCCESS;
    uint32_t  cIfacesReport = 0;
    char szPropPath[256];

#ifdef RT_OS_WINDOWS
    IP_ADAPTER_INFO *pAdpInfo = NULL;

# ifndef TARGET_NT4
    ULONG cbAdpInfo = sizeof(*pAdpInfo);
    pAdpInfo = (IP_ADAPTER_INFO *)RTMemAlloc(cbAdpInfo);
    if (!pAdpInfo)
    {
        VBoxServiceError("VMInfo/Network: Failed to allocate IP_ADAPTER_INFO\n");
        return VERR_NO_MEMORY;
    }
    DWORD dwRet = GetAdaptersInfo(pAdpInfo, &cbAdpInfo);
    if (dwRet == ERROR_BUFFER_OVERFLOW)
    {
        IP_ADAPTER_INFO *pAdpInfoNew = (IP_ADAPTER_INFO*)RTMemRealloc(pAdpInfo, cbAdpInfo);
        if (pAdpInfoNew)
        {
            pAdpInfo = pAdpInfoNew;
            dwRet = GetAdaptersInfo(pAdpInfo, &cbAdpInfo);
        }
    }
    else if (dwRet == ERROR_NO_DATA)
    {
        VBoxServiceVerbose(3, "VMInfo/Network: No network adapters available\n");

        /* If no network adapters available / present in the
         * system we pretend success to not bail out too early. */
        dwRet = ERROR_SUCCESS;
    }

    if (dwRet != ERROR_SUCCESS)
    {
        if (pAdpInfo)
            RTMemFree(pAdpInfo);
        VBoxServiceError("VMInfo/Network: Failed to get adapter info: Error %d\n", dwRet);
        return RTErrConvertFromWin32(dwRet);
    }
# endif /* !TARGET_NT4 */

    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR) /* Socket invalid. */
    {
        int wsaErr = WSAGetLastError();
        /* Don't complain/bail out with an error if network stack is not up; can happen
         * on NT4 due to start up when not connected shares dialogs pop up. */
        if (WSAENETDOWN == wsaErr)
        {
            VBoxServiceVerbose(0, "VMInfo/Network: Network is not up yet.\n");
            wsaErr = VINF_SUCCESS;
        }
        else
            VBoxServiceError("VMInfo/Network: Failed to get a socket: Error %d\n", wsaErr);
        if (pAdpInfo)
            RTMemFree(pAdpInfo);
        return RTErrConvertFromWin32(wsaErr);
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
        VBoxServiceError("VMInfo/Network: Failed to WSAIoctl() on socket: Error: %d\n", WSAGetLastError());
        if (pAdpInfo)
            RTMemFree(pAdpInfo);
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
        char szIp[32];
        RTStrPrintf(szIp, sizeof(szIp), "%s", inet_ntoa(pAddress->sin_addr));
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/IP", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szIp);

        pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Broadcast", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *)&(InterfaceList[i].iiNetmask);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Netmask", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/Status", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, nFlags & IFF_UP ? "Up" : "Down");

# ifndef TARGET_NT4
        IP_ADAPTER_INFO *pAdp;
        for (pAdp = pAdpInfo; pAdp; pAdp = pAdp->Next)
            if (!strcmp(pAdp->IpAddressList.IpAddress.String, szIp))
                break;

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/MAC", cIfacesReport);
        if (pAdp)
        {
            char szMac[32];
            RTStrPrintf(szMac, sizeof(szMac), "%02X%02X%02X%02X%02X%02X",
                        pAdp->Address[0], pAdp->Address[1], pAdp->Address[2],
                        pAdp->Address[3], pAdp->Address[4], pAdp->Address[5]);
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szMac);
        }
        else
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, NULL);
# endif /* !TARGET_NT4 */

        cIfacesReport++;
    }
    if (pAdpInfo)
        RTMemFree(pAdpInfo);
    if (sd >= 0)
        closesocket(sd);

#elif defined(RT_OS_FREEBSD)
    struct ifaddrs *pIfHead = NULL;

    /* Get all available interfaces */
    rc = getifaddrs(&pIfHead);
    if (rc < 0)
    {
        rc = RTErrConvertFromErrno(errno);
        VBoxServiceError("VMInfo/Network: Failed to get all interfaces: Error %Rrc\n");
        return rc;
    }

    /* Loop through all interfaces and set the data. */
    for (struct ifaddrs *pIfCurr = pIfHead; pIfCurr; pIfCurr = pIfCurr->ifa_next)
    {
        /*
         * Only AF_INET and no loopback interfaces
         * @todo: IPv6 interfaces
         */
        if (   pIfCurr->ifa_addr->sa_family == AF_INET
            && !(pIfCurr->ifa_flags & IFF_LOOPBACK))
        {
            char szInetAddr[NI_MAXHOST];

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_addr, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/IP", cIfacesReport);
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szInetAddr);

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_broadaddr, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Broadcast", cIfacesReport);
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szInetAddr);

            memset(szInetAddr, 0, NI_MAXHOST);
            getnameinfo(pIfCurr->ifa_netmask, sizeof(struct sockaddr_in),
                        szInetAddr, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Netmask", cIfacesReport);
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szInetAddr);

            /* Search for the AF_LINK interface of the current AF_INET one and get the mac. */
            for (struct ifaddrs *pIfLinkCurr = pIfHead; pIfLinkCurr; pIfLinkCurr = pIfLinkCurr->ifa_next)
            {
                if (   pIfLinkCurr->ifa_addr->sa_family == AF_LINK
                    && !strcmp(pIfCurr->ifa_name, pIfLinkCurr->ifa_name))
                {
                    char szMac[32];
                    uint8_t *pu8Mac = NULL;
                    struct sockaddr_dl *pLinkAddress = (struct sockaddr_dl *)pIfLinkCurr->ifa_addr;

                    AssertPtr(pLinkAddress);
                    pu8Mac = (uint8_t *)LLADDR(pLinkAddress);
                    RTStrPrintf(szMac, sizeof(szMac), "%02X%02X%02X%02X%02X%02X",
                                pu8Mac[0], pu8Mac[1], pu8Mac[2], pu8Mac[3],  pu8Mac[4], pu8Mac[5]);
                    RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/MAC", cIfacesReport);
                    VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szMac);
                    break;
                }
            }

            RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/Status", cIfacesReport);
            VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, pIfCurr->ifa_flags & IFF_UP ? "Up" : "Down");

            cIfacesReport++;
        }
    }

    /* Free allocated resources. */
    freeifaddrs(pIfHead);

#else /* !RT_OS_WINDOWS && !RT_OS_FREEBSD */
    int sd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sd < 0)
    {
        rc = RTErrConvertFromErrno(errno);
        VBoxServiceError("VMInfo/Network: Failed to get a socket: Error %Rrc\n", rc);
        return rc;
    }

    ifconf ifcfg;
    char buffer[1024] = {0};
    ifcfg.ifc_len = sizeof(buffer);
    ifcfg.ifc_buf = buffer;
    if (ioctl(sd, SIOCGIFCONF, &ifcfg) < 0)
    {
        close(sd);
        rc = RTErrConvertFromErrno(errno);
        VBoxServiceError("VMInfo/Network: Failed to ioctl(SIOCGIFCONF) on socket: Error %Rrc\n", rc);
        return rc;
    }

    ifreq* ifrequest = ifcfg.ifc_req;
    int cIfacesSystem = ifcfg.ifc_len / sizeof(ifreq);

    for (int i = 0; i < cIfacesSystem; ++i)
    {
        sockaddr_in *pAddress;
        if (ioctl(sd, SIOCGIFFLAGS, &ifrequest[i]) < 0)
        {
            rc = RTErrConvertFromErrno(errno);
            VBoxServiceError("VMInfo/Network: Failed to ioctl(SIOCGIFFLAGS) on socket: Error %Rrc\n", rc);
            break;
        }
        if (ifrequest[i].ifr_flags & IFF_LOOPBACK) /* Skip the loopback device. */
            continue;

        bool fIfUp = !!(ifrequest[i].ifr_flags & IFF_UP);
        pAddress = ((sockaddr_in *)&ifrequest[i].ifr_addr);
        Assert(pAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/IP", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        if (ioctl(sd, SIOCGIFBRDADDR, &ifrequest[i]) < 0)
        {
            rc = RTErrConvertFromErrno(errno);
            VBoxServiceError("VMInfo/Network: Failed to ioctl(SIOCGIFBRDADDR) on socket: Error %Rrc\n", rc);
            break;
        }
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_broadaddr;
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Broadcast", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

        if (ioctl(sd, SIOCGIFNETMASK, &ifrequest[i]) < 0)
        {
            rc = RTErrConvertFromErrno(errno);
            VBoxServiceError("VMInfo/Network: Failed to ioctl(SIOCGIFNETMASK) on socket: Error %Rrc\n", rc);
            break;
        }
# if defined(RT_OS_OS2) || defined(RT_OS_SOLARIS)
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_addr;
# else
        pAddress = (sockaddr_in *)&ifrequest[i].ifr_netmask;
# endif

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/V4/Netmask", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", inet_ntoa(pAddress->sin_addr));

# if defined(RT_OS_SOLARIS)
        /*
         * "ifreq" is obsolete on Solaris. We use the recommended "lifreq".
         * We might fail if the interface has not been assigned an IP address.
         * That doesn't matter; as long as it's plumbed we can pick it up.
         * But, if it has not acquired an IP address we cannot obtain it's MAC
         * address this way, so we just use all zeros there.
         */
        RTMAC IfMac;
        RT_ZERO(IfMac);
        struct lifreq IfReq;
        RT_ZERO(IfReq);
        AssertCompile(sizeof(IfReq.lifr_name) >= sizeof(ifrequest[i].ifr_name));
        strncpy(IfReq.lifr_name, ifrequest[i].ifr_name, sizeof(ifrequest[i].ifr_name));
        if (ioctl(sd, SIOCGLIFADDR, &IfReq) >= 0)
        {
            struct arpreq ArpReq;
            RT_ZERO(ArpReq);
            memcpy(&ArpReq.arp_pa, &IfReq.lifr_addr, sizeof(struct sockaddr_in));

            if (ioctl(sd, SIOCGARP, &ArpReq) >= 0)
                memcpy(&IfMac, ArpReq.arp_ha.sa_data, sizeof(IfMac));
            else
            {
                rc = RTErrConvertFromErrno(errno);
                VBoxServiceError("VMInfo/Network: failed to ioctl(SIOCGARP) on socket: Error %Rrc\n", rc);
                break;
            }
        }
        else
        {
            VBoxServiceVerbose(2, "VMInfo/Network: Interface %d has no assigned IP address, skipping ...\n", i);
            continue;
        }
# else
#  ifndef RT_OS_OS2 /** @todo port this to OS/2 */
        if (ioctl(sd, SIOCGIFHWADDR, &ifrequest[i]) < 0)
        {
            rc = RTErrConvertFromErrno(errno);
            VBoxServiceError("VMInfo/Network: Failed to ioctl(SIOCGIFHWADDR) on socket: Error %Rrc\n", rc);
            break;
        }
#  endif
# endif

# ifndef RT_OS_OS2 /** @todo port this to OS/2 */
        char szMac[32];
#  if defined(RT_OS_SOLARIS)
        uint8_t *pu8Mac = IfMac.au8;
#  else
        uint8_t *pu8Mac = (uint8_t*)&ifrequest[i].ifr_hwaddr.sa_data[0];        /* @todo see above */
#  endif
        RTStrPrintf(szMac, sizeof(szMac), "%02X%02X%02X%02X%02X%02X",
                    pu8Mac[0], pu8Mac[1], pu8Mac[2], pu8Mac[3],  pu8Mac[4], pu8Mac[5]);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/MAC", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, "%s", szMac);
# endif /* !OS/2*/

        RTStrPrintf(szPropPath, sizeof(szPropPath), "/VirtualBox/GuestInfo/Net/%u/Status", cIfacesReport);
        VBoxServicePropCacheUpdate(&g_VMInfoPropCache, szPropPath, fIfUp ? "Up" : "Down");
        cIfacesReport++;
    } /* For all interfaces */

    close(sd);
    if (RT_FAILURE(rc))
        VBoxServiceError("VMInfo/Network: Network enumeration for interface %u failed with error %Rrc\n", cIfacesReport, rc);

#endif /* !RT_OS_WINDOWS */

#if 0 /* Zapping not enabled yet, needs more testing first. */
    /*
     * Zap all stale network interface data if the former (saved) network ifaces count
     * is bigger than the current one.
     */

    /* Get former count. */
    uint32_t cIfacesReportOld;
    rc = VBoxServiceReadPropUInt32(g_uVMInfoGuestPropSvcClientID, "/VirtualBox/GuestInfo/Net/Count", &cIfacesReportOld,
                                   0 /* Min */, UINT32_MAX /* Max */);
    if (   RT_SUCCESS(rc)
        && cIfacesReportOld > cIfacesReport) /* Are some ifaces not around anymore? */
    {
        VBoxServiceVerbose(3, "VMInfo/Network: Stale interface data detected (%u old vs. %u current)\n",
                           cIfacesReportOld, cIfacesReport);

        uint32_t uIfaceDeleteIdx = cIfacesReport;
        do
        {
            VBoxServiceVerbose(3, "VMInfo/Network: Deleting stale data of interface %d ...\n", uIfaceDeleteIdx);
            rc = VBoxServicePropCacheUpdateByPath(&g_VMInfoPropCache, NULL /* Value, delete */, 0 /* Flags */, "/VirtualBox/GuestInfo/Net/%u", uIfaceDeleteIdx++);
        } while (RT_SUCCESS(rc));
    }
    else if (   RT_FAILURE(rc)
             && rc != VERR_NOT_FOUND)
    {
        VBoxServiceError("VMInfo/Network: Failed retrieving old network interfaces count with error %Rrc\n", rc);
    }
#endif

    /*
     * This property is a beacon which is _always_ written, even if the network configuration
     * does not change. If this property is missing, the host assumes that all other GuestInfo
     * properties are no longer valid.
     */
    VBoxServicePropCacheUpdate(&g_VMInfoPropCache, "/VirtualBox/GuestInfo/Net/Count", "%d",
                               cIfacesReport);

    /* Don't fail here; just report everything we got. */
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
        VBoxServiceError("VMInfo/Network: WSAStartup failed! Error: %Rrc\n", RTErrConvertFromWin32(WSAGetLastError()));
#endif /* RT_OS_WINDOWS */

    /*
     * Write the fixed properties first.
     */
    vboxserviceVMInfoWriteFixedProperties();

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        rc = vboxserviceVMInfoWriteUsers();
        if (RT_FAILURE(rc))
            break;

        rc = vboxserviceVMInfoWriteNetwork();
        if (RT_FAILURE(rc))
            break;

        /*
         * Flush all properties if we were restored.
         */
        uint64_t idNewSession = g_idVMInfoSession;
        VbglR3GetSessionId(&idNewSession);
        if (idNewSession != g_idVMInfoSession)
        {
            VBoxServiceVerbose(3, "VMInfo: The VM session ID changed, flushing all properties\n");
            vboxserviceVMInfoWriteFixedProperties();
            VBoxServicePropCacheFlush(&g_VMInfoPropCache);
            g_idVMInfoSession = idNewSession;
        }

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
            VBoxServiceError("VMInfo: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
        else if (RT_LIKELY(RT_SUCCESS(rc2)))
        {
            /* Reset event semaphore if it got triggered. */
            rc2 = RTSemEventMultiReset(g_hVMInfoEvent);
            if (RT_FAILURE(rc2))
                rc2 = VBoxServiceError("VMInfo: RTSemEventMultiReset failed; rc2=%Rrc\n", rc2);
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
        int rc = VbglR3GuestPropDelSet(g_uVMInfoGuestPropSvcClientID, &apszPat[0], RT_ELEMENTS(apszPat));

        /* Destroy property cache. */
        VBoxServicePropCacheDestroy(&g_VMInfoPropCache);

        /* Disconnect from guest properties service. */
        rc = VbglR3GuestPropDisconnect(g_uVMInfoGuestPropSvcClientID);
        if (RT_FAILURE(rc))
            VBoxServiceError("VMInfo: Failed to disconnect from guest property service! Error: %Rrc\n", rc);
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
    "              [--vminfo-interval <ms>]"
    ,
    /* pszOptions. */
    "    --vminfo-interval       Specifies the interval at which to retrieve the\n"
    "                            VM information. The default is 10000 ms.\n"
    ,
    /* methods */
    VBoxServiceVMInfoPreInit,
    VBoxServiceVMInfoOption,
    VBoxServiceVMInfoInit,
    VBoxServiceVMInfoWorker,
    VBoxServiceVMInfoStop,
    VBoxServiceVMInfoTerm
};

