/* $Id$ */
/** @file
 * VBoxServiceControlThread - Guest process handling.
 */

/*
 * Copyright (C) 2012-2013 Oracle Corporation
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
#include "VBoxServiceControl.h"

using namespace guestControl;

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int                  gstcntlProcessAssignPID(PVBOXSERVICECTRLPROCESS pThread, uint32_t uPID);
static int                  gstcntlProcessLock(PVBOXSERVICECTRLPROCESS pProcess);
static int                  gstcntlProcessRequestCancel(PVBOXSERVICECTRLREQUEST pThread);
static int                  gstcntlProcessSetupPipe(const char *pszHowTo, int fd, PRTHANDLE ph, PRTHANDLE *pph, PRTPIPE phPipe);
static int                  gstcntlProcessUnlock(PVBOXSERVICECTRLPROCESS pProcess);

/**
 * Initialies the passed in thread data structure with the parameters given.
 *
 * @return  IPRT status code.
 * @param   pProcess                    Process to initialize.
 * @param   pSession                    Guest session the process is bound to.
 * @param   pStartupInfo                Startup information.
 * @param   u32ContextID                The context ID bound to this request / command.
 */
static int gstcntlProcessInit(PVBOXSERVICECTRLPROCESS pProcess,
                              const PVBOXSERVICECTRLSESSION pSession,
                              const PVBOXSERVICECTRLPROCSTARTUPINFO pStartupInfo,
                              uint32_t u32ContextID)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pStartupInfo, VERR_INVALID_POINTER);

    /* General stuff. */
    pProcess->pSession   = pSession;
    pProcess->pAnchor    = NULL;
    pProcess->Node.pPrev = NULL;
    pProcess->Node.pNext = NULL;

    pProcess->fShutdown = false;
    pProcess->fStarted  = false;
    pProcess->fStopped  = false;

    pProcess->uContextID = u32ContextID;
    /* ClientID will be assigned when thread is started; every guest
     * process has its own client ID to detect crashes on a per-guest-process
     * level. */

    int rc = RTCritSectInit(&pProcess->CritSect);
    if (RT_FAILURE(rc))
        return rc;

    pProcess->uPID     = 0;          /* Don't have a PID yet. */
    pProcess->pRequest = NULL;       /* No request assigned yet. */

    /* Copy over startup info. */
    memcpy(&pProcess->StartupInfo, pStartupInfo, sizeof(VBOXSERVICECTRLPROCSTARTUPINFO));

    /* Adjust timeout value. */
    if (   pProcess->StartupInfo.uTimeLimitMS == UINT32_MAX
        || pProcess->StartupInfo.uTimeLimitMS == 0)
        pProcess->StartupInfo.uTimeLimitMS = RT_INDEFINITE_WAIT;

    if (RT_FAILURE(rc)) /* Clean up on failure. */
        GstCntlProcessFree(pProcess);
    return rc;
}


/**
 * Frees a guest process.
 *
 * @return  IPRT status code.
 * @param   pProcess                Guest process to free.
 */
int GstCntlProcessFree(PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    VBoxServiceVerbose(3, "[PID %RU32]: Freeing ...\n",
                       pProcess->uPID);


    /*
     * Destroy other thread data.
     */
    if (RTCritSectIsInitialized(&pProcess->CritSect))
        RTCritSectDelete(&pProcess->CritSect);

    /*
     * Destroy thread structure as final step.
     */
    RTMemFree(pProcess);
    pProcess = NULL;

    return VINF_SUCCESS;
}


/**
 * Signals a guest process thread that we want it to shut down in
 * a gentle way.
 *
 * @return  IPRT status code.
 * @param   pProcess            Process to stop.
 */
int GstCntlProcessStop(PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);

    VBoxServiceVerbose(3, "[PID %RU32]: Stopping ...\n",
                       pProcess->uPID);

    /* Do *not* set pThread->fShutdown or other stuff here!
     * The guest thread loop will do that as soon as it processes the quit message. */

    PVBOXSERVICECTRLREQUEST pRequest;
    int rc = GstCntlProcessRequestAlloc(&pRequest, VBOXSERVICECTRLREQUEST_PROC_TERM);
    if (RT_SUCCESS(rc))
    {
        rc = GstCntlProcessPerform(pProcess, pRequest,
                                   true /* Async */);
        if (RT_FAILURE(rc))
            VBoxServiceVerbose(3, "[PID %RU32]: Sending termination request failed with rc=%Rrc\n",
                               pProcess->uPID, rc);
        /* Deletion of pRequest will be done on request completion (asynchronous). */
    }

    return rc;
}


/**
 * Releases a previously acquired guest process (decreses the refcount).
 *
 * @param   pProcess            Process to unlock.
 */
void GstCntlProcessRelease(const PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturnVoid(pProcess);

    int rc = RTCritSectEnter(&pProcess->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pProcess->cRefs);
        pProcess->cRefs--;
        rc = RTCritSectLeave(&pProcess->CritSect);
        AssertRC(rc);
    }
}


/**
 * Wait for a guest process thread to shut down.
 * Note: Caller is responsible for locking!
 *
 * @return  IPRT status code.
 * @param   pProcess            Process to wait shutting down for.
 * @param   RTMSINTERVAL        Timeout in ms to wait for shutdown.
 * @param   pRc                 Where to store the thread's return code. Optional.
 */
int GstCntlProcessWait(const PVBOXSERVICECTRLPROCESS pProcess,
                       RTMSINTERVAL msTimeout, int *pRc)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    /* pRc is optional. */

    AssertMsgReturn(ASMAtomicReadBool(&pProcess->fStarted),
                    ("Tried to wait on guest process=%p which has not been started yet\n",
                     pProcess), VERR_INVALID_PARAMETER);

    /* Guest process already has been stopped, no need to wait. */
    if (ASMAtomicReadBool(&pProcess->fStopped))
        return VINF_SUCCESS;

    VBoxServiceVerbose(2, "[PID %RU32]: Waiting for shutdown (%RU32ms) ...\n",
                       pProcess->uPID, msTimeout);

    /* Wait a bit ... */
    int rcThread;
    Assert(pProcess->Thread != NIL_RTTHREAD);
    int rc = RTThreadWait(pProcess->Thread, msTimeout, &rcThread);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("[PID %RU32]: Waiting for shutting down thread returned error rc=%Rrc\n",
                         pProcess->uPID, rc);
    }
    else
    {
        VBoxServiceVerbose(3, "[PID %RU32]: Thread reported exit code=%Rrc\n",
                           pProcess->uPID, rcThread);
        if (pRc)
            *pRc = rcThread;
    }

    return rc;
}


/**
 * Closes the stdin pipe of a guest process.
 *
 * @return  IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   phStdInW            The standard input pipe handle.
 */
static int gstcntlProcessCloseStdIn(RTPOLLSET hPollSet, PRTPIPE phStdInW)
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


static const char* gstcntlProcessPollHandleToString(uint32_t idPollHnd)
{
    switch (idPollHnd)
    {
        case VBOXSERVICECTRLPIPEID_UNKNOWN:
            return "unknown";
        case VBOXSERVICECTRLPIPEID_STDIN:
            return "stdin";
        case VBOXSERVICECTRLPIPEID_STDIN_WRITABLE:
            return "stdin_writable";
        case VBOXSERVICECTRLPIPEID_STDOUT:
            return "stdout";
        case VBOXSERVICECTRLPIPEID_STDERR:
            return "stderr";
        case VBOXSERVICECTRLPIPEID_IPC_NOTIFY:
            return "ipc_notify";
        default:
            break;
    }

    return "unknown";
}


/**
 * Handle an error event on standard input.
 *
 * @return  IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe handle.
 */
static int gstcntlProcessHandleStdInErrorEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW)
{
    NOREF(fPollEvt);

    return gstcntlProcessCloseStdIn(hPollSet, phStdInW);
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
static int gstcntlProcessHandleOutputError(RTPOLLSET hPollSet, uint32_t fPollEvt,
                                           PRTPIPE phPipeR, uint32_t idPollHnd)
{
    AssertPtrReturn(phPipeR, VERR_INVALID_POINTER);

#ifdef DEBUG
    VBoxServiceVerbose(4, "gstcntlProcessHandleOutputError: fPollEvt=0x%x, idPollHnd=%s\n",
                       fPollEvt, gstcntlProcessPollHandleToString(idPollHnd));
#endif

    /* Remove pipe from poll set. */
    int rc2 = RTPollSetRemove(hPollSet, idPollHnd);
    AssertMsg(RT_SUCCESS(rc2) || rc2 == VERR_POLL_HANDLE_ID_NOT_FOUND, ("%Rrc\n", rc2));

    bool fClosePipe = true; /* By default close the pipe. */

    /* Check if there's remaining data to read from the pipe. */
    size_t cbReadable;
    rc2 = RTPipeQueryReadable(*phPipeR, &cbReadable);
    if (   RT_SUCCESS(rc2)
        && cbReadable)
    {
        VBoxServiceVerbose(3, "gstcntlProcessHandleOutputError: idPollHnd=%s has %zu bytes left, vetoing close\n",
                           gstcntlProcessPollHandleToString(idPollHnd), cbReadable);

        /* Veto closing the pipe yet because there's still stuff to read
         * from the pipe. This can happen on UNIX-y systems where on
         * error/hangup there still can be data to be read out. */
        fClosePipe = false;
    }
    else
        VBoxServiceVerbose(3, "gstcntlProcessHandleOutputError: idPollHnd=%s will be closed\n",
                           gstcntlProcessPollHandleToString(idPollHnd));

    if (   *phPipeR != NIL_RTPIPE
        && fClosePipe)
    {
        rc2 = RTPipeClose(*phPipeR);
        AssertRC(rc2);
        *phPipeR = NIL_RTPIPE;
    }

    return VINF_SUCCESS;
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
static int gstcntlProcessHandleOutputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt,
                                           PRTPIPE phPipeR, uint32_t idPollHnd)
{
#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "GstCntlProcessHandleOutputEvent: fPollEvt=0x%x, idPollHnd=%s\n",
                       fPollEvt, gstcntlProcessPollHandleToString(idPollHnd));
#endif

    int rc = VINF_SUCCESS;

#ifdef DEBUG
    size_t cbReadable;
    rc = RTPipeQueryReadable(*phPipeR, &cbReadable);
    if (   RT_SUCCESS(rc)
        && cbReadable)
    {
        VBoxServiceVerbose(4, "gstcntlProcessHandleOutputEvent: cbReadable=%ld\n",
                           cbReadable);
    }
#endif

#if 0
    //if (fPollEvt & RTPOLL_EVT_READ)
    {
        size_t cbRead = 0;
        uint8_t byData[_64K];
        rc = RTPipeRead(*phPipeR,
                        byData, sizeof(byData), &cbRead);
        VBoxServiceVerbose(4, "GstCntlProcessHandleOutputEvent cbRead=%u, rc=%Rrc\n",
                           cbRead, rc);

        /* Make sure we go another poll round in case there was too much data
           for the buffer to hold. */
        fPollEvt &= RTPOLL_EVT_ERROR;
    }
#endif

    if (fPollEvt & RTPOLL_EVT_ERROR)
        rc = gstcntlProcessHandleOutputError(hPollSet, fPollEvt,
                                             phPipeR, idPollHnd);
    return rc;
}


/**
 * Completes the given request. After returning pRequest won't be valid
 * anymore!
 *
 * @return  IPRT status code.
 * @param   pRequest                Pointer to request to signal.
 * @param   rc                      rc to set request result to.
 */
static int gstcntlProcessRequestComplete(PVBOXSERVICECTRLREQUEST pRequest, int rc)
{
    AssertPtrReturn(pRequest, VERR_INVALID_POINTER);

    int rc2;
    if (!pRequest->fAsync)
    {
        /* Assign overall result. */
        pRequest->rc = rc;

#ifdef DEBUG_andy
        VBoxServiceVerbose(4, "Handled req=%RU32, CID=%RU32, rc=%Rrc, cbData=%RU32, pvData=%p\n",
                           pRequest->enmType, pRequest->uCID, pRequest->rc,
                           pRequest->cbData, pRequest->pvData);
#endif
        /* Signal waiters. */
        rc2 = RTSemEventMultiSignal(pRequest->Event);
        AssertRC(rc2);

        pRequest = NULL;
    }
    else
    {
#ifdef DEBUG_andy
        VBoxServiceVerbose(4, "Deleting async req=%RU32, CID=%RU32, rc=%Rrc, cbData=%RU32, pvData=%p\n",
                           pRequest->enmType, pRequest->uCID, pRequest->rc,
                           pRequest->cbData, pRequest->pvData);
#endif
        GstCntlProcessRequestFree(pRequest);
        rc2 = VINF_SUCCESS;
    }

    return rc2;
}


static int gstcntlProcessRequestHandle(PVBOXSERVICECTRLPROCESS pProcess, PVBOXSERVICECTRLREQUEST pRequest,
                                       RTPOLLSET hPollSet, uint32_t fPollEvt,
                                       PRTPIPE phStdInW, PRTPIPE phStdOutR, PRTPIPE phStdErrR)
{
    AssertPtrReturn(phStdInW, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdOutR, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdErrR, VERR_INVALID_POINTER);
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    AssertPtrReturn(pRequest, VERR_INVALID_POINTER);

    VBoxServiceVerbose(4, "[PID %RU32]: Handling pRequest=%p\n",
                       pProcess->uPID, pRequest);

    /* Drain the notification pipe. */
    uint8_t abBuf[8];
    size_t cbIgnore;
    int rc = RTPipeRead(pProcess->hNotificationPipeR, abBuf, sizeof(abBuf), &cbIgnore);
    if (RT_FAILURE(rc))
        VBoxServiceError("Draining IPC notification pipe failed with rc=%Rrc\n", rc);

    bool fDefer = false; /* Whether the request completion should be deferred or not. */
    int rcReq = VINF_SUCCESS; /* Actual request result. */

    switch (pRequest->enmType)
    {
        case VBOXSERVICECTRLREQUEST_PROC_STDIN:
        case VBOXSERVICECTRLREQUEST_PROC_STDIN_EOF:
        {
            size_t cbWritten = 0;
            if (pRequest->cbData)
            {
                AssertPtrReturn(pRequest->pvData, VERR_INVALID_POINTER);
                if (*phStdInW != NIL_RTPIPE)
                {
                    rcReq = RTPipeWrite(*phStdInW,
                                        pRequest->pvData, pRequest->cbData, &cbWritten);
                }
                else
                    rcReq = VINF_EOF;
            }

            /*
             * If this is the last write + we have really have written all data
             * we need to close the stdin pipe on our end and remove it from
             * the poll set.
             */
            if (   pRequest->enmType == VBOXSERVICECTRLREQUEST_PROC_STDIN_EOF
                && pRequest->cbData  == cbWritten)
            {
                rc = gstcntlProcessCloseStdIn(hPollSet, phStdInW);
            }

            /* Report back actual data written (if any). */
            pRequest->cbData = cbWritten;
            break;
        }

        case VBOXSERVICECTRLREQUEST_PROC_STDOUT:
        case VBOXSERVICECTRLREQUEST_PROC_STDERR:
        {
            AssertPtrReturn(pRequest->pvData, VERR_INVALID_POINTER);
            AssertReturn(pRequest->cbData, VERR_INVALID_PARAMETER);

            PRTPIPE pPipeR = pRequest->enmType == VBOXSERVICECTRLREQUEST_PROC_STDERR
                           ? phStdErrR : phStdOutR;
            AssertPtr(pPipeR);

            size_t cbRead = 0;
            if (*pPipeR != NIL_RTPIPE)
            {
                rcReq = RTPipeRead(*pPipeR,
                                   pRequest->pvData, pRequest->cbData, &cbRead);
                if (RT_FAILURE(rcReq))
                {
                    RTPollSetRemove(hPollSet, pRequest->enmType == VBOXSERVICECTRLREQUEST_PROC_STDERR
                                              ? VBOXSERVICECTRLPIPEID_STDERR : VBOXSERVICECTRLPIPEID_STDOUT);
                    RTPipeClose(*pPipeR);
                    *pPipeR = NIL_RTPIPE;
                    if (rcReq == VERR_BROKEN_PIPE)
                        rcReq = VINF_EOF;
                }
            }
            else
                rcReq = VINF_EOF;

            /* Report back actual data read (if any). */
            pRequest->cbData = cbRead;
            break;
        }

        case VBOXSERVICECTRLREQUEST_PROC_TERM:
            pProcess->fShutdown = true;
            fDefer = true;
            break;

        default:
            rcReq = VERR_NOT_IMPLEMENTED;
            break;
    }

    if (   RT_FAILURE(rc)
        || !fDefer)
    {
        rc = gstcntlProcessRequestComplete(pRequest,
                                           RT_SUCCESS(rc) ? rcReq : rc);
    }
    else /* Completing the request defered. */
        rc = VINF_AIO_TASK_PENDING; /** @todo Find an own rc! */

    return rc;
}


