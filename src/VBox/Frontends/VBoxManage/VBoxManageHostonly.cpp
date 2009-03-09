/* $Id$ */
/** @file
 * VBoxManage - Implementation of hostonlyif command.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint2.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>

#include <vector>
#include <list>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/net.h>
#include <iprt/getopt.h>

#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;

#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
static int handleCreate(HandlerArg *a, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 1)
        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");

    int index = iStart;
    HRESULT rc;
    Bstr name(a->argv[iStart]);
    index++;

    ComPtr<IHost> host;
    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

    ComPtr<IHostNetworkInterface> hif;
    ComPtr<IProgress> progress;

    CHECK_ERROR(host, CreateHostOnlyNetworkInterface (name, hif.asOutParam(), progress.asOutParam()));

    showProgress(progress);

    HRESULT hr;
    CHECK_ERROR(progress, COMGETTER(ResultCode) (&hr));

    *pcProcessed = index - iStart;

    if(FAILED(hr))
    {
        com::ProgressErrorInfo info(progress);
        if (info.isBasicAvailable())
            RTPrintf("Error: failed to remove the host-only adapter. Error message: %lS\n", info.getText().raw());
        else
            RTPrintf("Error: failed to remove the host-only adapter. No error message available, HRESULT code: 0x%x\n", hr);

        return 1;
    }

    return 0;
}

static int handleRemove(HandlerArg *a, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 1)
        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");

    int index = iStart;
    HRESULT rc;

    Bstr name(a->argv[iStart]);
    index++;

    ComPtr<IHost> host;
    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

    ComPtr<IHostNetworkInterface> hif;
    CHECK_ERROR(host, FindHostNetworkInterfaceByName(name, hif.asOutParam()));

    GUID guid;
    CHECK_ERROR(hif, COMGETTER(Id)(&guid));

    ComPtr<IProgress> progress;
    CHECK_ERROR(host, RemoveHostOnlyNetworkInterface (guid, hif.asOutParam(),progress.asOutParam()));

    showProgress(progress);

    HRESULT hr;
    CHECK_ERROR(progress, COMGETTER(ResultCode) (&hr));

    *pcProcessed = index - iStart;

    if(FAILED(hr))
    {
        com::ProgressErrorInfo info(progress);
        if (info.isBasicAvailable())
            RTPrintf("Error: failed to remove the host-only adapter. Error message: %lS\n", info.getText().raw());
        else
            RTPrintf("Error: failed to remove the host-only adapter. No error message available, HRESULT code: 0x%x\n", hr);

        return 1;
    }

    return 0;
}
#endif

enum enOptionCodes
{
    DHCP = 1000,
    IP,
    NETMASK,
    IPV6,
    NETMASKLENGTHV6
};

static const RTGETOPTDEF g_aListOptions[]
    = {
        { "-dhcp",              DHCP, RTGETOPT_REQ_NOTHING },
        { "-ip",                IP, RTGETOPT_REQ_IPV4ADDR },
        { "-netmask",           NETMASK, RTGETOPT_REQ_IPV4ADDR },
        { "-ipv6",              IPV6, RTGETOPT_REQ_STRING },
        { "-netmasklengthv6",   NETMASKLENGTHV6, RTGETOPT_REQ_UINT8 }
      };

static int handleIpconfig(HandlerArg *a, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 2)
        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");

    int index = iStart;
    HRESULT rc;

    Bstr name(a->argv[iStart]);
    index++;

    bool bDhcp = false;
    bool bNetmasklengthv6 = false;
    uint8_t uNetmasklengthv6 = 0;
    const char *pIpv6 = NULL;
    bool bIp = false;
    RTNETADDRIPV4 ip;
    bool bNetmask = false;
    RTNETADDRIPV4 netmask;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState,
                 a->argc,
                 a->argv,
                 g_aListOptions,
                 RT_ELEMENTS(g_aListOptions),
                 index,
                 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case DHCP:   // -dhcp
                if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify -dhcp once.");
                else if(bNetmasklengthv6 || pIpv6 || bIp || bNetmask)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use -dhcp with static ip configuration parameters: -ip, -netmask, -ipv6 and -netmasklengthv6.");
                else
                    bDhcp = true;
            break;
            case IP:
                if(bIp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify -ip once.");
                else if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use -dhcp with static ip configuration parameters: -ip, -netmask, -ipv6 and -netmasklengthv6.");
                else if(bNetmasklengthv6 || pIpv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use ipv4 configuration (-ip and -netmask) with ipv6 (-ipv6 and -netmasklengthv6) simultaneously.");
                else
                {
                    bIp = true;
                    ip = ValueUnion.IPv4Addr;
                }
            break;
            case NETMASK:
                if(bNetmask)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify -netmask once.");
                else if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use -dhcp with static ip configuration parameters: -ip, -netmask, -ipv6 and -netmasklengthv6.");
                else if(bNetmasklengthv6 || pIpv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use ipv4 configuration (-ip and -netmask) with ipv6 (-ipv6 and -netmasklengthv6) simultaneously.");
                else
                {
                    bNetmask = true;
                    netmask = ValueUnion.IPv4Addr;
                }
            break;
            case IPV6:
                if(pIpv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify -ipv6 once.");
                else if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use -dhcp with static ip configuration parameters: -ip, -netmask, -ipv6 and -netmasklengthv6.");
                else if(bIp || bNetmask)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use ipv4 configuration (-ip and -netmask) with ipv6 (-ipv6 and -netmasklengthv6) simultaneously.");
                else
                    pIpv6 = ValueUnion.psz;
            break;
            case NETMASKLENGTHV6:
                if(bNetmasklengthv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify -netmasklengthv6 once.");
                else if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use -dhcp with static ip configuration parameters: -ip, -netmask, -ipv6 and -netmasklengthv6.");
                else if(bIp || bNetmask)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can not use ipv4 configuration (-ip and -netmask) with ipv6 (-ipv6 and -netmasklengthv6) simultaneously.");
                else
                {
                    bNetmasklengthv6 = true;
                    uNetmasklengthv6 = ValueUnion.u8;
                }
            break;
            default:
                if (c > 0)
                    return errorSyntax(USAGE_HOSTONLYIFS, "missing case: %c\n", c);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_HOSTONLYIFS, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_HOSTONLYIFS, "%Rrs", c);
        }
    }

    ComPtr<IHost> host;
    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

    ComPtr<IHostNetworkInterface> hif;
    CHECK_ERROR(host, FindHostNetworkInterfaceByName(name, hif.asOutParam()));

    if(bDhcp)
    {
        CHECK_ERROR(hif, EnableDynamicIpConfig ());
    }
    else if(bIp)
    {
        if(!bNetmask)
        {
            netmask.u = 0;
        }

        CHECK_ERROR(hif, EnableStaticIpConfig (ip.u, netmask.u));
    }
    else if(pIpv6)
    {
        Bstr ipv6str(pIpv6);
        CHECK_ERROR(hif, EnableStaticIpConfigV6 (ipv6str, (ULONG)uNetmasklengthv6));
    }
    else
    {
        return errorSyntax(USAGE_HOSTONLYIFS, "neither -dhcp nor -ip nor -ipv6 was spcfified");
    }

    return 0;
}


int handleHostonlyIf(HandlerArg *a)
{
    int result = 0;
    if (a->argc < 1)
        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");

    for (int i = 0; i < a->argc; i++)
    {
        if (strcmp(a->argv[i], "ipconfig") == 0)
        {
            int cProcessed;
            result = handleIpconfig(a, i+1, &cProcessed);
            break;
//            if(!rc)
//                i+= cProcessed;
//            else
//                break;
        }
#if defined(RT_OS_WINDOWS) && defined(VBOX_WITH_NETFLT)
        else if (strcmp(a->argv[i], "create") == 0)
        {
            int cProcessed;
            result = handleCreate(a, i+1, &cProcessed);
            if(!result)
                i+= cProcessed;
            else
                break;
        }
        else if (strcmp(a->argv[i], "remove") == 0)
        {
            int cProcessed;
            result = handleRemove(a, i+1, &cProcessed);
            if(!result)
                i+= cProcessed;
            else
                break;
        }
#endif
        else
        {
            result = errorSyntax(USAGE_HOSTONLYIFS, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
            break;
        }
    }

    return result;
}

#endif /* !VBOX_ONLY_DOCS */
