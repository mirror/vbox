
/* $Id$ */
/** @file
 * VBoxServiceControlExec - Utility functions for process execution.
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
#include <iprt/crc32.h>
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
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe handle.
 * @param   pStdInBuf           The standard input buffer.
 */
static void VBoxServiceControlExecProcHandleStdInErrorEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                            PVBOXSERVICECTRLSTDINBUF pStdInBuf)
{
    int rc2;
    if (pStdInBuf->off < pStdInBuf->cb)
    {
        rc2 = RTPollSetRemove(hPollSet, 4 /*TXSEXECHNDID_STDIN_WRITABLE*/);
        AssertRC(rc2);
    }

    rc2 = RTPollSetRemove(hPollSet, 0 /*TXSEXECHNDID_STDIN*/);
    AssertRC(rc2);

    rc2 = RTPipeClose(*phStdInW);
    AssertRC(rc2);
    *phStdInW = NIL_RTPIPE;

    RTMemFree(pStdInBuf->pch);
    pStdInBuf->pch          = NULL;
    pStdInBuf->off          = 0;
    pStdInBuf->cb           = 0;
    pStdInBuf->cbAllocated  = 0;
    pStdInBuf->fBitBucket   = true;
}


/**
 * Try write some more data to the standard input of the child.
 *
 * @returns IPRT status code.
 * @param   pStdInBuf           The standard input buffer.
 * @param   hStdInW             The standard input pipe.
 */
static int VBoxServiceControlExecProcWriteStdIn(PVBOXSERVICECTRLSTDINBUF pStdInBuf, RTPIPE hStdInW)
{
    size_t  cbToWrite = pStdInBuf->cb - pStdInBuf->off;
    size_t  cbWritten;
    int     rc = RTPipeWrite(hStdInW, &pStdInBuf->pch[pStdInBuf->off], cbToWrite, &cbWritten);
    if (RT_SUCCESS(rc))
    {
        Assert(cbWritten == cbToWrite);
        pStdInBuf->off += cbWritten;
    }
    return rc;
}


/**
 * Handle an event indicating we can write to the standard input pipe of the
 * child process.
 *
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe.
 * @param   pStdInBuf           The standard input buffer.
 */