/**
 * Execution loop which runs in a dedicated per-started-process thread and
 * handles all pipe input/output and signalling stuff.
 *
 * @return  IPRT status code.
 * @param   pProcess                    The guest process to handle.
 * @param   hProcess                    The actual process handle.
 * @param   cMsTimeout                  Time limit (in ms) of the process' life time.
 * @param   hPollSet                    The poll set to use.
 * @param   hStdInW                     Handle to the process' stdin write end.
 * @param   hStdOutR                    Handle to the process' stdout read end.
 * @param   hStdErrR                    Handle to the process' stderr read end.
 */
static int gstcntlProcessProcLoop(PVBOXSERVICECTRLPROCESS pProcess,
                                  RTPROCESS hProcess, RTPOLLSET hPollSet,
                                  PRTPIPE phStdInW, PRTPIPE phStdOutR, PRTPIPE phStdErrR)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    AssertPtrReturn(phStdInW, VERR_INVALID_PARAMETER);
    /* Rest is optional. */

    int                         rc;
    int                         rc2;
    uint64_t const              uMsStart            = RTTimeMilliTS();
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
    rc = gstcntlProcessAssignPID(pProcess, hProcess);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Unable to assign PID=%u, to new thread, rc=%Rrc\n",
                         hProcess, rc);
        return rc;
    }

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    VBoxServiceVerbose(2, "[PID %RU32]: Process \"%s\" started, CID=%u, User=%s, cMsTimeout=%RU32\n",
                       pProcess->uPID, pProcess->StartupInfo.szCmd, pProcess->uContextID,
                       pProcess->StartupInfo.szUser, pProcess->StartupInfo.uTimeLimitMS);
    VBGLR3GUESTCTRLCMDCTX ctxStart = { pProcess->uClientID, pProcess->uContextID };
    rc = VbglR3GuestCtrlProcCbStatus(&ctxStart,
                                     pProcess->uPID, PROC_STS_STARTED, 0 /* u32Flags */,
                                     NULL /* pvData */, 0 /* cbData */);

    /*
     * Process input, output, the test pipe and client requests.
     */
    while (   RT_SUCCESS(rc)
           && RT_UNLIKELY(!pProcess->fShutdown))
    {
        /*
         * Wait/Process all pending events.
         */
        uint32_t idPollHnd;
        uint32_t fPollEvt;
        rc2 = RTPollNoResume(hPollSet, cMsPollCur, &fPollEvt, &idPollHnd);
        if (pProcess->fShutdown)
            continue;

        cMsPollCur = 0; /* No rest until we've checked everything. */

        if (RT_SUCCESS(rc2))
        {
            switch (idPollHnd)
            {
                case VBOXSERVICECTRLPIPEID_STDIN:
                    rc = gstcntlProcessHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW);
                    break;

                case VBOXSERVICECTRLPIPEID_STDOUT:
                    rc = gstcntlProcessHandleOutputEvent(hPollSet, fPollEvt,
                                                         phStdOutR, idPollHnd);
                    break;

                case VBOXSERVICECTRLPIPEID_STDERR:
                    rc = gstcntlProcessHandleOutputEvent(hPollSet, fPollEvt,
                                                         phStdErrR, idPollHnd);
                    break;

                case VBOXSERVICECTRLPIPEID_IPC_NOTIFY:
#ifdef DEBUG_andy
                    VBoxServiceVerbose(4, "[PID %RU32]: IPC notify\n", pProcess->uPID);
#endif
                    rc = gstcntlProcessLock(pProcess);
                    if (RT_SUCCESS(rc))
                    {
                        /** @todo Implement request queue. */
                        rc = gstcntlProcessRequestHandle(pProcess, pProcess->pRequest,
                                                         hPollSet, fPollEvt,
                                                         phStdInW, phStdOutR, phStdErrR);
                        if (rc != VINF_AIO_TASK_PENDING)
                        {
                            pProcess->pRequest = NULL;
                        }
                        else
                            VBoxServiceVerbose(4, "[PID %RU32]: pRequest=%p will be handled deferred\n",
                                               pProcess->uPID, pProcess->pRequest);

                        rc2 = gstcntlProcessUnlock(pProcess);
                        AssertRC(rc2);
                    }
                    break;

                default:
                    AssertMsgFailed(("Unknown idPollHnd=%RU32\n", idPollHnd));
                    break;
            }

            if (RT_FAILURE(rc) || rc == VINF_EOF)
                break; /* Abort command, or client dead or something. */
        }
#ifdef DEBUG_andy
        VBoxServiceVerbose(4, "[PID %RU32]: Polling done, pollRc=%Rrc, pollCnt=%RU32, idPollHnd=%s, rc=%Rrc, fProcessAlive=%RTbool, fShutdown=%RTbool\n",
                           pProcess->uPID, rc2, RTPollSetGetCount(hPollSet), gstcntlProcessPollHandleToString(idPollHnd), rc, fProcessAlive, pProcess->fShutdown);
        VBoxServiceVerbose(4, "[PID %RU32]: stdOut=%s, stdErrR=%s\n",
                           pProcess->uPID,
                           *phStdOutR == NIL_RTPIPE ? "closed" : "open",
                           *phStdErrR == NIL_RTPIPE ? "closed" : "open");
