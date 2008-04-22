/* $Id$ */
/** @file
 * IPRT Testcase - IPv4.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/cidr.h>
#include <iprt/err.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define CHECKNETWORK(string, expected_result, expected_network, expected_netmask) \
    do { \
        RTIPV4ADDR network, netmask; \
        int result = RTCidrStrToIPv4(string, &network, &netmask); \
        if (expected_result && !result) \
        { \
            g_cErrors++; \
            RTPrintf("%s, %d: %s: expected %Vrc got %Vrc\n", \
                    __FUNCTION__, __LINE__, string, expected_result, result); \
        } \
        else if (   expected_result != result \
                 || (   result == VINF_SUCCESS \
                     && (   expected_network != network \
                         || expected_netmask != netmask))) \
        { \
            g_cErrors++; \
            RTPrintf("%s, %d: '%s': expected %Vrc got %Vrc, expected network %08x got %08x, expected netmask %08x got %08x\n", \
                    __FUNCTION__, __LINE__, string, expected_result, result, expected_network, network, expected_netmask, netmask); \
        } \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static unsigned g_cErrors = 0;


int main()
{
    RTR3Init();
    CHECKNETWORK("10.0.0/24",                VINF_SUCCESS, 0x0A000000, 0xFFFFFF00);
    CHECKNETWORK("10.0.0/8",                 VINF_SUCCESS, 0x0A000000, 0xFF000000);
    CHECKNETWORK("10.0.0./24",     VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("0.1.0/24",       VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.255.0.0/24",  VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.1234.0.0/24", VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.256.0.0/24",  VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.0.0/3",       VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.1.2.3/8",               VINF_SUCCESS, 0x0A010203, 0xFF000000);
    CHECKNETWORK("10.0.0/29",      VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.0.0/240",     VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.0.0/24.",     VERR_INVALID_PARAMETER,          0,          0);
    CHECKNETWORK("10.1.2/16",                VINF_SUCCESS, 0x0A010200, 0xFFFF0000);
    CHECKNETWORK("1.2.3.4",                  VINF_SUCCESS, 0x01020304, 0xFFFFFFFF);
    if (!g_cErrors)
        RTPrintf("tstIp: SUCCESS\n", g_cErrors);
    else
        RTPrintf("tstIp: FAILURE - %d errors\n", g_cErrors);
    return !!g_cErrors;
}

