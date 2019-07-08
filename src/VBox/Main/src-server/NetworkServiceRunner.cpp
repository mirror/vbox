/* $Id$ */
/** @file
 * VirtualBox Main - interface for VBox DHCP server
 */

/*
 * Copyright (C) 2009-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <map>
#include <string>
#include "NetworkServiceRunner.h"

#include <iprt/process.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/env.h>
#include <iprt/log.h>
#include <iprt/thread.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** @todo Convert to C strings as this is wastefull:    */
const std::string NetworkServiceRunner::kNsrKeyName      = "--name";
const std::string NetworkServiceRunner::kNsrKeyNetwork   = "--network";
const std::string NetworkServiceRunner::kNsrKeyTrunkType = "--trunk-type";
const std::string NetworkServiceRunner::kNsrTrunkName    = "--trunk-name";
const std::string NetworkServiceRunner::kNsrMacAddress   = "--mac-address";
const std::string NetworkServiceRunner::kNsrIpAddress    = "--ip-address";
const std::string NetworkServiceRunner::kNsrIpNetmask    = "--netmask";
const std::string NetworkServiceRunner::kNsrKeyNeedMain  = "--need-main";


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
struct NetworkServiceRunner::Data
{
    Data(const char* aProcName)
        : mProcName(aProcName)
        , mProcess(NIL_RTPROCESS)
        , mKillProcOnStop(false)
    {}
    const char *mProcName;
    RTPROCESS mProcess;
    std::map<std::string, std::string> mOptions; /**< @todo r=bird: A map for command line option/value pairs? really?
                                                  * Wouldn't a simple argument list have done it much much more efficiently? */
    bool mKillProcOnStop;
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


void NetworkServiceRunner::clearOptions()
{
    m->mOptions.clear();
}


void NetworkServiceRunner::detachFromServer()
{
    m->mProcess = NIL_RTPROCESS;
}


int NetworkServiceRunner::start(bool aKillProcOnStop)
{
    if (isRunning())
        return VINF_ALREADY_INITIALIZED;

    /*
     * Construct the path to the executable.  ASSUME it is relative to the
     * directory that holds VBoxSVC.
     */
    char szExePath[RTPATH_MAX];
    AssertReturn(RTProcGetExecutablePath(szExePath, RTPATH_MAX), VERR_FILENAME_TOO_LONG);
    RTPathStripFilename(szExePath);
    int vrc = RTPathAppend(szExePath, sizeof(szExePath), m->mProcName);
    AssertLogRelRCReturn(vrc, vrc);

    /*
     * Allocate the argument array and construct the argument vector.
     */
    size_t const cArgs     = 1 + m->mOptions.size() * 2 + 1;
    char const **papszArgs = (char const **)RTMemTmpAllocZ(sizeof(papszArgs[0]) * cArgs);
    AssertReturn(papszArgs, VERR_NO_TMP_MEMORY);

    size_t iArg = 0;
    papszArgs[iArg++] = szExePath;
    for (std::map<std::string, std::string>::const_iterator it = m->mOptions.begin(); it != m->mOptions.end(); ++it)
    {
        papszArgs[iArg++] = it->first.c_str();
        papszArgs[iArg++] = it->second.c_str();
    }
    Assert(iArg + 1 == cArgs);
    Assert(papszArgs[iArg] == NULL);

    /*
     * Start the process:
     */
    int rc = RTProcCreate(szExePath, papszArgs, RTENV_DEFAULT, 0, &m->mProcess);
    if (RT_FAILURE(rc))
        m->mProcess = NIL_RTPROCESS;

    m->mKillProcOnStop = aKillProcOnStop;

    RTMemTmpFree(papszArgs);
    return rc;
}


int NetworkServiceRunner::stop()
{
    /*
     * If the process already terminated, this function will also grab the exit
     * status and transition the process out of zombie status.
     */
    if (!isRunning())
        return VINF_OBJECT_DESTROYED;

    bool fDoKillProc = true;

    if (!m->mKillProcOnStop)
    {
        /*
         * This is a VBoxSVC Main client. Do NOT kill it but assume it was shut
         * down politely. Wait up to 1 second until the process is killed before
         * doing the final hard kill.
         */
        int rc = VINF_SUCCESS;
        for (unsigned int i = 0; i < 100; i++)
        {
            rc = RTProcWait(m->mProcess, RTPROCWAIT_FLAGS_NOBLOCK, NULL);
            if (RT_SUCCESS(rc))
                break;
            RTThreadSleep(10);
        }
        if (rc != VERR_PROCESS_RUNNING)
            fDoKillProc = false;
    }

    if (fDoKillProc)
    {
        RTProcTerminate(m->mProcess);
        int rc = RTProcWait(m->mProcess, RTPROCWAIT_FLAGS_BLOCK, NULL);
        NOREF(rc);
    }

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
