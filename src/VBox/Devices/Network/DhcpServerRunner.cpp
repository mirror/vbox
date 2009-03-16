/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include "DhcpServerRunner.h"
#include <iprt/process.h>
#include <iprt/param.h>
#include <iprt/env.h>

struct ARGDEF
{
    DHCPCFG Type;
    const char * Name;
};

#ifdef RT_OS_WINDOWS
# define DHCP_EXECUTABLE_NAME "VBoxNetDHCP.exe"
#else
# define DHCP_EXECUTABLE_NAME "VBoxNetDHCP"
#endif

static const ARGDEF g_aArgDefs[] = {
        {DHCPCFG_NAME, "--name "},
        {DHCPCFG_NETNAME, "--network "},
        {DHCPCFG_TRUNKTYPE, "--trunk-type "},
        {DHCPCFG_TRUNKNAME, "--trunk-name "},
        {DHCPCFG_MACADDRESS, "--mac-address "},
        {DHCPCFG_IPADDRESS, "--ip-address "},
        {DHCPCFG_LEASEDB, "--lease-db"},
        {DHCPCFG_VERBOSE, "--verbose"},
        {DHCPCFG_BEGINCONFIG, "--begin-config"},
        {DHCPCFG_GATEWAY, "--gateway "},
        {DHCPCFG_LOWERIP, "--lower-ip "},
        {DHCPCFG_UPPERIP, "--upper-ip "},
        {DHCPCFG_NETMASK, "--netmask "},
        {DHCPCFG_HELP, "--help"},
        {DHCPCFG_VERSION, "--version"}
};

static const ARGDEF * getArgDef(DHCPCFG type)
{
    for(int i = 0; i < sizeof(g_aArgDefs)/sizeof(g_aArgDefs[0]); i++)
    {
        if(g_aArgDefs[i].Type == type)
        {
            return &g_aArgDefs[i];
        }
    }
    return NULL;
}

void DhcpServerRunner::detachFromServer()
{
    mProcess = NIL_RTPROCESS;
}

int DhcpServerRunner::start()
{
    if(isRunning())
        return VINF_ALREADY_INITIALIZED;

    const char * args[DHCPCFG_NOTOPT_MAXVAL * 2];

    /* get the path to the executable */
//    const char *exePath = DHCP_EXECUTABLE_NAME;
    char exePathBuf [RTPATH_MAX];
    char *exePath = RTProcGetExecutableName (exePathBuf, RTPATH_MAX);
    char *substrSl = strrchr( exePath, '/');
    char *substrBs = strrchr( exePath, '\\');
    char *suffix = substrSl ? substrSl : substrBs;

    if(suffix)
    {
        suffix++;
        strcpy(suffix, DHCP_EXECUTABLE_NAME);
    }
    else
    {
        exePath = DHCP_EXECUTABLE_NAME;
    }

    int index = 0;

    args[index++] = exePath;

    for(int i = 0; i < DHCPCFG_NOTOPT_MAXVAL; i++)
    {
        if(!mOptions[i].isNull())
        {
            const ARGDEF * pArgDef = getArgDef((DHCPCFG)i);
            args[index++] = pArgDef->Name;
            if(!mOptions[i].isEmpty())
            {
                args[index++] = mOptions[i].raw();
            }
        }
    }

    args[index++] = NULL;

    int rc = RTProcCreate (exePath, args, RTENV_DEFAULT, 0, &mProcess);
    if (RT_FAILURE (rc))
    {
        mProcess = NIL_RTPROCESS;
    }
    return rc;
}

int DhcpServerRunner::stop()
{
    if(!isRunning())
        return VINF_OBJECT_DESTROYED;

    int rc = RTProcTerminate(mProcess);
    mProcess = NIL_RTPROCESS;
    return rc;
}

bool DhcpServerRunner::isRunning()
{
    if(mProcess == NIL_RTPROCESS)
        return false;

    RTPROCSTATUS status;
    int rc = RTProcWait(mProcess, RTPROCWAIT_FLAGS_NOBLOCK, &status);

    if(rc == VERR_PROCESS_RUNNING)
        return true;

    mProcess = NIL_RTPROCESS;
    return false;
}
