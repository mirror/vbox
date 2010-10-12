/* $Id$ */
/** @file
 * VBoxServiceControlExec - Utility functions for process execution.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#include <iprt/semaphore.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <VBox/version.h>
#include <VBox/VBoxGuestLib.h>
#include <VBox/HostServices/GuestControlSvc.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

using namespace guestControl;

extern RTLISTNODE g_GuestControlExecThreads;


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
    int rc = RTCritSectEnter(&pStdInBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        int rc2;
        /* If no data is to be processed anymore, remove the writable pipe from
         * the poll set. */
        if (pStdInBuf->cbProcessed <= pStdInBuf->cbSize)
        {
            rc2 = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
            AssertRC(rc2);

            rc2 = RTPipeClose(*phStdInW);
            AssertRC(rc2);
            *phStdInW = NIL_RTPIPE;

            /* Mark the stdin buffer as dead; we're not using it anymore. */
            pStdInBuf->fAlive = false;
        }

        rc2 = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_ERROR);
        AssertRC(rc2);

        rc2 = RTCritSectLeave(&pStdInBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Try write some more data to the standard input of the child.
 *
 * @returns IPRT status code.
 * @param   pStdInBuf           The standard input buffer.
 * @param   hStdInW             The standard input pipe.
 */
static int VBoxServiceControlExecProcWriteStdIn(PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf, RTPIPE hStdInW, size_t *pcbWritten)
{
    AssertPtrReturn(pcbWritten, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pStdInBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        size_t cbToWrite = pStdInBuf->cbSize - pStdInBuf->cbProcessed;
        int rc = VINF_SUCCESS;
        if (cbToWrite && pStdInBuf->fAlive)
        {
            rc = RTPipeWrite(hStdInW, &pStdInBuf->pbData[pStdInBuf->cbProcessed], cbToWrite, pcbWritten);
            if (RT_SUCCESS(rc))
            {
                Assert(*pcbWritten <= cbToWrite);
                pStdInBuf->cbProcessed += *pcbWritten;
            }
            else
                *pcbWritten = 0;
        }
        else
            rc = VERR_BAD_PIPE;
        int rc2 = RTCritSectLeave(&pStdInBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
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
 */
static int  VBoxServiceControlExecProcHandleStdInWritableEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                               PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf)
{
    VBoxServiceVerbose(4, "ControlExec: HandleStdInWritableEvent: fPollEvt=%#x\n", fPollEvt);

    int rc = VINF_SUCCESS;
    if (!(fPollEvt & RTPOLL_EVT_ERROR))
    {
        size_t cbWritten;
        rc = VBoxServiceControlExecProcWriteStdIn(pStdInBuf, *phStdInW, &cbWritten);
        if (   RT_FAILURE(rc)
            && rc != VERR_BAD_PIPE)
        {
            /** @todo Do we need to do something about this error condition? */
            AssertRC(rc);
        }

        /* No more data to write to stdin? Then remove stdin from poll set. */
        if (   cbWritten <= 0
            || rc == VERR_BAD_PIPE)
        {
            rc = RTPollSetRemove(hPollSet, VBOXSERVICECTRLPIPEID_STDIN_WRITABLE);
            AssertRC(rc);
        }
    }
    else
        rc = VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
    return rc;
}


static int VBoxServiceControlExecProcHandleInputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                      uint32_t uHandleId, PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf)
{
    VBoxServiceVerbose(4, "ControlExec: HandleInputEvent: fPollEvt=%#x\n", fPollEvt);

    int rc = VINF_SUCCESS;
    if (fPollEvt & RTPOLL_EVT_ERROR)
        rc = VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
    return rc;
}


/**
 * Handle pending output data or error on standard out, standard error or the
 * test pipe.
 *
 * @returns IPRT status code from client send.
 * @param   pThread             The thread specific data.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phPipeR             The pipe handle.
 * @param   pu32Crc             The current CRC-32 of the stream. (In/Out)
 * @param   uHandleId           The handle ID.
 *
 * @todo    Put the last 4 parameters into a struct!
 */
static int VBoxServiceControlExecProcHandleOutputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phPipeR,
                                                       uint32_t uHandleId, PVBOXSERVICECTRLEXECPIPEBUF pStdOutBuf)
{
    VBoxServiceVerbose(4, "ControlExec: HandleOutputEvent: fPollEvt=%#x\n", fPollEvt);

    /*
     * Try drain the pipe before acting on any errors.
     */
    int rc = VINF_SUCCESS;
    size_t  cbRead;
    uint8_t abBuf[_64K];

    int rc2 = RTPipeRead(*phPipeR, abBuf, sizeof(abBuf), &cbRead);
    if (RT_SUCCESS(rc2) && cbRead)
    {
#if 0
        /* Only used for "real-time" stdout/stderr data; gets sent immediately (later)! */
        rc = VbglR3GuestCtrlExecSendOut(pThread->uClientID, pThread->uContextID,
                                        pData->uPID, uHandleId, 0 /* u32Flags */,
                                        abBuf, cbRead);
        if (RT_FAILURE(rc))
        {
            VBoxServiceError("ControlExec: Error while sending real-time output data, rc=%Rrc, cbRead=%u, CID=%u, PID=%u\n",
                             rc, cbRead, pThread->uClientID, pData->uPID);
        }
        else
        {
#endif
            rc = VBoxServiceControlExecWritePipeBuffer(pStdOutBuf, abBuf, cbRead);
            if (RT_SUCCESS(rc))
            {
                /* Make sure we go another poll round in case there was too much data
                   for the buffer to hold. */
                fPollEvt &= RTPOLL_EVT_ERROR;
            }
#if 0
        }
#endif
    }
    else if (RT_FAILURE(rc2))
    {
        fPollEvt |= RTPOLL_EVT_ERROR;
        AssertMsg(rc2 == VERR_BROKEN_PIPE, ("%Rrc\n", rc));
    }

    /*
     * If an error was raised signalled,
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


/**
 * Handle a transport event or successful pfnPollIn() call.
 *
 * @returns IPRT status code from client send.
 * @retval  VINF_EOF indicates ABORT command.
 *
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   idPollHnd           The handle ID.
 * @param   hStdInW             The standard input pipe.
 * @param   pStdInBuf           The standard input buffer.
 */
static int VBoxServiceControlExecProcHandleTransportEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, uint32_t idPollHnd,
                                                          PRTPIPE phStdInW, PVBOXSERVICECTRLEXECPIPEBUF pStdInBuf)
{
    return 0; //RTPollSetAddPipe(hPollSet, *phStdInW, RTPOLL_EVT_WRITE, 4 /*TXSEXECHNDID_STDIN_WRITABLE*/);
}


static int VBoxServiceControlExecProcLoop(PVBOXSERVICECTRLTHREAD pThread,
                                          RTPROCESS hProcess, RTMSINTERVAL cMillies, RTPOLLSET hPollSet,
                                          RTPIPE hStdInW, RTPIPE hStdOutR, RTPIPE hStdErrR)
{
    int                         rc;
    int                         rc2;
    uint64_t const              MsStart             = RTTimeMilliTS();
    RTPROCSTATUS                ProcessStatus       = { 254, RTPROCEXITREASON_ABEND };
    bool                        fProcessAlive       = true;
    bool                        fProcessTimedOut    = false;
    uint64_t                    MsProcessKilled     = UINT64_MAX;
    RTMSINTERVAL const          cMsPollBase         = hStdInW != NIL_RTPIPE
                                                      ? 100   /* Need to poll for input. */
                                                      : 1000; /* Need only poll for process exit and aborts. */
    RTMSINTERVAL                cMsPollCur          = 0;

    AssertPtr(pThread);
    Assert(pThread->enmType == VBoxServiceCtrlThreadDataExec);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    /* Assign PID to thread data. */
    pData->uPID = hProcess;

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    VBoxServiceVerbose(3, "ControlExec: Process started: PID=%u, CID=%u, User=%s, PW=%s\n",
                       pData->uPID, pThread->uContextID, pData->pszUser, pData->pszPassword);
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
            VBoxServiceVerbose(4, "ControlExec: RTPollNoResume idPollHnd=%u\n", idPollHnd);
            switch (idPollHnd)
            {
                case VBOXSERVICECTRLPIPEID_STDIN_ERROR:
                    rc = VBoxServiceControlExecProcHandleInputEvent(hPollSet, fPollEvt, &hStdInW,
                                                                    VBOXSERVICECTRLPIPEID_STDIN_ERROR, &pData->stdIn);
                    break;

                case VBOXSERVICECTRLPIPEID_STDIN_WRITABLE:
                    rc = VBoxServiceControlExecProcHandleStdInWritableEvent(hPollSet, fPollEvt, &hStdInW, &pData->stdIn);
                    break;

                case VBOXSERVICECTRLPIPEID_STDOUT:
                    rc = VBoxServiceControlExecProcHandleOutputEvent(hPollSet, fPollEvt, &hStdOutR,
                                                                     VBOXSERVICECTRLPIPEID_STDOUT, &pData->stdOut);
                    break;

                case VBOXSERVICECTRLPIPEID_STDERR:
                    rc = VBoxServiceControlExecProcHandleOutputEvent(hPollSet, fPollEvt, &hStdErrR,
                                                                     VBOXSERVICECTRLPIPEID_STDERR, &pData->stdOut);
                    break;

                default:
                    //rc = VBoxServiceControlExecProcHandleTransportEvent(hPollSet, fPollEvt, idPollHnd, &hStdInW, &pData->stdIn);
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
        if (cMillies != RT_INDEFINITE_WAIT)
        {
            uint64_t u64Now = RTTimeMilliTS();
            uint64_t cMsElapsed = u64Now - MsStart;
            if (cMsElapsed >= cMillies)
            {
                VBoxServiceVerbose(3, "ControlExec: Process timed out (%ums elapsed > %ums timeout), killing ...", cMsElapsed, cMillies);

                fProcessTimedOut = true;
                if (    MsProcessKilled == UINT64_MAX
                    ||  u64Now - MsProcessKilled > 1000)
                {
                    if (u64Now - MsProcessKilled > 20*60*1000)
                        break; /* give up after 20 mins */
                    RTProcTerminate(hProcess);
                    MsProcessKilled = u64Now;
                    continue;
                }
                cMilliesLeft = 10000;
            }
            else
                cMilliesLeft = cMillies - (uint32_t)cMsElapsed;
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
            VBoxServiceVerbose(3, "ControlExec: Process (PID=%u) is still alive and not killed yet\n",
                               pData->uPID);

            MsProcessKilled = RTTimeMilliTS();
            RTProcTerminate(hProcess);
            RTThreadSleep(500);
        }

        for (size_t i = 0; i < 10; i++)
        {
            VBoxServiceVerbose(4, "ControlExec: Kill attempt %d/10: Waiting for process (PID=%u) exit ...\n",
                               i + 1, pData->uPID);
            rc2 = RTProcWait(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS(rc2))
            {
                VBoxServiceVerbose(4, "ControlExec: Kill attempt %d/10: Process (PID=%u) exited\n",
                                   i + 1, pData->uPID);
                fProcessAlive = false;
                break;
            }
            if (i >= 5)
            {
                VBoxServiceVerbose(4, "ControlExec: Kill attempt %d/10: Try to terminate (PID=%u) ...\n",
                                   i + 1, pData->uPID);
                RTProcTerminate(hProcess);
            }
            RTThreadSleep(i >= 5 ? 2000 : 500);
        }

        if (fProcessAlive)
            VBoxServiceVerbose(3, "ControlExec: Process (PID=%u) could not be killed\n", pData->uPID);
    }

    /*
     * If we don't have a client problem (RT_FAILURE(rc) we'll reply to the
     * clients exec packet now.
     */
    if (RT_SUCCESS(rc))
    {
        uint32_t uStatus = PROC_STS_UNDEFINED;
        uint32_t uFlags = 0;

        if (     fProcessTimedOut  && !fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlExec: Process timed out and got killed\n");
            uStatus = PROC_STS_TOK;
        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceVerbose(3, "ControlExec: Process timed out and did *not* get killed\n");
            uStatus = PROC_STS_TOA;
        }
        else if (pThread->fShutdown && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {
            VBoxServiceVerbose(3, "ControlExec: Process got terminated because system/service is about to shutdown\n");
            uStatus = PROC_STS_DWN; /* Service is stopping, process was killed. */
            uFlags = pData->uFlags; /* Return handed-in execution flags back to the host. */
        }
        else if (fProcessAlive)
        {
            VBoxServiceError("ControlExec: Process is alive when it should not!\n");
        }
        else if (MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceError("ControlExec: Process has been killed when it should not!\n");
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            VBoxServiceVerbose(3, "ControlExec: Process ended with RTPROCEXITREASON_NORMAL\n");

            uStatus = PROC_STS_TEN;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {
            VBoxServiceVerbose(3, "ControlExec: Process ended with RTPROCEXITREASON_SIGNAL\n");

            uStatus = PROC_STS_TES;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {
            VBoxServiceVerbose(3, "ControlExec: Process ended with RTPROCEXITREASON_ABEND\n");

            uStatus = PROC_STS_TEA;
            uFlags = ProcessStatus.iStatus;
        }
        else
        {
            VBoxServiceError("ControlExec: Process has reached an undefined status!\n");
        }

        VBoxServiceVerbose(3, "ControlExec: Process ended: PID=%u, CID=%u, Status=%u, Flags=%u\n",
                           pData->uPID, pThread->uContextID, uStatus, uFlags);
        rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                             pData->uPID, uStatus, uFlags,
                                             NULL /* pvData */, 0 /* cbData */);
        VBoxServiceVerbose(3, "ControlExec: Process loop ended with rc=%Rrc\n", rc);
    }
    else
        VBoxServiceError("ControlExec: Process loop failed with rc=%Rrc\n", rc);
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
    AssertPtr(ph);
    AssertPtr(pph);
    AssertPtr(phPipe);

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

int VBoxServiceControlExecInitPipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    AssertPtr(pBuf);

    /** @todo Add allocation size as function parameter! */
    pBuf->pbData = (uint8_t*)RTMemAlloc(_64K); /* Start with a 64k buffer. */
    AssertReturn(pBuf->pbData, VERR_NO_MEMORY);
    pBuf->cbAllocated = _64K;
    pBuf->cbSize = 0;
    pBuf->cbProcessed = 0;
    pBuf->fAlive = true;

    return RTCritSectInit(&pBuf->CritSect);
}

int VBoxServiceControlExecDestroyPipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    if (pBuf)
    {
        if (pBuf->pbData)
            RTMemFree(pBuf->pbData);
        pBuf->pbData = NULL;
        pBuf->cbAllocated = 0;
        pBuf->cbSize = 0;
        pBuf->cbProcessed = 0;
    }
    pBuf->fAlive = false;
    return RTCritSectDelete(&pBuf->CritSect);
}

int VBoxServiceControlExecReadPipeBufferContent(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                                uint8_t *pbBuffer, uint32_t cbBuffer, uint32_t *pcbToRead)
{
    AssertPtr(pBuf);
    AssertPtr(pcbToRead);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pBuf->cbSize >= pBuf->cbProcessed);
        if (*pcbToRead > pBuf->cbSize - pBuf->cbProcessed)
            *pcbToRead = pBuf->cbSize - pBuf->cbProcessed;

        if (*pcbToRead > cbBuffer)
            *pcbToRead = cbBuffer;

        if (*pcbToRead > 0)
        {
            memcpy(pbBuffer, pBuf->pbData + pBuf->cbProcessed, *pcbToRead);
            pBuf->cbProcessed += *pcbToRead;
        }
        else
        {
            pbBuffer = NULL;
            *pcbToRead = 0;
        }
        rc = RTCritSectLeave(&pBuf->CritSect);
    }
    return rc;
}

int VBoxServiceControlExecWritePipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                          uint8_t *pbData, uint32_t cbData)
{
    AssertPtr(pBuf);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pBuf->fAlive)
        {
            /** @todo Buffer size limit! 4MB */

            /* Resize buffer if not enough space for storing the read in data left. */
            uint8_t *pNewBuf;
            while (pBuf->cbAllocated - pBuf->cbSize < cbData)
            {
                pNewBuf = (uint8_t*)RTMemRealloc(pBuf->pbData, pBuf->cbAllocated + _4K);
                if (pNewBuf == NULL)
                    break;
                pBuf->cbAllocated += _4K;
                pBuf->pbData = pNewBuf;
            }

            rc = VINF_SUCCESS;
            if (pBuf->pbData)
            {
                memcpy(pBuf->pbData + pBuf->cbSize, pbData, cbData);
                pBuf->cbSize += cbData;
                /** @todo Add offset clamping! */
            }
            else
                rc = VERR_NO_MEMORY;
        }
        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}

/** Allocates and gives back a thread data struct which then can be used by the worker thread. */
int VBoxServiceControlExecAllocateThreadData(PVBOXSERVICECTRLTHREAD pThread,
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
    pThread->fStarted = false;
    pThread->fStopped = false;

    pThread->uContextID = u32ContextID;
    /* ClientID will be assigned when thread is started! */

    /* Specific stuff. */
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)RTMemAlloc(sizeof(VBOXSERVICECTRLTHREADDATAEXEC));
    if (pData == NULL)
        return VERR_NO_MEMORY;

    pData->uPID = 0; /* Don't have a PID yet. */
    pData->pszCmd = RTStrDup(pszCmd);
    pData->uFlags = uFlags;
    pData->uNumEnvVars = 0;
    pData->uNumArgs = 0; /* Initialize in case of RTGetOptArgvFromString() is failing ... */

    /* Prepare argument list. */
    int rc = RTGetOptArgvFromString(&pData->papszArgs, (int*)&pData->uNumArgs,
                                    (uNumArgs > 0) ? pszArgs : "", NULL);
    /* Did we get the same result? */
    Assert(uNumArgs == pData->uNumArgs);

    if (RT_SUCCESS(rc))
    {
        /* Prepare environment list. */
        if (uNumEnvVars)
        {
            pData->papszEnv = (char**)RTMemAlloc(uNumEnvVars * sizeof(char*));
            AssertPtr(pData->papszEnv);
            pData->uNumEnvVars = uNumEnvVars;

            const char *pcCur = pszEnv;
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
                int cbStr = RTStrAPrintf(&pData->papszEnv[i++], "%s", pcCur);
                if (cbStr < 0)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                cbLen += cbStr + 1; /* Skip terminating '\0' */
                pcCur += cbStr + 1; /* Skip terminating '\0' */
            }
        }

        pData->pszUser = RTStrDup(pszUser);
        pData->pszPassword = RTStrDup(pszPassword);
        pData->uTimeLimitMS = uTimeLimitMS;

        /* Adjust time limit value. */
        pData->uTimeLimitMS = (   (uTimeLimitMS == UINT32_MAX)
                               || (uTimeLimitMS == 0)) ?
                               RT_INDEFINITE_WAIT : uTimeLimitMS;

        /* Init buffers. */
        rc = VBoxServiceControlExecInitPipeBuffer(&pData->stdOut);
        if (RT_SUCCESS(rc))
            rc = VBoxServiceControlExecInitPipeBuffer(&pData->stdErr);
            if (RT_SUCCESS(rc))
                rc = VBoxServiceControlExecInitPipeBuffer(&pData->stdIn);
    }

    if (RT_FAILURE(rc))
    {
        VBoxServiceControlExecDestroyThreadData(pData);
    }
    else
    {
        pThread->enmType = VBoxServiceCtrlThreadDataExec;
        pThread->pvData = pData;
    }
    return rc;
}

