/* $Id$ */
/** @file
 * VBoxServiceControlExecThread - Thread for every started guest process.
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
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>

#include "VBoxServiceInternal.h"

using namespace guestControl;

/* Internal functions. */
static int vboxServiceControlThreadFree(PVBOXSERVICECTRLTHREAD pThread);

/**
 * Initialies the passed in thread data structure with the parameters given.
 *
 * @return  IPRT status code.
 * @param   pThread                     The thread's handle to allocate the data for.
 * @param   u32ContextID                The context ID bound to this request / command.
 * @param   pszCmd                      Full qualified path of process to start (without arguments).
 * @param   uFlags                      Process execution flags.
 * @param   pszArgs                     String of arguments to pass to the process to start.
 * @param   uNumArgs                    Number of arguments specified in pszArgs.
 * @param   pszEnv                      String of environment variables ("FOO=BAR") to pass to the process
 *                                      to start.
 * @param   cbEnv                       Size (in bytes) of environment variables.
 * @param   uNumEnvVars                 Number of environment variables specified in pszEnv.
 * @param   pszUser                     User name (account) to start the process under.
 * @param   pszPassword                 Password of specified user name (account).
 * @param   uTimeLimitMS                Time limit (in ms) of the process' life time.
 */
static int gstsvcCntlExecThreadInit(PVBOXSERVICECTRLTHREAD pThread,
                                    uint32_t u32ContextID,
                                    const char *pszCmd, uint32_t uFlags,
                                    const char *pszArgs, uint32_t uNumArgs,
                                    const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                    const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS)
{
    AssertPtr(pThread);

    /* General stuff. */
    pThread->Node.pPrev = NULL;
    pThread->Node.pNext = NULL;

    pThread->fShutdown = false;
    pThread->fStarted  = false;
    pThread->fStopped  = false;

    pThread->uContextID = u32ContextID;
    /* ClientID will be assigned when thread is started; every guest
     * process has its own client ID to detect crashes on a per-guest-process
     * level. */

    int rc = RTCritSectInit(&pThread->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    rc = RTSemEventMultiCreate(&pThread->RequestEvent);
    AssertRCReturn(rc, rc);

    pThread->uPID = 0; /* Don't have a PID yet. */
    pThread->pszCmd = RTStrDup(pszCmd);
    pThread->uFlags = uFlags;
    pThread->uTimeLimitMS = (   uTimeLimitMS == UINT32_MAX
                             || uTimeLimitMS == 0)
                          ? RT_INDEFINITE_WAIT : uTimeLimitMS;

    /* Prepare argument list. */
    pThread->uNumArgs = 0; /* Initialize in case of RTGetOptArgvFromString() is failing ... */
    rc = RTGetOptArgvFromString(&pThread->papszArgs, (int*)&pThread->uNumArgs,
                                (uNumArgs > 0) ? pszArgs : "", NULL);
    /* Did we get the same result? */
    Assert(uNumArgs == pThread->uNumArgs);

    if (RT_SUCCESS(rc))
    {
        /* Prepare environment list. */
        pThread->uNumEnvVars = 0;
        if (uNumEnvVars)
        {
            pThread->papszEnv = (char **)RTMemAlloc(uNumEnvVars * sizeof(char*));
            AssertPtr(pThread->papszEnv);
            pThread->uNumEnvVars = uNumEnvVars;

            const char *pszCur = pszEnv;
            uint32_t i = 0;
            uint32_t cbLen = 0;
            while (cbLen < cbEnv)
            {
                /* sanity check */
                if (i >= uNumEnvVars)
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                int cbStr = RTStrAPrintf(&pThread->papszEnv[i++], "%s", pszCur);
                if (cbStr < 0)
                {
                    rc = VERR_NO_STR_MEMORY;
                    break;
                }
                pszCur += cbStr + 1; /* Skip terminating '\0' */
                cbLen  += cbStr + 1; /* Skip terminating '\0' */
            }
        }

        /* User management. */
        pThread->pszUser     = RTStrDup(pszUser);
        AssertPtr(pThread->pszUser);
        pThread->pszPassword = RTStrDup(pszPassword);
        AssertPtr(pThread->pszPassword);
    }

    if (RT_FAILURE(rc)) /* Clean up on failure. */
        vboxServiceControlThreadFree(pThread);
    return rc;
}


/**
 * Frees a guest thread.
 *
 * @return  IPRT status code.
 * @param   pThread                 Thread to shut down.
 */
static int vboxServiceControlThreadFree(PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    VBoxServiceVerbose(1, "ControlThread: [PID %u]: Shutting down ...\n",
                       pThread->uPID);

    /* Signal the request event to unblock potential waiters. */
    int rc = RTSemEventMultiSignal(pThread->RequestEvent);
    if (RT_FAILURE(rc))
        VBoxServiceError("ControlThread: [PID %u]: Signalling request event failed, rc=%Rrc\n",
                         pThread->uPID, rc);

    rc = RTCritSectEnter(&pThread->CritSect);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Freeing thread data ...\n",
                           pThread->uPID);

        RTStrFree(pThread->pszCmd);
        if (pThread->uNumEnvVars)
        {
            for (uint32_t i = 0; i < pThread->uNumEnvVars; i++)
                RTStrFree(pThread->papszEnv[i]);
            RTMemFree(pThread->papszEnv);
        }
        RTGetOptArgvFree(pThread->papszArgs);
        RTStrFree(pThread->pszUser);
        RTStrFree(pThread->pszPassword);

        rc = RTSemEventMultiDestroy(pThread->RequestEvent);
        AssertRC(rc);

        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Cleaning up ...\n",
                           pThread->uPID);

        /* Set stopped status. */
        ASMAtomicXchgBool(&pThread->fStopped, true);

        rc = RTCritSectLeave(&pThread->CritSect);
        AssertRC(rc);
    }

    /*
     * Destroy other thread data.
     */
    if (RTCritSectIsInitialized(&pThread->CritSect))
        RTCritSectDelete(&pThread->CritSect);

    /*
     * Destroy thread structure as final step.
     */
    RTMemFree(pThread);
    pThread = NULL;

    return rc;
}


/**
 * Signals a guest process thread that we want it to shut down in
 * a gentle way.
 *
 * @return  IPRT status code.
 * @param   pThread             Thread to shut down.
 */
int VBoxServiceControlThreadSignalShutdown(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    VBoxServiceVerbose(3, "ControlThread: [PID %u]: Signalling shutdown ...\n",
                       pThread->uPID);

    /* Do *not* set pThread->fShutdown or other stuff here!
     * The guest thread loop will do that as soon as it processes the quit message. */

    VBOXSERVICECTRLREQUEST ctrlRequest;
    RT_ZERO(ctrlRequest);
    ctrlRequest.enmType = VBOXSERVICECTRLREQUEST_QUIT;

    int rc = VBoxServiceControlThreadPerform(pThread->uPID, &ctrlRequest);
    if (RT_FAILURE(rc))
        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Sending quit request failed with rc=%Rrc\n",
                           pThread->uPID, rc);
    return rc;
}


