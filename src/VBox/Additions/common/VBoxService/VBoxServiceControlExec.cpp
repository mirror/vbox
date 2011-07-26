/* $Id$ */
/** @file
 * VBoxServiceControlExec - Utility functions for process execution.
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
#include <iprt/crc.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/param.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>

#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"
#include "VBoxServicePipeBuf.h"
#include "VBoxServiceControlExecThread.h"

using namespace guestControl;

extern RTLISTNODE g_GuestControlThreads;
extern RTCRITSECT g_GuestControlThreadsCritSect;


/**
 * Handle an error event on standard input.
 *
 * @returns IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe handle.
 * @param   pStdInBuf           The standard input buffer.
 */
static int VBoxServiceControlExecProcHandleStdInErrorEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                           PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf)
{
    int rc = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
    /* Don't assert if writable handle is not in poll set anymore. */
    if (   RT_FAILURE(rc)
        && rc != VERR_POLL_HANDLE_ID_NOT_FOUND)
    {
        AssertRC(rc);
    }

    /* Close writable stdin pipe. */
    rc = RTPipeClose(*phStdInW);
    AssertRC(rc);
    *phStdInW = NIL_RTPIPE;

    /* Mark the stdin buffer as dead; we're not using it anymore. */
    rc = VBoxServicePipeBufSetStatus(pStdInBuf, false /* Disabled */);
    AssertRC(rc);

    /* Remove stdin error handle from set. */
    rc = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_ERROR);
    /* Don't assert if writable handle is not in poll set anymore. */
    if (   RT_FAILURE(rc)
        && rc != VERR_POLL_HANDLE_ID_NOT_FOUND)
    {
        AssertRC(rc);
    }
    else
        rc = VINF_SUCCESS;

    return rc;
}


/**
 * Try write some more data to the standard input of the child.
 *
 * @returns IPRT status code.
 * @retval  VINF_TRY_AGAIN if there is still data left in the buffer.
 *
 * @param   hPollSet            The polling set.
 * @param   pStdInBuf           The standard input buffer.
 * @param   hStdInW             The standard input pipe.
 * @param   pfClose             Pointer to a flag whether the pipe needs to be closed afterwards.
 */
static int VBoxServiceControlExecProcWriteStdIn(RTPOLLSET hPollSet, PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf, RTPIPE hStdInW,
                                                size_t *pcbWritten, bool *pfClose)
{
    AssertPtrReturn(pStdInBuf, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfClose, VERR_INVALID_PARAMETER);

    size_t cbLeft;
    int rc = VBoxServicePipeBufWriteToPipe(pStdInBuf, hStdInW, pcbWritten, &cbLeft);

    /* If we have written all data which is in the buffer set the close flag. */
    *pfClose = (cbLeft == 0) && VBoxServicePipeBufIsClosing(pStdInBuf);

    if (   !*pcbWritten
        && VBoxServicePipeBufIsEnabled(pStdInBuf))
    {
        /*
         * Nothing else left to write now? Remove the writable event from the poll set
         * to not trigger too high CPU loads.
         */
        int rc2 = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
        AssertRC(rc2);
    }

    VBoxServiceVerbose(3, "VBoxServiceControlExecProcWriteStdIn: Written=%u, Left=%u, rc=%Rrc\n",
                       *pcbWritten, cbLeft, rc);
    return rc;
}


/**
 * Handle an event indicating we can write to the standard input pipe of the
 * child process.
 *
 * @returns IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe.
 * @param   pStdInBuf           The standard input buffer.
 * @param   pcbWritten          Where to return the number of bytes written.
 */
