/* $Id$ */
/** @file
 * VBoxManage - Implementation of NAT Network command command.
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

typedef enum enMainOpCodes
{
    OP_ADD = 1000,
    OP_REMOVE,
    OP_MODIFY,
    OP_START,
    OP_STOP
} OPCODE;

static const RTGETOPTDEF g_aNATNetworkIPOptions[]
    = {
        { "--netname",          't', RTGETOPT_REQ_STRING },
        { "--network",          'n', RTGETOPT_REQ_STRING },
        { "--dhcp",             'h', RTGETOPT_REQ_BOOL },
        { "--ipv6",             '6', RTGETOPT_REQ_BOOL},
        { "--enable",           'e', RTGETOPT_REQ_NOTHING },
        { "--disable",          'd', RTGETOPT_REQ_NOTHING },

      };

static int handleOp(HandlerArg *a, OPCODE enmCode, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 2)
        return errorSyntax(USAGE_NATNETWORK, "Not enough parameters");

    int index = iStart;
    HRESULT rc;

    const char *pNetName = NULL;
    const char *pNetworkCidr = NULL;
    int enable = -1;
    int dhcp = -1;
    int ipv6 = -1;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState,
                 a->argc,
                 a->argv,
                 g_aNATNetworkIPOptions,
                 enmCode != OP_REMOVE ? RT_ELEMENTS(g_aNATNetworkIPOptions) : 4, /* we use only --netname and --ifname for remove*/
                 index,
                 RTGETOPTINIT_FLAGS_NO_STD_OPTS);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case 't':   // --netname
                if(pNetName)
                    return errorSyntax(USAGE_NATNETWORK, "You can only specify --netname once.");
                else
                {
                    pNetName = ValueUnion.psz;
                }
            break;
            case 'n':   // --network
                if(pNetworkCidr)
                    return errorSyntax(USAGE_NATNETWORK, "You can only specify --network once.");
                else
                {
                    pNetworkCidr = ValueUnion.psz;
                }
            break;
            case 'e':   // --enable
                if(enable >= 0)
                    return errorSyntax(USAGE_NATNETWORK, "You can specify either --enable or --disable once.");
                else
                {
                    enable = 1;
                }
            break;
            case 'd':   // --disable
                if(enable >= 0)
                    return errorSyntax(USAGE_NATNETWORK, "You can specify either --enable or --disable once.");
                else
                {
                    enable = 0;
                }
            break;
            case VINF_GETOPT_NOT_OPTION:
                return errorSyntax(USAGE_NATNETWORK, "unhandled parameter: %s", ValueUnion.psz);
            break;
            case 'h':
                if (dhcp != -1)
                    return errorSyntax(USAGE_NATNETWORK, "You can specify --dhcp once.");
                dhcp = ValueUnion.f;
                break;
            case '6':
                if (ipv6 != -1)
                    return errorSyntax(USAGE_NATNETWORK, "You can specify --ipv6 once.");
                ipv6 = ValueUnion.f;
                break;
            default:
                if (c > 0)
                {
                    if (RT_C_IS_GRAPH(c))
                        return errorSyntax(USAGE_NATNETWORK, "unhandled option: -%c", c);
                    else
                        return errorSyntax(USAGE_NATNETWORK, "unhandled option: %i", c);
                }
                else if (c == VERR_GETOPT_UNKNOWN_OPTION)
                    return errorSyntax(USAGE_NATNETWORK, "unknown option: %s", ValueUnion.psz);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_NATNETWORK, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_NATNETWORK, "%Rrs", c);
        }
    }

    if (!pNetName)
        return errorSyntax(USAGE_NATNETWORK, 
                           "You need to specify --netname option");
    /* verification */
    switch (enmCode)
    {
        case OP_ADD:
            if (!pNetworkCidr)
                return errorSyntax(USAGE_NATNETWORK, 
                                   "You need to specify --network option");
            break;
        case OP_MODIFY:
        case OP_REMOVE:
        case OP_START:
        case OP_STOP:
            break;
        default:
            AssertMsgFailedReturn(("Unknown operation (:%d)", enmCode), VERR_NOT_IMPLEMENTED);
    }

    Bstr NetName;
    NetName = Bstr(pNetName);
    

    ComPtr<INATNetwork> net;
    rc = a->virtualBox->FindNATNetworkByName(NetName.mutableRaw(), net.asOutParam());
    if(enmCode == OP_ADD)
    {
        if (SUCCEEDED(rc))
            return errorArgument("NATNetwork server already exists");

        CHECK_ERROR(a->virtualBox, CreateNATNetwork(NetName.raw(), net.asOutParam()));
        if (FAILED(rc))
            return errorArgument("Failed to create the NAT network service");
    }
    else if (FAILED(rc))
    {
        return errorArgument("NATNetwork server does not exist");
    }

    switch (enmCode)
    {
    case OP_ADD:
    case OP_MODIFY:
    {
        if (pNetworkCidr)
        {
            CHECK_ERROR(net, COMSETTER(Network)(Bstr(pNetworkCidr).raw()));
            if(FAILED(rc))
	      return errorArgument("Failed to set configuration");
        }
        if (dhcp >= 0)
        {
              CHECK_ERROR(net, COMSETTER(NeedDhcpServer) ((BOOL)dhcp));
              if(FAILED(rc))
                return errorArgument("Failed to set configuration");
        }
        
        if (ipv6 >= 0)
        {
              CHECK_ERROR(net, COMSETTER(IPv6Enabled) ((BOOL)ipv6));
              if(FAILED(rc))
                return errorArgument("Failed to set configuration");
        }
        
        if(enable >= 0)
        {
            CHECK_ERROR(net, COMSETTER(Enabled) ((BOOL)enable));
            if(FAILED(rc))
	      return errorArgument("Failed to set configuration");

        }
	break;
    }
    case OP_REMOVE:
    {
        CHECK_ERROR(a->virtualBox, RemoveNATNetwork(net));
        if(FAILED(rc))
	  return errorArgument("Failed to remove nat network");
	break;
    }
    case OP_START:
    {
	CHECK_ERROR(net, Start(Bstr("whatever").raw()));
        if(FAILED(rc))
	  return errorArgument("Failed to start network");
	break;
    }
    case OP_STOP:
    {
	CHECK_ERROR(net, Stop());
        if(FAILED(rc))
	  return errorArgument("Failed to start network");
	break;
    }
    default:;
    }
    return 0;
}


int handleNATNetwork(HandlerArg *a)
{
    if (a->argc < 1)
        return errorSyntax(USAGE_NATNETWORK, "Not enough parameters");

    int result;
    int cProcessed;
    if (strcmp(a->argv[0], "modify") == 0)
        result = handleOp(a, OP_MODIFY, 1, &cProcessed);
    else if (strcmp(a->argv[0], "add") == 0)
        result = handleOp(a, OP_ADD, 1, &cProcessed);
    else if (strcmp(a->argv[0], "remove") == 0)
        result = handleOp(a, OP_REMOVE, 1, &cProcessed);
    else if (strcmp(a->argv[0], "start") == 0)
        result = handleOp(a, OP_START, 1, &cProcessed);
    else if (strcmp(a->argv[0], "stop") == 0)
        result = handleOp(a, OP_STOP, 1, &cProcessed);
    else
        result = errorSyntax(USAGE_NATNETWORK, "Invalid parameter '%s'", Utf8Str(a->argv[0]).c_str());

    return result;
}

#endif /* !VBOX_ONLY_DOCS */
