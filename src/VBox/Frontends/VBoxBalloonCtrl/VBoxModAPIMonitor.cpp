
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
# include <VBox/com/com.h>
# include <VBox/com/string.h>
# include <VBox/com/Guid.h>
# include <VBox/com/array.h>
# include <VBox/com/ErrorInfo.h>
# include <VBox/com/errorprint.h>

# include <VBox/com/EventQueue.h>
# include <VBox/com/listeners.h>
# include <VBox/com/VirtualBox.h>
#endif /* !VBOX_ONLY_DOCS */

#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/version.h>

#include <package-generated.h>

#include <iprt/asm.h>
#include <iprt/buildconfig.h>
#include <iprt/critsect.h>
#include <iprt/getopt.h>
#include <iprt/initterm.h>
#include <iprt/path.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/system.h>

#include <map>
#include <string>
#include <signal.h>

#include "VBoxWatchdogInternal.h"

using namespace com;

#define VBOX_MOD_APIMON_NAME "apimon"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_APIMON
{
    GETOPTDEF_APIMON_ISLN_RESPONSE = 1000,
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

static unsigned long g_ulAPIMonIslnTimeoutMS = 0;
static Bstr g_strAPIMonGroups;
static Bstr g_strAPIMonIslnCmdResp;

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
                break;

            case GETOPTDEF_APIMON_ISLN_TIMEOUT:
                g_ulAPIMonIslnTimeoutMS = ValueUnion.u32;
                if (g_ulAPIMonIslnTimeoutMS < 1000)
                    g_ulAPIMonIslnTimeoutMS = 1000;
                break;

            case GETOPTDEF_APIMON_GROUPS:
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
        Bstr bstrValue;

        /* Host isolation timeout (in ms). */
        if (!g_ulAPIMonIslnTimeoutMS) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("VBoxInternal2/Watchdog/APIMonitor/IsolationTimeout").raw(),
                                                          bstrValue.asOutParam()));
            g_ulAPIMonIslnTimeoutMS = Utf8Str(bstrValue).toUInt32();
        }
        if (!g_ulAPIMonIslnTimeoutMS)
        {
             /* Default is 30 seconds timeout. */
            g_ulAPIMonIslnTimeoutMS = 30 * 1000;
        }

        /* VM groups to watch for. */
        if (g_strAPIMonGroups.isEmpty()) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("VBoxInternal2/Watchdog/APIMonitor/Groups").raw(),
                                                          g_strAPIMonGroups.asOutParam()));
        }

        /* Host isolation command response. */
        if (g_strAPIMonIslnCmdResp.isEmpty()) /* Not set by command line? */
        {
            CHECK_ERROR_BREAK(g_pVirtualBox, GetExtraData(Bstr("VBoxInternal2/Watchdog/APIMonitor/IsolationResponse").raw(),
                                                          g_strAPIMonIslnCmdResp.asOutParam()));
        }
    } while (0);

    return SUCCEEDED(rc) ? VINF_SUCCESS : VERR_COM_IPRT_ERROR; /* @todo Find a better rc! */
}

static DECLCALLBACK(int) VBoxModAPIMonitorMain(void)
{
    return VINF_SUCCESS; /* Nothing to do here right now. */
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
    "--apimon-isln-response   Sets the isolation response (shutdown VM).\n"
    "--apimon-isln-timeout    Sets the isolation timeout in ms (none).\n"
    "--apimon-groups          Sets the VM groups for monitoring (none).\n",
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

