/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox NAT Network service
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
#include "NATNetworkServiceRunner.h"
#include <iprt/process.h>
#include <iprt/param.h>
#include <iprt/env.h>

struct ARGDEF
{
    NATSCCFG Type;
    const char * Name;
};

#ifdef RT_OS_WINDOWS
# define NATSR_EXECUTABLE_NAME "VBoxNetLwipNAT.exe"
#else
# define NATSR_EXECUTABLE_NAME "VBoxNetLwipNAT"
#endif

static const ARGDEF g_aArgDefs[] =
{
    {NATSCCFG_NAME, "-n"},
    {NATSCCFG_TRUNKTYPE, "--trunk-type"},
    {NATSCCFG_MACADDRESS, "--mac-address"},
    {NATSCCFG_IPADDRESS, "--ip-address"},
    {NATSCCFG_NETMASK, "--netmask"},
    {NATSCCFG_PORTFORWARD4, "--pf4"},
    {NATSCCFG_PORTFORWARD6, "--pf6"}

};

static const ARGDEF * getArgDef(NATSCCFG type)
{
    for (unsigned i = 0; i < RT_ELEMENTS(g_aArgDefs); i++)
        if (g_aArgDefs[i].Type == type)
            return &g_aArgDefs[i];

    return NULL;
}

NATNetworkServiceRunner::NATNetworkServiceRunner()
{
    mProcess = NIL_RTPROCESS;
    for (unsigned i = 0; i < NATSCCFG_NOTOPT_MAXVAL; i++)
    {
        mOptionEnabled[i] = false;
    }
}

void NATNetworkServiceRunner::detachFromServer()
{
    mProcess = NIL_RTPROCESS;
}

int NATNetworkServiceRunner::start()
{
    if (isRunning())
        return VINF_ALREADY_INITIALIZED;

    const char * args[NATSCCFG_NOTOPT_MAXVAL * 2];

    /* get the path to the executable */
    char exePathBuf[RTPATH_MAX];
    const char *exePath = RTProcGetExecutablePath(exePathBuf, RTPATH_MAX);
    char *substrSl = strrchr(exePathBuf, '/');
    char *substrBs = strrchr(exePathBuf, '\\');
    char *suffix = substrSl ? substrSl : substrBs;

    if (suffix)
    {
        suffix++;
        strcpy(suffix, NATSR_EXECUTABLE_NAME);
    }
    else
        exePath = NATSR_EXECUTABLE_NAME;

    int index = 0;

    args[index++] = exePath;

    for (unsigned i = 0; i < NATSCCFG_NOTOPT_MAXVAL; i++)
    {
        if (mOptionEnabled[i])
        {
            const ARGDEF *pArgDef = getArgDef((NATSCCFG)i);
            if (!pArgDef)
                continue;
            args[index++] = pArgDef->Name;      // e.g. "--network"

            /* value can be null for e.g. --begin-config has no value
             * and thus check the mOptions string length here
             */
            if (mOptions[i].length())
                args[index++] = mOptions[i].c_str();  // value
        }
    }

    args[index++] = NULL;
    RTENV env;
    int rc = RTEnvCreate(&env);
    AssertRCReturn(rc,rc);
    
    RTEnvPutEx(env, "VBOX_LOG=e.l.f");

    rc = RTProcCreate(exePath, args, RTENV_DEFAULT, 0, &mProcess);
    if (RT_FAILURE(rc))
        mProcess = NIL_RTPROCESS;
    RTEnvDestroy(env);
    return rc;
}

int NATNetworkServiceRunner::stop()
{
    if (!isRunning())
        return VINF_OBJECT_DESTROYED;

    int rc = RTProcTerminate(mProcess);
    mProcess = NIL_RTPROCESS;
    return rc;
}

bool NATNetworkServiceRunner::isRunning()
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