static int  VBoxServiceControlExecProcHandleStdInWritableEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                               PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf, size_t *pcbWritten)
{
    AssertPtrReturn(pcbWritten, VERR_INVALID_PARAMETER);
    int rc;
    if (!(fPollEvt & RTPOLL_EVT_ERROR))
    {
        bool fClose;
        rc = VBoxServiceControlExecProcWriteStdIn(hPollSet,
                                                  pStdInBuf, *phStdInW,
                                                  pcbWritten, &fClose);
        if (   rc == VINF_TRY_AGAIN
            || rc == VERR_MORE_DATA)
            rc = VINF_SUCCESS;
        if (RT_FAILURE(rc))
        {
            if (   rc == VERR_BAD_PIPE
                || rc == VERR_BROKEN_PIPE)
            {
                rc = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
                AssertRC(rc);
            }
            else
            {
                /** @todo Do we need to do something about this error condition? */
                AssertRC(rc);
            }
        }
        else if (fClose)
        {
            /* If the pipe needs to be closed, do so. */
            rc = VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
        }
    }
    else
    {
        *pcbWritten = 0;
        rc = VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
    }
    return rc;
}


/**
 * Handle pending output data/error on stdout or stderr.
 *
 * @return  IPRT status code.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phPipeR             The pipe to be read from.
 * @param   uHandleId           Handle ID of the pipe to be read from.
 * @param   pBuf                Pointer to pipe buffer to store the read data into.
 */
static int VBoxServiceControlExecProcHandleOutputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phPipeR,
                                                       uint32_t uHandleId, PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    AssertPtrReturn(phPipeR, VERR_INVALID_POINTER);
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    /*
     * Try drain the pipe before acting on any errors.
     */
    int rc = VINF_SUCCESS;
    size_t  cbRead;
    uint8_t abBuf[_64K];

    int rc2 = RTPipeRead(*phPipeR, abBuf, sizeof(abBuf), &cbRead);
    if (RT_SUCCESS(rc2) && cbRead)
    {
        uint32_t cbWritten;
        rc = VBoxServicePipeBufWriteToBuf(pBuf, abBuf,
                                          cbRead, false /* Pending close */, &cbWritten);
#ifdef DEBUG_andy
        VBoxServiceVerbose(4, "ControlExec: Written output event [%u %u], cbRead=%u, cbWritten=%u, rc=%Rrc, uHandleId=%u, fPollEvt=%#x\n",
                           pBuf->uPID, pBuf->uPipeId, cbRead, cbWritten, rc, uHandleId, fPollEvt);
#endif
        if (RT_SUCCESS(rc))
        {
            Assert(cbRead == cbWritten);
            /* Make sure we go another poll round in case there was too much data
               for the buffer to hold. */
            fPollEvt &= RTPOLL_EVT_ERROR;
        }
    }
    else if (RT_FAILURE(rc2))
    {
        fPollEvt |= RTPOLL_EVT_ERROR;
        AssertMsg(rc2 == VERR_BROKEN_PIPE, ("%Rrc\n", rc));
    }

    /*
     * If an error was signalled, close reading stdout/stderr pipe.
     */
    if (fPollEvt & RTPOLL_EVT_ERROR)
    {
        rc2 = RTPollSetRemove(hPollSet, uHandleId);
        AssertRC(rc2);

        rc2 = RTPipeClose(*phPipeR);
        AssertRC(rc2);
        *phPipeR = NIL_RTPIPE;
    }
    return rc;
}


