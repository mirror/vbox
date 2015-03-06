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

#include <map>
#include <string>
#include "NetworkServiceRunner.h"
#include <iprt/process.h>
#include <iprt/param.h>
#include <iprt/env.h>


const std::string NetworkServiceRunner::kNsrKeyName = "--name";
const std::string NetworkServiceRunner::kNsrKeyNetwork = "--network";
const std::string NetworkServiceRunner::kNsrKeyTrunkType = "--trunk-type";
const std::string NetworkServiceRunner::kNsrTrunkName = "--trunk-name";
const std::string NetworkServiceRunner::kNsrMacAddress = "--mac-address";
const std::string NetworkServiceRunner::kNsrIpAddress = "--ip-address";
const std::string NetworkServiceRunner::kNsrIpNetmask = "--netmask";
const std::string NetworkServiceRunner::kNsrKeyNeedMain = "--need-main";

struct NetworkServiceRunner::Data
{
    Data(const char* aProcName):mProcName(aProcName), mProcess(NIL_RTPROCESS){}
    const char *mProcName;
    RTPROCESS mProcess;
    std::map<std::string, std::string> mOptions;
};

NetworkServiceRunner::NetworkServiceRunner(const char *aProcName)
{
    m = new NetworkServiceRunner::Data(aProcName);

}


NetworkServiceRunner::~NetworkServiceRunner()
{
    stop();
    delete m;
    m = NULL;
}


int NetworkServiceRunner::setOption(const std::string& key, const std::string& val)
{
    m->mOptions.insert(std::map<std::string, std::string>::value_type(key, val));
    return VINF_SUCCESS;
}


void NetworkServiceRunner::detachFromServer()
{
    m->mProcess = NIL_RTPROCESS;
}


int NetworkServiceRunner::start()
{
    if (isRunning())
        return VINF_ALREADY_INITIALIZED;

    const char * args[10*2];

    AssertReturn(m->mOptions.size() < 10, VERR_INTERNAL_ERROR);

    /* get the path to the executable */
    char exePathBuf[RTPATH_MAX];
    const char *exePath = RTProcGetExecutablePath(exePathBuf, RTPATH_MAX);
    char *substrSl = strrchr(exePathBuf, '/');
    char *substrBs = strrchr(exePathBuf, '\\');
    char *suffix = substrSl ? substrSl : substrBs;

    if (suffix)
    {
        suffix++;
        strcpy(suffix, m->mProcName);
    }

    int index = 0;

    args[index++] = exePath;

    std::map<std::string, std::string>::const_iterator it;
    for(it = m->mOptions.begin(); it != m->mOptions.end(); ++it)
    {
        args[index++] = it->first.c_str();
        args[index++] = it->second.c_str();
    }

    args[index++] = NULL;

    int rc = RTProcCreate(suffix ? exePath : m->mProcName, args, RTENV_DEFAULT, 0, &m->mProcess);
    if (RT_FAILURE(rc))
        m->mProcess = NIL_RTPROCESS;

    return rc;
}


int NetworkServiceRunner::stop()
{
    if (!isRunning())
        return VINF_OBJECT_DESTROYED;

    m->mProcess = NIL_RTPROCESS;
    return VINF_SUCCESS;
}

bool NetworkServiceRunner::isRunning()
{
    if (m->mProcess == NIL_RTPROCESS)
        return false;

    RTPROCSTATUS status;
    int rc = RTProcWait(m->mProcess, RTPROCWAIT_FLAGS_NOBLOCK, &status);

    if (rc == VERR_PROCESS_RUNNING)
        return true;

    m->mProcess = NIL_RTPROCESS;
    return false;
}
