
/* $Id$ */
/** @file
 * VBoxModAPIMonitor - API monitor module for detecting host isolation.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
# include <VBox/com/errorprint.h>
#endif /* !VBOX_ONLY_DOCS */

#include "VBoxWatchdogInternal.h"

using namespace com;

#define VBOX_MOD_APIMON_NAME "apimon"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_APIMON
{
    GETOPTDEF_APIMON_ISLN_RESPONSE = 3000,
    GETOPTDEF_APIMON_ISLN_TIMEOUT,
    GETOPTDEF_APIMON_GROUPS
};

/**
 * The module's command line arguments.
 */
static const RTGETOPTDEF g_aAPIMonitorOpts[] = {
    { "--apimon-isln-response",  GETOPTDEF_APIMON_ISLN_RESPONSE,  RTGETOPT_REQ_STRING },
    { "--apimon-isln-timeout",   GETOPTDEF_APIMON_ISLN_TIMEOUT,   RTGETOPT_REQ_UINT32 },
    { "--apimon-groups",         GETOPTDEF_APIMON_GROUPS,         RTGETOPT_REQ_STRING }
};

enum APIMON_RESPONSE
{
    /** Unknown / unhandled response. */
    APIMON_RESPONSE_UNKNOWN    = 0,
    /** Tries to shut down all running VMs in
     *  a gentle manner. */
    APIMON_RESPONSE_SHUTDOWN   = 200
};

static Bstr                         g_strAPIMonGroups;

static APIMON_RESPONSE              g_enmAPIMonIslnResp     = APIMON_RESPONSE_UNKNOWN;
static unsigned long                g_ulAPIMonIslnTimeoutMS = 0;
static Bstr                         g_strAPIMonIslnLastBeat;
static uint64_t                     g_uAPIMonIslnLastBeatMS = 0;