int VBoxServiceControlExecProcHandleStdInputNotify(RTPOLLSET hPollSet,
                                                   PRTPIPE phNotificationPipeR, PRTPIPE phInputPipeW)
{
#ifdef DEBUG_andy
    VBoxServiceVerbose(4, "ControlExec: HandleStdInputNotify\n");
#endif
    /* Drain the notification pipe. */
    uint8_t abBuf[8];
    size_t cbIgnore;
    int rc = RTPipeRead(*phNotificationPipeR, abBuf, sizeof(abBuf), &cbIgnore);
    if (RT_SUCCESS(rc))
    {
        /*
         * When the writable handle previously was removed from the poll set we need to add
         * it here again so that writable events from the started procecss get handled correctly.
         */
        RTHANDLE hWritableIgnored;
        rc = RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE, &hWritableIgnored);
        if (rc == VERR_POLL_HANDLE_ID_NOT_FOUND)
            rc = RTPollSetAddPipe(hPollSet, *phInputPipeW, RTPOLL_EVT_WRITE, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
    }
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
static int VBoxServiceControlExecProcLoop(PVBOXSERVICECTRLTHREAD pThread,
                                          RTPROCESS hProcess, RTMSINTERVAL cMsTimeout, RTPOLLSET hPollSet,
                                          PRTPIPE phStdInW, PRTPIPE phStdOutR, PRTPIPE phStdErrR)
{
    AssertPtrReturn(phStdInW, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phStdOutR, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phStdErrR, VERR_INVALID_PARAMETER);

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

    AssertPtr(pThread);
    Assert(pThread->enmType == kVBoxServiceCtrlThreadDataExec);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    /*
     * Assign PID to thread data.
     * Also check if there already was a thread with the same PID and shut it down -- otherwise
     * the first (stale) entry will be found and we get really weird results!
     */
    rc = VBoxServiceControlExecThreadAssignPID(pData, hProcess);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlExec: Unable to assign PID to new thread, rc=%Rrc\n", rc);
        return rc;
    }

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    VBoxServiceVerbose(3, "ControlExec: [PID %u]: Process started, CID=%u, User=%s\n",
                       pData->uPID, pThread->uContextID, pData->pszUser);
    rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                         pData->uPID, PROC_STS_STARTED, 0 /* u32Flags */,
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
            /*VBoxServiceVerbose(4, "ControlExec: [PID %u}: RTPollNoResume idPollHnd=%u\n",
                                 pData->uPID, idPollHnd);*/
            switch (idPollHnd)
            {
                case VBOXSERVICECTRLPIPEID_STDIN_ERROR:
                    rc = VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, &pData->stdIn);
                    break;

                case VBOXSERVICECTRLPIPEID_STDIN_INPUT_NOTIFY:
                    rc = VBoxServiceControlExecProcHandleStdInputNotify(hPollSet,
                                                                        &pData->stdIn.hNotificationPipeR, &pData->pipeStdInW);
                    AssertRC(rc);
                    /* Fall through. */
                case VBOXSERVICECTRLPIPEID_STDIN_WRITABLE:
                {
                    size_t cbWritten;
                    rc = VBoxServiceControlExecProcHandleStdInWritableEvent(hPollSet, fPollEvt, phStdInW,
                                                                            &pData->stdIn, &cbWritten);
                    break;
                }

                case VBOXSERVICECTRLPIPEID_STDOUT:
#ifdef DEBUG
                    VBoxServiceVerbose(4, "ControlExec: [PID %u]: StdOut fPollEvt=%#x\n",
                                       pData->uPID, fPollEvt);
#endif
                    rc = VBoxServiceControlExecProcHandleOutputEvent(hPollSet, fPollEvt, phStdOutR,
                                                                     VBOXSERVICECTRLPIPEID_STDOUT, &pData->stdOut);
                    break;

                case VBOXSERVICECTRLPIPEID_STDERR:
#ifdef DEBUG
                    VBoxServiceVerbose(4, "ControlExec: [PID %u]: StdErr: fPollEvt=%#x\n",
                                       pData->uPID, fPollEvt);
#endif
                    rc = VBoxServiceControlExecProcHandleOutputEvent(hPollSet, fPollEvt, phStdErrR,
                                                                     VBOXSERVICECTRLPIPEID_STDERR, &pData->stdErr);
                    break;

                default:
                    AssertMsgFailed(("PID=%u idPollHnd=%u fPollEvt=%#x\n",
                                     pData->uPID, idPollHnd, fPollEvt));
                    break;
            }
            if (RT_FAILURE(rc) || rc == VINF_EOF)
                break; /* Abort command, or client dead or something. */
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
                VBoxServiceVerbose(3, "ControlExec: [PID %u]: Timed out (%ums elapsed > %ums timeout), killing ...",
                                   pData->uPID, cMsElapsed, cMsTimeout);

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
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Is still alive and not killed yet\n",
                               pData->uPID);

            MsProcessKilled = RTTimeMilliTS();
            RTProcTerminate(hProcess);
            RTThreadSleep(500);
        }

        for (size_t i = 0; i < 10; i++)
        {
            VBoxServiceVerbose(4, "ControlExec: [PID %u]: Kill attempt %d/10: Waiting to exit ...\n",
                               pData->uPID, i + 1);
            rc2 = RTProcWait(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS(rc2))
            {
                VBoxServiceVerbose(4, "ControlExec: [PID %u]: Kill attempt %d/10: Exited\n",
                                   pData->uPID, i + 1);
                fProcessAlive = false;
                break;
            }
            if (i >= 5)
            {
                VBoxServiceVerbose(4, "ControlExec: [PID %u]: Kill attempt %d/10: Trying to terminate ...\n",
                                   pData->uPID, i + 1);
                RTProcTerminate(hProcess);
            }
            RTThreadSleep(i >= 5 ? 2000 : 500);
        }

        if (fProcessAlive)
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Could not be killed\n", pData->uPID);
    }

    /*
     * If we don't have a client problem (RT_FAILURE(rc)) we'll reply to the
     * clients exec packet now.
     */
    if (RT_SUCCESS(rc))
    {
        /* Mark this thread as stopped and do some action required for stopping ... */
        VBoxServiceControlExecThreadStop(pThread);

        uint32_t uStatus = PROC_STS_UNDEFINED;
        uint32_t uFlags = 0;

        if (     fProcessTimedOut  && !fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Timed out and got killed\n",
                               pData->uPID);
            uStatus = PROC_STS_TOK;
        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Timed out and did *not* get killed\n",
                               pData->uPID);
            uStatus = PROC_STS_TOA;
        }
        else if (pThread->fShutdown && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Got terminated because system/service is about to shutdown\n",
                               pData->uPID);
            uStatus = PROC_STS_DWN; /* Service is stopping, process was killed. */
            uFlags = pData->uFlags; /* Return handed-in execution flags back to the host. */
        }
        else if (fProcessAlive)
        {
            VBoxServiceError("ControlExec: [PID %u]: Is alive when it should not!\n",
                             pData->uPID);
        }
        else if (MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceError("ControlExec: [PID %u]: Has been killed when it should not!\n",
                             pData->uPID);
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Ended with RTPROCEXITREASON_NORMAL (%u)\n",
                               pData->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TEN;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Ended with RTPROCEXITREASON_SIGNAL (%u)\n",
                               pData->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TES;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {
            VBoxServiceVerbose(3, "ControlExec: [PID %u]: Ended with RTPROCEXITREASON_ABEND (%u)\n",
                               pData->uPID, ProcessStatus.iStatus);

            uStatus = PROC_STS_TEA;
            uFlags = ProcessStatus.iStatus;
        }
        else
            VBoxServiceError("ControlExec: [PID %u]: Reached an undefined state!\n",
                             pData->uPID);

        VBoxServiceVerbose(3, "ControlExec: [PID %u]: Ended, CID=%u, Status=%u, Flags=%u\n",
                           pData->uPID, pThread->uContextID, uStatus, uFlags);
        rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                             pData->uPID, uStatus, uFlags,
                                             NULL /* pvData */, 0 /* cbData */);
        VBoxServiceVerbose(3, "ControlExec: [PID %u]: Process loop ended with rc=%Rrc\n",
                           pData->uPID, rc);

        /*
         * Dump stdout for debugging purposes.
         * Only do that on *very* high verbosity (5+).
         */
        if (g_cVerbosity >= 5)
        {
            VBoxServiceVerbose(5, "[PID %u]: StdOut:\n", pData->uPID);

            uint8_t szBuf[_64K];
            uint32_t cbOffset = 0;
            uint32_t cbRead, cbLeft;
            while (   RT_SUCCESS(VBoxServicePipeBufPeek(&pData->stdOut, szBuf, sizeof(szBuf),
                                                        cbOffset, &cbRead, &cbLeft))
                   && cbRead)
            {
                cbOffset += cbRead;
                if (!cbLeft)
                    break;
            }

            VBoxServiceVerbose(5, "\n");
        }
    }
    else
        VBoxServiceError("ControlExec: [PID %u]: Loop failed with rc=%Rrc\n",
                         pData->uPID, rc);
    return rc;
}