static void VBoxServiceControlExecProcHandleStdInWritableEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                               PVBOXSERVICECTRLSTDINBUF pStdInBuf)
{
    int rc;
    if (!(fPollEvt & RTPOLL_EVT_ERROR))
    {
        rc = VBoxServiceControlExecProcWriteStdIn(pStdInBuf, *phStdInW);
        if (RT_FAILURE(rc) && rc != VERR_BAD_PIPE)
        {
            /** @todo do we need to do something about this error condition? */
            AssertRC(rc);
        }

        if (pStdInBuf->off < pStdInBuf->cb)
        {
            rc = RTPollSetRemove(hPollSet, 4 /*TXSEXECHNDID_STDIN_WRITABLE*/);
            AssertRC(rc);
        }
    }
    else
        VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
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
 * @param   pszOpcode           The opcode for the data upload.
 *
 * @todo    Put the last 4 parameters into a struct!
 */
static int VBoxServiceControlExecProcHandleOutputEvent(PVBOXSERVICECTRLTHREAD pThread,
                                                       RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phPipeR,
                                                       uint32_t *puCrc32 , uint32_t uHandleId)
{
    Log(("VBoxServiceControlExecProcHandleOutputEvent: fPollEvt=%#x\n",  fPollEvt));

    /*
     * Try drain the pipe before acting on any errors.
     */
    int rc = VINF_SUCCESS;
    size_t  cbRead;
    uint8_t abBuf[_64K];

    AssertPtr(pThread);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    int rc2 = RTPipeRead(*phPipeR, abBuf, sizeof(abBuf), &cbRead);
    if (RT_SUCCESS(rc2) && cbRead)
    {
#if 0
        /* Only used for "real-time" stdout/stderr data; gets sent immediately (later)! */
        rc = VbglR3GuestCtrlExecSendOut(pThread->uClientID, pThread->uContextID,
                                        pData->uPID, uHandleId, 0 /* u32Flags */,
                                        abBuf, cbRead);
#endif
        rc = VBoxServiceControlExecWritePipeBuffer(&pData->stdOut, abBuf, cbRead);
        if (RT_SUCCESS(rc))
        {
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
                                                          PRTPIPE phStdInW, PVBOXSERVICECTRLSTDINBUF pStdInBuf)
{

    int rc = RTPollSetAddPipe(hPollSet, *phStdInW, RTPOLL_EVT_WRITE, 4 /*TXSEXECHNDID_STDIN_WRITABLE*/);

    return rc;
}


static int VBoxServiceControlExecProcLoop(PVBOXSERVICECTRLTHREAD pThread,
                                          RTPROCESS hProcess, RTMSINTERVAL cMillies, RTPOLLSET hPollSet,
                                          RTPIPE hStdInW, RTPIPE hStdOutR, RTPIPE hStdErrR)
{
    int                         rc;
    int                         rc2;
    VBOXSERVICECTRLSTDINBUF     StdInBuf            = { 0, 0, NULL, 0, hStdInW == NIL_RTPIPE, RTCrc32Start() };
    uint32_t                    uStdOutCrc32        = RTCrc32Start();
    uint32_t                    uStdErrCrc32        = uStdOutCrc32;
    uint64_t const              MsStart             = RTTimeMilliTS();
    RTPROCSTATUS                ProcessStatus       = { 254, RTPROCEXITREASON_ABEND };
    bool                        fProcessAlive       = true;
    bool                        fProcessTimedOut    = false;
    uint64_t                    MsProcessKilled     = UINT64_MAX;
    bool const                  fHavePipes          = hStdInW    != NIL_RTPIPE
                                                      || hStdOutR   != NIL_RTPIPE
                                                      || hStdErrR   != NIL_RTPIPE;
    RTMSINTERVAL const          cMsPollBase         = hStdInW != NIL_RTPIPE
                                                      ? 100   /* need to poll for input */
                                                      : 1000; /* need only poll for process exit and aborts */
    RTMSINTERVAL                cMsPollCur          = 0;

    AssertPtr(pThread);

    AssertPtr(pThread);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    /* Assign PID to thread data. */
    pData->uPID = hProcess;

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    VBoxServiceVerbose(3, "Control: Process started: PID=%u, CID=%u\n",
                       pData->uPID, pThread->uContextID);
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

        cMsPollCur = 0;                 /* no rest until we've checked everything. */

        if (RT_SUCCESS(rc2))
        {
            switch (idPollHnd)
            {
                case 0 /* TXSEXECHNDID_STDIN */:
                    VBoxServiceControlExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, &hStdInW, &StdInBuf);
                    break;

                case 1 /* TXSEXECHNDID_STDOUT */:
                    rc = VBoxServiceControlExecProcHandleOutputEvent(pThread, hPollSet, fPollEvt, &hStdOutR, &uStdOutCrc32, 1 /* TXSEXECHNDID_STDOUT */);
                    break;

                case 2 /*TXSEXECHNDID_STDERR */:
                    rc = VBoxServiceControlExecProcHandleOutputEvent(pThread, hPollSet, fPollEvt, &hStdErrR, &uStdErrCrc32, 2 /*TXSEXECHNDID_STDERR */);
                    break;

                case 4 /* TXSEXECHNDID_STDIN_WRITABLE */:
                    VBoxServiceControlExecProcHandleStdInWritableEvent(hPollSet, fPollEvt, &hStdInW, &StdInBuf);
                    break;

                default:
                    rc = VBoxServiceControlExecProcHandleTransportEvent(hPollSet, fPollEvt, idPollHnd, &hStdInW, &StdInBuf);
                    break;
            }
            if (RT_FAILURE(rc) || rc == VINF_EOF)
                break; /* abort command, or client dead or something */
            continue;
        }

        /*
         * Check for incoming data.
         */

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
                VBoxServiceVerbose(3, "Control: Process timed out (%ums elapsed > %ums timeout), killing ...", cMsElapsed, cMillies);

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
            MsProcessKilled = RTTimeMilliTS();
            RTProcTerminate(hProcess);
            RTThreadSleep(500);
        }

        for (size_t i = 0; i < 10; i++)
        {
            rc2 = RTProcWait(hProcess, RTPROCWAIT_FLAGS_NOBLOCK, &ProcessStatus);
            if (RT_SUCCESS(rc2))
            {
                fProcessAlive = false;
                break;
            }
            if (i >= 5)
                RTProcTerminate(hProcess);
            RTThreadSleep(i >= 5 ? 2000 : 500);
        }
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
            uStatus = PROC_STS_TOK;
        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {
            uStatus = PROC_STS_TOA;
        }
        else if (pThread->fShutdown && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {
            uStatus = PROC_STS_DWN; /* Service is stopping, process was killed. */
        }
        else if (fProcessAlive)
        {
            VBoxServiceError("Control: Process is alive when it should not!\n");
        }
        else if (MsProcessKilled != UINT64_MAX)
        {
            VBoxServiceError("Control: Process has been killed when it should not!\n");
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {
            uStatus = PROC_STS_TEN;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {
            uStatus = PROC_STS_TES;
            uFlags = ProcessStatus.iStatus;
        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {
            uStatus = PROC_STS_TEA;
            uFlags = ProcessStatus.iStatus;
        }
        else
        {
            VBoxServiceError("Control: Process has reached an undefined status!\n");
        }

        VBoxServiceVerbose(3, "Control: Process ended: PID=%u, CID=%u, Status=%u, Flags=%u\n",
                           pData->uPID, pThread->uContextID, uStatus, uFlags);
        rc = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID,
                                             pData->uPID, uStatus, uFlags,
                                             NULL /* pvData */, 0 /* cbData */);
    }
    RTMemFree(StdInBuf.pch);
    VBoxServiceVerbose(3, "Control: Process loop ended with rc=%Rrc\n", rc);
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

int VBoxServiceControlExecReadPipeBufferContent(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                                uint8_t *pbBuffer, uint32_t cbBuffer, uint32_t *pcbToRead)
{
    AssertPtr(pcbToRead);

// LOCKING

    Assert(pBuf->cbOffset >= pBuf->cbRead);
    if (*pcbToRead > pBuf->cbOffset - pBuf->cbRead)
        *pcbToRead = pBuf->cbOffset - pBuf->cbRead;

    if (*pcbToRead > cbBuffer)
        *pcbToRead = cbBuffer;

    if (*pcbToRead > 0)
    {
        memcpy(pbBuffer, pBuf->pbData + pBuf->cbRead, *pcbToRead);
        pBuf->cbRead += *pcbToRead;
    }
    else
        pbBuffer = NULL;
    return VINF_SUCCESS;
}

int VBoxServiceControlExecWritePipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                          uint8_t *pbData, uint32_t cbData)
{
    AssertPtr(pBuf);

// LOCKING

    /** @todo Use RTMemCache or RTMemObj here? */
    uint8_t *pNewBuf;
    while (pBuf->cbSize - pBuf->cbOffset < cbData)
    {
        pNewBuf = (uint8_t*)RTMemRealloc(pBuf->pbData, pBuf->cbSize + _4K);
        if (pNewBuf == NULL)
            break;
        pBuf->cbSize += _4K;
        pBuf->pbData = pNewBuf;
    }

    int rc = VINF_SUCCESS;
    if (pBuf->pbData)
    {
        memcpy(pBuf->pbData + pBuf->cbOffset, pbData, cbData);
        pBuf->cbOffset += cbData;
        /** @todo Add offset clamping! */
    }
    else
        rc = VERR_NO_MEMORY;
    return rc;
}

/** Allocates and gives back a thread data struct which then can be used by the worker thread. */
int VBoxServiceControlExecAllocateThreadData(PVBOXSERVICECTRLTHREAD pThread,
                                             uint32_t u32ContextID,
                                             const char *pszCmd, uint32_t uFlags,
                                             const char *pszArgs, uint32_t uNumArgs,
                                             const char *pszEnv, uint32_t cbEnv, uint32_t uNumEnvVars,
                                             const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
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

    /* Prepare argument list. Always add actual command line as first argument (argv[0]). */
    char *pszArgsTemp = NULL;
    int rc = RTStrAPrintf(&pszArgsTemp, "%s %s", pszCmd, (uNumArgs > 0) ? pszArgs : "");
    if (RT_SUCCESS(rc) && pszArgsTemp)
    {
        rc = RTGetOptArgvFromString(&pData->papszArgs, (int*)&pData->uNumArgs,
                                    pszArgsTemp, NULL);
        RTStrFree(pszArgsTemp);
    }

    /* Did we get at least as much arguments as told? */
    Assert(pData->uNumArgs >= uNumArgs);

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
                if (RTStrAPrintf(&pData->papszEnv[i++], "%s", pcCur) < 0)
                {
                    rc = VERR_NO_MEMORY;
                    break;
                }
                cbLen += strlen(pcCur) + 1; /* Skip terminating zero. */
                pcCur += cbLen;
            }
        }

        pData->pszStdIn = RTStrDup(pszStdIn);
        pData->pszStdOut = RTStrDup(pszStdOut);
        pData->pszStdErr = RTStrDup(pszStdErr);
        pData->pszUser = RTStrDup(pszUser);
        pData->pszPassword = RTStrDup(pszPassword);
        pData->uTimeLimitMS = uTimeLimitMS;

        /* Adjust time limit value. */
        pData->uTimeLimitMS = (   (uTimeLimitMS == UINT32_MAX)
                               || (uTimeLimitMS == 0)) ?
                               RT_INDEFINITE_WAIT : uTimeLimitMS;

        /* Init buffers. */
        pData->stdOut.pbData = NULL;
        pData->stdOut.cbSize = 0;
        pData->stdOut.cbOffset = 0;
        pData->stdOut.cbRead = 0;

        pData->stdErr.pbData = NULL;
        pData->stdErr.cbSize = 0;
        pData->stdErr.cbOffset = 0;
        pData->stdErr.cbRead = 0;
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

void VBoxServiceControlExecDestroyPipeBuffer(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    if (pBuf)
    {
        if (pBuf->pbData)
            RTMemFree(pBuf->pbData);
        pBuf->pbData = NULL;
        pBuf->cbSize = 0;
        pBuf->cbOffset = 0;
        pBuf->cbRead = 0;
    }
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
        RTStrFree(pData->pszStdIn);
        RTStrFree(pData->pszStdOut);
        RTStrFree(pData->pszStdErr);
        RTStrFree(pData->pszUser);
        RTStrFree(pData->pszPassword);
    
        VBoxServiceControlExecDestroyPipeBuffer(&pData->stdOut);
        VBoxServiceControlExecDestroyPipeBuffer(&pData->stdErr);

        RTMemFree(pData);
        pData = NULL;
    }
}

DECLCALLBACK(int) VBoxServiceControlExecProcessWorker(PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtr(pThread);
    PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
    AssertPtr(pData);

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());
    VBoxServiceVerbose(3, "Control: Thread of process \"%s\" started\n", pData->pszCmd);

    int rc = VbglR3GuestCtrlConnect(&pThread->uClientID);
    if (RT_FAILURE(rc))
    {
        VBoxServiceError("Control: Thread failed to connect to the guest control service, aborted! Error: %Rrc\n", rc);
        return rc;
    }

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
            RTPIPE      hStdInW;
            rc = VBoxServiceControlExecSetupPipe(0 /* stdin */, &hStdIn, &phStdIn, &hStdInW);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE    hStdOut;
                PRTHANDLE   phStdOut;
                RTPIPE      hStdOutR;
                rc = VBoxServiceControlExecSetupPipe(1 /* stdout */, &hStdOut, &phStdOut, &hStdOutR);
                if (RT_SUCCESS(rc))
                {
                    RTHANDLE    hStdErr;
                    PRTHANDLE   phStdErr;
                    RTPIPE      hStdErrR;
                    rc = VBoxServiceControlExecSetupPipe(2 /* stderr */, &hStdErr, &phStdErr, &hStdErrR);
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
                            rc = RTPollSetAddPipe(hPollSet, hStdInW, RTPOLL_EVT_ERROR, 0 /* TXSEXECHNDID_STDIN */);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdOutR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 1 /* TXSEXECHNDID_STDOUT */);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdErrR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 2 /* TXSEXECHNDID_TESTPIPE */);
                            if (RT_SUCCESS(rc))
                            {
                                RTPROCESS hProcess;
                                rc = RTProcCreateEx(pData->pszCmd, pData->papszArgs, hEnv, pData->uFlags,
                                                    phStdIn, phStdOut, phStdErr,
                                                    strlen(pData->pszUser) ? pData->pszUser : NULL,
                                                    strlen(pData->pszUser) && strlen(pData->pszPassword) ? pData->pszPassword : NULL,
                                                    &hProcess);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * Close the child ends of any pipes and redirected files.
                                     */
                                    int rc2 = RTHandleClose(phStdIn);   AssertRC(rc2);
                                    phStdIn    = NULL;
                                    rc2 = RTHandleClose(phStdOut);  AssertRC(rc2);
                                    phStdOut   = NULL;
                                    rc2 = RTHandleClose(phStdErr);  AssertRC(rc2);
                                    phStdErr   = NULL;

                                    /* Enter the process loop. */
                                    rc = VBoxServiceControlExecProcLoop(pThread,
                                                                        hProcess, pData->uTimeLimitMS, hPollSet,
                                                                        hStdInW, hStdOutR, hStdErrR);

                                    /*
                                     * The handles that are no longer in the set have
                                     * been closed by the above call in order to prevent
                                     * the guest from getting stuck accessing them.
                                     * So, NIL the handles to avoid closing them again.
                                     */
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 0 /* stdin */, NULL)))
                                        hStdInW = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 1 /* stdout */, NULL)))
                                        hStdOutR = NIL_RTPIPE;
                                    if (RT_FAILURE(RTPollSetQueryHandle(hPollSet, 2 /* stderr */, NULL)))
                                        hStdErrR = NIL_RTPIPE;
                                }
                                else /* Something went wrong; report error! */
                                {
                                    int rc2 = VbglR3GuestCtrlExecReportStatus(pThread->uClientID, pThread->uContextID, pData->uPID,
                                                                              PROC_STS_ERROR, rc,
                                                                              NULL /* pvData */, 0 /* cbData */);
                                    if (RT_FAILURE(rc2))
                                        VBoxServiceError("Control: Could not report process start error! Error: %Rrc (process error %Rrc)\n",
                                                         rc2, rc);
                                }
                            }
                        }
                        RTPipeClose(hStdErrR);
                        RTHandleClose(phStdErr);
                    }
                    RTPipeClose(hStdOutR);
                    RTHandleClose(phStdOut);
                }
                RTPipeClose(hStdInW);
                RTHandleClose(phStdIn);
            }
        }
        RTEnvDestroy(hEnv);
    }

    VbglR3GuestCtrlDisconnect(pThread->uClientID);
    VBoxServiceVerbose(3, "Control: Thread of process \"%s\" (PID: %u) ended with rc=%Rrc\n",
                       pData->pszCmd, pData->uPID, rc);
    return rc;
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
                                  const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
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
                                                      pszStdIn, pszStdOut, pszStdErr,
                                                      pszUser, pszPassword,
                                                      uTimeLimitMS);
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadCreate(&pThread->Thread, VBoxServiceControlExecThread,
                                (void *)(PVBOXSERVICECTRLTHREAD*)pThread, 0,
                                RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "Exec");
            if (RT_FAILURE(rc))
            {
                VBoxServiceError("Control: RTThreadCreate failed, rc=%Rrc\n, pThread=%p\n",
                                 rc, pThread);
            }
            else
            {
                /* Wait for the thread to initialize. */
                RTThreadUserWait(pThread->Thread, 60 * 1000);
                if (pThread->fShutdown)
                {
                    VBoxServiceError("Control: Thread for process \"%s\" failed to start!\n", pszCmd);
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