/**
 * Wait for a guest process thread to shut down.
 *
 * @return  IPRT status code.
 * @param   pThread             Thread to wait shutting down for.
 * @param   RTMSINTERVAL        Timeout in ms to wait for shutdown.
 */
int VBoxServiceControlThreadWaitForShutdown(const PVBOXSERVICECTRLTHREAD pThread,
                                            RTMSINTERVAL msTimeout)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    int rc = VINF_SUCCESS;
    if (   pThread->Thread != NIL_RTTHREAD
        && !ASMAtomicReadBool(&pThread->fStopped)) /* Only shutdown threads which aren't yet. */
    {
        VBoxServiceVerbose(2, "ControlThread: [PID %u]: Waiting for shutdown ...\n",
                           pThread->uPID);

        /* Wait a bit ... */
        int rcThread;
        rc = RTThreadWait(pThread->Thread, msTimeout, &rcThread);
        if (RT_FAILURE(rcThread))
        {
            VBoxServiceError("ControlThread: [PID %u]: Shutdown returned error rc=%Rrc\n",
                             pThread->uPID, rcThread);
            if (RT_SUCCESS(rc))
                rc = rcThread;
        }
    }
    return rc;
}


/**
 * Returns the guest process thread's current status.
 *
 * @return  VBOXSERVICECTRLTHREADSTATUS
 * @param   pThread             Thread to determine current status for.
 */
VBOXSERVICECTRLTHREADSTATUS VBoxServiceControlThreadGetStatus(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VBOXSERVICECTRLTHREADSTATUS_UNKNOWN);

    int rc = RTCritSectEnter(&pThread->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pThread->fStarted != pThread->fStopped);

        VBOXSERVICECTRLTHREADSTATUS enmStatus = VBOXSERVICECTRLTHREADSTATUS_UNKNOWN;
        /** @todo Add more logic here. */
        /** @todo Remove fStarted/fStopped and just use this VBOXSERVICECTRLTHREADSTATUS. */
        if (pThread->fStarted)
            enmStatus = VBOXSERVICECTRLTHREADSTATUS_STARTED;
        else if (pThread->fStopped)
            enmStatus = VBOXSERVICECTRLTHREADSTATUS_STOPPED;
        else
            AssertMsgFailed(("ControlThread: Uknown thread status (0x%x)\n"));

        rc = RTCritSectLeave(&pThread->CritSect);
        AssertRC(rc);

        return enmStatus;
    }

    return VBOXSERVICECTRLTHREADSTATUS_UNKNOWN;
}


/**
 * Closes the stdin pipe of a guest process.
 *
 * @return  IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   phStdInW            The standard input pipe handle.
 */
static int VBoxServiceControlThreadCloseStdIn(RTPOLLSET hPollSet, PRTPIPE phStdInW)
{
    AssertPtrReturn(phStdInW, VERR_INVALID_POINTER);

    int rc = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN);
    if (rc != VERR_POLL_HANDLE_ID_NOT_FOUND)
        AssertRC(rc);

    if (*phStdInW != NIL_RTPIPE)
    {
        rc = RTPipeClose(*phStdInW);
        AssertRC(rc);
        *phStdInW = NIL_RTPIPE;
    }

    return rc;
}


/**
 * Handle an error event on standard input.
 *
 * @return  IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe handle.
 */
static int VBoxServiceControlThreadHandleStdInErrorEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW)
{
    NOREF(fPollEvt);

    return VBoxServiceControlThreadCloseStdIn(hPollSet, phStdInW);
}


/**
 * Handle pending output data or error on standard out or standard error.
 *
 * @returns IPRT status code from client send.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phPipeR             The pipe handle.
 * @param   idPollHnd           The pipe ID to handle.
 *
 */
static int VBoxServiceControlThreadHandleOutputError(RTPOLLSET hPollSet, uint32_t fPollEvt,
                                                     PRTPIPE phPipeR, uint32_t idPollHnd)
{
    int rc = VINF_SUCCESS;

    rc = RTPollSetRemove(hPollSet, idPollHnd);
    AssertRC(rc);

    rc = RTPipeClose(*phPipeR);
    AssertRC(rc);
    *phPipeR = NIL_RTPIPE;

    return rc;
}


/**
 * Handle pending output data or error on standard out or standard error.
 *
 * @returns IPRT status code from client send.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phPipeR             The pipe handle.
 * @param   idPollHnd           The pipe ID to handle.
 *
 */
static int VBoxServiceControlThreadHandleOutputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt,
                                                     PRTPIPE phPipeR, uint32_t idPollHnd)
{
    int rc = VINF_SUCCESS;

    if (fPollEvt & RTPOLL_EVT_READ)
    {

        /* Make sure we go another poll round in case there was too much data
           for the buffer to hold. */
        fPollEvt &= RTPOLL_EVT_ERROR;
    }

    if (fPollEvt & RTPOLL_EVT_ERROR)
        rc = VBoxServiceControlThreadHandleOutputError(hPollSet, fPollEvt,
                                                       phPipeR, idPollHnd);
    return rc;
}


