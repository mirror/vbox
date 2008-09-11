/* $Id$ */
/** @file
 * VBoxVMInfoNet - Network information for the host.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * Sun Microsystems, Inc. confidential
 * All rights reserved
 */

#include "VBoxService.h"
#include "VBoxVMInfo.h"
#include "VBoxVMInfoNet.h"

int vboxVMInfoNet(VBOXINFORMATIONCONTEXT* a_pCtx)
{
    DWORD dwCurIface = 0;

    SOCKET sd = WSASocket(AF_INET, SOCK_DGRAM, 0, 0, 0, 0);
    if (sd == SOCKET_ERROR)
    {
        Log(("vboxVMInfoThread: Failed to get a socket: Error %d\n", WSAGetLastError()));
        return -1;
    }

    INTERFACE_INFO InterfaceList[20];
    unsigned long nBytesReturned;
    if (    WSAIoctl(sd, SIO_GET_INTERFACE_LIST, 0, 0, &InterfaceList,
                     sizeof(InterfaceList), &nBytesReturned, 0, 0)
        ==  SOCKET_ERROR)
    {
        Log(("vboxVMInfoThread: Failed calling WSAIoctl: Error: %d\n", WSAGetLastError()));
        return -1;
    }

    char szPropPath [_MAX_PATH+1] = {0};
    char szTemp [_MAX_PATH+1] = {0};
    int nNumInterfaces = nBytesReturned / sizeof(INTERFACE_INFO);

    RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestInfo/Net/Count");
    vboxVMInfoWritePropInt(a_pCtx, szPropPath, (nNumInterfaces > 1 ? nNumInterfaces-1 : 0));

    dwCurIface = 0;

    for (int i = 0; i < nNumInterfaces; ++i)
    {
        if (InterfaceList[i].iiFlags & IFF_LOOPBACK)    /* Skip loopback device. */
            continue;

        sockaddr_in *pAddress;
        pAddress = (sockaddr_in *) & (InterfaceList[i].iiAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestInfo/Net/%d/V4/IP", i);
        vboxVMInfoWriteProp(a_pCtx, szPropPath, inet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *) & (InterfaceList[i].iiBroadcastAddress);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestInfo/Net/%d/V4/Broadcast", i);
        vboxVMInfoWriteProp(a_pCtx, szPropPath, inet_ntoa(pAddress->sin_addr));

        pAddress = (sockaddr_in *) & (InterfaceList[i].iiNetmask);
        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestInfo/Net/%d/V4/Netmask", i);
        vboxVMInfoWriteProp(a_pCtx, szPropPath, inet_ntoa(pAddress->sin_addr));

        u_long nFlags = InterfaceList[i].iiFlags;
        if (nFlags & IFF_UP)
            RTStrPrintf(szTemp, sizeof(szTemp), "Up");
        else
            RTStrPrintf(szTemp, sizeof(szTemp), "Down");

        RTStrPrintf(szPropPath, sizeof(szPropPath), "GuestInfo/Net/%d/Status", i);
        vboxVMInfoWriteProp(a_pCtx, szPropPath, szTemp);
    }

    closesocket(sd);

    return 0;
}

