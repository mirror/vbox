
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

#define VBOX_MOD_APIMONITOR_NAME "apimonitor"

/**
 * The module's RTGetOpt-IDs for the command line.
 */
enum GETOPTDEF_APIMONITOR
{
    GETOPTDEF_APIMONITOR_GROUPS = 1000,
};

/**
 * The module's command line arguments.
 */
static const RTGETOPTDEF g_aAPIMonitorOpts[] = {
    { "--apimonitor-groups",         GETOPTDEF_APIMONITOR_GROUPS,         RTGETOPT_REQ_STRING }
};

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
            case GETOPTDEF_APIMONITOR_GROUPS:
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
    return VINF_SUCCESS; /* Nothing to do here right now. */
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
    VBOX_MOD_APIMONITOR_NAME,
    /* pszDescription. */
    "API monitor for host isolation detection",
    /* pszDepends. */
    NULL,
    /* uPriority. */
    0 /* Not used */,
    /* pszUsage. */
    " [--apimonitor-groups <string>]\n"
    ,
    /* pszOptions. */
    "--apimonitor-groups    Sets the VM groups for monitoring (none).\n",
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

