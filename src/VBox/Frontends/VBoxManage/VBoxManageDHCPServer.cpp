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

typedef enum enMainOpCodes
{
    OP_ADD = 1000,
    OP_REMOVE,
    OP_MODIFY,
} OPCODE;

enum enOptionCodes
{
    NETNAME = 1000,
    IFNAME,
    IP,
    NETMASK,
    LOWERIP,
    UPPERIP,
    ENABLE,
    DISABLE
};

static const RTGETOPTDEF g_aListOptions[]
    = {
        { "-netname",           NETNAME, RTGETOPT_REQ_STRING },
        { "-ifname",            IFNAME,  RTGETOPT_REQ_STRING },
        { "-ip",                IP,      RTGETOPT_REQ_STRING },
        { "-netmask",           NETMASK, RTGETOPT_REQ_STRING },
        { "-lowerip",           LOWERIP, RTGETOPT_REQ_STRING },
        { "-upperip",           UPPERIP, RTGETOPT_REQ_STRING },
        { "-enable",            ENABLE,  RTGETOPT_REQ_NOTHING },
        { "-disable",           DISABLE,  RTGETOPT_REQ_NOTHING }
      };

static int handleOp(HandlerArg *a, OPCODE enmCode, int iStart, int *pcProcessed)
{
    if (a->argc - iStart < 2)
        return errorSyntax(USAGE_DHCPSERVER, "Not enough parameters");

    int index = iStart;
    HRESULT rc;

    const char *pNetName = NULL;
    const char *pIfName = NULL;
    const char * pIp = NULL;
    const char * pNetmask = NULL;
    const char * pLowerIp = NULL;
    const char * pUpperIp = NULL;
    int enable = -1;

    int c;
    RTGETOPTUNION ValueUnion;
    RTGETOPTSTATE GetState;
    RTGetOptInit(&GetState,
                 a->argc,
                 a->argv,
                 g_aListOptions,
                 enmCode != OP_REMOVE ? RT_ELEMENTS(g_aListOptions): 2, /* we use only -netname and -ifname for remove*/
                 index,
                 0 /* fFlags */);
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case NETNAME:
                if(pNetName)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -netname once.");
                else if (pIfName)
                    return errorSyntax(USAGE_DHCPSERVER, "You can either use a -netname or -ifname for identifying the dhcp server.");
                else
                {
                    pNetName = ValueUnion.psz;
                }
            break;
            case IFNAME:
                if(pIfName)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -ifname once.");
                else if (pNetName)
                    return errorSyntax(USAGE_DHCPSERVER, "You can either use a -netname or -ipname for identifying the dhcp server.");
                else
                {
                    pIfName = ValueUnion.psz;
                }
            break;
            case IP:
                if(pIp)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -ip once.");
                else
                {
                    pIp = ValueUnion.psz;
                }
            break;
            case NETMASK:
                if(pNetmask)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -netmask once.");
                else
                {
                    pNetmask = ValueUnion.psz;
                }
            break;
            case LOWERIP:
                if(pLowerIp)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -lowerip once.");
                else
                {
                    pLowerIp = ValueUnion.psz;
                }
            break;
            case UPPERIP:
                if(pUpperIp)
                    return errorSyntax(USAGE_DHCPSERVER, "You can only specify -upperip once.");
                else
                {
                    pUpperIp = ValueUnion.psz;
                }
            break;
            case ENABLE:
                if(enable >= 0)
                    return errorSyntax(USAGE_DHCPSERVER, "You can specify either -enable or -disable once.");
                else
                {
                    enable = 1;
                }
            break;
            case DISABLE:
                if(enable >= 0)
                    return errorSyntax(USAGE_DHCPSERVER, "You can specify either -enable or -disable once.");
                else
                {
                    enable = 0;
                }
            break;
            case VINF_GETOPT_NOT_OPTION:
            case VERR_GETOPT_UNKNOWN_OPTION:
                return errorSyntax(USAGE_DHCPSERVER, "Unknown option \"%s\".", ValueUnion.psz);
            break;
            default:
                if (c > 0)
                    return errorSyntax(USAGE_DHCPSERVER, "missing case: %c\n", c);
                else if (ValueUnion.pDef)
                    return errorSyntax(USAGE_DHCPSERVER, "%s: %Rrs", ValueUnion.pDef->pszLong, c);
                else
                    return errorSyntax(USAGE_DHCPSERVER, "%Rrs", c);
        }
    }

    if(! pNetName && !pIfName)
        return errorSyntax(USAGE_DHCPSERVER, "You need to specify either -netname or -ifname to identify the dhcp server");

    if(enmCode != OP_REMOVE)
    {
        if(enable < 0 || pIp || pNetmask || pLowerIp || pUpperIp)
        {
            if(!pIp)
                return errorSyntax(USAGE_DHCPSERVER, "You need to specify -ip option");

            if(!pNetmask)
                return errorSyntax(USAGE_DHCPSERVER, "You need to specify -netmask option");

            if(!pLowerIp)
                return errorSyntax(USAGE_DHCPSERVER, "You need to specify -lowerip option");

            if(!pUpperIp)
                return errorSyntax(USAGE_DHCPSERVER, "You need to specify -upperip option");
        }
    }

    Bstr NetName;
    if(!pNetName)
    {
        ComPtr<IHost> host;
        CHECK_ERROR(a->virtualBox, COMGETTER(Host)(host.asOutParam()));

        ComPtr<IHostNetworkInterface> hif;
        CHECK_ERROR(host, FindHostNetworkInterfaceByName(Bstr(pIfName).mutableRaw(), hif.asOutParam()));
        if(FAILED(rc))
            return errorArgument("could not find interface '%s'", pIfName);

        CHECK_ERROR(hif, COMGETTER(NetworkName) (NetName.asOutParam()));
        if(FAILED(rc))
            return errorArgument("could not get network name for the interface '%s'", pIfName);
    }
    else
    {
        NetName = Bstr(pNetName);
    }

    ComPtr<IDHCPServer> svr;
    rc = a->virtualBox->FindDHCPServerByNetworkName(NetName.mutableRaw(), svr.asOutParam());
    if(enmCode == OP_ADD)
    {
        if(SUCCEEDED(rc))
            return errorArgument("dhcp server already exists");

        CHECK_ERROR(a->virtualBox, CreateDHCPServer(NetName.mutableRaw(), svr.asOutParam()));
        if(FAILED(rc))
            return errorArgument("failed to create server");
    }
    else if(FAILED(rc))
    {
        return errorArgument("dhcp server does not exist");
    }

    if(enmCode != OP_REMOVE)
    {
        if (pIp || pNetmask || pLowerIp || pUpperIp)
        {
            CHECK_ERROR(svr, SetConfiguration (Bstr(pIp).mutableRaw(), Bstr(pNetmask).mutableRaw(), Bstr(pLowerIp).mutableRaw(), Bstr(pUpperIp).mutableRaw()));
            if(FAILED(rc))
                return errorArgument("failed to set configuration");
        }

        if(enable >= 0)
        {
            CHECK_ERROR(svr, COMSETTER(Enabled) ((BOOL)enable));
        }
    }
    else
    {
        CHECK_ERROR(a->virtualBox, RemoveDHCPServer(svr));
        if(FAILED(rc))
            return errorArgument("failed to remove server");
    }

    return 0;
}


int handleDHCPServer(HandlerArg *a)
{
    int result = 0;
    if (a->argc < 1)
        return errorSyntax(USAGE_DHCPSERVER, "Not enough parameters");

    for (int i = 0; i < a->argc; i++)
    {
        if (strcmp(a->argv[i], "modify") == 0)
        {
            int cProcessed;
            result = handleOp(a, OP_MODIFY, i+1, &cProcessed);
            break;
        }
        else if (strcmp(a->argv[i], "add") == 0)
        {
            int cProcessed;
            result = handleOp(a, OP_ADD, i+1, &cProcessed);
            break;
        }
        else if (strcmp(a->argv[i], "remove") == 0)
        {
            int cProcessed;
            result = handleOp(a, OP_REMOVE, i+1, &cProcessed);
            break;
        }
        else
        {
            result = errorSyntax(USAGE_DHCPSERVER, "Invalid parameter '%s'", Utf8Str(a->argv[i]).raw());
            break;
        }
    }

    return result;
}

#endif /* !VBOX_ONLY_DOCS */