int apimonResponseToEnum(const char *pszResponse, APIMON_RESPONSE *pResp)
{
    AssertPtrReturn(pszResponse, VERR_INVALID_POINTER);
    AssertPtrReturn(pResp, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;
    if (   !RTStrICmp(pszResponse, "shutdown")
        || !RTStrICmp(pszResponse, "poweroff"))
    {
        *pResp = APIMON_RESPONSE_SHUTDOWN;
    }
    else
        *pResp = APIMON_RESPONSE_UNKNOWN;

    return (*pResp > APIMON_RESPONSE_UNKNOWN ? VINF_SUCCESS : VERR_INVALID_PARAMETER);
}

int apimonMachineControl(const Bstr &strUuid, PVBOXWATCHDOG_MACHINE pMachine,
                         APIMON_RESPONSE enmResp)
{
    /** @todo Add other commands (with enmResp) here. */
    AssertPtrReturn(pMachine, VERR_INVALID_PARAMETER);

    serviceLog("Shutting down machine \"%ls\"\n", strUuid.raw());

    /* Open a session for the VM. */
    HRESULT rc;
    CHECK_ERROR_RET(pMachine->machine, LockMachine(g_pSession, LockType_Shared), VERR_ACCESS_DENIED);
    do
    {
        /* get the associated console */
        ComPtr<IConsole> console;
        CHECK_ERROR_BREAK(g_pSession, COMGETTER(Console)(console.asOutParam()));

        if (!g_fDryrun)
        {
            ComPtr<IProgress> progress;
            CHECK_ERROR_BREAK(console, PowerDown(progress.asOutParam()));
            if (g_fVerbose)
            {
                serviceLogVerbose(("Waiting for shutting down machine \"%ls\" ...\n",
                                   strUuid.raw()));
                progress->WaitForCompletion(-1);
            }
        }
    } while (0);

    /* Unlock the machine again. */
    g_pSession->UnlockMachine();

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR;
}

int apimonTrigger(APIMON_RESPONSE enmResp)
{
    int rc = VINF_SUCCESS;

    /** @todo Add proper grouping support! */
    bool fAllGroups = g_strAPIMonGroups.isEmpty();
    mapVMIter it = g_mapVM.begin();
    while (it != g_mapVM.end())
    {
        if (   !it->second.group.compare(g_strAPIMonGroups, Bstr::CaseInsensitive)
            || fAllGroups)
        {
            rc = apimonMachineControl(it->first /* Uuid */,
                                       &it->second, enmResp);
        }
        it++;
    }

    return rc;
}

/* Callbacks. */
static DECLCALLBACK(int) VBoxModAPIMonitorPreInit(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOption(int argc, char **argv)
{
    if (!argc) /* Take a shortcut. */
        return -1;

    AssertPtrReturn(argv, VERR_INVALID_PARAMETER);

    RTGETOPTSTATE GetState;
    int rc = RTGetOptInit(&GetState, argc, argv,
                          g_aAPIMonitorOpts, RT_ELEMENTS(g_aAPIMonitorOpts),
                          0 /* First */, 0 /*fFlags*/);
    if (RT_FAILURE(rc))
        return rc;

    rc = 0; /* Set default parsing result to valid. */

    int c;
    RTGETOPTUNION ValueUnion;
    while ((c = RTGetOpt(&GetState, &ValueUnion)))
    {
        switch (c)
        {
            case GETOPTDEF_APIMON_ISLN_RESPONSE:
                rc = apimonResponseToEnum(ValueUnion.psz, &g_enmAPIMonIslnResp);
                if (RT_FAILURE(rc))
                    rc = -1; /* Option unknown. */
                break;

            case GETOPTDEF_APIMON_ISLN_TIMEOUT:
                g_ulAPIMonIslnTimeoutMS = ValueUnion.u32;
                if (g_ulAPIMonIslnTimeoutMS < 1000)
                    g_ulAPIMonIslnTimeoutMS = 1000;
                break;

            case GETOPTDEF_APIMON_GROUPS:
                g_strAPIMonGroups = ValueUnion.psz;
                break;

            default:
                rc = -1; /* We don't handle this option, skip. */
                break;
        }
    }

    return rc;
}

static DECLCALLBACK(int) VBoxModAPIMonitorInit(void)
{
    HRESULT rc = S_OK;

    do
    {
        Bstr strValue;

        /* Host isolation timeout (in ms). */
        if (!g_ulAPIMonIslnTimeoutMS) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/IsolationTimeout").raw(),
                                                          strValue.asOutParam()));
            if (!strValue.isEmpty())
                g_ulAPIMonIslnTimeoutMS = Utf8Str(strValue).toUInt32();
        }
        if (!g_ulAPIMonIslnTimeoutMS) /* Still not set? Use a default. */
        {
            serviceLogVerbose(("API monitor isolation timeout not given, defaulting to 30s\n"));

            /* Default is 30 seconds timeout. */
            g_ulAPIMonIslnTimeoutMS = 30 * 1000;
        }

        /* VM groups to watch for. */
        if (g_strAPIMonGroups.isEmpty()) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/Groups").raw(),
                                                          g_strAPIMonGroups.asOutParam()));
        }

        /* Host isolation command response. */
        if (g_enmAPIMonIslnResp == APIMON_RESPONSE_UNKNOWN) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/IsolationResponse").raw(),
                                                          strValue.asOutParam()));
            if (!strValue.isEmpty())
            {
                int rc2 = apimonResponseToEnum(Utf8Str(strValue).c_str(), &g_enmAPIMonIslnResp);
                if (RT_FAILURE(rc2))
                {
                    serviceLog("Warning: API monitor response string invalid (%ls), default to shutdown\n",
                               strValue.raw());
                    g_enmAPIMonIslnResp = APIMON_RESPONSE_SHUTDOWN;
                }
            }
            else
                g_enmAPIMonIslnResp = APIMON_RESPONSE_SHUTDOWN;
        }
    } while (0);

    if (SUCCEEDED(rc))
    {
        g_uAPIMonIslnLastBeatMS = 0;
    }

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better rc! */
}