/**
 * Sets up the redirection / pipe / nothing for one of the standard handles.
 *
 * @returns IPRT status code.  No client replies made.
 * @param   fd                  Which standard handle it is (0 == stdin, 1 ==
 *                              stdout, 2 == stderr).
 * @param   ph                  The generic handle that @a pph may be set
 *                              pointing to.  Always set.
 * @param   pph                 Pointer to the RTProcCreateExec argument.
 *                              Always set.
 * @param   phPipe              Where to return the end of the pipe that we
 *                              should service.  Always set.
 */
static int VBoxServiceControlExecSetupPipe(int fd, PRTHANDLE ph, PRTHANDLE *pph, PRTPIPE phPipe)
{
    AssertPtrReturn(ph, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pph, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phPipe, VERR_INVALID_PARAMETER);

    ph->enmType = RTHANDLETYPE_PIPE;
    ph->u.hPipe = NIL_RTPIPE;
    *pph        = NULL;
    *phPipe     = NIL_RTPIPE;

    int rc;

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
static int VBoxServiceControlExecMakeFullPath(const char *pszPath, char *pszExpanded, size_t cbExpanded)
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
    VBoxServiceVerbose(3, "ControlExec: VBoxServiceControlExecMakeFullPath: %s -> %s\n",
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
static int VBoxServiceControlExecResolveExecutable(const char *pszFileName, char *pszResolved, size_t cbResolved)
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

        rc = VBoxServiceControlExecMakeFullPath(pszExecResolved, pszResolved, cbResolved);
#ifdef DEBUG
        VBoxServiceVerbose(3, "ControlExec: VBoxServiceControlExecResolveExecutable: %s -> %s\n",
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
static int VBoxServiceControlExecPrepareArgv(const char *pszArgv0,
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
static int VBoxServiceControlExecCreateProcess(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
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
            rc = VBoxServiceControlExecPrepareArgv(szSysprepCmd /* argv0 */, papszArgs, &papszArgsExp);
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
        rc = VBoxServiceControlExecResolveExecutable(VBOXSERVICE_NAME, szExecExp, sizeof(szExecExp));
    }
    else
    {
#endif
        /*
         * Do the environment variables expansion on executable and arguments.
         */
        rc = VBoxServiceControlExecResolveExecutable(pszExec, szExecExp, sizeof(szExecExp));
#ifdef VBOXSERVICE_TOOLBOX
    }
#endif
    if (RT_SUCCESS(rc))
    {
        char **papszArgsExp;
        rc = VBoxServiceControlExecPrepareArgv(pszExec /* Always use the unmodified executable name as argv0. */,
                                               papszArgs /* Append the rest of the argument vector (if any). */, &papszArgsExp);
        if (RT_SUCCESS(rc))
        {
            uint32_t uProcFlags = 0;
            if (fFlags)
            {
                /* Process Main flag "ExecuteProcessFlag_Hidden". */
                if (fFlags & RT_BIT(2))
                    uProcFlags = RTPROC_FLAGS_HIDDEN;
                /* Process Main flag "ExecuteProcessFlag_NoProfile". */
                if (fFlags & RT_BIT(3))
                    uProcFlags = RTPROC_FLAGS_NO_PROFILE;
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
            VBoxServiceVerbose(3, "ControlExec: Command: %s\n", szExecExp);
            for (size_t i = 0; papszArgsExp[i]; i++)
                VBoxServiceVerbose(3, "ControlExec:\targv[%ld]: %s\n", i, papszArgsExp[i]);
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
 * The actual worker routine (lopp) for a started guest process.
 *
 * @return  IPRT status code.
 * @param   PVBOXSERVICECTRLTHREAD         Thread data associated with a started process.
 */
static DECLCALLBACK(int) VBoxServiceControlExecProcessWorker(PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtr(pThread);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    VBoxServiceVerbose(3, "ControlExec: Thread of process \"%s\" started\n", pData->pszCmd);

    int rc = VbglR3GuestCtrlConnect(&pThread->uClientID);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlExec: Thread failed to connect to the guest control service, aborted! Error: %Rrc\n", rc);
        RTThreadUserSignal(RTThreadSelf());
        return rc;
    }

    bool fSignalled = false; /* Indicator whether we signalled the thread user event already. */

    /*
     * Create the environment.
     */
    RTENV hEnv;
    rc = RTEnvClone(&hEnv, RTENV_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        size_t i;
        for (i = 0; i < pData->uNumEnvVars && pData->papszEnv; i++)
        {
            rc = RTEnvPutEx(hEnv, pData->papszEnv[i]);
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
            rc = VBoxServiceControlExecSetupPipe(0 /*STDIN_FILENO*/, &hStdIn, &phStdIn, &pData->pipeStdInW);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE    hStdOut;
                PRTHANDLE   phStdOut;
                RTPIPE      hStdOutR;
                rc = VBoxServiceControlExecSetupPipe(1 /*STDOUT_FILENO*/, &hStdOut, &phStdOut, &hStdOutR);
                if (RT_SUCCESS(rc))
                {
                    RTHANDLE    hStdErr;
                    PRTHANDLE   phStdErr;
                    RTPIPE      hStdErrR;
                    rc = VBoxServiceControlExecSetupPipe(2 /*STDERR_FILENO*/, &hStdErr, &phStdErr, &hStdErrR);
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
                            rc = RTPollSetAddPipe(hPollSet, pData->pipeStdInW, RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDIN_ERROR);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdOutR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDOUT);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdErrR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, VBOXSERVICECTRLPIPEID_STDERR);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pData->pipeStdInW, RTPOLL_EVT_WRITE, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, pData->stdIn.hNotificationPipeR, RTPOLL_EVT_READ, VBOXSERVICECTRLPIPEID_STDIN_INPUT_NOTIFY);
                            if (RT_SUCCESS(rc))
                            {
                                RTPROCESS hProcess;
                                rc = VBoxServiceControlExecCreateProcess(pData->pszCmd, pData->papszArgs, hEnv, pData->uFlags,
                                                                         phStdIn, phStdOut, phStdErr,
                                                                         pData->pszUser, pData->pszPassword,
                                                                         &hProcess);
                                if (RT_FAILURE(rc))
                                    VBoxServiceError("ControlExec: Error starting process, rc=%Rrc\n", rc);
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
                                    rc = VBoxServiceControlExecProcLoop(pThread,
                                                                        hProcess, pData->uTimeLimitMS, hPollSet,
                                                                        &pData->pipeStdInW, &hStdOutR, &hStdErrR);

                                    /*
                                     * The handles that are no longer in the set have
                                     * been closed by the above call in order to prevent
                                     * the guest from getting stuck accessing them.
                                     * So, NIL the handles to avoid closing them again.
                                     */
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE, NULL)))
                                        pData->pipeStdInW = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_INPUT_NOTIFY, NULL)))
                                        pData->stdIn.hNotificationPipeR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDOUT, NULL)))
                                        hStdOutR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, VBOXSERVICECTRLPIPEID_STDERR, NULL)))
                                        hStdErrR = NIL_RTPIPE;
                                }
                                else /* Something went wrong; report error! */
                                {
                                    VBoxServiceError("ControlExec: Could not start process '%s' (CID: %u)! Error: %Rrc\n",
                                                     pData->pszCmd, pThread->uContextID, rc);

                                    rc2 = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID, pData->uPID,
                                                                          PROC_STS_ERROR, rc,
                                                                          NULL /* pvData */, 0 /* cbData */);
                                    if (RT_FAILURE(rc2))
                                        VBoxServiceError("ControlExec: Could not report process start error! Error: %Rrc (process error %Rrc)\n",
                                                         rc2, rc);
                                }
                            }
                            RTPollSetDestroy(hPollSet);
                            RTPipeClose(pData->stdIn.hNotificationPipeR);
                        }
                        RTPipeClose(hStdErrR);
                        RTHandleClose(phStdErr);
                    }
                    RTPipeClose(hStdOutR);
                    RTHandleClose(phStdOut);
                }
                RTPipeClose(pData->pipeStdInW);
                RTHandleClose(phStdIn);
            }
        }
        RTEnvDestroy(hEnv);
    }

    VbglR3GuestCtrlDisconnect(pThread->uClientID);
    VBoxServiceVerbose(3, "ControlExec: [PID %u]: Thread of process \"%s\" ended with rc=%Rrc\n",
                       pData->uPID, pData->pszCmd, rc);

    /*
     * If something went wrong signal the user event so that others don't wait
     * forever on this thread.
     */
    if (RT_FAILURE(rc) && !fSignalled)
        RTThreadUserSignal(RTThreadSelf());
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
static DECLCALLBACK(int) VBoxServiceControlExecThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLTHREAD pThread = (VBOXSERVICECTRLTHREAD*)pvUser;
    AssertPtr(pThread);
    return VBoxServiceControlExecProcessWorker(pThread);
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
 */
