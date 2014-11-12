/* $Id$ */
/** @file
 * NAT - Windows ICMP API based ping proxy.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "slirp.h"
#include "ip_icmp.h"

#include <iphlpapi.h>
#include <icmpapi.h>


int
icmpwin_init(PNATState pData)
{
    pData->icmp_socket.sh = IcmpCreateFile();
    pData->phEvents[VBOX_ICMP_EVENT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
    pData->cbIcmpBuffer = sizeof(ICMP_ECHO_REPLY) * 10;
    pData->pvIcmpBuffer = RTMemAlloc(pData->cbIcmpBuffer);

    return 0;
}


void
icmpwin_finit(PNATState pData)
{
    IcmpCloseHandle(pData->icmp_socket.sh);
    RTMemFree(pData->pvIcmpBuffer);
}
