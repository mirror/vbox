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

using namespace guestControl;

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The control interval (milliseconds). */
uint32_t g_ControlInterval = 0;
/** The semaphore we're blocking our main control thread on. */
RTSEMEVENTMULTI             g_hControlEvent = NIL_RTSEMEVENTMULTI;
/** The guest control service client ID. */
static uint32_t             g_GuestControlSvcClientID = 0;
/** How many started guest processes are kept into memory for supplying
 *  information to the host. Default is 25 processes. If 0 is specified,
 *  the maximum number of processes is unlimited. */
uint32_t                    g_GuestControlProcsMaxKept = 25;
/** List of guest control threads. */
RTLISTNODE                  g_GuestControlThreads;
/** Critical section protecting g_GuestControlExecThreads. */
RTCRITSECT                  g_GuestControlThreadsCritSect;

static int VBoxServiceControlStartAllowed(bool *pbAllowed);

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
                    rc = VBoxServiceControlHandleCmdStartProc(g_GuestControlSvcClientID, uNumParms);
                    break;

                case HOST_EXEC_SET_INPUT:
                    /** @todo Make buffer size configurable via guest properties/argv! */
                    rc = VBoxServiceControlHandleCmdSetInput(g_GuestControlSvcClientID, uNumParms, _1M /* Buffer size */);
                    break;

                case HOST_EXEC_GET_OUTPUT:
                    rc = VBoxServiceControlHandleCmdGetOutput(g_GuestControlSvcClientID, uNumParms);
                    break;

                default:
                    VBoxServiceVerbose(3, "Control: Unsupported message from host! Msg=%u\n", uMsg);
                    /* Don't terminate here; just wait for the next message. */
                    break;
            }
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

    return rc;
}


/**
 * Handles starting processes on the guest.
 *
 * @returns IPRT status code.
 * @param   u32ClientId                 The HGCM client session ID.
 * @param   uNumParms                   The number of parameters the host is offering.
 */
int VBoxServiceControlHandleCmdStartProc(uint32_t uClientID, uint32_t uNumParms)
{
    uint32_t uContextID;
    char szCmd[_1K];
    uint32_t uFlags;
    char szArgs[_1K];
    uint32_t uNumArgs;
    char szEnv[_64K];
    uint32_t cbEnv = sizeof(szEnv);
    uint32_t uNumEnvVars;
    char szUser[128];
    char szPassword[128];
    uint32_t uTimeLimitMS;

#if 0 /* for valgrind */
    RT_ZERO(szCmd);
    RT_ZERO(szArgs);
    RT_ZERO(szEnv);
    RT_ZERO(szUser);
    RT_ZERO(szPassword);
#endif

    if (uNumParms != 11)
        return VERR_INVALID_PARAMETER;

    int rc = VbglR3GuestCtrlExecGetHostCmd(uClientID,
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
                                           /* Credentials */
                                           szUser,     sizeof(szUser),
                                           szPassword, sizeof(szPassword),
                                           /* Timelimit */
                                           &uTimeLimitMS);
    if (RT_SUCCESS(rc))
    {
#ifdef DEBUG
        VBoxServiceVerbose(3, "Control: Start process szCmd=%s, uFlags=%u, szArgs=%s, szEnv=%s, szUser=%s, szPW=%s, uTimeout=%u\n",
                           szCmd, uFlags, uNumArgs ? szArgs : "<None>", uNumEnvVars ? szEnv : "<None>", szUser, szPassword, uTimeLimitMS);
#endif
        bool fAllowed = false;
        rc = VBoxServiceControlStartAllowed(&fAllowed);
        if (RT_FAILURE(rc))
            VBoxServiceError("Control: Error determining whether process can be started or not, rc=%Rrc\n", rc);

        if (   RT_SUCCESS(rc)
            && fAllowed)
        {
            rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
            if (RT_SUCCESS(rc))
            {
                /** @todo Put the following params into a struct! */
                RTLISTNODE *pThreadNode;
                rc = VBoxServiceControlThreadStart(uClientID, uContextID,
                                                   szCmd, uFlags, szArgs, uNumArgs,
                                                   szEnv, cbEnv, uNumEnvVars,
                                                   szUser, szPassword, uTimeLimitMS,
                                                   &pThreadNode);
                if (RT_SUCCESS(rc))
                {
                    /* Insert thread node into thread list. */
                    /*rc =*/ RTListAppend(&g_GuestControlThreads, pThreadNode);
                }

                int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
                if (RT_SUCCESS(rc))
                    rc = rc2;
            }
        }
        else /* Process start is not allowed due to policy settings. */
        {
            /* Tell the host. */
            rc = VbglR3GuestCtrlExecReportStatus(uClientID, uContextID, 0 /* PID, invalid. */,
                                                 PROC_STS_ERROR, VERR_MAX_PROCS_REACHED,
                                                 NULL /* pvData */, 0 /* cbData */);
        }
    }
    else
        VBoxServiceError("Control: Failed to retrieve exec start command! Error: %Rrc\n", rc);
    return rc;
}