static int VBoxServiceControlThreadHandleRequest(RTPOLLSET hPollSet, uint32_t fPollEvt,
                                                 PRTPIPE phStdInW, PRTPIPE phStdOutR, PRTPIPE phStdErrR,
                                                 PVBOXSERVICECTRLTHREAD pThread)
{
#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "ControlThread: HandleIPCRequest\n");
#endif

    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdInW, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdOutR, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdErrR, VERR_INVALID_POINTER);

    /* Drain the notification pipe. */
    uint8_t abBuf[8];
    size_t cbIgnore;
    int rc = RTPipeRead(pThread->hNotificationPipeR, abBuf, sizeof(abBuf), &cbIgnore);
    if (RT_FAILURE(rc))
        VBoxServiceError("ControlThread: Draining IPC notification pipe failed with rc=%Rrc\n", rc);

    int rcReq = VINF_SUCCESS; /* Actual request result. */

    PVBOXSERVICECTRLREQUEST pRequest = pThread->pRequest;
    if (!pRequest)
    {
        VBoxServiceError("ControlThread: IPC request is invalid\n");
        return VERR_INVALID_POINTER;
    }

    switch (pRequest->enmType)
    {
        case VBOXSERVICECTRLREQUEST_QUIT: /* Main control asked us to quit. */
        {
            /** @todo Check for some conditions to check to
             *        veto quitting. */
            pThread->fShutdown = true;
            rcReq = VINF_SUCCESS;
            break;
        }

        case VBOXSERVICECTRLREQUEST_STDIN_WRITE:
        case VBOXSERVICECTRLREQUEST_STDIN_WRITE_EOF:
        {
            AssertPtrReturn(pRequest->pvData, VERR_INVALID_POINTER);
            AssertReturn(pRequest->cbData, VERR_INVALID_PARAMETER);

            size_t cbWritten = 0;
            if (   *phStdInW != NIL_RTPIPE
                && pRequest->cbData)
            {
                rcReq = RTPipeWrite(*phStdInW,
                                    pRequest->pvData, pRequest->cbData, &cbWritten);
            }
            else
                rcReq = VINF_EOF;

            /*
             * If this is the last write + we have really have written all data
             * we need to close the stdin pipe on our end and remove it from
             * the poll set.
             */
            if (   pRequest->enmType == VBOXSERVICECTRLREQUEST_STDIN_WRITE_EOF
                && pRequest->cbData  == cbWritten)
            {
                rc = VBoxServiceControlThreadCloseStdIn(hPollSet, phStdInW);
            }

            /* Reqport back actual data written (if any). */
            pRequest->cbData = cbWritten;
            break;
        }

        case VBOXSERVICECTRLREQUEST_STDOUT_READ:
        case VBOXSERVICECTRLREQUEST_STDERR_READ:
        {
            AssertPtrReturn(pRequest->pvData, VERR_INVALID_POINTER);
            AssertReturn(pRequest->cbData, VERR_INVALID_PARAMETER);

            PRTPIPE pPipeR = pRequest->enmType == VBOXSERVICECTRLREQUEST_STDERR_READ
                           ? phStdErrR : phStdOutR;
            AssertPtr(pPipeR);

            size_t cbRead = 0;
            if (*pPipeR != NIL_RTPIPE)
            {
                rcReq = RTPipeRead(*pPipeR,
                                   pRequest->pvData, pRequest->cbData, &cbRead);
                if (rcReq == VERR_BROKEN_PIPE)
                    rcReq = VINF_EOF;
            }
            else
                rcReq = VINF_EOF;

            /* Report back actual data read (if any). */
            pRequest->cbData = cbRead;
            break;
        }

        default:
            rcReq = VERR_NOT_IMPLEMENTED;
            break;
    }

    /* Assign overall result. */
    pRequest->rc = RT_SUCCESS(rc)
                 ? rcReq : rc;

    VBoxServiceVerbose(2, "ControlThread: [PID %u]: Handled req=%u, CID=%u, rcReq=%Rrc, cbData=%u\n",
                       pThread->uPID, pRequest->enmType, pRequest->uCID, rcReq, pRequest->cbData);

    /* In any case, regardless of the result, we notify
     * the main guest control to unblock it. */
    int rc2 = RTSemEventMultiSignal(pThread->RequestEvent);
    AssertRC(rc2);
    /* No access to pRequest here anymore -- could be out of scope
     * or modified already! */

    return rc;
}


/**
 * Execution loop which runs in a dedicated per-started-process thread and
 * handles all pipe input/output and signalling stuff.
 *
 * @return  IPRT status code.
 * @param   pThread                     The process' thread handle.
 * @param   hProcess                    The actual process handle.
 * @param   cMsTimeout                  Time limit (in ms) of the process' life time.
 * @param   hPollSet                    The poll set to use.
 * @param   hStdInW                     Handle to the process' stdin write end.
 * @param   hStdOutR                    Handle to the process' stdout read end.
 * @param   hStdErrR                    Handle to the process' stderr read end.
 */
