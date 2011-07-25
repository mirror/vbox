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
#include <iprt/asm.h>
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
/** The guest control service client ID. */
static uint32_t             g_GuestControlSvcClientID = 0;
/** How many started guest processes are kept into memory for supplying
 *  information to the host. Default is 5 processes. If 0 is specified,
 *  the maximum number of processes is unlimited. */
uint32_t                    g_GuestControlProcsMaxKept = 5;
/** List of guest control threads. */
RTLISTNODE                  g_GuestControlThreads;
/** Critical section protecting g_GuestControlExecThreads. */
RTCRITSECT                  g_GuestControlThreadsCritSect;

/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceControlPreInit(void)
{
#ifdef VBOX_WITH_GUEST_PROPS
    /*
     * Read the service options from the VM's guest properties.
     * Note that these options can be overridden by the command line options later.
     */
    uint32_t uGuestPropSvcClientID;
    int rc = VbglR3GuestPropConnect(&uGuestPropSvcClientID);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_HGCM_SERVICE_NOT_FOUND) /* Host service is not available. */
        {
            VBoxServiceVerbose(0, "Control: Guest property service is not available, skipping\n");
            rc = VINF_SUCCESS;
        }
        else
            VBoxServiceError("Control: Failed to connect to the guest property service! Error: %Rrc\n", rc);
    }
    else
    {
        rc = VBoxServiceReadPropUInt32(uGuestPropSvcClientID, "/VirtualBox/GuestAdd/VBoxService/--control-procs-max-kept",
                                       &g_GuestControlProcsMaxKept, 0, UINT32_MAX - 1);

        VbglR3GuestPropDisconnect(uGuestPropSvcClientID);
    }

    if (rc == VERR_NOT_FOUND) /* If a value is not found, don't be sad! */
        rc = VINF_SUCCESS;
    return rc;
#else
    /* Nothing to do here yet. */
    return VINF_SUCCESS;
#endif
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
    else if (!strcmp(argv[*pi], "--control-procs-max-kept"))
        rc = VBoxServiceArgUInt32(argc, argv, "", pi,
                                  &g_GuestControlProcsMaxKept, 0, UINT32_MAX - 1);
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
        VBoxServiceVerbose(3, "Control: Service client ID: %#x\n", g_GuestControlSvcClientID);

        /* Init thread list. */
        RTListInit(&g_GuestControlThreads);
        rc = RTCritSectInit(&g_GuestControlThreadsCritSect);
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

void VBoxServiceControlThreadSignalShutdown(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturnVoid(pThread);
    ASMAtomicXchgBool(&pThread->fShutdown, true);
}


int VBoxServiceControlThreadWaitForShutdown(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    int rc = VINF_SUCCESS;
    if (pThread->Thread != NIL_RTTHREAD)
    {
        /* Wait a bit ... */
        rc = RTThreadWait(pThread->Thread, 30 * 1000 /* Wait 30 seconds max. */, NULL);
    }
    return rc;
}


static void VBoxServiceControlDestroyThreads(void)
{
    VBoxServiceVerbose(3, "Control: Destroying threads ...\n");

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        /* Signal all threads that we want to shutdown. */
        PVBOXSERVICECTRLTHREAD pNode;
        RTListForEach(&g_GuestControlThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
            VBoxServiceControlThreadSignalShutdown(pNode);

        /* Wait for threads to shutdown. */
        RTListForEach(&g_GuestControlThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
        {
            int rc2 = VBoxServiceControlThreadWaitForShutdown(pNode);
            if (RT_FAILURE(rc2))
                VBoxServiceError("Control: Thread failed to stop; rc2=%Rrc\n", rc2);

            /* Destroy thread specific data. */
            switch (pNode->enmType)
            {
                case kVBoxServiceCtrlThreadDataExec:
                    VBoxServiceControlExecThreadDataDestroy((PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData);
                    break;

                default:
                    break;
            }
        }

        /* Finally destroy thread list. */
        pNode = RTListGetFirst(&g_GuestControlThreads, VBOXSERVICECTRLTHREAD, Node);
        while (pNode)
        {
            PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICECTRLTHREAD, Node);
            bool fLast = RTListNodeIsLast(&g_GuestControlThreads, &pNode->Node);

            RTListNodeRemove(&pNode->Node);
            RTMemFree(pNode);

            if (fLast)
                break;

            pNode = pNext;
        }

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    RTCritSectDelete(&g_GuestControlThreadsCritSect);
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceControlTerm(void)
{
    VBoxServiceVerbose(3, "Control: Terminating ...\n");

    VBoxServiceControlDestroyThreads();

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
    "              [--control-interval <ms>] [--control-procs-max-kept <x>]"
    ,
    /* pszOptions. */
    "    --control-interval      Specifies the interval at which to check for\n"
    "                            new control commands. The default is 1000 ms.\n"
    "    --control-procs-max-kept\n"
    "                            Specifies how many started guest processes are\n"
    "                            kept into memory to work with.\n"
    ,
    /* methods */
    VBoxServiceControlPreInit,
    VBoxServiceControlOption,
    VBoxServiceControlInit,
    VBoxServiceControlWorker,
    VBoxServiceControlStop,
    VBoxServiceControlTerm
};