#endif
        if (RT_UNLIKELY(pProcess->fShutdown))
            break; /* We were asked to shutdown. */

        /*
         * Check for process death.
         */
        if (fProcessAlive)
        {
            rc2 = RTProcWaitNoResume(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
#ifdef DEBUG_andy
            VBoxServiceVerbose(4, "[PID %RU32]: RTProcWaitNoResume=%Rrc\n",
                               pProcess->uPID, rc2);
#endif
            if (RT_SUCCESS_NP(rc2))
            {
                fProcessAlive = false;
                /* Note: Don't bail out here yet. First check in the next block below
                 *       if all needed pipe outputs have been consumed. */
            }
            else
            {
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
        }

        /*
         * If the process has terminated and all output has been consumed,
         * we should be heading out.
         */
        if (!fProcessAlive)
        {
            if (   fProcessTimedOut
                || (   *phStdOutR == NIL_RTPIPE
                    && *phStdErrR == NIL_RTPIPE)
               )
            {
                break;
            }
        }

        /*
         * Check for timed out, killing the process.
         */
        uint32_t cMilliesLeft = RT_INDEFINITE_WAIT;
        if (   pProcess->StartupInfo.uTimeLimitMS != RT_INDEFINITE_WAIT
            && pProcess->StartupInfo.uTimeLimitMS != 0)
        {
            uint64_t u64Now = RTTimeMilliTS();
            uint64_t cMsElapsed = u64Now - uMsStart;
            if (cMsElapsed >= pProcess->StartupInfo.uTimeLimitMS)
            {
                VBoxServiceVerbose(3, "[PID %RU32]: Timed out (%RU64ms elapsed > %RU64ms timeout), killing ...\n",
                                   pProcess->uPID, cMsElapsed, pProcess->StartupInfo.uTimeLimitMS);

                fProcessTimedOut = true;
                if (    MsProcessKilled == UINT64_MAX
                    ||  u64Now - MsProcessKilled > 1000)
                {
                    if (u64Now - MsProcessKilled > 20*60*1000)
                        break; /* Give up after 20 mins. */
                    rc2 = RTProcTerminate(hProcess);
                    VBoxServiceVerbose(3, "[PID %RU32]: Killing process resulted in rc=%Rrc\n",
                                       pProcess->uPID, rc2);
                    MsProcessKilled = u64Now;
                    continue;
                }
                cMilliesLeft = 10000;
            }
            else
                cMilliesLeft = pProcess->StartupInfo.uTimeLimitMS - (uint32_t)cMsElapsed;
        }

        /* Reset the polling interval since we've done all pending work. */
        cMsPollCur = fProcessAlive
                   ? cMsPollBase
                   : RT_MS_1MIN;
        if (cMilliesLeft < cMsPollCur)
            cMsPollCur = cMilliesLeft;
    }

    VBoxServiceVerbose(3, "[PID %RU32]: Loop ended: fShutdown=%RTbool, fProcessAlive=%RTbool, fProcessTimedOut=%RTbool, MsProcessKilled=%RU32\n",
                       pProcess->uPID, pProcess->fShutdown, fProcessAlive, fProcessTimedOut, MsProcessKilled, MsProcessKilled);
    VBoxServiceVerbose(3, "[PID %RU32]: *phStdOutR=%s, *phStdErrR=%s\n",
                       pProcess->uPID, *phStdOutR == NIL_RTPIPE ? "closed" : "open", *phStdErrR == NIL_RTPIPE ? "closed" : "open");

    /* Signal that this thread is in progress of shutting down. */
    ASMAtomicXchgBool(&pProcess->fShutdown, true);

    /*
     * Try kill the process if it's still alive at this point.
     */
    if (fProcessAlive)
    {
        if (MsProcessKilled == UINT64_MAX)
        {
            VBoxServiceVerbose(2, "[PID %RU32]: Is still alive and not killed yet\n",
                               pProcess->uPID);

            MsProcessKilled = RTTimeMilliTS();
            rc2 = RTProcTerminate(hProcess);
            if (RT_FAILURE(rc))
                VBoxServiceError("PID %RU32]: Killing process failed with rc=%Rrc\n",
                                 pProcess->uPID, rc);
            RTThreadSleep(500);
        }

        for (size_t i = 0; i < 10; i++)
        {
            VBoxServiceVerbose(4, "[PID %RU32]: Kill attempt %d/10: Waiting to exit ...\n",
                               pProcess->uPID, i + 1);
            rc2 = RTProcWait(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS(rc2))
            {
                VBoxServiceVerbose(4, "[PID %RU32]: Kill attempt %d/10: Exited\n",
                                   pProcess->uPID, i + 1);
                fProcessAlive = false;
                break;
            }
            if (i >= 5)
            {
                VBoxServiceVerbose(4, "[PID %RU32]: Kill attempt %d/10: Trying to terminate ...\n",
                                   pProcess->uPID, i + 1);
                rc2 = RTProcTerminate(hProcess);
                if (RT_FAILURE(rc))
                    VBoxServiceError("PID %RU32]: Killing process failed with rc=%Rrc\n",
                                     pProcess->uPID, rc);
            }
            RTThreadSleep(i >= 5 ? 2000 : 500);
        }

        if (fProcessAlive)
            VBoxServiceError("[PID %RU32]: Could not be killed\n", pProcess->uPID);
    }

    rc2 = gstcntlProcessLock(pProcess);
    if (RT_SUCCESS(rc2))
    {
        if (pProcess->pRequest)
        {
            switch (pProcess->pRequest->enmType)
            {
                /* Handle deferred termination request. */
                case VBOXSERVICECTRLREQUEST_PROC_TERM:
                    rc2 = gstcntlProcessRequestComplete(pProcess->pRequest,
                                                        fProcessAlive ? VINF_SUCCESS : VERR_PROCESS_RUNNING);
                    AssertRC(rc2);
                    break;

                /* Cancel all others.
                 ** @todo Clear request queue as soon as it's implemented. */
                default:
                    rc2 = gstcntlProcessRequestCancel(pProcess->pRequest);
                    AssertRC(rc2);
                    break;
            }

            pProcess->pRequest = NULL;
        }
#ifdef DEBUG
        else
            VBoxServiceVerbose(3, "[PID %RU32]: No pending request found\n", pProcess->uPID);
#endif
        rc2 = gstcntlProcessUnlock(pProcess);
        AssertRC(rc2);
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
            VBoxServiceVerbose(3, "[PID %RU32]: Timed out and got killed\n",
                               pProcess->uPID);
            uStatus = PROC_STS_TOK;
        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "[PID %RU32]: Timed out and did *not* get killed\n",
                               pProcess->uPID);
            uStatus = PROC_STS_TOA;
        }
        else if (pProcess->fShutdown && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {
            VBoxServiceVerbose(3, "[PID %RU32]: Got terminated because system/service is about to shutdown\n",
                               pProcess->uPID);
            uStatus = PROC_STS_DWN; /* Service is stopping, process was killed. */
            uFlags = pProcess->StartupInfo.uFlags; /* Return handed-in execution flags back to the host. */
        }
        else if (fProcessAlive)
        {
            VBoxServiceError("[PID %RU32]: Is alive when it should not!\n",
                             pProcess->uPID);
        }
        else if (MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceError("[PID %RU32]: Has been killed when it should not!\n",
                             pProcess->uPID);
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            VBoxServiceVerbose(3, "[PID %RU32]: Ended with RTPROCEXITREASON_NORMAL (Exit code: %u)\n",
                               pProcess->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TEN;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {
            VBoxServiceVerbose(3, "[PID %RU32]: Ended with RTPROCEXITREASON_SIGNAL (Signal: %u)\n",
                               pProcess->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TES;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {
            /* ProcessStatus.iStatus will be undefined. */
            VBoxServiceVerbose(3, "[PID %RU32]: Ended with RTPROCEXITREASON_ABEND\n",
                               pProcess->uPID);

            uStatus = PROC_STS_TEA;
            uFlags = ProcessStatus.iStatus;
        }
        else
            VBoxServiceVerbose(1, "[PID %RU32]: Handling process status %u not implemented\n",
                               pProcess->uPID, ProcessStatus.enmReason);

        VBoxServiceVerbose(2, "[PID %RU32]: Ended, ClientID=%u, CID=%u, Status=%u, Flags=0x%x\n",
                           pProcess->uPID, pProcess->uClientID, pProcess->uContextID, uStatus, uFlags);

        if (!(pProcess->StartupInfo.uFlags & EXECUTEPROCESSFLAG_WAIT_START))
        {
            VBGLR3GUESTCTRLCMDCTX ctxEnd = { pProcess->uClientID, pProcess->uContextID };
            rc2 = VbglR3GuestCtrlProcCbStatus(&ctxEnd,
                                              pProcess->uPID, uStatus, uFlags,
                                              NULL /* pvData */, 0 /* cbData */);
            if (RT_FAILURE(rc2))
                VBoxServiceError("[PID %RU32]: Error reporting final status to host; rc=%Rrc\n",
                                 pProcess->uPID, rc2);
        }
        else
            VBoxServiceVerbose(3, "[PID %RU32]: Was started detached, no final status sent to host\n",
                               pProcess->uPID);
    }

    VBoxServiceVerbose(3, "[PID %RU32]: Process loop returned with rc=%Rrc\n",
                       pProcess->uPID, rc);
    return rc;
}


/**
 * Initializes a pipe's handle and pipe object.
 *
 * @return  IPRT status code.
 * @param   ph                      The pipe's handle to initialize.
 * @param   phPipe                  The pipe's object to initialize.
 */
static int gstcntlProcessInitPipe(PRTHANDLE ph, PRTPIPE phPipe)
{
    AssertPtrReturn(ph, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phPipe, VERR_INVALID_PARAMETER);

    ph->enmType = RTHANDLETYPE_PIPE;
    ph->u.hPipe = NIL_RTPIPE;
    *phPipe     = NIL_RTPIPE;

    return VINF_SUCCESS;
}


/**
 * Allocates a guest thread request with the specified request data.
 *
 * @return  IPRT status code.
 * @param   ppReq                   Pointer that will receive the newly allocated request.
 *                                  Must be freed later with GstCntlProcessRequestFree().
 * @param   enmType                 Request type.
 * @param   pvData                  Payload data, based on request type.
 * @param   cbData                  Size of payload data (in bytes).
 * @param   uCID                    Context ID to which this request belongs to.
 */
int GstCntlProcessRequestAllocEx(PVBOXSERVICECTRLREQUEST   *ppReq,
                                 VBOXSERVICECTRLREQUESTTYPE enmType,
                                 void                      *pvData,
                                 size_t                     cbData,
                                 uint32_t                   uCID)
{
    AssertPtrReturn(ppReq, VERR_INVALID_POINTER);

    PVBOXSERVICECTRLREQUEST pReq =
        (PVBOXSERVICECTRLREQUEST)RTMemAlloc(sizeof(VBOXSERVICECTRLREQUEST));
    AssertPtrReturn(pReq, VERR_NO_MEMORY);

    RT_ZERO(*pReq);
    pReq->fAsync  = false;
    pReq->enmType = enmType;
    pReq->uCID    = uCID;
    pReq->cbData  = cbData;
    pReq->pvData  = pvData;

    /* Set request result to some defined state in case
     * it got cancelled. */
    pReq->rc      = VERR_CANCELLED;

    int rc = RTSemEventMultiCreate(&pReq->Event);
    AssertRC(rc);

    if (RT_SUCCESS(rc))
    {
        *ppReq = pReq;
        return VINF_SUCCESS;
    }

    RTMemFree(pReq);
    return rc;
}


/**
 * Allocates a guest thread request with the specified request data.
 *
 * @return  IPRT status code.
 * @param   ppReq                   Pointer that will receive the newly allocated request.
 *                                  Must be freed later with GstCntlProcessRequestFree().
 * @param   enmType                 Request type.
 */
int GstCntlProcessRequestAlloc(PVBOXSERVICECTRLREQUEST *ppReq,
                               VBOXSERVICECTRLREQUESTTYPE enmType)
{
    return GstCntlProcessRequestAllocEx(ppReq, enmType,
                                        NULL /* pvData */, 0 /* cbData */,
                                        0 /* ContextID */);
}


/**
 * Cancels a previously fired off guest process request.
 * Note: Caller is responsible for locking!
 *
 * @return  IPRT status code.
 * @param   pReq                    Request to cancel.
 */
static int gstcntlProcessRequestCancel(PVBOXSERVICECTRLREQUEST pReq)
{
    if (!pReq) /* Silently skip non-initialized requests. */
        return VINF_SUCCESS;

    VBoxServiceVerbose(4, "Cancelling outstanding request=0x%p\n", pReq);

    return RTSemEventMultiSignal(pReq->Event);
}


/**
 * Frees a formerly allocated guest thread request.
 *
 * @return  IPRT status code.
 * @param   pReq                    Request to free.
 */
void GstCntlProcessRequestFree(PVBOXSERVICECTRLREQUEST pReq)
{
    AssertPtrReturnVoid(pReq);

    VBoxServiceVerbose(4, "Freeing request=0x%p (event=%RTsem)\n",
                       pReq, &pReq->Event);

    int rc = RTSemEventMultiDestroy(pReq->Event);
    AssertRC(rc);

    RTMemFree(pReq);
    pReq = NULL;
}


/**
 * Waits for a guest thread's event to get triggered.
 *
 * @return  IPRT status code.
 * @param   pReq                    Request to wait for.
 */
int GstCntlProcessRequestWait(PVBOXSERVICECTRLREQUEST pReq)
{
    AssertPtrReturn(pReq, VERR_INVALID_POINTER);

    /* Wait on the request to get completed (or we are asked to abort/shutdown). */
    int rc = RTSemEventMultiWait(pReq->Event, RT_INDEFINITE_WAIT);
    if (RT_SUCCESS(rc))
    {
        VBoxServiceVerbose(4, "Performed request with rc=%Rrc, cbData=%RU32\n",
                           pReq->rc, pReq->cbData);

        /* Give back overall request result. */
        rc = pReq->rc;
    }
    else
        VBoxServiceError("Waiting for request failed, rc=%Rrc\n", rc);

    return rc;
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
static int gstcntlProcessSetupPipe(const char *pszHowTo, int fd,
                                   PRTHANDLE ph, PRTHANDLE *pph, PRTPIPE phPipe)
{
    AssertPtrReturn(ph, VERR_INVALID_POINTER);
    AssertPtrReturn(pph, VERR_INVALID_POINTER);
    AssertPtrReturn(phPipe, VERR_INVALID_POINTER);

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
        rc = VINF_SUCCESS; /* Same as parent (us). */

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
static int gstcntlProcessMakeFullPath(const char *pszPath, char *pszExpanded, size_t cbExpanded)
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
    VBoxServiceVerbose(3, "VBoxServiceControlExecMakeFullPath: %s -> %s\n",
                       pszPath, pszExpanded);
#endif
    return rc;
}


/**
 * Resolves the full path of a specified executable name. This function also
 * resolves internal VBoxService tools to its appropriate executable path + name if
 * VBOXSERVICE_NAME is specified as pszFileName.
 *
 * @return  IPRT status code.
 * @param   pszFileName                 File name to resolve.
 * @param   pszResolved                 Pointer to a string where the resolved file name will be stored.
 * @param   cbResolved                  Size (in bytes) of resolved file name string.
 */
static int gstcntlProcessResolveExecutable(const char *pszFileName,
                                           char *pszResolved, size_t cbResolved)
{
    AssertPtrReturn(pszFileName, VERR_INVALID_POINTER);
    AssertPtrReturn(pszResolved, VERR_INVALID_POINTER);
    AssertReturn(cbResolved, VERR_INVALID_PARAMETER);

    int rc = VINF_SUCCESS;

    char szPathToResolve[RTPATH_MAX];
    if (    (g_pszProgName && (RTStrICmp(pszFileName, g_pszProgName) == 0))
        || !RTStrICmp(pszFileName, VBOXSERVICE_NAME))
    {
        /* Resolve executable name of this process. */
        if (!RTProcGetExecutablePath(szPathToResolve, sizeof(szPathToResolve)))
            rc = VERR_FILE_NOT_FOUND;
    }
    else
    {
        /* Take the raw argument to resolve. */
        rc = RTStrCopy(szPathToResolve, sizeof(szPathToResolve), pszFileName);
    }

    if (RT_SUCCESS(rc))
    {
        rc = gstcntlProcessMakeFullPath(szPathToResolve, pszResolved, cbResolved);
        if (RT_SUCCESS(rc))
            VBoxServiceVerbose(3, "Looked up executable: %s -> %s\n",
                               pszFileName, pszResolved);
    }

    if (RT_FAILURE(rc))
        VBoxServiceError("Failed to lookup executable \"%s\" with rc=%Rrc\n",
                         pszFileName, rc);
    return rc;
}


/**
 * Constructs the argv command line by resolving environment variables
 * and relative paths.
 *
 * @return IPRT status code.
 * @param  pszArgv0         First argument (argv0), either original or modified version.  Optional.
 * @param  papszArgs        Original argv command line from the host, starting at argv[1].
 * @param  ppapszArgv       Pointer to a pointer with the new argv command line.
 *                          Needs to be freed with RTGetOptArgvFree.
 */
static int gstcntlProcessAllocateArgv(const char *pszArgv0,
                                      const char * const *papszArgs,
                                      bool fExpandArgs, char ***ppapszArgv)
{
    AssertPtrReturn(ppapszArgv, VERR_INVALID_POINTER);

    VBoxServiceVerbose(3, "GstCntlProcessPrepareArgv: pszArgv0=%p, papszArgs=%p, fExpandArgs=%RTbool, ppapszArgv=%p\n",
                       pszArgv0, papszArgs, fExpandArgs, ppapszArgv);

    int rc = VINF_SUCCESS;
    uint32_t cArgs;
    for (cArgs = 0; papszArgs[cArgs]; cArgs++)
    {
        if (cArgs >= UINT32_MAX - 2)
            return VERR_BUFFER_OVERFLOW;
    }

    /* Allocate new argv vector (adding + 2 for argv0 + termination). */
    size_t cbSize = (cArgs + 2) * sizeof(char*);
    char **papszNewArgv = (char**)RTMemAlloc(cbSize);
    if (!papszNewArgv)
        return VERR_NO_MEMORY;

#ifdef DEBUG
    VBoxServiceVerbose(3, "GstCntlProcessAllocateArgv: cbSize=%RU32, cArgs=%RU32\n",
                       cbSize, cArgs);
#endif

    size_t i = 0; /* Keep the argument counter in scope for cleaning up on failure. */

    rc = RTStrDupEx(&papszNewArgv[0], pszArgv0);
    if (RT_SUCCESS(rc))
    {
        for (; i < cArgs; i++)
        {
            char *pszArg;
#if 0 /* Arguments expansion -- untested. */
            if (fExpandArgs)
            {
                /* According to MSDN the limit on older Windows version is 32K, whereas
                 * Vista+ there are no limits anymore. We still stick to 4K. */
                char szExpanded[_4K];
# ifdef RT_OS_WINDOWS
                if (!ExpandEnvironmentStrings(papszArgs[i], szExpanded, sizeof(szExpanded)))
                    rc = RTErrConvertFromWin32(GetLastError());
# else
                /* No expansion for non-Windows yet. */
                rc = RTStrCopy(papszArgs[i], sizeof(szExpanded), szExpanded);
# endif
                if (RT_SUCCESS(rc))
                    rc = RTStrDupEx(&pszArg, szExpanded);
            }
            else
#endif
                rc = RTStrDupEx(&pszArg, papszArgs[i]);

            if (RT_FAILURE(rc))
                break;

            papszNewArgv[i + 1] = pszArg;
        }

        if (RT_SUCCESS(rc))
        {
            /* Terminate array. */
            papszNewArgv[cArgs + 1] = NULL;

            *ppapszArgv = papszNewArgv;
        }
    }

    if (RT_FAILURE(rc))
    {
        for (i; i > 0; i--)
            RTStrFree(papszNewArgv[i]);
        RTMemFree(papszNewArgv);
    }

    return rc;
}


/**
 * Assigns a valid PID to a guest control thread and also checks if there already was
 * another (stale) guest process which was using that PID before and destroys it.
 *
 * @return  IPRT status code.
 * @param   pThread        Thread to assign PID to.
 * @param   uPID           PID to assign to the specified guest control execution thread.
 */
int gstcntlProcessAssignPID(PVBOXSERVICECTRLPROCESS pThread, uint32_t uPID)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);
    AssertReturn(uPID, VERR_INVALID_PARAMETER);

    AssertPtr(pThread->pSession);
    int rc = RTCritSectEnter(&pThread->pSession->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* Search old threads using the desired PID and shut them down completely -- it's
         * not used anymore. */
        PVBOXSERVICECTRLPROCESS pThreadCur;
        bool fTryAgain = false;
        do
        {
            RTListForEach(&pThread->pSession->lstProcessesActive, pThreadCur, VBOXSERVICECTRLPROCESS, Node)
            {
                if (pThreadCur->uPID == uPID)
                {
                    Assert(pThreadCur != pThread); /* can't happen */
                    uint32_t uTriedPID = uPID;
                    uPID += 391939;
                    VBoxServiceVerbose(2, "PID %RU32 was used before, trying again with %u ...\n",
                                       uTriedPID, uPID);
                    fTryAgain = true;
                    break;
                }
            }
        } while (fTryAgain);

        /* Assign PID to current thread. */
        pThread->uPID = uPID;

        rc = RTCritSectLeave(&pThread->pSession->CritSect);
        AssertRC(rc);
    }

    return rc;
}


void gstcntlProcessFreeArgv(char **papszArgv)
{
    if (papszArgv)
    {
        size_t i = 0;
        while (papszArgv[i])
            RTStrFree(papszArgv[i++]);
        RTMemFree(papszArgv);
    }
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
static int gstcntlProcessCreateProcess(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                                       PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                                       const char *pszPassword, PRTPROCESS phProcess)
{
    AssertPtrReturn(pszExec, VERR_INVALID_PARAMETER);
    AssertPtrReturn(papszArgs, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phProcess, VERR_INVALID_PARAMETER);

    int  rc = VINF_SUCCESS;
    char szExecExp[RTPATH_MAX];

    /* Do we need to expand environment variables in arguments? */
    bool fExpandArgs = (fFlags & EXECUTEPROCESSFLAG_EXPAND_ARGUMENTS) ? true  : false;

#ifdef RT_OS_WINDOWS
    /*
     * If sysprep should be executed do this in the context of VBoxService, which
     * (usually, if started by SCM) has administrator rights. Because of that a UI
     * won't be shown (doesn't have a desktop).
     */
    if (!RTStrICmp(pszExec, "sysprep"))
    {
        /* Use a predefined sysprep path as default. */
        char szSysprepCmd[RTPATH_MAX] = "C:\\sysprep\\sysprep.exe";
        /** @todo Check digital signature of file above before executing it? */

        /*
         * On Windows Vista (and up) sysprep is located in "system32\\Sysprep\\sysprep.exe",
         * so detect the OS and use a different path.
         */
        OSVERSIONINFOEX OSInfoEx;
        RT_ZERO(OSInfoEx);
        OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
        BOOL fRet = GetVersionEx((LPOSVERSIONINFO) &OSInfoEx);
        if (    fRet
            &&  OSInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT
            &&  OSInfoEx.dwMajorVersion >= 6 /* Vista or later */)
        {
            rc = RTEnvGetEx(RTENV_DEFAULT, "windir", szSysprepCmd, sizeof(szSysprepCmd), NULL);
#ifndef RT_ARCH_AMD64
            /* Don't execute 64-bit sysprep from a 32-bit service host! */
            char szSysWow64[RTPATH_MAX];
            if (RTStrPrintf(szSysWow64, sizeof(szSysWow64), "%s", szSysprepCmd))
            {
                rc = RTPathAppend(szSysWow64, sizeof(szSysWow64), "SysWow64");
                AssertRC(rc);
            }
            if (   RT_SUCCESS(rc)
                && RTPathExists(szSysWow64))
                VBoxServiceVerbose(0, "Warning: This service is 32-bit; could not execute sysprep on 64-bit OS!\n");
#endif
            if (RT_SUCCESS(rc))
                rc = RTPathAppend(szSysprepCmd, sizeof(szSysprepCmd), "system32\\Sysprep\\sysprep.exe");
            if (RT_SUCCESS(rc))
                RTPathChangeToDosSlashes(szSysprepCmd, false /* No forcing necessary */);

            if (RT_FAILURE(rc))
                VBoxServiceError("Failed to detect sysrep location, rc=%Rrc\n", rc);
        }
        else if (!fRet)
            VBoxServiceError("Failed to retrieve OS information, last error=%ld\n", GetLastError());

        VBoxServiceVerbose(3, "Sysprep executable is: %s\n", szSysprepCmd);

        if (RT_SUCCESS(rc))
        {
            char **papszArgsExp;
            rc = gstcntlProcessAllocateArgv(szSysprepCmd /* argv0 */, papszArgs,
                                            fExpandArgs, &papszArgsExp);
            if (RT_SUCCESS(rc))
            {
                /* As we don't specify credentials for the sysprep process, it will
                 * run under behalf of the account VBoxService was started under, most
                 * likely local system. */
                rc = RTProcCreateEx(szSysprepCmd, papszArgsExp, hEnv, 0 /* fFlags */,
                                    phStdIn, phStdOut, phStdErr, NULL /* pszAsUser */,
                                    NULL /* pszPassword */, phProcess);
                gstcntlProcessFreeArgv(papszArgsExp);
            }
        }

        if (RT_FAILURE(rc))
            VBoxServiceVerbose(3, "Starting sysprep returned rc=%Rrc\n", rc);

        return rc;
    }
#endif /* RT_OS_WINDOWS */

#ifdef VBOXSERVICE_TOOLBOX
    if (RTStrStr(pszExec, "vbox_") == pszExec)
    {
        /* We want to use the internal toolbox (all internal
         * tools are starting with "vbox_" (e.g. "vbox_cat"). */
        rc = gstcntlProcessResolveExecutable(VBOXSERVICE_NAME, szExecExp, sizeof(szExecExp));
    }
    else
    {
#endif
        /*
         * Do the environment variables expansion on executable and arguments.
         */
        rc = gstcntlProcessResolveExecutable(pszExec, szExecExp, sizeof(szExecExp));
#ifdef VBOXSERVICE_TOOLBOX
    }
#endif
    if (RT_SUCCESS(rc))
    {
        char **papszArgsExp;
        rc = gstcntlProcessAllocateArgv(pszExec /* Always use the unmodified executable name as argv0. */,
                                        papszArgs /* Append the rest of the argument vector (if any). */,
                                        fExpandArgs, &papszArgsExp);
        if (RT_FAILURE(rc))
        {
            /* Don't print any arguments -- may contain passwords or other sensible data! */
            VBoxServiceError("Could not prepare arguments, rc=%Rrc\n", rc);
        }
        else
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
            if (pszAsUser && *pszAsUser)
                uProcFlags |= RTPROC_FLAGS_SERVICE;
#ifdef DEBUG
            VBoxServiceVerbose(3, "Command: %s\n", szExecExp);
            for (size_t i = 0; papszArgsExp[i]; i++)
                VBoxServiceVerbose(3, "\targv[%ld]: %s\n", i, papszArgsExp[i]);
#endif
            VBoxServiceVerbose(3, "Starting process \"%s\" ...\n", szExecExp);

            /* Do normal execution. */
            rc = RTProcCreateEx(szExecExp, papszArgsExp, hEnv, uProcFlags,
                                phStdIn, phStdOut, phStdErr,
                                pszAsUser   && *pszAsUser   ? pszAsUser   : NULL,
                                pszPassword && *pszPassword ? pszPassword : NULL,
                                phProcess);

            VBoxServiceVerbose(3, "Starting process \"%s\" returned rc=%Rrc\n",
                               szExecExp, rc);

            gstcntlProcessFreeArgv(papszArgsExp);
        }
    }
    return rc;
}

/**
 * The actual worker routine (loop) for a started guest process.
 *
 * @return  IPRT status code.
 * @param   PVBOXSERVICECTRLPROCESS         Guest process.
 */
static int gstcntlProcessProcessWorker(PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    VBoxServiceVerbose(3, "Thread of process pThread=0x%p = \"%s\" started\n",
                       pProcess, pProcess->StartupInfo.szCmd);

    int rc = GstCntlSessionListSet(pProcess->pSession,
                                   pProcess, VBOXSERVICECTRLTHREADLIST_RUNNING);
    AssertRC(rc);

    rc = VbglR3GuestCtrlConnect(&pProcess->uClientID);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Thread failed to connect to the guest control service, aborted! Error: %Rrc\n", rc);
        RTThreadUserSignal(RTThreadSelf());
        return rc;
    }
    VBoxServiceVerbose(3, "Guest process \"%s\" got client ID=%u, flags=0x%x\n",
                       pProcess->StartupInfo.szCmd, pProcess->uClientID, pProcess->StartupInfo.uFlags);

    bool fSignalled = false; /* Indicator whether we signalled the thread user event already. */

    /*
     * Prepare argument list.
     */
    char **papszArgs;
    uint32_t uNumArgs = 0; /* Initialize in case of RTGetOptArgvFromString() is failing ... */
    rc = RTGetOptArgvFromString(&papszArgs, (int*)&uNumArgs,
                                (pProcess->StartupInfo.uNumArgs > 0) ? pProcess->StartupInfo.szArgs : "", NULL);
    /* Did we get the same result? */
    Assert(pProcess->StartupInfo.uNumArgs == uNumArgs);

    /*
     * Prepare environment variables list.
     */
    char **papszEnv = NULL;
    uint32_t uNumEnvVars = 0; /* Initialize in case of failing ... */
    if (RT_SUCCESS(rc))
    {
        /* Prepare environment list. */
        if (pProcess->StartupInfo.uNumEnvVars)
        {
            papszEnv = (char **)RTMemAlloc(pProcess->StartupInfo.uNumEnvVars * sizeof(char*));
            AssertPtr(papszEnv);
            uNumEnvVars = pProcess->StartupInfo.uNumEnvVars;

            const char *pszCur = pProcess->StartupInfo.szEnv;
            uint32_t i = 0;
            uint32_t cbLen = 0;
            while (cbLen < pProcess->StartupInfo.cbEnv)
            {
                /* sanity check */
                if (i >= pProcess->StartupInfo.uNumEnvVars)
                {
                    rc = VERR_INVALID_PARAMETER;
                    break;
                }
                int cbStr = RTStrAPrintf(&papszEnv[i++], "%s", pszCur);
                if (cbStr < 0)
                {
                    rc = VERR_NO_STR_MEMORY;
                    break;
                }
                pszCur += cbStr + 1; /* Skip terminating '\0' */
                cbLen  += cbStr + 1; /* Skip terminating '\0' */
            }
            Assert(i == pProcess->StartupInfo.uNumEnvVars);
        }
    }

    /*
     * Create the environment.
     */
    RTENV hEnv;
    if (RT_SUCCESS(rc))
        rc = RTEnvClone(&hEnv, RTENV_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        size_t i;
        for (i = 0; i < uNumEnvVars && papszEnv; i++)
        {
            rc = RTEnvPutEx(hEnv, papszEnv[i]);
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
            rc = gstcntlProcessSetupPipe("|", 0 /*STDIN_FILENO*/,
                                         &hStdIn, &phStdIn, &pProcess->pipeStdInW);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE    hStdOut;
                PRTHANDLE   phStdOut;
                RTPIPE      pipeStdOutR;
                rc = gstcntlProcessSetupPipe(  (pProcess->StartupInfo.uFlags & EXECUTEPROCESSFLAG_WAIT_STDOUT)
                                             ? "|" : "/dev/null",
                                             1 /*STDOUT_FILENO*/,
                                             &hStdOut, &phStdOut, &pipeStdOutR);
                if (RT_SUCCESS(rc))
                {
                    RTHANDLE    hStdErr;
                    PRTHANDLE   phStdErr;
                    RTPIPE      pipeStdErrR;
                    rc = gstcntlProcessSetupPipe(  (pProcess->StartupInfo.uFlags & EXECUTEPROCESSFLAG_WAIT_STDERR)
                                                 ? "|" : "/dev/null",
                                                 2 /*STDERR_FILENO*/,
                                                 &hStdErr, &phStdErr, &pipeStdErrR);
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
                            uint32_t uFlags = RTPOLL_EVT_ERROR;
#if 0
                            /* Add reading event to pollset to get some more information. */
                            uFlags |= RTPOLL_EVT_READ;
#endif
                            /* Stdin. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pProcess->pipeStdInW, RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDIN);
                            /* Stdout. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pipeStdOutR, uFlags, VBOXSERVICECTRLPIPEID_STDOUT);
                            /* Stderr. */
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pipeStdErrR, uFlags, VBOXSERVICECTRLPIPEID_STDERR);
                            /* IPC notification pipe. */
                            if (RT_SUCCESS(rc))
                                rc = RTPipeCreate(&pProcess->hNotificationPipeR, &pProcess->hNotificationPipeW, 0 /* Flags */);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pProcess->hNotificationPipeR, RTPOLL_EVT_READ, VBOXSERVICECTRLPIPEID_IPC_NOTIFY);

                            if (RT_SUCCESS(rc))
                            {
                                AssertPtr(pProcess->pSession);
                                bool fNeedsImpersonation = !(pProcess->pSession->uFlags & VBOXSERVICECTRLSESSION_FLAG_FORK);

                                RTPROCESS hProcess;
                                rc = gstcntlProcessCreateProcess(pProcess->StartupInfo.szCmd, papszArgs, hEnv, pProcess->StartupInfo.uFlags,
                                                                 phStdIn, phStdOut, phStdErr,
                                                                 fNeedsImpersonation ? pProcess->StartupInfo.szUser : NULL,
                                                                 fNeedsImpersonation ? pProcess->StartupInfo.szPassword : NULL,
                                                                 &hProcess);
                                if (RT_FAILURE(rc))
                                    VBoxServiceError("Error starting process, rc=%Rrc\n", rc);
                                /*
                                 * Tell the session thread that it can continue
                                 * spawning guest processes. This needs to be done after the new
                                 * process has been started because otherwise signal handling
                                 * on (Open) Solaris does not work correctly (see @bugref{5068}).
                                 */
                                int rc2 = RTThreadUserSignal(RTThreadSelf());
                                if (RT_SUCCESS(rc))
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
                                    rc = gstcntlProcessProcLoop(pProcess, hProcess, hPollSet,
                                                                &pProcess->pipeStdInW, &pipeStdOutR, &pipeStdErrR);

                                    /*
                                     * The handles that are no longer in the set have
                                     * been closed by the above call in order to prevent
                                     * the guest from getting stuck accessing them.
                                     * So, NIL the handles to avoid closing them again.
                                     */
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_IPC_NOTIFY, NULL)))
                                    {
                                        pProcess->hNotificationPipeR = NIL_RTPIPE;
                                        pProcess->hNotificationPipeW = NIL_RTPIPE;
                                    }
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDERR, NULL)))
                                        pipeStdErrR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDOUT, NULL)))
                                        pipeStdOutR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDIN, NULL)))
                                        pProcess->pipeStdInW = NIL_RTPIPE;
                                }
                            }
                            RTPollSetDestroy(hPollSet);

                            RTPipeClose(pProcess->hNotificationPipeR);
                            pProcess->hNotificationPipeR = NIL_RTPIPE;
                            RTPipeClose(pProcess->hNotificationPipeW);
                            pProcess->hNotificationPipeW = NIL_RTPIPE;
                        }
                        RTPipeClose(pipeStdErrR);
                        pipeStdErrR = NIL_RTPIPE;
                        RTHandleClose(phStdErr);
                        if (phStdErr)
                            RTHandleClose(phStdErr);
                    }
                    RTPipeClose(pipeStdOutR);
                    pipeStdOutR = NIL_RTPIPE;
                    RTHandleClose(&hStdOut);
                    if (phStdOut)
                        RTHandleClose(phStdOut);
                }
                RTPipeClose(pProcess->pipeStdInW);
                pProcess->pipeStdInW = NIL_RTPIPE;
                RTHandleClose(phStdIn);
            }
        }
        RTEnvDestroy(hEnv);
    }

    /* Move thread to stopped thread list. */
    /*int rc2 = GstCntlSessionListSet(pProcess->pSession,
                                    pProcess, VBOXSERVICECTRLTHREADLIST_STOPPED);
    AssertRC(rc2);*/

    if (pProcess->uClientID)
    {
        if (RT_FAILURE(rc))
        {
            VBGLR3GUESTCTRLCMDCTX ctx = { pProcess->uClientID, pProcess->uContextID };
            int rc2 = VbglR3GuestCtrlProcCbStatus(&ctx,
                                                  pProcess->uPID, PROC_STS_ERROR, rc,
                                                  NULL /* pvData */, 0 /* cbData */);
            if (RT_FAILURE(rc2))
                VBoxServiceError("Could not report process failure error; rc=%Rrc (process error %Rrc)\n",
                                 rc2, rc);
        }

        /* Disconnect this client from the guest control service. This also cancels all
         * outstanding host requests. */
        VBoxServiceVerbose(3, "[PID %RU32]: Disconnecting (client ID=%u) ...\n",
                           pProcess->uPID, pProcess->uClientID);
        VbglR3GuestCtrlDisconnect(pProcess->uClientID);
        pProcess->uClientID = 0;
    }

    /* Free argument + environment variable lists. */
    if (uNumEnvVars)
    {
        for (uint32_t i = 0; i < uNumEnvVars; i++)
            RTStrFree(papszEnv[i]);
        RTMemFree(papszEnv);
    }
    if (uNumArgs)
        RTGetOptArgvFree(papszArgs);

    /* Update stopped status. */
    ASMAtomicXchgBool(&pProcess->fStopped, true);

    /*
     * If something went wrong signal the user event so that others don't wait
     * forever on this thread.
     */
    if (RT_FAILURE(rc) && !fSignalled)
        RTThreadUserSignal(RTThreadSelf());

    VBoxServiceVerbose(3, "[PID %RU32]: Thread of process \"%s\" ended with rc=%Rrc\n",
                       pProcess->uPID, pProcess->StartupInfo.szCmd, rc);
    return rc;
}