/**
 * Gets output from stdout/stderr of a specified guest process.
 *
 * @return  IPRT status code.
 * @param   uPID                    PID of process to retrieve the output from.
 * @param   uHandleId               Stream ID (stdout = 0, stderr = 2) to get the output from.
 * @param   uTimeout                Timeout (in ms) to wait for output becoming available.
 * @param   pvBuf                   Pointer to a pre-allocated buffer to store the output.
 * @param   cbBuf                   Size (in bytes) of the pre-allocated buffer.
 * @param   pcbRead                 Pointer to number of bytes read.  Optional.
 */
int VBoxServiceControlExecGetOutput(uint32_t uPID, uint32_t uHandleId, uint32_t uTimeout,
                                    void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    int rc = VINF_SUCCESS;

    VBOXSERVICECTRLREQUEST ctrlRequest;
    ctrlRequest.cbData = cbBuf;
    ctrlRequest.pvData = (uint8_t*)pvBuf;

    switch (uHandleId)
    {
        case OUTPUT_HANDLE_ID_STDERR:
            ctrlRequest.enmType = VBOXSERVICECTRLREQUEST_STDERR_READ;
            break;

        case OUTPUT_HANDLE_ID_STDOUT:
        case OUTPUT_HANDLE_ID_STDOUT_DEPRECATED:
            ctrlRequest.enmType = VBOXSERVICECTRLREQUEST_STDOUT_READ;
            break;

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    if (RT_SUCCESS(rc))
        rc = VBoxServiceControlThreadPerform(uPID, &ctrlRequest);

    if (RT_SUCCESS(rc))
    {
        if (pcbRead)
            *pcbRead = ctrlRequest.cbData;
    }
    else /* Something went wrong, nothing read. */
        *pcbRead = 0;

    return rc;
}


/**
 * Injects input to a specified running process.
 *
 * @return  IPRT status code.
 * @param   uPID                    PID of process to set the input for.
 * @param   fPendingClose           Flag indicating whether this is the last input block sent to the process.
 * @param   pvBuf                   Pointer to a buffer containing the actual input data.
 * @param   cbBuf                   Size (in bytes) of the input buffer data.
 * @param   pcbWritten              Pointer to number of bytes written to the process.  Optional.
 */
int VBoxServiceControlSetInput(uint32_t uPID, bool fPendingClose,
                               void *pvBuf, uint32_t cbBuf,
                               uint32_t *pcbWritten)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_PARAMETER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc = VINF_SUCCESS;

    VBOXSERVICECTRLREQUEST ctrlRequest;
    ctrlRequest.cbData  = cbBuf;
    ctrlRequest.pvData  = pvBuf;
    ctrlRequest.enmType = fPendingClose
                        ? VBOXSERVICECTRLREQUEST_STDIN_WRITE_EOF : VBOXSERVICECTRLREQUEST_STDIN_WRITE;
    if (RT_SUCCESS(rc))
        rc = VBoxServiceControlThreadPerform(uPID, &ctrlRequest);

    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = ctrlRequest.cbData;
    }

    return rc;
}


/**
 * Handles input for a started process by copying the received data into its
 * stdin pipe.
 *
 * @returns IPRT status code.
 * @param   u32ClientId                 The HGCM client session ID.
 * @param   uNumParms                   The number of parameters the host is offering.
 * @param   cMaxBufSize                 The maximum buffer size for retrieving the input data.
 */