/** Frees an allocated thread data structure along with all its allocated parameters. */
void VBoxServiceControlExecDestroyThreadData(PVBOXSERVICECTRLTHREADDATAEXEC pData)
{
    if (pData)
    {
        RTStrFree(pData->pszCmd);
        if (pData->uNumEnvVars)
        {
            for (uint32_t i = 0; i < pData->uNumEnvVars; i++)
                RTStrFree(pData->papszEnv[i]);
            RTMemFree(pData->papszEnv);
        }
        RTGetOptArgvFree(pData->papszArgs);
        RTStrFree(pData->pszUser);
        RTStrFree(pData->pszPassword);

        VBoxServiceControlExecDestroyPipeBuffer(&pData->stdOut);
        VBoxServiceControlExecDestroyPipeBuffer(&pData->stdErr);
        VBoxServiceControlExecDestroyPipeBuffer(&pData->stdIn);

        RTMemFree(pData);
        pData = NULL;
    }
}

int VBoxServiceControlExecCreateProcess(const char *pszExec, const char * const *papszArgs, RTENV hEnv, uint32_t fFlags,
                                        PCRTHANDLE phStdIn, PCRTHANDLE phStdOut, PCRTHANDLE phStdErr, const char *pszAsUser,
                                        const char *pszPassword, PRTPROCESS phProcess)
{
    int  rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    /*
     * If sysprep should be executed do this in the context of VBoxService, which
     * (usually, if started by SCM) has administrator rights. Because of that a UI
     * won't be shown (doesn't have a desktop).
     */
    if (stricmp(pszExec, "sysprep") == 0)
    {
        /* Get the predefined path of sysprep.exe (depending on Windows OS). */
        char szSysprepCmd[RTPATH_MAX] = "C:\\sysprep\\sysprep.exe";
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
        rc = RTProcCreateEx(szSysprepCmd, papszArgs, hEnv, 0 /* fFlags */,
                            phStdIn, phStdOut, phStdErr, NULL /* pszAsUser */,
                            NULL /* pszPassword */, phProcess);
    }
    else
    {
#endif
        /* Do normal execution. */
        rc = RTProcCreateEx(pszExec, papszArgs, hEnv, fFlags,
                            phStdIn, phStdOut, phStdErr, pszAsUser,
                            pszPassword, phProcess);
#ifdef RT_OS_WINDOWS
    }
#endif
    return rc;
}

