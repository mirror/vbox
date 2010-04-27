/* $Id$ */
/** @file
 *
 * Testcase for the guest control service.
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
#include <VBox/HostServices/GuestControlSvc.h>
#include <iprt/alloca.h>
#include <iprt/initterm.h>
#include <iprt/crc32.h>
#include <iprt/ctype.h>
#include <iprt/env.h>
#include <iprt/file.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/thread.h>

#include "../gctrl.h"

using namespace guestControl;

extern "C" DECLCALLBACK(DECLEXPORT(int)) VBoxHGCMSvcLoad (VBOXHGCMSVCFNTABLE *pTable);
char g_szImageName[RTPATH_MAX];

/** Prototypes. */
int guestExecSendStdOut(VBOXHGCMSVCFNTABLE *pTable,
                        const char *pszStdOut, uint32_t cbStdOut);

/** Simple call handle structure for the guest call completion callback */
struct VBOXHGCMCALLHANDLE_TYPEDEF
{
    /** Where to store the result code */
    int32_t rc;
};

/**
 * For buffering process input supplied by the client.
 */
typedef struct TXSEXECSTDINBUF
{
    /** The mount of buffered data. */
    size_t  cb;
    /** The current data offset. */
    size_t  off;
    /** The data buffer. */
    char   *pch;
    /** The amount of allocated buffer space. */
    size_t  cbAllocated;
    /** Send further input into the bit bucket (stdin is dead). */
    bool    fBitBucket;
    /** The CRC-32 for standard input (received part). */
    uint32_t uCrc32;
} TXSEXECSTDINBUF;
/** Pointer to a standard input buffer. */
typedef TXSEXECSTDINBUF *PTXSEXECSTDINBUF;

/** Call completion callback for guest calls. */
static void callComplete(VBOXHGCMCALLHANDLE callHandle, int32_t rc)
{
    callHandle->rc = rc;
}

/**
 * Initialise the HGCM service table as much as we need to start the
 * service
 * @param  pTable the table to initialise
 */
void initTable(VBOXHGCMSVCFNTABLE *pTable, VBOXHGCMSVCHELPERS *pHelpers)
{
    pTable->cbSize = sizeof (VBOXHGCMSVCFNTABLE);
    pTable->u32Version = VBOX_HGCM_SVC_VERSION;
    pHelpers->pfnCallComplete = callComplete;
    pTable->pHelpers = pHelpers;
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
static int guestExecSetupPipe(int fd, PRTHANDLE ph, PRTHANDLE *pph, PRTPIPE phPipe)
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

/**
 * Handle an error event on standard input.
 *
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phStdInW            The standard input pipe handle.
 * @param   pStdInBuf           The standard input buffer.
 */
static void guestExecProcHandleStdInErrorEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                               PTXSEXECSTDINBUF pStdInBuf)
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
static int guestExecProcWriteStdIn(PTXSEXECSTDINBUF pStdInBuf, RTPIPE hStdInW)
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
static void guestExecProcHandleStdInWritableEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phStdInW,
                                                  PTXSEXECSTDINBUF pStdInBuf)
{
    int rc;
    if (!(fPollEvt & RTPOLL_EVT_ERROR))
    {
        rc = guestExecProcWriteStdIn(pStdInBuf, *phStdInW);
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
        guestExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, phStdInW, pStdInBuf);
}

/**
 * Handle pending output data or error on standard out, standard error or the
 * test pipe.
 *
 * @returns IPRT status code from client send.
 * @param   hPollSet            The polling set.
 * @param   fPollEvt            The event mask returned by RTPollNoResume.
 * @param   phPipeR             The pipe handle.
 * @param   pu32Crc             The current CRC-32 of the stream. (In/Out)
 * @param   uHandleId           The handle ID.
 * @param   pszOpcode           The opcode for the data upload.
 *
 * @todo    Put the last 4 parameters into a struct!
 */
static int guestExecProcHandleOutputEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, PRTPIPE phPipeR,
                                          uint32_t *puCrc32 , uint32_t uHandleId)
{
    Log(("guestExecProcHandleOutputEvent: fPollEvt=%#x\n",  fPollEvt));

    /*
     * Try drain the pipe before acting on any errors.
     */
    int rc = VINF_SUCCESS;

    char    abBuf[_64K];
    size_t  cbRead;
    int     rc2 = RTPipeRead(*phPipeR, abBuf, sizeof(abBuf), &cbRead);
    if (RT_SUCCESS(rc2) && cbRead)
    {
        Log(("Crc32=%#x ", *puCrc32));

#if 1
            abBuf[cbRead] = '\0';
            RTPrintf("%s: %s\n", uHandleId == 1 ? "StdOut: " : "StdErr: ", abBuf);
#endif

        /**puCrc32 = RTCrc32Process(*puCrc32, abBuf, cbRead);
        Log(("cbRead=%#x Crc32=%#x \n", cbRead, *puCrc32));
        Pkt.uCrc32 = RTCrc32Finish(*puCrc32);*/
       /* if (g_fDisplayOutput)
        {
            if (enmHndId == TXSEXECHNDID_STDOUT)
                RTStrmPrintf(g_pStdErr, "%.*s", cbRead, Pkt.abBuf);
            else if (enmHndId == TXSEXECHNDID_STDERR)
                RTStrmPrintf(g_pStdErr, "%.*s", cbRead, Pkt.abBuf);
        }

        rc = txsReplyInternal(&Pkt.Hdr, pszOpcode, cbRead + sizeof(uint32_t));*/

        /* Make sure we go another poll round in case there was too much data
           for the buffer to hold. */
        fPollEvt &= RTPOLL_EVT_ERROR;
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
static int guestExecProcHandleTransportEvent(RTPOLLSET hPollSet, uint32_t fPollEvt, uint32_t idPollHnd,
                                             PRTPIPE phStdInW, PTXSEXECSTDINBUF pStdInBuf)
{

    int rc = RTPollSetAddPipe(hPollSet, *phStdInW, RTPOLL_EVT_WRITE, 4 /*TXSEXECHNDID_STDIN_WRITABLE*/);

    return rc;
}

static int guestExecProcLoop(VBOXHGCMSVCFNTABLE *pTable,
                             RTPROCESS hProcess, RTMSINTERVAL cMillies, RTPOLLSET hPollSet,
                             RTPIPE hStdInW, RTPIPE hStdOutR, RTPIPE hStdErrR)
{
    int                 rc;
    int                 rc2;
    TXSEXECSTDINBUF     StdInBuf            = { 0, 0, NULL, 0, hStdInW == NIL_RTPIPE, RTCrc32Start() };
    uint32_t            uStdOutCrc32        = RTCrc32Start();
    uint32_t            uStdErrCrc32        = uStdOutCrc32;
    uint64_t const      MsStart             = RTTimeMilliTS();
    RTPROCSTATUS        ProcessStatus       = { 254, RTPROCEXITREASON_ABEND };
    bool                fProcessAlive       = true;
    bool                fProcessTimedOut    = false;
    uint64_t            MsProcessKilled     = UINT64_MAX;
    bool const          fHavePipes          = hStdInW    != NIL_RTPIPE
                                           || hStdOutR   != NIL_RTPIPE
                                           || hStdErrR   != NIL_RTPIPE;
    RTMSINTERVAL const  cMsPollBase         = hStdInW != NIL_RTPIPE
                                              ? 100   /* need to poll for input */
                                              : 1000; /* need only poll for process exit and aborts */
    RTMSINTERVAL        cMsPollCur          = 0;

    /*
     * Before entering the loop, tell the host that we've started the guest
     * and that it's now OK to send input to the process.
     */
    rc = VINF_SUCCESS;

    /*
     * Process input, output, the test pipe and client requests.
     */
    while (RT_SUCCESS(rc))
    {
        /*
         * Wait/Process all pending events.
         */
        uint32_t idPollHnd;
        uint32_t fPollEvt;
        rc2 = RTPollNoResume(hPollSet, cMsPollCur, &fPollEvt, &idPollHnd);

        cMsPollCur = 0;                 /* no rest until we've checked everything. */

        if (RT_SUCCESS(rc2))
        {
            switch (idPollHnd)
            {
                case 0 /* TXSEXECHNDID_STDIN */:
                    guestExecProcHandleStdInErrorEvent(hPollSet, fPollEvt, &hStdInW, &StdInBuf);
                    break;

                case 1 /* TXSEXECHNDID_STDOUT */:
                    rc = guestExecProcHandleOutputEvent(hPollSet, fPollEvt, &hStdOutR, &uStdOutCrc32, 1 /* TXSEXECHNDID_STDOUT */);
                    break;

                case 2 /*TXSEXECHNDID_STDERR */:
                    rc = guestExecProcHandleOutputEvent(hPollSet, fPollEvt, &hStdErrR, &uStdErrCrc32, 2 /*TXSEXECHNDID_STDERR */);
                    break;

                case 4 /* TXSEXECHNDID_STDIN_WRITABLE */:
                    guestExecProcHandleStdInWritableEvent(hPollSet, fPollEvt, &hStdInW, &StdInBuf);
                    break;

                default:
                    rc = guestExecProcHandleTransportEvent(hPollSet, fPollEvt, idPollHnd, &hStdInW, &StdInBuf);
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
        if (     fProcessTimedOut  && !fProcessAlive && MsProcessKilled != UINT64_MAX)
        {

        }
        else if (fProcessTimedOut  &&  fProcessAlive && MsProcessKilled != UINT64_MAX)
        {

        }
        /*else if (g_fTerminate && (fProcessAlive || MsProcessKilled != UINT64_MAX))
        {

        }*/
        else if (fProcessAlive)
        {

        }
        else if (MsProcessKilled != UINT64_MAX)
        {

        }
        else if (   ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL
                 && ProcessStatus.iStatus   == 0)
        {

        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_NORMAL)
        {

        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_SIGNAL)
        {

        }
        else if (ProcessStatus.enmReason == RTPROCEXITREASON_ABEND)
        {

        }
        else
        {

        }
    }

    RTMemFree(StdInBuf.pch);
    return rc;
}

int guestExecProcess(VBOXHGCMSVCFNTABLE *pTable,
                     uint32_t fFlags, const char *pszExecName,
                     uint32_t cArgs,    const char * const *papszArgs,
                     uint32_t cEnvVars, const char * const *papszEnv,
                     const char *pszStdIn, const char *pszStdOut, const char *pszStdErr,
                     const char *pszUsername, const char *pszPassword, RTMSINTERVAL cMillies)
{
#if 0
    /* Print some info */
    RTPrintf("Flags: %u\n"
             "# Args: %u\n"
             "# Env: %u\n"
             "StdIn: %s, StdOut: %s, StdErr: %s\n"
             "User: %s, Timelimit: %u\n",
                fFlags, cArgs, cEnvVars,
                pszStdIn, pszStdOut, pszStdErr,
                pszUsername ? pszUsername : "<None>", cMillies);
    for (uint32_t i=0; i<cArgs; i++)
        RTPrintf("Arg %u: %s\n", i, papszArgs[i]);
    for (uint32_t i=0; i<cEnvVars; i++)
        RTPrintf("Env %u: %s\n", i, papszEnv[i]);
#endif

    /*
     * Create the environment.
     */
    RTENV hEnv;
    int rc = RTEnvClone(&hEnv, RTENV_DEFAULT);
    if (RT_SUCCESS(rc))
    {
        size_t i;
        for (i = 0; i < cEnvVars; i++)
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
            RTPIPE      hStdInW;
            rc = guestExecSetupPipe(0 /* stdin */, &hStdIn, &phStdIn, &hStdInW);
            if (RT_SUCCESS(rc))
            {
                RTHANDLE    hStdOut;
                PRTHANDLE   phStdOut;
                RTPIPE      hStdOutR;
                rc = guestExecSetupPipe(1 /* stdout */, &hStdOut, &phStdOut, &hStdOutR);
                if (RT_SUCCESS(rc))
                {
                    RTHANDLE    hStdErr;
                    PRTHANDLE   phStdErr;
                    RTPIPE      hStdErrR;
                    rc = guestExecSetupPipe(2 /* stderr */, &hStdErr, &phStdErr, &hStdErrR);
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
                                rc = RTPollSetAddPipe(hPollSet, hStdOutR,   RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 1 /* TXSEXECHNDID_STDOUT */);
                            if (RT_SUCCESS(rc))
                                rc = RTPollSetAddPipe(hPollSet, hStdErrR,   RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 2 /* TXSEXECHNDID_TESTPIPE */);
                            if (RT_SUCCESS(rc))
                            {
                                RTPROCESS hProcess;
                                rc = RTProcCreateEx(pszExecName, papszArgs, hEnv, 0 /*fFlags*/,
                                                    phStdIn, phStdOut, phStdErr,
                                                    /*pszUsername, pszPassword,*/ NULL, NULL,
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

                                    rc = guestExecProcLoop(pTable,
                                                           hProcess, cMillies, hPollSet,
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

    return rc;
}

int guestExecHandleCmdExecute(VBOXHGCMSVCFNTABLE *pTable, PVBOXHGCMSVCPARM paParms, uint32_t cParms)
{
    char *pcBuf;
    uint32_t cbLen;

    if(cParms < 13)
        return VERR_INVALID_PARAMETER;

    /* flags */
    uint32_t uFlags;
    int rc = paParms[1].getUInt32(&uFlags);
    if (RT_SUCCESS(rc))
    {
        /* cmd */
        rc = paParms[2].getString(&pcBuf, &cbLen);
        char *pszExecName = RTStrDup(pcBuf);

        /* arguments */
        if (   pszExecName
            && RT_SUCCESS(rc))
        {
            /* argc */
            uint32_t uArgc;
            paParms[3].getUInt32(&uArgc);

            /* argv */
            char *pcData;
            paParms[4].getBuffer((void**)&pcData, &cbLen);
            AssertPtr(pcData);

            char **ppaArg;
            int iArgs;
            rc = RTGetOptArgvFromString(&ppaArg, &iArgs, pcData, NULL);
            Assert(uArgc == iArgs);

            /* environment */
            if (RT_SUCCESS(rc))
            {
                /* envc */
                uint32_t uEnvc;
                paParms[5].getUInt32(&uEnvc);

                /* envv */
                paParms[6].getBuffer((void**)&pcData, &cbLen);
                AssertPtr(pcData);

                char **ppaEnv = NULL;
                if (uEnvc)
                {
                    ppaEnv = (char**)RTMemAlloc(uEnvc * sizeof(char*));
                    AssertPtr(ppaEnv);

                    char *pcCur = pcData;
                    uint32_t i = 0;
                    while (pcCur < pcData + cbLen)
                    {
                        if (RTStrAPrintf(&ppaEnv[i++], "%s", pcCur) < 0)
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }
                        pcCur += strlen(pcCur) + 1; /* Skip terminating zero. */
                    }
                }
                if (RT_SUCCESS(rc))
                {
                    /* stdin */
                    rc = paParms[7].getString(&pcBuf, &cbLen);
                    char *pszStdIn = RTStrDup(pcBuf);
                    if (   pszStdIn
                        && RT_SUCCESS(rc))
                    {
                        /* stdout */
                        rc = paParms[8].getString(&pcBuf, &cbLen);
                        char *pszStdOut = RTStrDup(pcBuf);
                        if (   pszStdOut
                            && RT_SUCCESS(rc))
                        {
                            /* stderr */
                            rc = paParms[9].getString(&pcBuf, &cbLen);
                            char *pszStdErr = RTStrDup(pcBuf);
                            if (   pszStdErr
                                && RT_SUCCESS(rc))
                            {
                                /* user */
                                rc = paParms[10].getString(&pcBuf, &cbLen);
                                char *pszUser = RTStrDup(pcBuf);
                                if (   pszUser
                                    && RT_SUCCESS(rc))
                                {
                                    /* password */
                                    rc = paParms[11].getString(&pcBuf, &cbLen);
                                    char *pszPassword = RTStrDup(pcBuf);
                                    if (RT_SUCCESS(rc))
                                    {
                                        /* time to wait (0 = max, no time limit) */
                                        RTMSINTERVAL msTimeLimit;
                                        rc = paParms[12].getUInt32(&msTimeLimit);
                                        if (RT_SUCCESS(rc))
                                        {
                                            rc = guestExecProcess(pTable,
                                                                  uFlags, pszExecName,
                                                                  uArgc, ppaArg,
                                                                  uEnvc, ppaEnv,
                                                                  pszStdIn, pszStdOut, pszStdIn,
                                                                  pszUser, pszPassword,
                                                                  msTimeLimit == UINT32_MAX ? RT_INDEFINITE_WAIT : msTimeLimit);
                                        }
                                    }
                                    RTStrFree(pszUser);
                                }
                                RTStrFree(pszStdErr);
                            }
                            RTStrFree(pszStdOut);
                        }
                        RTStrFree(pszStdIn);
                    }
                    for (uint32_t i=0; i<uEnvc; i++)
                        RTStrFree(ppaEnv[i]);
                    RTMemFree(ppaEnv);
                }
                RTGetOptArgvFree(ppaArg);
            }
            RTStrFree(pszExecName);
        }
    }
    return rc;
}

/** Sends the current stdout (stderr later?) data to the host. */
int guestExecSendStdOut(VBOXHGCMSVCFNTABLE *pTable,
                        const char *pszStdOut, uint32_t cbStdOut)
{
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    int command = GUEST_EXEC_SEND_STDOUT;
    VBOXHGCMSVCPARM paParms[3];

    if (   pszStdOut == NULL
        || cbStdOut == 0)
    {
        return VERR_INVALID_PARAMETER;
    }

    paParms[0].setUInt32(123 /* @todo PID. */);
    paParms[1].setUInt32(1 /* @todo Pipe ID. */);
    paParms[2].setPointer((void*)pszStdOut, cbStdOut + 1);

    pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL, command,
                    3, paParms);
    int rc = VINF_SUCCESS;
    if (RT_FAILURE(callHandle.rc))
    {
        RTPrintf("guestSendStdOut failed with callHandle.rc=%Rrc\n", callHandle.rc);
        rc = callHandle.rc;
    }
    return rc;
}

/** Gets a new message (command) from the host and does the appropriate action. */
int guestGetHostMsg(VBOXHGCMSVCFNTABLE *pTable)
{
    VBOXHGCMCALLHANDLE_TYPEDEF callHandle = { VINF_SUCCESS };
    int command = GUEST_GET_HOST_MSG;
    VBOXHGCMSVCPARM paParms[13];
    /* Work around silly constant issues - we ought to allow passing
     * constant strings in the hgcm parameters. */
    pTable->pfnCall(pTable->pvService, &callHandle, 0, NULL, command,
                    13, paParms);
    int rc;
    if (RT_FAILURE(callHandle.rc))
    {
        RTPrintf("guestGetHostMsg failed with callHandle.rc=%Rrc\n", callHandle.rc);
        rc = callHandle.rc;
    }
    else
    {
        uint32_t uCmd;

        /*
         * Parameter 0 *always* specifies the actual command the host wants
         * to execute.
         */
        rc = paParms[0].getUInt32(&uCmd);
        if (RT_SUCCESS(rc))
        {
            switch(uCmd)
            {
                case GETHOSTMSG_EXEC_CMD:
                    rc = guestExecHandleCmdExecute(pTable, paParms, 13);
                    break;

                case GETHOSTMSG_EXEC_STDIN:
                    //rc = guestExecHandleCmdStdIn(pTable, paParms, 12);
                    break;

                default:
                    RTPrintf("guestGetHostMsg: Invalid host command: %u\n", uCmd);
                    rc = VERR_INVALID_PARAMETER;
                    break;
            }
        }
    }
    return rc;
}

/** This represents the execution service thread in VBoxService. */
static DECLCALLBACK(int) guestThread(RTTHREAD Thread, void *pvUser)
{
    VBOXHGCMSVCFNTABLE *pTable = (VBOXHGCMSVCFNTABLE*)pvUser;
    AssertPtr(pTable);

    int rc = guestGetHostMsg(pTable);
    if(RT_FAILURE(rc))
        return rc;

    /* We don't remove the current request from the host queue yet,
     * so don't try to get a new host message more than once atm. */

    for(;;) /* Run forever atm. */
    {
        RTThreadSleep(1);
        /** @tdo Add graceful shutdown here. */
    }
    return rc;
}

int hostExecCmd(VBOXHGCMSVCFNTABLE *pTable,
                uint32_t u32Flags,
                const char *pszCmd,
                uint32_t u32Argc,
                const void *pvArgs,
                uint32_t cbArgs,
                uint32_t u32Envc,
                const void *pvEnv,
                uint32_t cbEnv,
                const char *pszStdin,
                const char *pszStdOut,
                const char *pszStdErr,
                const char *pszUser,
                const char *pszPassword,
                uint32_t u32TimeLimit)
{
    int command = HOST_EXEC_CMD;
    VBOXHGCMSVCPARM paParms[13];

    /** @todo Put some assert macros here! */
    paParms[0].setUInt32(HOST_EXEC_CMD);
    paParms[1].setUInt32(u32Flags);
    paParms[2].setPointer((void*)pszCmd, (uint32_t)strlen(pszCmd) + 1);
    paParms[3].setUInt32(u32Argc);
    paParms[4].setPointer((void*)pvArgs, cbArgs);
    paParms[5].setUInt32(u32Envc);
    paParms[6].setPointer((void*)pvEnv, cbEnv);
    paParms[7].setPointer((void*)pszStdin, (uint32_t)strlen(pszStdin) + 1);
    paParms[8].setPointer((void*)pszStdOut, (uint32_t)strlen(pszStdOut) + 1);
    paParms[9].setPointer((void*)pszStdErr, (uint32_t)strlen(pszStdErr) + 1);
    paParms[10].setPointer((void*)pszUser, (uint32_t)strlen(pszUser) + 1);
    paParms[11].setPointer((void*)pszPassword, (uint32_t)strlen(pszPassword) + 1);
    paParms[12].setUInt32(u32TimeLimit);

    int rc = pTable->pfnHostCall(pTable->pvService, command,
                                 13, paParms);
    if (RT_FAILURE(rc))
    {
        RTPrintf("hostDoExecute() call failed with rc=%Rrc\n", rc);
    }
    return rc;
}

int hostExecGetStatus(VBOXHGCMSVCFNTABLE *pTable)
{
    int command = HOST_EXEC_GET_STATUS;
    VBOXHGCMSVCPARM paParms[13];

    /** @todo Put some assert macros here! */
    paParms[0].setUInt32(0 /* @todo 0=Execute */);

    return 0;
}

/**
 * Test the EXECUTE function.
 * @returns iprt status value to indicate whether the test went as expected.
 * @note    prints its own diagnostic information to stdout.
 */
int testExecute(VBOXHGCMSVCFNTABLE *pTable)
{
    RTPrintf("Testing the EXECUTE call.\n");
    if (!VALID_PTR(pTable->pfnHostCall))
    {
        RTPrintf("Invalid pfnHostCall() pointer\n");
        return VERR_INVALID_POINTER;
    }

    int rc = VINF_SUCCESS;
    /*
     * The host code (= later Main?) will run in this thread,
     * while the client (guest) code will run in another one (= VBoxService in guest).
     */
    RTTHREAD threadGuest;
    rc = RTThreadCreate(&threadGuest, guestThread, pTable /* Save call table. */,
                        0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "GUEST");

    /* This is the host code. */
    if(RT_SUCCESS(rc))
    {
        void *pvArgs;
        uint32_t cArgs;
        uint32_t cbArgs;

        void *pvEnv = NULL;
        uint32_t cEnv;
        uint32_t cbEnv;

        char szCmdLine[_1K];

        /* Test stdout*/
        RTStrPrintf(szCmdLine, sizeof(szCmdLine),
                    "%s --test-stdout", g_szImageName); /* Include image name as argv[0]. */
        rc = gctrlPrepareExecArgv(szCmdLine, &pvArgs, &cbArgs, &cArgs);
        if (RT_SUCCESS(rc))
        {
            rc = gctrlAddToExecEnvv("VBOX_LOG=all", &pvEnv, &cbEnv, &cEnv);
            if (RT_SUCCESS(rc))
                rc = gctrlAddToExecEnvv("VBOX_LOG_DEST=file=c:\\baz\\barfoo.txt", &pvEnv, &cbEnv, &cEnv);
            if (RT_SUCCESS(rc))
                rc = gctrlAddToExecEnvv("HOME=iN-WoNdeRlAnd", &pvEnv, &cbEnv, &cEnv);
            if (RT_SUCCESS(rc))
            {
                rc = hostExecCmd(pTable,
                                 456123,
                                 g_szImageName,
                                 cArgs,
                                 pvArgs,
                                 cbArgs,
                                 cEnv,
                                 pvEnv,
                                 cbEnv,
                                 "|",
                                 "|",
                                 "|",
                                 "vbox",
                                 "password",
                                 UINT32_MAX);
                RTMemFree(pvEnv);
            }
            RTMemFree(pvArgs);
        }
        if (RT_SUCCESS(rc))
        {
            for(;;)
            {
                RTThreadSleep(1);
            }
        }

        /* Wait for guest thread to finish. */
        int rc2;
        rc = RTThreadWait(threadGuest, RT_INDEFINITE_WAIT, &rc2);
        if (RT_FAILURE(rc))
            RTPrintf("RTThreadWait failed, rc=%Rrc\n", rc);
        else if (RT_FAILURE(rc2))
            RTPrintf("Thread failed, rc2=%Rrc\n", rc2);
    }
    return rc;
}

int selfTestExecStdOut(int h, int argc, char **argv)
{
    RTStrmPrintf(g_pStdOut, "This is the first text to StdOut!\n");
    RTStrmPrintf(g_pStdErr, "Hum, this really should be ignored because it's stderr! :-/\n");
    /*for (int i=0; i<10; i++)
    {
        RTStrmPrintf(g_pStdOut, "This is the i=%d to StdOut! Waiting ...\n", i+1);
        RTStrmPrintf(g_pStdErr, "This is the i=%d to StdErr!\n", i+1);
        RTThreadSleep(1000);

    }*/
    RTThreadSleep(5000);
    return VINF_SUCCESS;
}

RTEXITCODE selfTestExecStd(int h, int argc, char **argv)
{
    int rc = VINF_SUCCESS;

#if 0
    /* Dump arguments and environment. */
    RTENV env;
    rc = RTEnvCreate(&env);
    if (RT_SUCCESS(rc))
    {
        const char *const *pcEnv = RTEnvGetExecEnvP(env);
        RTPrintf("Environment: %s\n" , pcEnv);
        rc = RTEnvDestroy(env);
    }
#endif

    /* Do the test(s) based on the handle number(s). */
    switch (h)
    {
        case 0: /* stdin */
            break;

        case 1: /* stdout */
            rc = selfTestExecStdOut(h, argc, argv);
            break;

        case 2: /* stderr */
            break;

        case 3: /* all std* */
            break;
    }

    return RTEXITCODE_SUCCESS;
}

/**
 * Parses the arguments.
 *
 * @returns Exit code. Exit if this is non-zero or @a *pfExit is set.
 * @param   argc                The number of arguments.
 * @param   argv                The argument vector.
 * @param   pfExit              For indicating exit when the exit code is zero.
 */
static RTEXITCODE parseArgv(int argc, char **argv, bool *pfExit)
{
#if 1
    RTPrintf("ArgC: %d\n", argc);
    for (int i=0; i<argc; i++)
        RTPrintf("ArgV: %s\n", argv[i]);
#endif

    if (argc == 2)
    {
        *pfExit = true;
        if (!strcmp(argv[1], "--test-stdin"))
            return selfTestExecStd(0 /* stdin */, argc, argv);
        else if (!strcmp(argv[1], "--test-stdout"))
            return selfTestExecStd(1 /* stdout */, argc, argv);
        else if (!strcmp(argv[1], "--test-stderr"))
            return selfTestExecStd(2 /* stderr */, argc, argv);
        else if (!strcmp(argv[1], "--test-all"))
            return selfTestExecStd(3 /* all */, argc, argv);

        RTPrintf("Unknown test! Exit.\n");
        return RTEXITCODE_FAILURE;
    }
    return RTEXITCODE_SUCCESS;
}

int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
        return RTMsgInitFailure(rc);

    /* Save image name for later use. */
    if (!RTProcGetExecutableName(g_szImageName, sizeof(g_szImageName)))
        return RTMsgErrorExit(RTEXITCODE_FAILURE, "RTProcGetExecutableName failed\n");

    VBOXHGCMSVCFNTABLE svcTable;
    VBOXHGCMSVCHELPERS svcHelpers;

    initTable(&svcTable, &svcHelpers);

    RTPrintf("PID: %u\n", RTProcSelf ());

    bool fExit = false;
    rc = parseArgv(argc, argv, &fExit);
    if (rc || fExit)
        return rc;

    /* The function is inside the service, not HGCM. */
    if (RT_FAILURE(VBoxHGCMSvcLoad(&svcTable)))
    {
        RTPrintf("Failed to start the HGCM service.\n");
        return 1;
    }
    if (RT_FAILURE(testExecute(&svcTable)))
        return 1;
    RTPrintf("tstGuestControlSvc: SUCCEEDED.\n");
    return 0;
}
