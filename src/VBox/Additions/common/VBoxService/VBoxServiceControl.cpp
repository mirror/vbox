/* $Id$ */
/** @file
 * VBoxServiceControl - Host-driven Guest Control.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <iprt/assert.h>
#include <iprt/getopt.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServiceControlExecThread.h"

using namespace guestControl;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The control interval (milliseconds). */
uint32_t g_ControlInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI      g_hControlEvent = NIL_RTSEMEVENTMULTI;
/** The Guest Control service client ID. */
static uint32_t             g_GuestControlSvcClientID = 0;
/** List of spawned processes. */
RTLISTNODE                  g_GuestControlExecThreads;
/** Critical section protecting g_GuestControlExecThreads. */
RTCRITSECT                  g_GuestControlExecThreadsCritSect;

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
    {
        VBoxServiceVerbose(3, "Control: Service Client ID: %#x\n", g_GuestControlSvcClientID);

        /* Init thread list. */
        RTListInit(&g_GuestControlExecThreads);
        rc = RTCritSectInit(&g_GuestControlExecThreadsCritSect);
        AssertRC(rc);
    }
    else
    {
        /* If the service was not found, we disable this service without
           causing VBoxService to fail. */
        if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
        {
            VBoxServiceVerbose(0, "Control: Guest control service is not available\n");
            rc = VERR_SERVICE_DISABLED;
        }
        else
            VBoxServiceError("Control: Failed to connect to the guest control service! Error: %Rrc\n", rc);
        RTSemEventMultiDestroy(g_hControlEvent);
        g_hControlEvent = NIL_RTSEMEVENTMULTI;
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
        VBoxServiceVerbose(3, "Control: Waiting for host msg ...\n");
        rc = VbglR3GuestCtrlWaitForHostMsg(g_GuestControlSvcClientID, &uMsg, &uNumParms);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_TOO_MUCH_DATA)
            {
                VBoxServiceVerbose(4, "Control: Message requires %ld parameters, but only 2 supplied -- retrying request (no error!)...\n", uNumParms);
                rc = VINF_SUCCESS; /* Try to get "real" message in next block below. */
            }
            else
                VBoxServiceVerbose(3, "Control: Getting host message failed with %Rrc\n", rc); /* VERR_GEN_IO_FAILURE seems to be normal if ran  into timeout. */
        }

        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Control: Msg=%u (%u parms) retrieved\n", uMsg, uNumParms);
            switch(uMsg)
            {
                case HOST_CANCEL_PENDING_WAITS:
                    VBoxServiceVerbose(3, "Control: Host asked us to quit ...\n");
                    break;

                case HOST_EXEC_CMD:
                    rc = VBoxServiceControlExecHandleCmdStartProcess(g_GuestControlSvcClientID, uNumParms);
                    break;

                case HOST_EXEC_SET_INPUT:
                    /** @todo Make buffer size configurable via guest properties/argv! */
                    rc = VBoxServiceControlExecHandleCmdSetInput(g_GuestControlSvcClientID, uNumParms, _1M /* Buffer size */);
                    break;

                case HOST_EXEC_GET_OUTPUT:
                    rc = VBoxServiceControlExecHandleCmdGetOutput(g_GuestControlSvcClientID, uNumParms);
                    break;

                default:
                    VBoxServiceVerbose(3, "Control: Unsupported message from host! Msg=%u\n", uMsg);
                    /* Don't terminate here; just wait for the next message. */
                    break;
            }

            if (RT_FAILURE(rc))
                VBoxServiceVerbose(3, "Control: Message was processed with rc=%Rrc\n", rc);
        }

        /* Do we need to shutdown? */
        if (   *pfShutdown
            || uMsg == HOST_CANCEL_PENDING_WAITS)
        {
            rc = VINF_SUCCESS;
            break;
        }

        /* Let's sleep for a bit and let others run ... */
        RTThreadYield();
    }

    RTSemEventMultiDestroy(g_hControlEvent);
    g_hControlEvent = NIL_RTSEMEVENTMULTI;
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceControlStop(void)
{
    VBoxServiceVerbose(3, "Control: Stopping ...\n");

    /** @todo Later, figure what to do if we're in RTProcWait(). it's a very
     *        annoying call since doesn't support timeouts in the posix world. */
    RTSemEventMultiSignal(g_hControlEvent);

    /*
     * Ask the host service to cancel all pending requests so that we can
     * shutdown properly here.
     */
    if (g_GuestControlSvcClientID)
    {
        int rc = VbglR3GuestCtrlCancelPendingWaits(g_GuestControlSvcClientID);
        if (RT_FAILURE(rc))
            VBoxServiceError("Control: Cancelling pending waits failed; rc=%Rrc\n", rc);
    }
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceControlTerm(void)
{
    VBoxServiceVerbose(3, "Control: Terminating ...\n");

    VBoxServiceControlExecThreadsShutdown();

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
   "              [--control-interval <ms>]"
    ,
    /* pszOptions. */
    "    --control-interval      Specifies the interval at which to check for\n"
    "                            new control commands. The default is 1000 ms.\n"
    ,
    /* methods */
    VBoxServiceControlPreInit,
    VBoxServiceControlOption,
    VBoxServiceControlInit,
    VBoxServiceControlWorker,
    VBoxServiceControlStop,
    VBoxServiceControlTerm
};