DECLCALLBACK(int) VBoxServiceControlExecProcessWorker(PVBOXSERVICECTRLTHREAD pThread)
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
                            {
                                RTPROCESS hProcess;
                                rc = VBoxServiceControlExecCreateProcess(pData->pszCmd, pData->papszArgs, hEnv, RTPROC_FLAGS_SERVICE,
                                                                         phStdIn, phStdOut, phStdErr,
                                                                         pData->pszUser, pData->pszPassword,
                                                                         &hProcess);

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
                                                                        pData->pipeStdInW, hStdOutR, hStdErrR);

                                    /*
                                     * The handles that are no longer in the set have
                                     * been closed by the above call in order to prevent
                                     * the guest from getting stuck accessing them.
                                     * So, NIL the handles to avoid closing them again.
                                     */
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 0 /* stdin */, NULL)))
                                        pData->pipeStdInW = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 1 /* stdout */, NULL)))
                                        hStdOutR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 2 /* stderr */, NULL)))
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
    VBoxServiceVerbose(3, "ControlExec: Thread of process \"%s\" (PID: %u) ended with rc=%Rrc\n",
                       pData->pszCmd, pData->uPID, rc);

    /*
     * If something went wrong signal the user event so that others don't wait
     * forever on this thread.
     */
    if (RT_FAILURE(rc) && !fSignalled)
        RTThreadUserSignal(RTThreadSelf());
    return rc;
}