int VBoxServiceControlHandleCmdSetInput(uint32_t u32ClientId, uint32_t uNumParms, size_t cbMaxBufSize)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uFlags;
    uint32_t cbSize;

    AssertReturn(RT_IS_POWER_OF_TWO(cbMaxBufSize), VERR_INVALID_PARAMETER);
    uint8_t *pabBuffer = (uint8_t*)RTMemAlloc(cbMaxBufSize);
    AssertPtrReturn(pabBuffer, VERR_NO_MEMORY);

    uint32_t uStatus = INPUT_STS_UNDEFINED; /* Status sent back to the host. */
    uint32_t cbWritten = 0; /* Number of bytes written to the guest. */

    /*
     * Ask the host for the input data.
     */
    int rc = VbglR3GuestCtrlExecGetHostCmdInput(u32ClientId, uNumParms,
                                                &uContextID, &uPID, &uFlags,
                                                pabBuffer, cbMaxBufSize, &cbSize);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: [PID %u]: Failed to retrieve exec input command! Error: %Rrc\n",
                         uPID, rc);
    }
    else if (cbSize >  cbMaxBufSize)
    {
        VBoxServiceError("Control: [PID %u]: Too much input received! cbSize=%u, cbMaxBufSize=%u\n",
                         uPID, cbSize, cbMaxBufSize);
        rc = VERR_INVALID_PARAMETER;
    }
    else
    {
        /*
         * Is this the last input block we need to deliver? Then let the pipe know ...
         */
        bool fPendingClose = false;
        if (uFlags & INPUT_FLAG_EOF)
        {
            fPendingClose = true;
            VBoxServiceVerbose(4, "Control: [PID %u]: Got last input block of size %u ...\n",
                               uPID, cbSize);
        }

        rc = VBoxServiceControlSetInput(uPID, fPendingClose, pabBuffer,
                                        cbSize, &cbWritten);
        VBoxServiceVerbose(4, "Control: [PID %u]: Written input, rc=%Rrc, uFlags=0x%x, fPendingClose=%d, cbSize=%u, cbWritten=%u\n",
                           uPID, rc, uFlags, fPendingClose, cbSize, cbWritten);
        if (RT_SUCCESS(rc))
        {
            if (cbWritten || !cbSize) /* Did we write something or was there anything to write at all? */
            {
                uStatus = INPUT_STS_WRITTEN;
                uFlags = 0;
            }
        }
        else
        {
            if (rc == VERR_BAD_PIPE)
                uStatus = INPUT_STS_TERMINATED;
            else if (rc == VERR_BUFFER_OVERFLOW)
                uStatus = INPUT_STS_OVERFLOW;
        }
    }
    RTMemFree(pabBuffer);

    /*
     * If there was an error and we did not set the host status
     * yet, then do it now.
     */
    if (   RT_FAILURE(rc)
        && uStatus == INPUT_STS_UNDEFINED)
    {
        uStatus = INPUT_STS_ERROR;
        uFlags = rc;
    }
    Assert(uStatus > INPUT_STS_UNDEFINED);

    VBoxServiceVerbose(3, "Control: [PID %u]: Input processed, CID=%u, uStatus=%u, uFlags=0x%x, cbWritten=%u\n",
                       uPID, uContextID, uStatus, uFlags, cbWritten);

    /* Note: Since the context ID is unique the request *has* to be completed here,
     *       regardless whether we got data or not! Otherwise the progress object
     *       on the host never will get completed! */
    rc = VbglR3GuestCtrlExecReportStatusIn(u32ClientId, uContextID, uPID,
                                           uStatus, uFlags, (uint32_t)cbWritten);

    if (RT_FAILURE(rc))
        VBoxServiceError("Control: [PID %u]: Failed to report input status! Error: %Rrc\n",
                         uPID, rc);
    return rc;
}


/**
 * Handles the guest control output command.
 *
 * @return  IPRT status code.
 * @param   u32ClientId     idClient    The HGCM client session ID.
 * @param   uNumParms       cParms      The number of parameters the host is
 *                                      offering.
 */
int VBoxServiceControlHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlExecGetHostCmdOutput(u32ClientId, uNumParms,
                                                 &uContextID, &uPID, &uHandleID, &uFlags);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbRead = 0;
        uint8_t *pBuf = (uint8_t*)RTMemAlloc(_64K);
        if (pBuf)
        {
            rc = VBoxServiceControlExecGetOutput(uPID, uHandleID, RT_INDEFINITE_WAIT /* Timeout */,
                                                 pBuf, _64K /* cbSize */, &cbRead);
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(3, "Control: [PID %u]: Got output, CID=%u, cbRead=%u, uHandle=%u, uFlags=%u\n",
                                   uPID, uContextID, cbRead, uHandleID, uFlags);
            else
                VBoxServiceError("Control: [PID %u]: Failed to retrieve output, CID=%u, uHandle=%u, rc=%Rrc\n",
                                 uPID, uContextID, uHandleID, rc);
            /* Note: Since the context ID is unique the request *has* to be completed here,
             *       regardless whether we got data or not! Otherwise the progress object
             *       on the host never will get completed! */
            /* cbRead now contains actual size. */
            int rc2 = VbglR3GuestCtrlExecSendOut(u32ClientId, uContextID, uPID, uHandleID, uFlags,
                                                 pBuf, cbRead);
            if (RT_SUCCESS(rc))
                rc = rc2;
            RTMemFree(pBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
        VBoxServiceError("Control: [PID %u]: Error handling output command! Error: %Rrc\n",
                         uPID, rc);
    return rc;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceControlStop(void)
{
    VBoxServiceVerbose(3, "Control: Stopping ...\n");

    /** @todo Later, figure what to do if we're in RTProcWait(). It's a very
     *        annoying call since doesn't support timeouts in the posix world. */
    if (g_hControlEvent != NIL_RTSEMEVENTMULTI)
        RTSemEventMultiSignal(g_hControlEvent);

    /*
     * Ask the host service to cancel all pending requests so that we can
     * shutdown properly here.
     */
    if (g_GuestControlSvcClientID)
    {
        VBoxServiceVerbose(3, "Control: Cancelling pending waits (client ID=%u) ...\n",
                           g_GuestControlSvcClientID);

        int rc = VbglR3GuestCtrlCancelPendingWaits(g_GuestControlSvcClientID);
        if (RT_FAILURE(rc))
            VBoxServiceError("Control: Cancelling pending waits failed; rc=%Rrc\n", rc);
    }
}


static void VBoxServiceControlDestroyThreads(void)
{
    VBoxServiceVerbose(3, "Control: Destroying threads ...\n");

    /* Signal all threads that we want to shutdown. */
    PVBOXSERVICECTRLTHREAD pThread;
    RTListForEach(&g_GuestControlThreads, pThread, VBOXSERVICECTRLTHREAD, Node)
        VBoxServiceControlThreadSignalShutdown(pThread);

    /* Wait for threads to shutdown and destroy thread list. */
    pThread = RTListGetFirst(&g_GuestControlThreads, VBOXSERVICECTRLTHREAD, Node);
    while (pThread)
    {
        PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pThread->Node, VBOXSERVICECTRLTHREAD, Node);
        bool fLast = RTListNodeIsLast(&g_GuestControlThreads, &pThread->Node);

        int rc2 = VBoxServiceControlThreadWaitForShutdown(pThread,
                                                          30 * 1000 /* Wait 30 seconds max. */);
        if (RT_FAILURE(rc2))
            VBoxServiceError("Control: Guest process thread failed to stop; rc=%Rrc\n", rc2);

        if (fLast)
            break;

        pThread = pNext;
    }

    AssertMsg(RTListIsEmpty(&g_GuestControlThreads),
              ("Guest process thread list still contains children when it should not\n"));

    /* Destroy critical section. */
    RTCritSectDelete(&g_GuestControlThreadsCritSect);
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceControlTerm(void)
{
    VBoxServiceVerbose(3, "Control: Terminating ...\n");

    VBoxServiceControlDestroyThreads();

    VBoxServiceVerbose(3, "Control: Disconnecting client ID=%u ...\n",
                       g_GuestControlSvcClientID);
    VbglR3GuestCtrlDisconnect(g_GuestControlSvcClientID);
    g_GuestControlSvcClientID = 0;

    if (g_hControlEvent != NIL_RTSEMEVENTMULTI)
    {
        RTSemEventMultiDestroy(g_hControlEvent);
        g_hControlEvent = NIL_RTSEMEVENTMULTI;
    }
}


/**
 * Determines whether starting a new guest process according to the
 * maximum number of concurrent guest processes defined is allowed or not.
 *
 * @return  IPRT status code.
 * @param   pbAllowed           True if starting (another) guest process
 *                              is allowed, false if not.
 */
static int VBoxServiceControlStartAllowed(bool *pbAllowed)
{
    AssertPtrReturn(pbAllowed, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        /*
         * Check if we're respecting our memory policy by checking
         * how many guest processes are started and served already.
         */
        bool fLimitReached = false;
        if (g_GuestControlProcsMaxKept) /* If we allow unlimited processes (=0), take a shortcut. */
        {
            uint32_t uProcsRunning = 0;
            uint32_t uProcsStopped = 0;
            PVBOXSERVICECTRLTHREAD pThread;
            RTListForEach(&g_GuestControlThreads, pThread, VBOXSERVICECTRLTHREAD, Node)
            {
    // THREAD LOCKING!!
                Assert(pThread->fStarted != pThread->fStopped);
                if (pThread->fStarted)
                    uProcsRunning++;
                else if (pThread->fStopped)
                    uProcsStopped++;
                else
                    AssertMsgFailed(("Control: Guest process neither started nor stopped!?\n"));
            }

            VBoxServiceVerbose(2, "Control: Maximum served guest processes set to %u, running=%u, stopped=%u\n",
                               g_GuestControlProcsMaxKept, uProcsRunning, uProcsStopped);

            int32_t iProcsLeft = (g_GuestControlProcsMaxKept - uProcsRunning - 1);
            if (iProcsLeft < 0)
            {
                VBoxServiceVerbose(3, "Control: Maximum running guest processes reached (%u)\n",
                                   g_GuestControlProcsMaxKept);
                fLimitReached = true;
            }
        }

        *pbAllowed = !fLimitReached;

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


/**
 * Finds a (formerly) started process given by its PID.
 *
 * @return  PVBOXSERVICECTRLTHREAD      Process structure if found, otherwise NULL.
 * @param   uPID                        PID to search for.
 */
PVBOXSERVICECTRLTHREAD VBoxServiceControlGetThreadByPID(uint32_t uPID)
{
    PVBOXSERVICECTRLTHREAD pThread = NULL;
    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pThreadCur;
        RTListForEach(&g_GuestControlThreads, pThreadCur, VBOXSERVICECTRLTHREAD, Node)
        {
            if (pThreadCur->uPID == uPID)
            {
                pThread = pThreadCur;
                break;
            }
        }

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return pThread;
}


/**
 * Removes the specified guest process thread from the global thread
 * list.
 *
 * @return  IPRT status code.
 * @param   pThread             Thread to remove.
 */
void VBoxServiceControlRemoveThread(PVBOXSERVICECTRLTHREAD pThread)
{
    if (!pThread)
        return;

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(4, "Control: Removing thread (PID: %u) from thread list\n",
                           pThread->uPID);
        RTListNodeRemove(&pThread->Node);

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        AssertRC(rc2);
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
    "              [--control-interval <ms>] [--control-procs-max-kept <x>]\n"
    "              [--control-procs-mem-std[in|out|err] <KB>]"
    ,
    /* pszOptions. */
    "    --control-interval      Specifies the interval at which to check for\n"
    "                            new control commands. The default is 1000 ms.\n"
    "    --control-procs-max-kept\n"
    "                            Specifies how many started guest processes are\n"
    "                            kept into memory to work with. Default is 25.\n"
    ,
    /* methods */
    VBoxServiceControlPreInit,
    VBoxServiceControlOption,
    VBoxServiceControlInit,
    VBoxServiceControlWorker,
    VBoxServiceControlStop,
    VBoxServiceControlTerm
};