static int VBoxServiceControlThreadProcLoop(PVBOXSERVICECTRLTHREAD pThread,
                                            RTPROCESS hProcess, RTMSINTERVAL cMsTimeout, RTPOLLSET hPollSet,
                                            PRTPIPE phStdInW, PRTPIPE phStdOutR, PRTPIPE phStdErrR)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdInW, VERR_INVALID_PARAMETER);
    /* Rest is optional. */

    int                         rc;
    int                         rc2;
    uint64_t const              MsStart             = RTTimeMilliTS();
    RTPROCSTATUS                ProcessStatus       = { 254, RTPROCEXITREASON_ABEND };
    bool                        fProcessAlive       = true;
    bool                        fProcessTimedOut    = false;
    uint64_t                    MsProcessKilled     = UINT64_MAX;
    RTMSINTERVAL const          cMsPollBase         = *phStdInW != NIL_RTPIPE
                                                      ? 100   /* Need to poll for input. */
                                                      : 1000; /* Need only poll for process exit and aborts. */
    RTMSINTERVAL                cMsPollCur          = 0;

    /*
     * Assign PID to thread data.
     * Also check if there already was a thread with the same PID and shut it down -- otherwise
     * the first (stale) entry will be found and we get really weird results!
     */
    rc = VBoxServiceControlAssignPID(pThread, hProcess);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlThread: Unable to assign PID=%u, to new thread, rc=%Rrc\n",
                         hProcess, rc);
        return rc;
    }

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    VBoxServiceVerbose(2, "ControlThread: [PID %u]: Process \"%s\" started, CID=%u, User=%s\n",
                       pThread->uPID, pThread->pszCmd, pThread->uContextID, pThread->pszUser);
    rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                         pThread->uPID, PROC_STS_STARTED, 0 /* u32Flags */,
                                         NULL /* pvData */, 0 /* cbData */);

    /*
     * Process input, output, the test pipe and client requests.
     */
    while (   RT_SUCCESS(rc)
           && RT_UNLIKELY(!pThread->fShutdown))
    {
        /*
         * Wait/Process all pending events.
         */
        uint32_t idPollHnd;
        uint32_t fPollEvt;
        rc2 = RTPollNoResume(hPollSet, cMsPollCur, &fPollEvt, &idPollHnd);
        if (pThread->fShutdown)
            continue;

        cMsPollCur = 0; /* No rest until we've checked everything. */

        if (RT_SUCCESS(rc2))
        {
            /*VBoxServiceVerbose(4, "ControlThread: [PID %u}: RTPollNoResume idPollHnd=%u\n",
                                 pThread->uPID, idPollHnd);*/
            switch (idPollHnd)
            {
                case VBOXSERVICECTRLPIPEID_STDIN:
                    rc = VBoxServiceControlThreadHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW);
                    break;

                case VBOXSERVICECTRLPIPEID_STDOUT:
                    rc = VBoxServiceControlThreadHandleOutputEvent(hPollSet, fPollEvt,
                                                                   phStdOutR, idPollHnd);
                    break;

                case VBOXSERVICECTRLPIPEID_STDERR:
                    rc = VBoxServiceControlThreadHandleOutputEvent(hPollSet, fPollEvt,
                                                                   phStdErrR, idPollHnd);
                    break;

                case VBOXSERVICECTRLPIPEID_IPC_NOTIFY:
                    rc = VBoxServiceControlThreadHandleRequest(hPollSet, fPollEvt,
                                                               phStdInW, phStdOutR, phStdErrR, pThread);
                    break;
            }

            if (RT_FAILURE(rc) || rc == VINF_EOF)
                break; /* Abort command, or client dead or something. */

            if (RT_UNLIKELY(pThread->fShutdown))
                break; /* We were asked to shutdown. */

            continue;
        }

        /*
         * Check for process death.
         */
        if (fProcessAlive)
        {
            rc2 = RTProcWaitNoResume(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS_NP(rc2))
            {
                fProcessAlive = false;
                continue;
            }
            if (RT_UNLIKELY(rc2 == VERR_INTERRUPTED))
                continue;
            if (RT_UNLIKELY(rc2 == VERR_PROCESS_NOT_FOUND))
            {
                fProcessAlive = false;
                ProcessStatus.enmReason = RTPROCEXITREASON_ABEND;
                ProcessStatus.iStatus   = 255;
                AssertFailed();
            }
            else
                AssertMsg(rc2 == VERR_PROCESS_RUNNING, ("%Rrc\n", rc2));
        }

        /*
         * If the process has terminated, we're should head out.
         */
        if (!fProcessAlive)
            break;

        /*
         * Check for timed out, killing the process.
         */
        uint32_t cMilliesLeft = RT_INDEFINITE_WAIT;
        if (cMsTimeout != RT_INDEFINITE_WAIT)
        {
            uint64_t u64Now = RTTimeMilliTS();
            uint64_t cMsElapsed = u64Now - MsStart;
            if (cMsElapsed >= cMsTimeout)
            {
                VBoxServiceVerbose(3, "ControlThread: [PID %u]: Timed out (%ums elapsed > %ums timeout), killing ...",
                                   pThread->uPID, cMsElapsed, cMsTimeout);

                fProcessTimedOut = true;
                if (    MsProcessKilled == UINT64_MAX
                    ||  u64Now - MsProcessKilled > 1000)
                {
                    if (u64Now - MsProcessKilled > 20*60*1000)
                        break; /* Give up after 20 mins. */
                    RTProcTerminate(hProcess);
                    MsProcessKilled = u64Now;
                    continue;
                }
                cMilliesLeft = 10000;
            }
            else
                cMilliesLeft = cMsTimeout - (uint32_t)cMsElapsed;
        }

        /* Reset the polling interval since we've done all pending work. */
        cMsPollCur = cMilliesLeft >= cMsPollBase ? cMsPollBase : cMilliesLeft;

        /*
         * Need to exit?
         */
        if (pThread->fShutdown)
            break;
    }

    /*
     * Try kill the process if it's still alive at this point.
     */
    if (fProcessAlive)
    {
        if (MsProcessKilled == UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Is still alive and not killed yet\n",
                               pThread->uPID);

            MsProcessKilled = RTTimeMilliTS();
            RTProcTerminate(hProcess);
            RTThreadSleep(500);
        }

        for (size_t i = 0; i < 10; i++)
        {
            VBoxServiceVerbose(4, "ControlThread: [PID %u]: Kill attempt %d/10: Waiting to exit ...\n",
                               pThread->uPID, i + 1);
            rc2 = RTProcWait(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS(rc2))
            {
                VBoxServiceVerbose(4, "ControlThread: [PID %u]: Kill attempt %d/10: Exited\n",
                                   pThread->uPID, i + 1);
                fProcessAlive = false;
                break;
            }
            if (i >= 5)
            {
                VBoxServiceVerbose(4, "ControlThread: [PID %u]: Kill attempt %d/10: Trying to terminate ...\n",
                                   pThread->uPID, i + 1);
                RTProcTerminate(hProcess);
            }
            RTThreadSleep(i >= 5 ? 2000 : 500);
        }

        if (fProcessAlive)
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Could not be killed\n", pThread->uPID);
    }

    /*
     * If we don't have a client problem (RT_FAILURE(rc)) we'll reply to the
     * clients exec packet now.
     */
    if (RT_SUCCESS(rc))
    {
        uint32_t uStatus = PROC_STS_UNDEFINED;
        uint32_t uFlags = 0;

        if (     fProcessTimedOut  && !fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Timed out and got killed\n",
                               pThread->uPID);
            uStatus = PROC_STS_TOK;
        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Timed out and did *not* get killed\n",
                               pThread->uPID);
            uStatus = PROC_STS_TOA;
        }
        else if (pThread->fShutdown && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Got terminated because system/service is about to shutdown\n",
                               pThread->uPID);
            uStatus = PROC_STS_DWN; /* Service is stopping, process was killed. */
            uFlags = pThread->uFlags; /* Return handed-in execution flags back to the host. */
        }
        else if (fProcessAlive)
        {
            VBoxServiceError("ControlThread: [PID %u]: Is alive when it should not!\n",
                             pThread->uPID);
        }
        else if (MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceError("ControlThread: [PID %u]: Has been killed when it should not!\n",
                             pThread->uPID);
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Ended with RTPROCEXITREASON_NORMAL (%u)\n",
                               pThread->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TEN;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Ended with RTPROCEXITREASON_SIGNAL (%u)\n",
                               pThread->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TES;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Ended with RTPROCEXITREASON_ABEND (%u)\n",
                               pThread->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TEA;
            uFlags = ProcessStatus.iStatus;
        }
        else
            VBoxServiceVerbose(1, "ControlThread: [PID %u]: Handling process status %u not implemented\n",
                               pThread->uPID, ProcessStatus.enmReason);

        VBoxServiceVerbose(2, "ControlThread: [PID %u]: Ended, ClientID=%u, CID=%u, Status=%u, Flags=0x%x\n",
                           pThread->uPID, pThread->uClientID, pThread->uContextID, uStatus, uFlags);
        rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                             pThread->uPID, uStatus, uFlags,
                                             NULL /* pvData */, 0 /* cbData */);
        if (RT_FAILURE(rc))
            VBoxServiceError("ControlThread: [PID %u]: Error reporting final status to host; rc=%Rrc\n",
                             pThread->uPID, rc);

        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Process loop ended with rc=%Rrc\n",
                           pThread->uPID, rc);
    }
    else
        VBoxServiceError("ControlThread: [PID %u]: Loop failed with rc=%Rrc\n",
                         pThread->uPID, rc);
    return rc;
}


static int vboxServiceControlThreadInitPipe(PRTHANDLE ph, PRTPIPE phPipe)
{
    AssertPtrReturn(ph, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phPipe, VERR_INVALID_PARAMETER);

    ph->enmType = RTHANDLETYPE_PIPE;
    ph->u.hPipe = NIL_RTPIPE;
    *phPipe     = NIL_RTPIPE;

    return VINF_SUCCESS;
}


/**
 * Sets up the redirection / pipe / nothing for one of the standard handles.
 *
 * @returns IPRT status code.  No client replies made.
 * @param   pszHowTo            How to set up this standard handle.
 * @param   fd                  Which standard handle it is (0 == stdin, 1 ==
 *                              stdout, 2 == stderr).
 * @param   ph                  The generic handle that @a pph may be set
 *                              pointing to.  Always set.
 * @param   pph                 Pointer to the RTProcCreateExec argument.
 *                              Always set.
 * @param   phPipe              Where to return the end of the pipe that we
 *                              should service.
 */