int VBoxServiceControlExecProcess(uint32_t uClientID, uint32_t uContextID,
                                  const char *pszCmd, uint32_t uFlags,
                                  const char *pszArgs, uint32_t uNumArgs,
                                  const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                  const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS)
{
    bool fAllowed = false;
    int rc = VBoxServiceControlExecThreadStartAllowed(&fAllowed);
    if (RT_FAILURE(rc))
        VBoxServiceError("ControlExec: Error determining whether process can be started or not, rc=%Rrc\n", rc);

    if (fAllowed)
    {
        /*
         * Allocate new thread data and assign it to our thread list.
         */
        PVBOXSERVICECTRLTHREAD pThread = (PVBOXSERVICECTRLTHREAD)RTMemAlloc(sizeof(VBOXSERVICECTRLTHREAD));
        if (pThread)
        {
            rc = VBoxServiceControlExecThreadAlloc(pThread,
                                                   uContextID,
                                                   pszCmd, uFlags,
                                                   pszArgs, uNumArgs,
                                                   pszEnv, cbEnv, uNumEnvVars,
                                                   pszUser, pszPassword,
                                                   uTimeLimitMS);
            if (RT_SUCCESS(rc))
            {
                static uint32_t uCtrlExecThread = 0;
                char szThreadName[32];
                if (!RTStrPrintf(szThreadName, sizeof(szThreadName), "controlexec%ld", uCtrlExecThread++))
                    AssertMsgFailed(("Unable to create unique control exec thread name!\n"));

                rc = RTThreadCreate(&pThread->Thread, VBoxServiceControlExecThread,
                                    (void *)(PVBOXSERVICECTRLTHREAD*)pThread, 0,
                                    RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, szThreadName);
                if (RT_FAILURE(rc))
                {
                    VBoxServiceError("ControlExec: RTThreadCreate failed, rc=%Rrc\n, pThread=%p\n",
                                     rc, pThread);
                }
                else
                {
                    VBoxServiceVerbose(4, "ControlExec: Waiting for thread to initialize ...\n");

                    /* Wait for the thread to initialize. */
                    RTThreadUserWait(pThread->Thread, 60 * 1000 /* 60 seconds max. */);
                    if (pThread->fShutdown)
                    {
                        VBoxServiceError("ControlExec: Thread for process \"%s\" failed to start!\n", pszCmd);
                        rc = VERR_GENERAL_FAILURE;
                    }
                    else
                    {
                        pThread->fStarted = true;
                        /*rc =*/ RTListAppend(&g_GuestControlThreads, &pThread->Node);
                    }
                }

                if (RT_FAILURE(rc))
                    VBoxServiceControlExecThreadDataDestroy((PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData);
            }
            if (RT_FAILURE(rc))
                RTMemFree(pThread);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else /* Process start is not allowed due to policy settings. */
    {
        VBoxServiceVerbose(3, "ControlExec: Guest process limit is reached!\n");

        /* Tell the host. */
        rc = VbglR3GuestCtrlExecReportStatus(uClientID, uContextID, 0 /* PID */,
                                             PROC_STS_ERROR, VERR_MAX_PROCS_REACHED,
                                             NULL /* pvData */, 0 /* cbData */);
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
int VBoxServiceControlExecHandleCmdStartProcess(uint32_t u32ClientId, uint32_t uNumParms)
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
                                           /* Credentials */
                                           szUser,     sizeof(szUser),
                                           szPassword, sizeof(szPassword),
                                           /* Timelimit */
                                           &uTimeLimitMS);
#ifdef DEBUG
    VBoxServiceVerbose(3, "ControlExec: Start process szCmd=%s, uFlags=%u, szArgs=%s, szEnv=%s, szUser=%s, szPW=%s, uTimeout=%u\n",
                       szCmd, uFlags, uNumArgs ? szArgs : "<None>", uNumEnvVars ? szEnv : "<None>", szUser, szPassword, uTimeLimitMS);
#endif
    if (RT_SUCCESS(rc))
    {
        /** @todo Put the following params into a struct! */
        rc = VBoxServiceControlExecProcess(u32ClientId, uContextID,
                                           szCmd, uFlags, szArgs, uNumArgs,
                                           szEnv, cbEnv, uNumEnvVars,
                                           szUser, szPassword, uTimeLimitMS);
    }
    else
        VBoxServiceError("ControlExec: Failed to retrieve exec start command! Error: %Rrc\n", rc);
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
int VBoxServiceControlExecHandleCmdSetInput(uint32_t u32ClientId, uint32_t uNumParms, size_t cbMaxBufSize)
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
        VBoxServiceError("ControlExec: [PID %u]: Failed to retrieve exec input command! Error: %Rrc\n",
                         uPID, rc);
    }
    else if (cbSize >  cbMaxBufSize)
    {
        VBoxServiceError("ControlExec: [PID %u]: Maximum input buffer size is too small! cbSize=%u, cbMaxBufSize=%u\n",
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
            VBoxServiceVerbose(4, "ControlExec: [PID %u]: Got last input block of size %u ...\n",
                               uPID, cbSize);
        }

        rc = VBoxServiceControlExecThreadSetInput(uPID, fPendingClose, pabBuffer,
                                                  cbSize, &cbWritten);
        VBoxServiceVerbose(4, "ControlExec: [PID %u]: Written input, rc=%Rrc, uFlags=0x%x, fPendingClose=%d, cbSize=%u, cbWritten=%u\n",
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

    VBoxServiceVerbose(3, "ControlExec: [PID %u]: Input processed, uStatus=%u, uFlags=0x%x, cbWritten=%u\n",
                       uPID, uStatus, uFlags, cbWritten);

    /* Note: Since the context ID is unique the request *has* to be completed here,
     *       regardless whether we got data or not! Otherwise the progress object
     *       on the host never will get completed! */
    rc = VbglR3GuestCtrlExecReportStatusIn(u32ClientId, uContextID, uPID,
                                           uStatus, uFlags, (uint32_t)cbWritten);

    if (RT_FAILURE(rc))
        VBoxServiceError("ControlExec: [PID %u]: Failed to report input status! Error: %Rrc\n",
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
int VBoxServiceControlExecHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms)
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
            rc = VBoxServiceControlExecThreadGetOutput(uPID, uHandleID, RT_INDEFINITE_WAIT /* Timeout */,
                                                       pBuf, _64K /* cbSize */, &cbRead);
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(3, "ControlExec: [PID %u]: Got output, cbRead=%u, uHandle=%u, uFlags=%u\n",
                                   uPID, cbRead, uHandleID, uFlags);
            else
                VBoxServiceError("ControlExec: [PID %u]: Failed to retrieve output, uHandle=%u, rc=%Rrc\n",
                                 uPID, uHandleID, rc);

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
        VBoxServiceError("ControlExec: [PID %u]: Failed to handle output command! Error: %Rrc\n",
                         uPID, rc);
    return rc;
}

