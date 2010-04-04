
/* $Id$ */
/** @file
 * VBoxServiceControl - Host-driven Guest Control.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <iprt/assert.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

using namespace guestControl;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The control interval (millseconds). */
uint32_t g_ControlInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_hControlEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t         g_GuestControlSvcClientID = 0;

/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceControlPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceControlOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    int rc = -1;
    if (ppszShort)
        /* no short options */;
    else if (!strcmp(argv[*pi], "--control-interval"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_ControlInterval, 1, UINT32_MAX - 1);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceControlInit(void)
{
    /*
     * If not specified, find the right interval default.
     * Then create the event sem to block on.
     */
    if (!g_ControlInterval)
        g_ControlInterval = 1000;

    int rc = RTSemEventMultiCreate(&g_hControlEvent);
    AssertRCReturn(rc, rc);

    rc = VbglR3GuestCtrlConnect(&g_GuestControlSvcClientID);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "Control: Service Client ID: %#x\n", g_GuestControlSvcClientID);
    else
    {
        VBoxServiceError("Control: Failed to connect to the guest control service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_hControlEvent);
        g_hControlEvent = NIL_RTSEMEVENTMULTI;
    }

    return rc;
}


static int VBoxServiceControlHandleCmdExec(uint32_t u32ClientId, uint32_t uNumParms)
{
    VBoxServiceVerbose(3, "VBoxServiceControlHandleCmdExec: Called uNumParms=%ld\n", uNumParms);

    VBOXSERVICECTRLPROCDATA execData;
    execData.cbEnv = sizeof(execData.szEnv);

    int rc = VbglR3GuestCtrlGetHostCmdExec(u32ClientId, uNumParms,
                                           execData.szCmd, sizeof(execData.szCmd),
                                           &execData.uFlags,
                                           execData.szArgs, sizeof(execData.szArgs), &execData.uNumArgs,
                                           execData.szEnv, &execData.cbEnv, &execData.uNumEnvVars,
                                           execData.szStdIn, sizeof(execData.szStdIn),
                                           execData.szStdOut, sizeof(execData.szStdOut),
                                           execData.szStdErr, sizeof(execData.szStdErr),
                                           execData.szUser, sizeof(execData.szUser),
                                           execData.szPassword, sizeof(execData.szPassword),
                                           &execData.uTimeLimitMS);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: Failed to retrieve execution command! Error: %Rrc\n", rc);
    }
    else
    {
        /* Adjust time limit value. */
        execData.uTimeLimitMS = UINT32_MAX ?
            RT_INDEFINITE_WAIT : execData.uTimeLimitMS;

        /* Prepare argument list. */
        char **ppaArg;
        int iArgs;
        rc = RTGetOptArgvFromString(&ppaArg, &iArgs, execData.szArgs, NULL);
        Assert(execData.uNumArgs == iArgs);
        if (RT_SUCCESS(rc))
        {
            /* Prepare environment list. */
            char **ppaEnv;
            if (execData.uNumEnvVars)
            {
                ppaEnv = (char**)RTMemAlloc(execData.uNumEnvVars * sizeof(char*));
                AssertPtr(ppaEnv);

                char *pcCur = execData.szEnv;
                uint32_t i = 0;
                while (pcCur < execData.szEnv + execData.cbEnv)
                {
                    if (RTStrAPrintf(&ppaEnv[i++], "%s", pcCur) < 0)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }
                    pcCur += strlen(pcCur) + 1; /* Skip terminating zero. */
                }
            }

            if (RT_SUCCESS(rc))
            {
                /* Do the actual execution. */
                rc = VBoxServiceControlExecProcess(&execData, ppaArg, ppaEnv);
                for (uint32_t i = 0; i < execData.uNumEnvVars; i++)
                    RTStrFree(ppaEnv[i]);
                RTMemFree(ppaEnv);
            }
            RTGetOptArgvFree(ppaArg);
        }
    }
    return rc;
}


/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceControlWorker(bool volatile *pfShutdown)
{
    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    Assert(g_GuestControlSvcClientID > 0);

    int rc = VINF_SUCCESS;

    /*
     * Execution loop.
     *
     * @todo
     */
    for (;;)
    {
        uint32_t uMsg;
        uint32_t uNumParms;
        rc = VbglR3GuestCtrlGetHostMsg(g_GuestControlSvcClientID, &uMsg, &uNumParms);
        if (RT_SUCCESS(rc))
        {
            switch(uMsg)
            {
                case GETHOSTMSG_EXEC_CMD:
                    rc = VBoxServiceControlHandleCmdExec(g_GuestControlSvcClientID, uNumParms);
                    break;

                default:
                    VBoxServiceVerbose(3, "VBoxServiceControlWorker: Unsupported message from host! Msg=%ld\n", uMsg);
                    /* Don't terminate here; just wait for the next message. */
                    break;
            }
            break; /* DEBUG BREAK */
        }

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_hControlEvent, g_ControlInterval);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("VBoxServiceControlWorker: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    RTSemEventMultiDestroy(g_hControlEvent);
    g_hControlEvent = NIL_RTSEMEVENTMULTI;
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceControlStop(void)
{
    /** @todo Later, figure what to do if we're in RTProcWait(). it's a very
     *        annoying call since doesn't support timeouts in the posix world. */
    RTSemEventMultiSignal(g_hControlEvent);
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceControlTerm(void)
{
    /* Nothing here yet. */
    VbglR3GuestCtrlDisconnect(g_GuestControlSvcClientID);
    g_GuestControlSvcClientID = 0;

    if (g_hControlEvent != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(g_hControlEvent);
        g_hControlEvent = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_Control =
{
    /* pszName. */
    "control",
    /* pszDescription. */
    "Host-driven Guest Control",
    /* pszUsage. */
   "[--control-interval <ms>]"
    ,
    /* pszOptions. */
    "    --control-interval   Specifies the interval at which to check for\n"
    "                         new ocntrol commands. The default is 1000 ms.\n"
    ,
    /* methods */
    VBoxServiceControlPreInit,
    VBoxServiceControlOption,
    VBoxServiceControlInit,
    VBoxServiceControlWorker,
    VBoxServiceControlStop,
    VBoxServiceControlTerm
};