static int VBoxServiceControlThreadSetupPipe(const char *pszHowTo, int fd,
                                             PRTHANDLE ph, PRTHANDLE *pph, PRTPIPE phPipe)
{
    AssertPtrReturn(ph, VERR_INVALID_POINTER);
    AssertPtrReturn(ph, VERR_INVALID_POINTER);
    AssertPtrReturn(pph, VERR_INVALID_POINTER);

    int rc;

    ph->enmType = RTHANDLETYPE_PIPE;
    ph->u.hPipe = NIL_RTPIPE;
    *pph        = NULL;
    *phPipe     = NIL_RTPIPE;

    if (!strcmp(pszHowTo, "|"))
    {
        /*
         * Setup a pipe for forwarding to/from the client.
         * The ph union struct will be filled with a pipe read/write handle
         * to represent the "other" end to phPipe.
         */
        if (fd == 0) /* stdin? */
        {
            /* Connect a wrtie pipe specified by phPipe to stdin. */
            rc = RTPipeCreate(&ph->u.hPipe, phPipe, RTPIPE_C_INHERIT_READ);
        }
        else /* stdout or stderr? */
        {
            /* Connect a read pipe specified by phPipe to stdout or stderr. */
            rc = RTPipeCreate(phPipe, &ph->u.hPipe, RTPIPE_C_INHERIT_WRITE);
        }

        if (RT_FAILURE(rc))
            return rc;

        ph->enmType = RTHANDLETYPE_PIPE;
        *pph = ph;
    }
    else if (!strcmp(pszHowTo, "/dev/null"))
    {
        /*
         * Redirect to/from /dev/null.
         */
        RTFILE hFile;
        rc = RTFileOpenBitBucket(&hFile, fd == 0 ? RTFILE_O_READ : RTFILE_O_WRITE);
        if (RT_FAILURE(rc))
            return rc;

        ph->enmType = RTHANDLETYPE_FILE;
        ph->u.hFile = hFile;
        *pph = ph;
    }
    else /* Add other piping stuff here. */
        rc = VERR_INVALID_PARAMETER;

    return rc;
}


/**
 * Expands a file name / path to its real content. This only works on Windows
 * for now (e.g. translating "%TEMP%\foo.exe" to "C:\Windows\Temp" when starting
 * with system / administrative rights).
 *
 * @return  IPRT status code.
 * @param   pszPath                     Path to resolve.
 * @param   pszExpanded                 Pointer to string to store the resolved path in.
 * @param   cbExpanded                  Size (in bytes) of string to store the resolved path.
 */
static int VBoxServiceControlThreadMakeFullPath(const char *pszPath, char *pszExpanded, size_t cbExpanded)
{
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!ExpandEnvironmentStrings(pszPath, pszExpanded, cbExpanded))
        rc = RTErrConvertFromWin32(GetLastError());
#else
    /* No expansion for non-Windows yet. */
    rc = RTStrCopy(pszExpanded, cbExpanded, pszPath);
#endif
#ifdef DEBUG
    VBoxServiceVerbose(3, "ControlThread: VBoxServiceControlExecMakeFullPath: %s -> %s\n",
                       pszPath, pszExpanded);
#endif
    return rc;
}


/**
 * Resolves the full path of a specified executable name. This function also
 * resolves internal VBoxService tools to its appropriate executable path + name.
 *
 * @return  IPRT status code.
 * @param   pszFileName                 File name to resovle.
 * @param   pszResolved                 Pointer to a string where the resolved file name will be stored.
 * @param   cbResolved                  Size (in bytes) of resolved file name string.
 */
static int VBoxServiceControlThreadResolveExecutable(const char *pszFileName, char *pszResolved, size_t cbResolved)
{
    int rc = VINF_SUCCESS;

    /* Search the path of our executable. */
    char szVBoxService[RTPATH_MAX];
    if (RTProcGetExecutablePath(szVBoxService, sizeof(szVBoxService)))
    {
        char *pszExecResolved = NULL;
        if (   (g_pszProgName && RTStrICmp(pszFileName, g_pszProgName) == 0)
            || !RTStrICmp(pszFileName, VBOXSERVICE_NAME))
        {
            /* We just want to execute VBoxService (no toolbox). */
            pszExecResolved = RTStrDup(szVBoxService);
        }
        else /* Nothing to resolve, copy original. */
            pszExecResolved = RTStrDup(pszFileName);
        AssertPtr(pszExecResolved);

        rc = VBoxServiceControlThreadMakeFullPath(pszExecResolved, pszResolved, cbResolved);
#ifdef DEBUG
        VBoxServiceVerbose(3, "ControlThread: VBoxServiceControlExecResolveExecutable: %s -> %s\n",
                           pszFileName, pszResolved);
#endif
        RTStrFree(pszExecResolved);
    }
    return rc;
}


/**
 * Constructs the argv command line by resolving environment variables
 * and relative paths.
 *
 * @return IPRT status code.
 * @param  pszArgv0         First argument (argv0), either original or modified version.
 * @param  papszArgs        Original argv command line from the host, starting at argv[1].
 * @param  ppapszArgv       Pointer to a pointer with the new argv command line.
 *                          Needs to be freed with RTGetOptArgvFree.
 */
static int VBoxServiceControlThreadPrepareArgv(const char *pszArgv0,
                                               const char * const *papszArgs, char ***ppapszArgv)
{
/** @todo RTGetOptArgvToString converts to MSC quoted string, while
 *        RTGetOptArgvFromString takes bourne shell according to the docs...
 * Actually, converting to and from here is a very roundabout way of prepending
 * an entry (pszFilename) to an array (*ppapszArgv). */
    int rc = VINF_SUCCESS;
    char *pszNewArgs = NULL;
    if (pszArgv0)
        rc = RTStrAAppend(&pszNewArgs, pszArgv0);
    if (   RT_SUCCESS(rc)
        && papszArgs)

    {
        char *pszArgs;
        rc = RTGetOptArgvToString(&pszArgs, papszArgs,
                                  RTGETOPTARGV_CNV_QUOTE_MS_CRT); /* RTGETOPTARGV_CNV_QUOTE_BOURNE_SH */
        if (RT_SUCCESS(rc))
        {
            rc = RTStrAAppend(&pszNewArgs, " ");
            if (RT_SUCCESS(rc))
                rc = RTStrAAppend(&pszNewArgs, pszArgs);
        }
    }

    if (RT_SUCCESS(rc))
    {
        int iNumArgsIgnored;
        rc = RTGetOptArgvFromString(ppapszArgv, &iNumArgsIgnored,
                                    pszNewArgs ? pszNewArgs : "", NULL /* Use standard separators. */);
    }

    if (pszNewArgs)
        RTStrFree(pszNewArgs);
    return rc;
}


