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
static uint32_t             g_cMsControlInterval = 0;
/** The semaphore we're blocking our main control thread on. */
static RTSEMEVENTMULTI      g_hControlEvent = NIL_RTSEMEVENTMULTI;
/** The guest control service client ID. */
static uint32_t             g_GuestControlSvcClientID = 0;
/** How many started guest processes are kept into memory for supplying
 *  information to the host. Default is 25 processes. If 0 is specified,
 *  the maximum number of processes is unlimited. */
static uint32_t             g_GuestControlProcsMaxKept = 25;
/** List of guest control threads (VBOXSERVICECTRLTHREAD). */
static RTLISTANCHOR         g_GuestControlThreads;
/** Critical section protecting g_GuestControlExecThreads. */
static RTCRITSECT           g_GuestControlThreadsCritSect;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/** @todo Shorten "VBoxServiceControl" to "gstsvcCntl". */
static int VBoxServiceControlStartAllowed(bool *pbAllowed);
static int VBoxServiceControlHandleCmdStartProc(uint32_t u32ClientId, uint32_t uNumParms);
static int VBoxServiceControlHandleCmdSetInput(uint32_t u32ClientId, uint32_t uNumParms, size_t cbMaxBufSize);
static int VBoxServiceControlHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms);



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
                                  &g_cMsControlInterval, 1, UINT32_MAX - 1);
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
    if (!g_cMsControlInterval)
        g_cMsControlInterval = 1000;

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
        VBoxServiceVerbose(3, "Control: Waiting for host msg ...\n");
        uint32_t uMsg;
        uint32_t cParms;
        rc = VbglR3GuestCtrlWaitForHostMsg(g_GuestControlSvcClientID, &uMsg, &cParms);
        if (rc == VERR_TOO_MUCH_DATA)
        {
            VBoxServiceVerbose(4, "Control: Message requires %ld parameters, but only 2 supplied -- retrying request (no error!)...\n", cParms);
            rc = VINF_SUCCESS; /* Try to get "real" message in next block below. */
        }
        else if (RT_FAILURE(rc))
            VBoxServiceVerbose(3, "Control: Getting host message failed with %Rrc\n", rc); /* VERR_GEN_IO_FAILURE seems to be normal if ran into timeout. */
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Control: Msg=%u (%u parms) retrieved\n", uMsg, cParms);
            switch (uMsg)
            {
                case HOST_CANCEL_PENDING_WAITS:
                    VBoxServiceVerbose(3, "Control: Host asked us to quit ...\n");
                    break;

                case HOST_EXEC_CMD:
                    rc = VBoxServiceControlHandleCmdStartProc(g_GuestControlSvcClientID, cParms);
                    break;

                case HOST_EXEC_SET_INPUT:
                    /** @todo Make buffer size configurable via guest properties/argv! */
                    rc = VBoxServiceControlHandleCmdSetInput(g_GuestControlSvcClientID, cParms, _1M /* Buffer size */);
                    break;

                case HOST_EXEC_GET_OUTPUT:
                    rc = VBoxServiceControlHandleCmdGetOutput(g_GuestControlSvcClientID, cParms);
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
 * @param   idClient        The HGCM client session ID.
 * @param   cParms          The number of parameters the host is offering.
 */
static int VBoxServiceControlHandleCmdStartProc(uint32_t idClient, uint32_t cParms)
{
    uint32_t    uContextID;
    char        szCmd[_1K];
    uint32_t    uFlags;
    char        szArgs[_1K];
    uint32_t    cArgs;
    char        szEnv[_64K];
    uint32_t    cbEnv = sizeof(szEnv);
    uint32_t    cEnvVars;
    char        szUser[128];
    char        szPassword[128];
    uint32_t    uTimeLimitMS;

#if 0 /* for valgrind */
    RT_ZERO(szCmd);
    RT_ZERO(szArgs);
    RT_ZERO(szEnv);
    RT_ZERO(szUser);
    RT_ZERO(szPassword);
#endif

    int rc;
    bool fStartAllowed = false; /* Flag indicating whether starting a process is allowed or not. */
    if (cParms == 11)
    {
        rc = VbglR3GuestCtrlExecGetHostCmdExec(idClient,
                                               cParms,
                                               &uContextID,
                                               /* Command */
                                               szCmd,      sizeof(szCmd),
                                               /* Flags */
                                               &uFlags,
                                               /* Arguments */
                                               szArgs,     sizeof(szArgs), &cArgs,
                                               /* Environment */
                                               szEnv, &cbEnv, &cEnvVars,
                                               /* Credentials */
                                               szUser,     sizeof(szUser),
                                               szPassword, sizeof(szPassword),
                                               /* Timelimit */
                                               &uTimeLimitMS);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "Control: Request to start process szCmd=%s, uFlags=0x%x, szArgs=%s, szEnv=%s, szUser=%s, uTimeout=%u\n",
                               szCmd, uFlags, cArgs ? szArgs : "<None>", cEnvVars ? szEnv : "<None>", szUser, uTimeLimitMS);
            rc = VBoxServiceControlStartAllowed(&fStartAllowed);
            if (RT_SUCCESS(rc))
            {
                if (fStartAllowed)
                {
                    rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
                    if (RT_SUCCESS(rc))
                    {
                        /** @todo Put the following params into a struct! */
                        PRTLISTNODE pThreadNode;
                        rc = VBoxServiceControlThreadStart(idClient, uContextID,
                                                           szCmd, uFlags, szArgs, cArgs,
                                                           szEnv, cbEnv, cEnvVars,
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
                else
                    rc = VERR_MAX_PROCS_REACHED; /* Maximum number of processes reached. */
            }
        }
    }
    else
        rc = VERR_INVALID_PARAMETER; /* Incorrect number of parameters. */

    /* In case of an error we need to notify the host to not wait forever for our response. */
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: Starting process failed with rc=%Rrc\n", rc);

        int rc2 = VbglR3GuestCtrlExecReportStatus(idClient, uContextID, 0 /* PID, invalid. */,
                                                  PROC_STS_ERROR, rc,
                                                  NULL /* pvData */, 0 /* cbData */);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("Control: Error sending start process status to host, rc=%Rrc\n", rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }
    }

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
int VBoxServiceControlExecGetOutput(uint32_t uPID, uint32_t uCID,
                                    uint32_t uHandleId, uint32_t uTimeout,
                                    void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    int rc = VINF_SUCCESS;
    VBOXSERVICECTRLREQUESTTYPE reqType;
    switch (uHandleId)
    {
        case OUTPUT_HANDLE_ID_STDERR:
            reqType = VBOXSERVICECTRLREQUEST_STDERR_READ;
            break;

        case OUTPUT_HANDLE_ID_STDOUT:
        case OUTPUT_HANDLE_ID_STDOUT_DEPRECATED:
            reqType = VBOXSERVICECTRLREQUEST_STDOUT_READ;
            break;

        default:
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    PVBOXSERVICECTRLREQUEST pRequest;
    if (RT_SUCCESS(rc))
    {
        rc = VBoxServiceControlThreadRequestAllocEx(&pRequest, reqType,
                                                    pvBuf, cbBuf, uCID);
        if (RT_SUCCESS(rc))
            rc = VBoxServiceControlThreadPerform(uPID, pRequest);

        if (RT_SUCCESS(rc))
        {
            if (pcbRead)
                *pcbRead = pRequest->cbData;
        }

        VBoxServiceControlThreadRequestFree(pRequest);
    }

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
int VBoxServiceControlSetInput(uint32_t uPID, uint32_t uCID,
                               bool fPendingClose,
                               void *pvBuf, uint32_t cbBuf,
                               uint32_t *pcbWritten)
{
    AssertPtrReturn(pvBuf, VERR_INVALID_PARAMETER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PVBOXSERVICECTRLREQUEST pRequest;
    int rc = VBoxServiceControlThreadRequestAllocEx(&pRequest,
                                                      fPendingClose
                                                    ? VBOXSERVICECTRLREQUEST_STDIN_WRITE_EOF
                                                    : VBOXSERVICECTRLREQUEST_STDIN_WRITE,
                                                    pvBuf, cbBuf, uCID);
    if (RT_SUCCESS(rc))
    {
        rc = VBoxServiceControlThreadPerform(uPID, pRequest);
        if (RT_SUCCESS(rc))
        {
            if (pcbWritten)
                *pcbWritten = pRequest->cbData;
        }

        VBoxServiceControlThreadRequestFree(pRequest);
    }

    return rc;
}


/**
 * Handles input for a started process by copying the received data into its
 * stdin pipe.
 *
 * @returns IPRT status code.
 * @param   idClient                    The HGCM client session ID.
 * @param   cParms                      The number of parameters the host is
 *                                      offering.
 * @param   cMaxBufSize                 The maximum buffer size for retrieving the input data.
 */
static int VBoxServiceControlHandleCmdSetInput(uint32_t idClient, uint32_t cParms, size_t cbMaxBufSize)
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
    int rc = VbglR3GuestCtrlExecGetHostCmdInput(idClient, cParms,
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

        rc = VBoxServiceControlSetInput(uPID, uContextID, fPendingClose, pabBuffer,
                                        cbSize, &cbWritten);
        VBoxServiceVerbose(4, "Control: [PID %u]: Written input, CID=%u, rc=%Rrc, uFlags=0x%x, fPendingClose=%d, cbSize=%u, cbWritten=%u\n",
                           uPID, uContextID, rc, uFlags, fPendingClose, cbSize, cbWritten);
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
    rc = VbglR3GuestCtrlExecReportStatusIn(idClient, uContextID, uPID,
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
 * @param   idClient        The HGCM client session ID.
 * @param   cParms          The number of parameters the host is offering.
 */
static int VBoxServiceControlHandleCmdGetOutput(uint32_t idClient, uint32_t cParms)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlExecGetHostCmdOutput(idClient, cParms,
                                                 &uContextID, &uPID, &uHandleID, &uFlags);
    if (RT_SUCCESS(rc))
    {
        uint8_t *pBuf = (uint8_t*)RTMemAlloc(_64K);
        if (pBuf)
        {
            uint32_t cbRead = 0;
            rc = VBoxServiceControlExecGetOutput(uPID, uContextID, uHandleID, RT_INDEFINITE_WAIT /* Timeout */,
                                                 pBuf, _64K /* cbSize */, &cbRead);
            VBoxServiceVerbose(3, "Control: Got output returned with rc=%Rrc (PID=%u, CID=%u, cbRead=%u, uHandle=%u, uFlags=%u)\n",
                               rc, uPID, uContextID, cbRead, uHandleID, uFlags);

            /** Note: Don't convert/touch/modify/whatever the output data here! This might be binary
             *        data which the host needs to work with -- so just pass through all data unfiltered! */

            /* Note: Since the context ID is unique the request *has* to be completed here,
             *       regardless whether we got data or not! Otherwise the progress object
             *       on the host never will get completed! */
            int rc2 = VbglR3GuestCtrlExecSendOut(idClient, uContextID, uPID, uHandleID, uFlags,
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


/**
 * Destroys all guest process threads which are still active.
 */
static void VBoxServiceControlDestroyThreads(void)
{
    VBoxServiceVerbose(2, "Control: Destroying threads ...\n");

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

#ifdef DEBUG
    PVBOXSERVICECTRLTHREAD pThreadCur;
    uint32_t cThreads = 0;
    RTListForEach(&g_GuestControlThreads, pThreadCur, VBOXSERVICECTRLTHREAD, Node)
        cThreads++;
    VBoxServiceVerbose(4, "Control: Guest process threads left=%u\n", cThreads);
#endif
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
                  VBoxServiceControlThreadActive(pThread)
                ? uProcsRunning++
                : uProcsStopped++;
            }

            VBoxServiceVerbose(3, "Control: Maximum served guest processes set to %u, running=%u, stopped=%u\n",
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
 * Finds a (formerly) started process given by its PID and locks it. Must be unlocked
 * by the caller with VBoxServiceControlThreadUnlock().
 *
 * @return  PVBOXSERVICECTRLTHREAD      Process structure if found, otherwise NULL.
 * @param   uPID                        PID to search for.
 */
PVBOXSERVICECTRLTHREAD VBoxServiceControlGetThreadLocked(uint32_t uPID)
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
                rc = RTCritSectEnter(&pThreadCur->CritSect);
                if (RT_SUCCESS(rc))
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
 * Unlocks a previously locked guest process thread.
 *
 * @param   pThread                 Thread to unlock.
 */
void VBoxServiceControlThreadUnlock(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtr(pThread);

    int rc = RTCritSectLeave(&pThread->CritSect);
    AssertRC(rc);
}


/**
 * Assigns a valid PID to a guest control thread and also checks if there already was
 * another (stale) guest process which was using that PID before and destroys it.
 *
 * @return  IPRT status code.
 * @param   pThread        Thread to assign PID to.
 * @param   uPID           PID to assign to the specified guest control execution thread.
 */
int VBoxServiceControlAssignPID(PVBOXSERVICECTRLTHREAD pThread, uint32_t uPID)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertReturn(uPID, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        /* Search old threads using the desired PID and shut them down completely -- it's
         * not used anymore. */
        PVBOXSERVICECTRLTHREAD pThreadCur;
        bool fTryAgain = false;
        do
        {
            RTListForEach(&g_GuestControlThreads, pThreadCur, VBOXSERVICECTRLTHREAD, Node)
            {
                if (pThreadCur->uPID == uPID)
                {
                    Assert(pThreadCur != pThread); /* can't happen */
                    uint32_t uTriedPID = uPID;
                    uPID += 391939;
                    VBoxServiceVerbose(2, "ControlThread: PID %u was used before, trying again with %u ...\n",
                                       uTriedPID, uPID);
                    fTryAgain = true;
                    break;
                }
            }
        } while (fTryAgain);

        /* Assign PID to current thread. */
        pThread->uPID = uPID;

        rc = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        AssertRC(rc);
    }

    return rc;
}


/**
 * Removes the specified guest process thread from the global thread
 * list.
 *
 * @return  IPRT status code.
 * @param   pThread             Thread to remove.
 */
void VBoxServiceControlRemoveThread(const PVBOXSERVICECTRLTHREAD pThread)
{
    if (!pThread)
        return;

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(4, "Control: Removing thread (PID: %u) from thread list\n",
                           pThread->uPID);
        RTListNodeRemove(&pThread->Node);

#ifdef DEBUG
        PVBOXSERVICECTRLTHREAD pThreadCur;
        uint32_t cThreads = 0;
        RTListForEach(&g_GuestControlThreads, pThreadCur, VBOXSERVICECTRLTHREAD, Node)
            cThreads++;
        VBoxServiceVerbose(4, "Control: Guest process threads left=%u\n", cThreads);
#endif
        rc = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        AssertRC(rc);
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

