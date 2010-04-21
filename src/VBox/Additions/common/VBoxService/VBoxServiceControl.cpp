
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

using namespace guestControl;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The control interval (millseconds). */
uint32_t g_ControlInterval = 0;
/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI      g_hControlEvent = NIL_RTSEMEVENTMULTI;
/** The guest property service client ID. */
static uint32_t             g_GuestControlSvcClientID = 0;
/** List of spawned processes */
RTLISTNODE                  g_GuestControlExecThreads;


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

    /* Init thread list. */
    RTListInit(&g_GuestControlExecThreads);
    return rc;
}


static int VBoxServiceControlHandleCmdStartProcess(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    char szCmd[_1K];
    uint32_t uFlags;
    char szArgs[_1K];
    uint32_t uNumArgs;
    char szEnv[_64K];
    uint32_t cbEnv = sizeof(szEnv);
    uint32_t uNumEnvVars;
    char szStdIn[_1K];
    char szStdOut[_1K];
    char szStdErr[_1K];
    char szUser[128];
    char szPassword[128];
    uint32_t uTimeLimitMS;

    if (uNumParms != 14)
        return VERR_INVALID_PARAMETER;

    int rc = VbglR3GuestCtrlExecGetHostCmd(u32ClientId,
                                           uNumParms,
                                           &uContextID,
                                           /* Command */
                                           szCmd,      sizeof(szCmd),
                                           /* Flags */
                                           &uFlags,
                                           /* Arguments */
                                           szArgs,     sizeof(szArgs), &uNumArgs,
                                           /* Environment */
                                           szEnv, &cbEnv, &uNumEnvVars,
                                           /* Pipes */
                                           szStdIn,    sizeof(szStdIn),
                                           szStdOut,   sizeof(szStdOut),
                                           szStdErr,   sizeof(szStdErr),
                                           /* Credentials */
                                           szUser,     sizeof(szUser),
                                           szPassword, sizeof(szPassword),
                                           /* Timelimit */
                                           &uTimeLimitMS);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: Failed to retrieve exec start command! Error: %Rrc\n", rc);
    }
    else
    {
        rc = VBoxServiceControlExecProcess(uContextID, szCmd, uFlags, szArgs, uNumArgs,
                                           szEnv, cbEnv, uNumEnvVars,
                                           szStdIn, szStdOut, szStdErr,
                                           szUser, szPassword, uTimeLimitMS);
    }
    return rc;
}


static int VBoxServiceControlHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlExecGetHostCmdOutput(u32ClientId, uNumParms,
                                                 &uContextID, &uPID, &uHandleID, &uFlags);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: Failed to retrieve exec output command! Error: %Rrc\n", rc);
    }
    else
    {
        /* Let's have a look if we have a running process with PID = uPID ... */
        PVBOXSERVICECTRLTHREAD pNode;
        bool bFound = false;
        RTListForEach(&g_GuestControlExecThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
        {
            if (   pNode->fStarted 
                && pNode->enmType == VBoxServiceCtrlThreadDataExec)
            {
                PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
                if (pData && pData->uPID == uPID)
                {
                    bFound = true;
                    break;
                }
            }
        }

        if (bFound)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);

            uint32_t cbRead = _4K; /* Try reading 4k. */
            uint8_t *pBuf = (uint8_t*)RTMemAlloc(cbRead);
            if (pBuf)
            {
                rc = VBoxServiceControlExecReadPipeBufferContent(&pData->stdOut, pBuf, cbRead, &cbRead);
                if (RT_SUCCESS(rc))
                {    
                    AssertPtr(pBuf);
                    /* cbRead now contains actual size. */
                    rc = VbglR3GuestCtrlExecSendOut(u32ClientId, uContextID, uPID, 0 /* handle ID */, 0 /* flags */,
                                                    pBuf, cbRead);
                }
                RTMemFree(pBuf);
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_NOT_FOUND; /* PID not found! */
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
        VBoxServiceVerbose(4, "Control: Waiting for host msg ...\n");
        rc = VbglR3GuestCtrlGetHostMsg(g_GuestControlSvcClientID, &uMsg, &uNumParms, 1000 /* 1s timeout */);
        if (rc == VERR_TOO_MUCH_DATA)
        {
            VBoxServiceVerbose(3, "Control: Message requires %ld parameters, but only 2 supplied -- retrying request ...\n", uNumParms);
            rc = VINF_SUCCESS;
        }
        else if (rc == VERR_TIMEOUT)
        {
            VBoxServiceVerbose(3, "Control: Wait timed out, waiting for next round ...\n");
            RTThreadSleep(100);
        }
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Control: Msg=%u (%u parms) retrieved\n", uMsg, uNumParms);
            switch(uMsg)
            {
                case GETHOSTMSG_EXEC_START_PROCESS:
                    rc = VBoxServiceControlHandleCmdStartProcess(g_GuestControlSvcClientID, uNumParms);
                    break;

                case GETHOSTMSG_EXEC_GET_OUTPUT:
                    rc = VBoxServiceControlHandleCmdGetOutput(g_GuestControlSvcClientID, uNumParms);
                    break;

                default:
                    VBoxServiceVerbose(3, "Control: Unsupported message from host! Msg=%u\n", uMsg);
                    /* Don't terminate here; just wait for the next message. */
                    break;
            }

            if (RT_FAILURE(rc))
                VBoxServiceVerbose(3, "Control: Message was processed with rc=%Rrc\n", rc);
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
            VBoxServiceError("Control: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
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
    /* Signal all threads that we want to shutdown. */
    PVBOXSERVICECTRLTHREAD pNode;
    RTListForEach(&g_GuestControlExecThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
        ASMAtomicXchgBool(&pNode->fShutdown, true);

    /* Wait for threads to shutdown. */
    RTListForEach(&g_GuestControlExecThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
    {
        if (pNode->Thread != NIL_RTTHREAD)
        {
            int rc2 = RTThreadWait(pNode->Thread, 30 * 1000 /* Wait 30 seconds max. */, NULL);
            if (RT_FAILURE(rc2))
                VBoxServiceError("Control: Thread failed to stop; rc2=%Rrc\n", rc2);
        }

        /* Destroy thread specific data. */
        switch (pNode->enmType)
        {
            case VBoxServiceCtrlThreadDataExec:
                VBoxServiceControlExecDestroyThreadData((PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData);
                break;

            default:
                break;
        }
    }

    /* Finally destroy thread list. */
    pNode = RTListNodeGetFirst(&g_GuestControlExecThreads, VBOXSERVICECTRLTHREAD, Node);
    while (pNode)
    {
        PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICECTRLTHREAD, Node);

        RTListNodeRemove(&pNode->Node);
        RTMemFree(pNode);

        if (pNext && RTListNodeIsLast(&g_GuestControlExecThreads, &pNext->Node))
              break;

        pNode = pNext;
    }

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