/**
 * Helper function to create/start a process on the guest.
 *
 * @return  IPRT status code.
 * @param   pszExec                     Full qualified path of process to start (without arguments).
 * @param   papszArgs                   Pointer to array of command line arguments.
 * @param   hEnv                        Handle to environment block to use.
 * @param   fFlags                      Process execution flags.
 * @param   phStdIn                     Handle for the process' stdin pipe.
 * @param   phStdOut                    Handle for the process' stdout pipe.
 * @param   phStdErr                    Handle for the process' stderr pipe.
 * @param   pszAsUser                   User name (account) to start the process under.
 * @param   pszPassword                 Password of the specified user.
 * @param   phProcess                   Pointer which will receive the process handle after
 *                                      successful process start.
 */
static int VBoxServiceControlThreadCreateProcess(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                                                 PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                                                 const char *pszPassword, PRTPROCESS phProcess)
{
    AssertPtrReturn(pszExec, VERR_INVALID_PARAMETER);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phProcess, VERR_INVALID_PARAMETER);

    int  rc = VINF_SUCCESS;
    char szExecExp[RTPATH_MAX];
#ifdef RT_OS_WINDOWS
    /*
     * If sysprep should be executed do this in the context of VBoxService, which
     * (usually, if started by SCM) has administrator rights. Because of that a UI
     * won't be shown (doesn't have a desktop).
     */
    if (RTStrICmp(pszExec, "sysprep") == 0)
    {
        /* Use a predefined sysprep path as default. */
        char szSysprepCmd[RTPATH_MAX] = "C:\\sysprep\\sysprep.exe";

        /*
         * On Windows Vista (and up) sysprep is located in "system32\\sysprep\\sysprep.exe",
         * so detect the OS and use a different path.
         */
        OSVERSIONINFOEX OSInfoEx;
        RT_ZERO(OSInfoEx);
        OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        if (    GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
            &&  OSInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT
            &&  OSInfoEx.dwMajorVersion >= 6 /* Vista or later */)
        {
            rc = RTEnvGetEx(RTENV_DEFAULT, "windir", szSysprepCmd, sizeof(szSysprepCmd), NULL);
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szSysprepCmd, sizeof(szSysprepCmd), "system32\\sysprep\\sysprep.exe");
        }

        if (RT_SUCCESS(rc))
        {
            char **papszArgsExp;
            rc = VBoxServiceControlThreadPrepareArgv(szSysprepCmd /* argv0 */, papszArgs, &papszArgsExp);
            if (RT_SUCCESS(rc))
            {
                rc = RTProcCreateEx(szSysprepCmd, papszArgsExp, hEnv, 0 /* fFlags */,
                                    phStdIn, phStdOut, phStdErr, NULL /* pszAsUser */,
                                    NULL /* pszPassword */, phProcess);
            }
            RTGetOptArgvFree(papszArgsExp);
        }
        return rc;
    }
#endif /* RT_OS_WINDOWS */

#ifdef VBOXSERVICE_TOOLBOX
    if (RTStrStr(pszExec, "vbox_") == pszExec)
    {
        /* We want to use the internal toolbox (all internal
         * tools are starting with "vbox_" (e.g. "vbox_cat"). */
        rc = VBoxServiceControlThreadResolveExecutable(VBOXSERVICE_NAME, szExecExp, sizeof(szExecExp));
    }
    else
    {
#endif
        /*
         * Do the environment variables expansion on executable and arguments.
         */
        rc = VBoxServiceControlThreadResolveExecutable(pszExec, szExecExp, sizeof(szExecExp));
#ifdef VBOXSERVICE_TOOLBOX
    }
#endif
    if (RT_SUCCESS(rc))
    {
        char **papszArgsExp;
        rc = VBoxServiceControlThreadPrepareArgv(pszExec /* Always use the unmodified executable name as argv0. */,
                                                 papszArgs /* Append the rest of the argument vector (if any). */, &papszArgsExp);
        if (RT_SUCCESS(rc))
        {
            uint32_t uProcFlags = 0;
            if (fFlags)
            {
                if (fFlags & EXECUTEPROCESSFLAG_HIDDEN)
                    uProcFlags |= RTPROC_FLAGS_HIDDEN;
                if (fFlags & EXECUTEPROCESSFLAG_NO_PROFILE)
                    uProcFlags |= RTPROC_FLAGS_NO_PROFILE;
            }

            /* If no user name specified run with current credentials (e.g.
             * full service/system rights). This is prohibited via official Main API!
             *
             * Otherwise use the RTPROC_FLAGS_SERVICE to use some special authentication
             * code (at least on Windows) for running processes as different users
             * started from our system service. */
            if (*pszAsUser)
                uProcFlags |= RTPROC_FLAGS_SERVICE;
#ifdef DEBUG
            VBoxServiceVerbose(3, "ControlThread: Command: %s\n", szExecExp);
            for (size_t i = 0; papszArgsExp[i]; i++)
                VBoxServiceVerbose(3, "ControlThread:\targv[%ld]: %s\n", i, papszArgsExp[i]);
#endif
            /* Do normal execution. */
            rc = RTProcCreateEx(szExecExp, papszArgsExp, hEnv, uProcFlags,
                                phStdIn, phStdOut, phStdErr,
                                *pszAsUser   ? pszAsUser   : NULL,
                                *pszPassword ? pszPassword : NULL,
                                phProcess);
            RTGetOptArgvFree(papszArgsExp);
        }
    }
    return rc;
}

/**
 * The actual worker routine (loop) for a started guest process.
 *
 * @return  IPRT status code.
 * @param   PVBOXSERVICECTRLTHREAD         Thread data associated with a started process.
 */