static int gstcntlProcessLock(PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    int rc = RTCritSectEnter(&pProcess->CritSect);
    AssertRC(rc);
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
static DECLCALLBACK(int) gstcntlProcessThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLPROCESS pProcess = (VBOXSERVICECTRLPROCESS*)pvUser;
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    return gstcntlProcessProcessWorker(pProcess);
}


static int gstcntlProcessUnlock(PVBOXSERVICECTRLPROCESS pProcess)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    int rc = RTCritSectLeave(&pProcess->CritSect);
    AssertRC(rc);
    return rc;
}


/**
 * Executes (starts) a process on the guest. This causes a new thread to be created
 * so that this function will not block the overall program execution.
 *
 * @return  IPRT status code.
 * @param   pSession                    Guest session.
 * @param   pStartupInfo                Startup info.
 * @param   uContextID                  Context ID to associate the process to start with.

 */
int GstCntlProcessStart(const PVBOXSERVICECTRLSESSION pSession,
                        const PVBOXSERVICECTRLPROCSTARTUPINFO pStartupInfo,
                        uint32_t uContextID)
{
    AssertPtrReturn(pSession, VERR_INVALID_POINTER);
    AssertPtrReturn(pStartupInfo, VERR_INVALID_POINTER);

    /*
     * Allocate new thread data and assign it to our thread list.
     */
    PVBOXSERVICECTRLPROCESS pProcess = (PVBOXSERVICECTRLPROCESS)RTMemAlloc(sizeof(VBOXSERVICECTRLPROCESS));
    if (!pProcess)
        return VERR_NO_MEMORY;

    int rc = gstcntlProcessInit(pProcess, pSession, pStartupInfo, uContextID);
    if (RT_SUCCESS(rc))
    {
        static uint32_t s_uCtrlExecThread = 0;
        if (s_uCtrlExecThread++ == UINT32_MAX)
            s_uCtrlExecThread = 0; /* Wrap around to not let IPRT freak out. */
        rc = RTThreadCreateF(&pProcess->Thread, gstcntlProcessThread,
                             pProcess /*pvUser*/, 0 /*cbStack*/,
                             RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "gctl%u", s_uCtrlExecThread);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("Creating thread for guest process failed: rc=%Rrc, pProcess=%p\n",
                             rc, pProcess);
        }
        else
        {
            VBoxServiceVerbose(4, "Waiting for thread to initialize ...\n");

            /* Wait for the thread to initialize. */
            rc = RTThreadUserWait(pProcess->Thread, 60 * 1000 /* 60 seconds max. */);
            AssertRC(rc);
            if (   ASMAtomicReadBool(&pProcess->fShutdown)
                || RT_FAILURE(rc))
            {
                VBoxServiceError("Thread for process \"%s\" failed to start, rc=%Rrc\n",
                                 pStartupInfo->szCmd, rc);
            }
            else
            {
                ASMAtomicXchgBool(&pProcess->fStarted, true);
            }
        }
    }

    if (RT_FAILURE(rc))
        GstCntlProcessFree(pProcess);

    return rc;
}