static DECLCALLBACK(int) VBoxModAPIMonitorMain(void)
{
    static uint64_t uLastRun = 0;
    uint64_t uNow = RTTimeProgramMilliTS();
    uint64_t uDelta = uNow - uLastRun;
    if (uDelta < 1000)
        return VINF_SUCCESS;
    uLastRun = uNow;

    int vrc = VINF_SUCCESS;
    HRESULT rc;

    serviceLogVerbose(("Checking for API heartbeat (%RU64ms) ...\n",
                       g_ulAPIMonIslnTimeoutMS));

    do
    {
        Bstr strHeartbeat;
        CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("Watchdog/APIMonitor/Heartbeat").raw(),
                                                      strHeartbeat.asOutParam()));
        if (   SUCCEEDED(rc)
            && !strHeartbeat.isEmpty()
            && g_strAPIMonIslnLastBeat.compare(strHeartbeat, Bstr::CaseSensitive))
        {
            serviceLogVerbose(("API heartbeat received, resetting timeout\n"));

            g_uAPIMonIslnLastBeatMS = 0;
            g_strAPIMonIslnLastBeat = strHeartbeat;
        }
        else
        {
            g_uAPIMonIslnLastBeatMS += uDelta;
            if (g_uAPIMonIslnLastBeatMS > g_ulAPIMonIslnTimeoutMS)
            {
                serviceLogVerbose(("No API heartbeat within time received (%RU64ms)\n",
                                   g_ulAPIMonIslnTimeoutMS));

                vrc = apimonTrigger(g_enmAPIMonIslnResp);
                g_uAPIMonIslnLastBeatMS = 0;
            }
        }
    } while (0);

    if (FAILED(rc))
        vrc = VERR_COM_IPRT_ERROR;

    return vrc;
}

static DECLCALLBACK(int) VBoxModAPIMonitorStop(void)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) VBoxModAPIMonitorTerm(void)
{
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineRegistered(const Bstr &strUuid)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineUnregistered(const Bstr &strUuid)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnMachineStateChanged(const Bstr &strUuid,
                                                                MachineState_T enmState)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) VBoxModAPIMonitorOnServiceStateChanged(bool fAvailable)
{
    if (!fAvailable)
        apimonTrigger(g_enmAPIMonIslnResp);
    return VINF_SUCCESS;
}

/**
 * The 'apimonitor' module description.
 */
VBOXMODULE g_ModAPIMonitor =
{
    /* pszName. */
    VBOX_MOD_APIMON_NAME,
    /* pszDescription. */
    "API monitor for host isolation detection",
    /* pszDepends. */
    NULL,
    /* uPriority. */
    0 /* Not used */,
    /* pszUsage. */
    " [--apimon-isln-response=<cmd>] [--apimon-isln-timeout=<ms>]\n"
    " [--apimon-groups=<string>]\n",
    /* pszOptions. */
    "--apimon-isln-response Sets the isolation response (shutdown VM).\n"
    "--apimon-isln-timeout  Sets the isolation timeout in ms (30s).\n"
    "--apimon-groups        Sets the VM groups for monitoring (none).\n",
    /* methods. */
    VBoxModAPIMonitorPreInit,
    VBoxModAPIMonitorOption,
    VBoxModAPIMonitorInit,
    VBoxModAPIMonitorMain,
    VBoxModAPIMonitorStop,
    VBoxModAPIMonitorTerm,
    /* callbacks. */
    VBoxModAPIMonitorOnMachineRegistered,
    VBoxModAPIMonitorOnMachineUnregistered,
    VBoxModAPIMonitorOnMachineStateChanged,
    VBoxModAPIMonitorOnServiceStateChanged
};