static int VBoxServiceControlThreadProcessWorker(PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    VBoxServiceVerbose(3, "ControlThread: Thread of process \"%s\" started\n", pThread->pszCmd);

    int rc = VbglR3GuestCtrlConnect(&pThread->uClientID);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlThread: Thread failed to connect to the guest control service, aborted! Error: %Rrc\n", rc);
        RTThreadUserSignal(RTThreadSelf());
        return rc;
    }
    VBoxServiceVerbose(3, "ControlThread: Guest process \"%s\" got client ID=%u, flags=0x%x\n",
                       pThread->pszCmd, pThread->uClientID, pThread->uFlags);

    bool fSignalled = false; /* Indicator whether we signalled the thread user event already. */

    /*
     * Create the environment.
     */
    RTENV hEnv;
    rc = RTEnvClone(&hEnv, RTENV_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        size_t i;
        for (i = 0; i < pThread->uNumEnvVars && pThread->papszEnv; i++)
        {
            rc = RTEnvPutEx(hEnv, pThread->papszEnv[i]);
            if (RT_FAILURE(rc))
                break;
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Setup the redirection of the standard stuff.
             */
            /** @todo consider supporting: gcc stuff.c >file 2>&1.  */
            RTHANDLE    hStdIn;
            PRTHANDLE   phStdIn;
            rc = VBoxServiceControlThreadSetupPipe("|", 0 /*STDIN_FILENO*/,
                                                   &hStdIn, &phStdIn, &pThread->pipeStdInW);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE    hStdOut;
                PRTHANDLE   phStdOut;
                RTPIPE      hStdOutR;
                rc = VBoxServiceControlThreadSetupPipe(  (pThread->uFlags & EXECUTEPROCESSFLAG_WAIT_STDOUT)
                                                       ? "|" : "/dev/null",
                                                       1 /*STDOUT_FILENO*/,
                                                       &hStdOut, &phStdOut, &hStdOutR);
                if (RT_SUCCESS(rc))
                {
                    RTHANDLE    hStdErr;
                    PRTHANDLE   phStdErr;
                    RTPIPE      hStdErrR;
                    rc = VBoxServiceControlThreadSetupPipe(  (pThread->uFlags & EXECUTEPROCESSFLAG_WAIT_STDERR)
                                                           ? "|" : "/dev/null",
                                                           2 /*STDERR_FILENO*/,
                                                           &hStdErr, &phStdErr, &hStdErrR);
                    if (RT_SUCCESS(rc))
                    {
                        /*
                         * Create a poll set for the pipes and let the
                         * transport layer add stuff to it as well.
                         */
                        RTPOLLSET hPollSet;
                        rc = RTPollSetCreate(&hPollSet);
                        if (RT_SUCCESS(rc))
                        {
                            /* Stdin. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pThread->pipeStdInW, RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDIN);
                            /* Stdout. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdOutR, RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDOUT);
                            /* Stderr. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdErrR, RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDERR);
                            /* IPC notification pipe. */
                            if (RT_SUCCESS(rc))
                                rc = RTPipeCreate(&pThread->hNotificationPipeR, &pThread->hNotificationPipeW, 0 /* Flags */);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pThread->hNotificationPipeR, RTPOLL_EVT_READ, VBOXSERVICECTRLPIPEID_IPC_NOTIFY);

                            if (RT_SUCCESS(rc))
                            {
                                RTPROCESS hProcess;
                                rc = VBoxServiceControlThreadCreateProcess(pThread->pszCmd, pThread->papszArgs, hEnv, pThread->uFlags,
                                                                           phStdIn, phStdOut, phStdErr,
                                                                           pThread->pszUser, pThread->pszPassword,
                                                                           &hProcess);
                                if (RT_FAILURE(rc))
                                    VBoxServiceError("ControlThread: Error starting process, rc=%Rrc\n", rc);
                                /*
                                 * Tell the control thread that it can continue
                                 * spawning services. This needs to be done after the new
                                 * process has been started because otherwise signal handling
                                 * on (Open) Solaris does not work correctly (see #5068).
                                 */
                                int rc2 = RTThreadUserSignal(RTThreadSelf());
                                if (RT_FAILURE(rc2))
                                    rc = rc2;
                                fSignalled = true;

                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * Close the child ends of any pipes and redirected files.
                                     */
                                    rc2 = RTHandleClose(phStdIn);   AssertRC(rc2);
                                    phStdIn    = NULL;
                                    rc2 = RTHandleClose(phStdOut);  AssertRC(rc2);
                                    phStdOut   = NULL;
                                    rc2 = RTHandleClose(phStdErr);  AssertRC(rc2);
                                    phStdErr   = NULL;

                                    /* Enter the process loop. */
                                    rc = VBoxServiceControlThreadProcLoop(pThread,
                                                                          hProcess, pThread->uTimeLimitMS, hPollSet,
                                                                          &pThread->pipeStdInW, &hStdOutR, &hStdErrR);
                                    /*
                                     * The handles that are no longer in the set have
                                     * been closed by the above call in order to prevent
                                     * the guest from getting stuck accessing them.
                                     * So, NIL the handles to avoid closing them again.
                                     */
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_IPC_NOTIFY, NULL)))
                                    {
                                        pThread->hNotificationPipeR = NIL_RTPIPE;
                                        pThread->hNotificationPipeW = NIL_RTPIPE;
                                    }
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDERR, NULL)))
                                        hStdErrR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDOUT, NULL)))
                                        hStdOutR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDIN, NULL)))
                                        pThread->pipeStdInW = NIL_RTPIPE;
                                }
                            }
                            RTPollSetDestroy(hPollSet);

                            RTPipeClose(pThread->hNotificationPipeR);
                            pThread->hNotificationPipeR = NIL_RTPIPE;
                            RTPipeClose(pThread->hNotificationPipeW);
                            pThread->hNotificationPipeW = NIL_RTPIPE;
                        }
                        RTPipeClose(hStdErrR);
                        hStdErrR = NIL_RTPIPE;
                        RTHandleClose(phStdErr);
                    }
                    RTPipeClose(hStdOutR);
                    hStdOutR = NIL_RTPIPE;
                    RTHandleClose(phStdOut);
                }
                RTPipeClose(pThread->pipeStdInW);
                RTHandleClose(phStdIn);
            }
        }
        RTEnvDestroy(hEnv);
    }

    if (pThread->uClientID)
    {
        int rc2;
        if (RT_FAILURE(rc))
        {
            rc2 = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID, pThread->uPID,
                                                  PROC_STS_ERROR, rc,
                                                  NULL /* pvData */, 0 /* cbData */);
            if (RT_FAILURE(rc2))
                VBoxServiceError("ControlThread: Could not report process failure error; rc=%Rrc (process error %Rrc)\n",
                                 rc2, rc);
        }

        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Cancelling pending waits (client ID=%u)\n",
                           pThread->uPID, pThread->uClientID);
        rc2 = VbglR3GuestCtrlCancelPendingWaits(pThread->uClientID);
        if (RT_FAILURE(rc2))
        {
            VBoxServiceError("ControlThread: [PID %u]: Cancelling pending waits failed; rc=%Rrc\n",
                             pThread->uPID, rc2);
            if (RT_SUCCESS(rc))
                rc = rc2;
        }

        /* Disconnect from guest control service. */
        VBoxServiceVerbose(3, "ControlThread: [PID %u]: Disconnecting (client ID=%u) ...\n",
                           pThread->uPID, pThread->uClientID);
        VbglR3GuestCtrlDisconnect(pThread->uClientID);
        pThread->uClientID = 0;
    }

    VBoxServiceVerbose(3, "ControlThread: [PID %u]: Thread of process \"%s\" ended with rc=%Rrc\n",
                       pThread->uPID, pThread->pszCmd, rc);

    ASMAtomicXchgBool(&pThread->fStarted, false);

    /*
     * If something went wrong signal the user event so that others don't wait
     * forever on this thread.
     */
    if (RT_FAILURE(rc) && !fSignalled)
        RTThreadUserSignal(RTThreadSelf());

    /*
     * Remove thread from global thread list. After this it's safe to shutdown
     * and deallocate this thread.
     */
    VBoxServiceControlRemoveThread(pThread);

    int rc2 = vboxServiceControlThreadFree(pThread);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}