/**
 * Performs a request to a specific (formerly started) guest process and waits
 * for its response.
 * Note: Caller is responsible for locking!
 *
 * @return  IPRT status code.
 * @param   pProcess            Guest process to perform operation on.
 * @param   pRequest            Pointer to request  to perform.
 */
int GstCntlProcessPerform(PVBOXSERVICECTRLPROCESS pProcess,
                          PVBOXSERVICECTRLREQUEST pRequest,
                          bool                    fAsync)
{
    AssertPtrReturn(pProcess, VERR_INVALID_POINTER);
    AssertPtrReturn(pRequest, VERR_INVALID_POINTER);
    AssertReturn(pRequest->enmType > VBOXSERVICECTRLREQUEST_UNKNOWN, VERR_INVALID_PARAMETER);
    /* Rest in pRequest is optional (based on the request type). */

    int rc = VINF_SUCCESS;

    if (   ASMAtomicReadBool(&pProcess->fShutdown)
        || ASMAtomicReadBool(&pProcess->fStopped))
    {
        rc = VERR_CANCELLED;
    }
    else
    {
        rc = gstcntlProcessLock(pProcess);
        if (RT_SUCCESS(rc))
        {
            AssertMsgReturn(pProcess->pRequest == NULL,
                            ("Another request still is in progress (%p)\n", pProcess->pRequest),
                            VERR_ALREADY_EXISTS);

            VBoxServiceVerbose(3, "[PID %RU32]: Sending pRequest=%p\n",
                               pProcess->uPID, pRequest);

            /* Set request structure pointer. */
            pProcess->pRequest = pRequest;

            /** @todo To speed up simultaneous guest process handling we could add a worker threads
             *        or queue in order to wait for the request to happen. Later. */
            /* Wake up guest thread by sending a wakeup byte to the notification pipe so
             * that RTPoll unblocks (returns) and we then can do our requested operation. */
            Assert(pProcess->hNotificationPipeW != NIL_RTPIPE);
            size_t cbWritten = 0;
            if (RT_SUCCESS(rc))
                rc = RTPipeWrite(pProcess->hNotificationPipeW, "i", 1, &cbWritten);

            int rcWait = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
            {
                Assert(cbWritten);
                if (!fAsync)
                {
                    VBoxServiceVerbose(3, "[PID %RU32]: Waiting for response on pRequest=%p, enmType=%u, pvData=0x%p, cbData=%u\n",
                                       pProcess->uPID, pRequest, pRequest->enmType, pRequest->pvData, pRequest->cbData);

                    rc = gstcntlProcessUnlock(pProcess);
                    if (RT_SUCCESS(rc))
                        rcWait = GstCntlProcessRequestWait(pRequest);

                    /* NULL current request in any case. */
                    pProcess->pRequest = NULL;
                }
            }

            if (   RT_FAILURE(rc)
                || fAsync)
            {
                int rc2 = gstcntlProcessUnlock(pProcess);
                AssertRC(rc2);
            }

            if (RT_SUCCESS(rc))
                rc = rcWait;
        }
    }

    VBoxServiceVerbose(3, "[PID %RU32]: Performed pRequest=%p, enmType=%u, uCID=%u, pvData=0x%p, cbData=%u, rc=%Rrc\n",
                       pProcess->uPID, pRequest, pRequest->enmType, pRequest->uCID, pRequest->pvData, pRequest->cbData, rc);
    return rc;
}

