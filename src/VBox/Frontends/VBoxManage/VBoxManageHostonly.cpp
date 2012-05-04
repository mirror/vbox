/* $Id$ */
/** @file
 * VBoxManage - Implementation of hostonlyif command.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#ifndef VBOX_ONLY_DOCS
#include <VBox/com/com.h>
#include <VBox/com/array.h>
#include <VBox/com/ErrorInfo.h>
#include <VBox/com/errorprint.h>
#include <VBox/com/EventQueue.h>

#include <VBox/com/VirtualBox.h>
#endif /* !VBOX_ONLY_DOCS */

#include <iprt/cidr.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/net.h>
#include <iprt/getopt.h>
#include <iprt/ctype.h>

#include <VBox/log.h>

#include "VBoxManage.h"

#ifndef VBOX_ONLY_DOCS
using namespace com;

#if defined(VBOX_WITH_NETFLT) && !defined(RT_OS_SOLARIS)
static int handleCreate(HandlerArg *a, int iStart, int *pcProcessed)
{
//    if (a->argc - iStart < 1)
//        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");

    int index = iStart;
    HRESULT rc;
//    Bstr name(a->argv[iStart]);
//    index++;

    ComPtr<IHost> host;
    CHECK_ERROR_RET(a->virtualBox, COMGETTER(Host)(host.asOutParam()), 1);

    ComPtr<IHostNetworkInterface> hif;
    ComPtr<IProgress> progress;

    CHECK_ERROR_RET(host, CreateHostOnlyNetworkInterface (hif.asOutParam(), progress.asOutParam()), 1);

    rc = showProgress(progress);
    *pcProcessed = index - iStart;
    CHECK_PROGRESS_ERROR_RET(progress, ("Failed to create the host-only adapter"), 1);

    Bstr name;
    CHECK_ERROR(hif, COMGETTER(Name) (name.asOutParam()));

    RTPrintf("Interface '%ls' was successfully created\n", name.raw());

    return 0;
}

static int handleRemove(HandlerArg *a, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 1)
    {
        *pcProcessed = 0;
        return errorSyntax(USAGE_HOSTONLYIFS, "Not enough parameters");
    }

    int index = iStart;
    HRESULT rc;

    Bstr name(a->argv[iStart]);
    index++;

    ComPtr<IHost> host;
    CHECK_ERROR_RET(a->virtualBox, COMGETTER(Host)(host.asOutParam()), 1);

    ComPtr<IHostNetworkInterface> hif;
    CHECK_ERROR_RET(host, FindHostNetworkInterfaceByName(name.raw(), hif.asOutParam()), 1);

    Bstr guid;
    CHECK_ERROR_RET(hif, COMGETTER(Id)(guid.asOutParam()), 1);

    ComPtr<IProgress> progress;
    CHECK_ERROR_RET(host, RemoveHostOnlyNetworkInterface(guid.raw(), progress.asOutParam()), 1);

    rc = showProgress(progress);
    *pcProcessed = index - iStart;
    CHECK_PROGRESS_ERROR_RET(progress, ("Failed to remove the host-only adapter"), 1);

    return 0;
}
#endif

