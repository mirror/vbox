/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server
 */

/*
 * Copyright (C) 2009-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "NetworkServiceRunner.h"
#include <iprt/process.h>
#include <iprt/param.h>
#include <iprt/env.h>

struct ARGDEF
{
    NETCFG Type;
    const char * Name;
};

static const ARGDEF g_aArgDefs[] =
{
    {NETCFG_NAME, "--name"},
    {NETCFG_NETNAME, "--network"},
    {NETCFG_TRUNKTYPE, "--trunk-type"},
    {NETCFG_TRUNKNAME, "--trunk-name"},
    {NETCFG_MACADDRESS, "--mac-address"},
    {NETCFG_IPADDRESS, "--ip-address"},
    {NETCFG_VERBOSE, "--verbose"},
    {NETCFG_NETMASK, "--netmask"},
};

static const ARGDEF * getArgDef(NETCFG type)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aArgDefs); i++)
        if (g_aArgDefs[i].Type == type)
            return &g_aArgDefs[i];

    return NULL;
}

void NetworkServiceRunner::detachFromServer()
{
    mProcess = NIL_RTPROCESS;
}

int NetworkServiceRunner::start()
{
    if (isRunning())
        return VINF_ALREADY_INITIALIZED;

    const char * args[NETCFG_NOTOPT_MAXVAL * 2];

    /* get the path to the executable */
    char exePathBuf[RTPATH_MAX];
    const char *exePath = RTProcGetExecutablePath(exePathBuf, RTPATH_MAX);
    char *substrSl = strrchr(exePathBuf, '/');
    char *substrBs = strrchr(exePathBuf, '\\');
    char *suffix = substrSl ? substrSl : substrBs;

    if (suffix)
    {
        suffix++;
        strcpy(suffix, mProcName);
    }

    int index = 0;

    args[index++] = exePath;

    for (unsigned i = 0; i < NETCFG_NOTOPT_MAXVAL; i++)
    {
        if (mOptionEnabled[i])
        {
            const ARGDEF *pArgDef = getArgDef((NETCFG)i);
            if (!pArgDef)
                continue;
            args[index++] = pArgDef->Name;

            if (mOptions[i].length())
                args[index++] = mOptions[i].c_str();  // value
        }
    }

    args[index++] = NULL;

    int rc = RTProcCreate(suffix ? exePath : mProcName, args, RTENV_DEFAULT, 0, &mProcess);
    if (RT_FAILURE(rc))
        mProcess = NIL_RTPROCESS;

    return rc;
}

int NetworkServiceRunner::stop()
{
    if (!isRunning())
        return VINF_OBJECT_DESTROYED;

    int rc = RTProcTerminate(mProcess);
    RTProcWait(mProcess, RTPROCWAIT_FLAGS_BLOCK, NULL);
    mProcess = NIL_RTPROCESS;
    return rc;
}

bool NetworkServiceRunner::isRunning()
{
    if (mProcess == NIL_RTPROCESS)
        return false;

    RTPROCSTATUS status;
    int rc = RTProcWait(mProcess, RTPROCWAIT_FLAGS_NOBLOCK, &status);

    if (rc == VERR_PROCESS_RUNNING)
        return true;

    mProcess = NIL_RTPROCESS;
    return false;
}