PVBOXSERVICECTRLTHREAD VBoxServiceControlExecFindProcess(uint32_t uPID)
{
    PVBOXSERVICECTRLTHREAD pNode;
    bool fFound = false;
    RTListForEach(&g_GuestControlExecThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
    {
        if (   pNode->fStarted
            && pNode->enmType == VBoxServiceCtrlThreadDataExec)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            if (pData && pData->uPID == uPID)
            {
                return pNode;
            }
        }
    }
    return NULL;
}

static DECLCALLBACK(int) VBoxServiceControlExecThread(RTTHREAD ThreadSelf, void *pvUser)
{
    PVBOXSERVICECTRLTHREAD pThread = (VBOXSERVICECTRLTHREAD*)pvUser;
    AssertPtr(pThread);
    return VBoxServiceControlExecProcessWorker(pThread);
}

int VBoxServiceControlExecProcess(uint32_t uContextID, const char *pszCmd, uint32_t uFlags,
                                  const char *pszArgs, uint32_t uNumArgs,
                                  const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                  const char *pszUser, const char *pszPassword, uint32_t uTimeLimitMS)
{
    PVBOXSERVICECTRLTHREAD pThread = (PVBOXSERVICECTRLTHREAD)RTMemAlloc(sizeof(VBOXSERVICECTRLTHREAD));

    int rc;
    if (pThread)
    {
        rc = VBoxServiceControlExecAllocateThreadData(pThread,
                                                      uContextID,
                                                      pszCmd, uFlags,
                                                      pszArgs, uNumArgs,
                                                      pszEnv, cbEnv, uNumEnvVars,
                                                      pszUser, pszPassword,
                                                      uTimeLimitMS);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreate(&pThread->Thread, VBoxServiceControlExecThread,
                                (void *)(PVBOXSERVICECTRLTHREAD*)pThread, 0,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "Exec");
            if (RT_FAILURE(rc))
            {
                VBoxServiceError("ControlExec: RTThreadCreate failed, rc=%Rrc\n, pThread=%p\n",
                                 rc, pThread);
            }
            else
            {
                VBoxServiceVerbose(4, "ControlExec: Waiting for thread to initialize ...\n");

                /* Wait for the thread to initialize. */
                RTThreadUserWait(pThread->Thread, 60 * 1000);
                if (pThread->fShutdown)
                {
                    VBoxServiceError("ControlExec: Thread for process \"%s\" failed to start!\n", pszCmd);
                    rc = VERR_GENERAL_FAILURE;
                }
                else
                {
                    pThread->fStarted = true;
                    /*rc =*/ RTListAppend(&g_GuestControlExecThreads, &pThread->Node);
                }
            }

            if (RT_FAILURE(rc))
                VBoxServiceControlExecDestroyThreadData((PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData);
        }
        if (RT_FAILURE(rc))
            RTMemFree(pThread);
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   u32ClientId
 * @param   uNumParms
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
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlExec: Failed to retrieve exec start command! Error: %Rrc\n", rc);
    }
    else
    {
        rc = VBoxServiceControlExecProcess(uContextID, szCmd, uFlags, szArgs, uNumArgs,
                                           szEnv, cbEnv, uNumEnvVars,
                                           szUser, szPassword, uTimeLimitMS);
    }

    VBoxServiceVerbose(3, "ControlExec: VBoxServiceControlExecHandleCmdStartProcess returned with %Rrc\n", rc);
    return rc;
}