static const RTGETOPTDEF g_aHostOnlyIPOptions[]
    = {
        { "--dhcp",             'd', RTGETOPT_REQ_NOTHING },
        { "-dhcp",              'd', RTGETOPT_REQ_NOTHING },    // deprecated
        { "--ip",               'a', RTGETOPT_REQ_STRING },
        { "-ip",                'a', RTGETOPT_REQ_STRING },     // deprecated
        { "--netmask",          'm', RTGETOPT_REQ_STRING },
        { "-netmask",           'm', RTGETOPT_REQ_STRING },     // deprecated
        { "--ipv6",             'b', RTGETOPT_REQ_STRING },
        { "-ipv6",              'b', RTGETOPT_REQ_STRING },     // deprecated
        { "--netmasklengthv6",  'l', RTGETOPT_REQ_UINT8 },
        { "-netmasklengthv6",   'l', RTGETOPT_REQ_UINT8 }       // deprecated
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
    uint32_t uNetmasklengthv6 = (uint32_t)-1;
    const char *pIpv6 = NULL;
    const char *pIp = NULL;
    const char *pNetmask = NULL;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState,
                 a->argc,
                 a->argv,
                 g_aHostOnlyIPOptions,
                 RT_ELEMENTS(g_aHostOnlyIPOptions),
                 index,
                 RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 'd':   // --dhcp
                if (bDhcp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify --dhcp once.");
                else
                    bDhcp = true;
            break;
            case 'a':   // --ip
                if(pIp)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify --ip once.");
                else
                    pIp = ValueUnion.psz;
            break;
            case 'm':   // --netmask
                if(pNetmask)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify --netmask once.");
                else
                    pNetmask = ValueUnion.psz;
            break;
            case 'b':   // --ipv6
                if(pIpv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify --ipv6 once.");
                else
                    pIpv6 = ValueUnion.psz;
            break;
            case 'l':   // --netmasklengthv6
                if(bNetmasklengthv6)
                    return errorSyntax(USAGE_HOSTONLYIFS, "You can only specify --netmasklengthv6 once.");
                else
                {
                    bNetmasklengthv6 = true;
                    uNetmasklengthv6 = ValueUnion.u8;
                }
            break;
            case VINF_GETOPT_NOT_OPTION:
                return errorSyntax(USAGE_HOSTONLYIFS, "unhandled parameter: %s", ValueUnion.psz);
            break;
            default:
                if (c > 0)
                {
                    if (RT_C_IS_GRAPH(c))
                        return errorSyntax(USAGE_HOSTONLYIFS, "unhandled option: -%c", c);
                    else
                        return errorSyntax(USAGE_HOSTONLYIFS, "unhandled option: %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_HOSTONLYIFS, "unknown option: %s", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_HOSTONLYIFS, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_HOSTONLYIFS, "%Rrs", c);
        }
    }

    /* parameter sanity check */
    if (bDhcp && (bNetmasklengthv6 || pIpv6 || pIp || pNetmask))
        return errorSyntax(USAGE_HOSTONLYIFS, "You can not use --dhcp with static ip configuration parameters: --ip, --netmask, --ipv6 and --netmasklengthv6.");
    if((pIp || pNetmask) && (bNetmasklengthv6 || pIpv6))
        return errorSyntax(USAGE_HOSTONLYIFS, "You can not use ipv4 configuration (--ip and --netmask) with ipv6 (--ipv6 and --netmasklengthv6) simultaneously.");

    ComPtr<IHost> host;
    CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

    ComPtr<IHostNetworkInterface> hif;
    CHECK_ERROR(host, FindHostNetworkInterfaceByName(name.raw(),
                                                     hif.asOutParam()));

    if (FAILED(rc))
        return errorArgument("Could not find interface '%s'", a->argv[iStart]);

    if (bDhcp)
    {
        CHECK_ERROR(hif, EnableDynamicIpConfig ());
    }
    else if (pIp)
    {
        if (!pNetmask)
            pNetmask = "255.255.255.0"; /* ?? */

        CHECK_ERROR(hif, EnableStaticIpConfig(Bstr(pIp).raw(),
                                              Bstr(pNetmask).raw()));
    }
    else if (pIpv6)
    {
        if (uNetmasklengthv6 == (uint32_t)-1)
            uNetmasklengthv6 = 64; /* ?? */

        BOOL bIpV6Supported;
        CHECK_ERROR(hif, COMGETTER(IPV6Supported)(&bIpV6Supported));
        if (!bIpV6Supported)
        {
            RTMsgError("IPv6 setting is not supported for this adapter");
            return 1;
        }


        Bstr ipv6str(pIpv6);
        CHECK_ERROR(hif, EnableStaticIpConfigV6(ipv6str.raw(),
                                                (ULONG)uNetmasklengthv6));
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
#if defined(VBOX_WITH_NETFLT) && !defined(RT_OS_SOLARIS)
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
            result = errorSyntax(USAGE_HOSTONLYIFS, "Invalid parameter '%s'", Utf8Str(a->argv[i]).c_str());
            break;
        }
    }

    return result;
}

#endif /* !VBOX_ONLY_DOCS */