/**
 * Thread main routine for a started process.
 *
 * @return IPRT status code.
 * @param  RTTHREAD             Pointer to the thread's data.
 * @param  void*                User-supplied argument pointer.
 *
 */
static DECLCALLBACK(int) VBoxServiceControlThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLTHREAD pThread = (VBOXSERVICECTRLTHREAD*)pvUser;
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    return VBoxServiceControlThreadProcessWorker(pThread);
}


/**
 * Executes (starts) a process on the guest. This causes a new thread to be created
 * so that this function will not block the overall program execution.
 *
 * @return  IPRT status code.
 * @param   uClientID                   Client ID for accessing host service.
 * @param   uContextID                  Context ID to associate the process to start with.
 * @param   pszCmd                      Full qualified path of process to start (without arguments).
 * @param   uFlags                      Process execution flags.
 * @param   pszArgs                     String of arguments to pass to the process to start.
 * @param   uNumArgs                    Number of arguments specified in pszArgs.
 * @param   pszEnv                      String of environment variables ("FOO=BAR") to pass to the process
 *                                      to start.
 * @param   cbEnv                       Size (in bytes) of environment variables.
 * @param   uNumEnvVars                 Number of environment variables specified in pszEnv.
 * @param   pszUser                     User name (account) to start the process under.
 * @param   pszPassword                 Password of specified user name (account).
 * @param   uTimeLimitMS                Time limit (in ms) of the process' life time.
 * @param   ppNode                       The thread's list node to insert into the global thread list
 *                                      on success.
 */
int VBoxServiceControlThreadStart(uint32_t uClientID, uint32_t uContextID,
                                  const char *pszCmd, uint32_t uFlags,
                                  const char *pszArgs, uint32_t uNumArgs,
                                  const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                  const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS,
                                  PRTLISTNODE *ppNode)
{
    AssertPtrReturn(ppNode, VERR_INVALID_POINTER);

    /*
     * Allocate new thread data and assign it to our thread list.
     */
    PVBOXSERVICECTRLTHREAD pThread = (PVBOXSERVICECTRLTHREAD)RTMemAlloc(sizeof(VBOXSERVICECTRLTHREAD));
    if (!pThread)
        return VERR_NO_MEMORY;

    int rc = gstsvcCntlExecThreadInit(pThread,
                                      uContextID,
                                      pszCmd, uFlags,
                                      pszArgs, uNumArgs,
                                      pszEnv, cbEnv, uNumEnvVars,
                                      pszUser, pszPassword,
                                      uTimeLimitMS);
    if (RT_SUCCESS(rc))
    {
        static uint32_t s_uCtrlExecThread = 0;

        rc = RTThreadCreateF(&pThread->Thread, VBoxServiceControlThread,
                             pThread /*pvUser*/, 0 /*cbStack*/,
                             RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "controlexec%u", s_uCtrlExecThread++);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("ControlThread: RTThreadCreate failed, rc=%Rrc\n, pThread=%p\n",
                             rc, pThread);
        }
        else
        {
            VBoxServiceVerbose(4, "ControlThread: Waiting for thread to initialize ...\n");

            /* Wait for the thread to initialize. */
            RTThreadUserWait(pThread->Thread, 60 * 1000 /* 60 seconds max. */);
            if (ASMAtomicReadBool(&pThread->fShutdown))
            {
                VBoxServiceError("ControlThread: Thread for process \"%s\" failed to start!\n", pszCmd);
                rc = VERR_GENERAL_FAILURE;
            }
            else
            {
                pThread->fStarted = true;

                /* Return the thread's node. */
                *ppNode = &pThread->Node;
            }
        }
    }

    if (RT_FAILURE(rc))
        RTMemFree(pThread);

    return rc;
}


/**
 * Performs a request to a specific (formerly started) guest process and waits
 * for its response.
 *
 * @return  IPRT status code.
 * @param   uPID                PID of guest process to perform a request to.
 * @param   pRequest            Pointer to request  to perform.
 */
int VBoxServiceControlThreadPerform(uint32_t uPID, PVBOXSERVICECTRLREQUEST pRequest)
{
    AssertPtrReturn(pRequest, VERR_INVALID_POINTER);
    AssertReturn(pRequest->enmType > VBOXSERVICECTRLREQUEST_UNKNOWN, VERR_INVALID_PARAMETER);
    /* Rest in pRequest is optional (based on the request type). */

    int rc = VINF_SUCCESS;
    PVBOXSERVICECTRLTHREAD pThread = VBoxServiceControlGetThreadLocked(uPID);
    if (pThread)
    {
        /* Set request result to some defined state in case
         * it got cancelled. */
        pRequest->rc = VERR_CANCELLED;

        /* Set request structure pointer. */
        pThread->pRequest = pRequest;

        /** @todo To speed up simultaneous guest process handling we could add a worker threads
         *        or queue in order to wait for the request to happen. Later. */

        /* Wake up guest thrad by sending a wakeup byte to the notification pipe so
         * that RTPoll unblocks (returns) and we then can do our requested operation. */
        if (pThread->hNotificationPipeW == NIL_RTPIPE)
            rc = VERR_BROKEN_PIPE;
        size_t cbWritten;
        if (RT_SUCCESS(rc))
            rc = RTPipeWrite(pThread->hNotificationPipeW, "i", 1, &cbWritten);

        if (   RT_SUCCESS(rc)
            && cbWritten)
        {
            VBoxServiceVerbose(3, "ControlThread: [PID %u]: Waiting for response on enmType=%u, pvData=0x%p, cbData=%u\n",
                               uPID, pRequest->enmType, pRequest->pvData, pRequest->cbData);
            /* Wait on the request to get completed (or we are asked to abort/shutdown). */
            rc = RTSemEventMultiWait(pThread->RequestEvent, RT_INDEFINITE_WAIT);
            if (RT_SUCCESS(rc))
            {
                VBoxServiceVerbose(4, "ControlThread: [PID %u]: Performed with rc=%Rrc, cbData=%u\n",
                                   uPID, pRequest->rc, pRequest->cbData);

                /* Give back overall request result. */
                rc = pRequest->rc;

                /* Reset the semaphore. */
                int rc2 = RTSemEventMultiReset(pThread->RequestEvent);
                if (RT_FAILURE(rc2))
                    VBoxServiceError("ControlThread: [PID %u]: Unable to reset request event, rc=%Rrc\n",
                                     uPID, rc2);
            }
            else
                VBoxServiceError("ControlThread: [PID %u]: Wait failed, rc=%Rrc\n",
                                 uPID, rc);
        }

        VBoxServiceControlThreadUnlock(pThread);
    }
    else /* PID not found! */
        rc = VERR_NOT_FOUND;

    VBoxServiceVerbose(3, "ControlThread: [PID %u]: Performed enmType=%u, uCID=%u, pvData=0x%p, cbData=%u, rc=%Rrc\n",
                       uPID, pRequest->enmType, pRequest->uCID, pRequest->pvData, pRequest->cbData, rc);
    return rc;
}