/**
 * Handles input for the started process by copying the received data into its
 * stdin pipe.
 *
 * @return  int
 *
 * @param   u32ClientId
 * @param   uNumParms
 */
int VBoxServiceControlExecHandleCmdSetInput(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uFlags;
    uint8_t  aBuffer[_64K];
    uint32_t cbSize;

    if (uNumParms != 5)
        return VERR_INVALID_PARAMETER;

    int rc = VbglR3GuestCtrlExecGetHostCmdInput(u32ClientId, uNumParms,
                                                &uContextID, &uPID, &uFlags,
                                                aBuffer, sizeof(aBuffer), &cbSize);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlExec: Failed to retrieve exec input command! Error: %Rrc\n", rc);
    }
    else
    {
        PVBOXSERVICECTRLTHREAD pNode = VBoxServiceControlExecFindProcess(uPID);
        if (pNode)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);
            rc = VBoxServiceControlExecWritePipeBuffer(&pData->stdIn, aBuffer, cbSize);
            if (RT_SUCCESS(rc))
            {
                size_t cbWritten;
                rc = VBoxServiceControlExecProcWriteStdIn(&pData->stdIn, pData->pipeStdInW, &cbWritten);
                if (RT_SUCCESS(rc))
                    rc = VbglR3GuestCtrlExecReportStatusIn(u32ClientId, uContextID, uPID, 0 /* Flags */,
                                                           cbWritten);
            }
        }
        else
            rc = VERR_NOT_FOUND; /* PID not found! */
    }
    VBoxServiceVerbose(3, "ControlExec: VBoxServiceControlExecHandleCmdSetInput returned with %Rrc\n", rc);
    return rc;
}


/**
 *
 *
 * @return  int
 *
 * @param   u32ClientId
 * @param   uNumParms
 */
int VBoxServiceControlExecHandleCmdGetOutput(uint32_t u32ClientId, uint32_t uNumParms)
{
    uint32_t uContextID;
    uint32_t uPID;
    uint32_t uHandleID;
    uint32_t uFlags;

    int rc = VbglR3GuestCtrlExecGetHostCmdOutput(u32ClientId, uNumParms,
                                                 &uContextID, &uPID, &uHandleID, &uFlags);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("ControlExec: Failed to retrieve exec output command! Error: %Rrc\n", rc);
    }
    else
    {
        PVBOXSERVICECTRLTHREAD pNode = VBoxServiceControlExecFindProcess(uPID);
        if (pNode)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);

            const uint32_t cbSize = _4K;
            uint32_t cbRead = cbSize;
            uint8_t *pBuf = (uint8_t*)RTMemAlloc(cbSize);
            if (pBuf)
            {
                rc = VBoxServiceControlExecReadPipeBufferContent(&pData->stdOut, pBuf, cbSize, &cbRead);
                if (RT_SUCCESS(rc))
                {
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
    VBoxServiceVerbose(3, "ControlExec: VBoxServiceControlExecHandleCmdGetOutput returned with %Rrc\n", rc);
    return rc;
}

