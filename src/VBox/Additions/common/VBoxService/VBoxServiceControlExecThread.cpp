/* $Id$ */
/** @file
 * VBoxServiceControlExecThread - Thread for an executed guest process.
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
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include <VBox/HostServices/GuestControlSvc.h>

#include "VBoxServicePipeBuf.h"
#include "VBoxServiceControlExecThread.h"

extern uint32_t g_GuestControlProcsMaxKept;
extern RTLISTNODE g_GuestControlThreads;
extern RTCRITSECT g_GuestControlThreadsCritSect;

PVBOXSERVICECTRLTHREAD vboxServiceControlExecThreadGetByPID(uint32_t uPID);
int VBoxServiceControlExecThreadShutdown(const PVBOXSERVICECTRLTHREAD pThread);

/**
 *  Allocates and gives back a thread data struct which then can be used by the worker thread.
 *  Needs to be freed with VBoxServiceControlExecDestroyThreadData().
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
int VBoxServiceControlExecThreadAlloc(PVBOXSERVICECTRLTHREAD pThread,
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
            pData->papszEnv = (char **)RTMemAlloc(uNumEnvVars * sizeof(char*));
            AssertPtr(pData->papszEnv);
            pData->uNumEnvVars = uNumEnvVars;

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
                int cbStr = RTStrAPrintf(&pData->papszEnv[i++], "%s", pszCur);
                if (cbStr < 0)
                {
                    rc = VERR_NO_STR_MEMORY;
                    break;
                }
                pszCur += cbStr + 1; /* Skip terminating '\0' */
                cbLen  += cbStr + 1; /* Skip terminating '\0' */
            }
        }

        pData->pszUser = RTStrDup(pszUser);
        pData->pszPassword = RTStrDup(pszPassword);
        pData->uTimeLimitMS = uTimeLimitMS;

        /* Adjust time limit value. */
        pData->uTimeLimitMS = (   uTimeLimitMS == UINT32_MAX
                               || uTimeLimitMS == 0)
                            ? RT_INDEFINITE_WAIT : uTimeLimitMS;

        /* Init buffers. */
        rc = VBoxServicePipeBufInit(&pData->stdOut, VBOXSERVICECTRLPIPEID_STDOUT,
                                    false /*fNeedNotificationPipe*/);
        if (RT_SUCCESS(rc))
        {
            rc = VBoxServicePipeBufInit(&pData->stdErr, VBOXSERVICECTRLPIPEID_STDERR,
                                        false /*fNeedNotificationPipe*/);
            if (RT_SUCCESS(rc))
                rc = VBoxServicePipeBufInit(&pData->stdIn, VBOXSERVICECTRLPIPEID_STDIN,
                                            true /*fNeedNotificationPipe*/);
        }

        if (RT_SUCCESS(rc))
        {
            pThread->enmType = kVBoxServiceCtrlThreadDataExec;
            pThread->pvData = pData;
        }
    }

    if (RT_FAILURE(rc))
        VBoxServiceControlExecThreadDataDestroy(pData);
    return rc;
}


/**
 * Assigns a valid PID to a guest control thread and also checks if there already was
 * another (stale) guest process which was using that PID before and destroys it.
 *
 * @return  IPRT status code.
 * @param   pData          Pointer to guest control execution thread data.
 * @param   uPID           PID to assign to the specified guest control execution thread.
 */
int VBoxServiceControlExecThreadAssignPID(PVBOXSERVICECTRLTHREADDATAEXEC pData, uint32_t uPID)
{
    AssertPtrReturn(pData, VERR_INVALID_POINTER);
    AssertReturn(uPID, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        /* Search an old thread using the desired PID and shut it down completely -- it's
         * not used anymore. */
        PVBOXSERVICECTRLTHREAD pOldNode = vboxServiceControlExecThreadGetByPID(uPID);
        if (   pOldNode
            && pOldNode->pvData != pData)
        {
            PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pOldNode->Node, VBOXSERVICECTRLTHREAD, Node);

            VBoxServiceVerbose(3, "ControlExec: PID %u was used before, shutting down stale exec thread ...\n",
                               uPID);
            AssertPtr(pOldNode->pvData);
            rc = VBoxServiceControlExecThreadShutdown(pOldNode);
            if (RT_FAILURE(rc))
            {
                VBoxServiceVerbose(3, "ControlExec: Unable to shut down stale exec thread, rc=%Rrc\n", rc);
                /* Keep going. */
            }

            RTListNodeRemove(&pOldNode->Node);
            RTMemFree(pOldNode);
        }

        /* Assign PID to current thread. */
        pData->uPID = uPID;
        VBoxServicePipeBufSetPID(&pData->stdIn, pData->uPID);
        VBoxServicePipeBufSetPID(&pData->stdOut, pData->uPID);
        VBoxServicePipeBufSetPID(&pData->stdErr, pData->uPID);

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}


/**
 *  Frees an allocated thread data structure along with all its allocated parameters.
 *
 * @param   pData          Pointer to thread data to free.
 */
void VBoxServiceControlExecThreadDataDestroy(PVBOXSERVICECTRLTHREADDATAEXEC pData)
{
    if (pData)
    {
        VBoxServiceVerbose(3, "ControlExec: [PID %u]: Destroying thread data ...\n",
                           pData->uPID);

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

        VBoxServicePipeBufDestroy(&pData->stdOut);
        VBoxServicePipeBufDestroy(&pData->stdErr);
        VBoxServicePipeBufDestroy(&pData->stdIn);

        RTMemFree(pData);
        pData = NULL;
    }
}


/**
 * Finds a (formerly) started process given by its PID.
 * Internal function, does not do locking -- this must be done from the caller function!
 *
 * @return  PVBOXSERVICECTRLTHREAD      Process structure if found, otherwise NULL.
 * @param   uPID                        PID to search for.
 */
PVBOXSERVICECTRLTHREAD vboxServiceControlExecThreadGetByPID(uint32_t uPID)
{
    PVBOXSERVICECTRLTHREAD pNode = NULL;
    RTListForEach(&g_GuestControlThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
    {
        if (pNode->enmType == kVBoxServiceCtrlThreadDataExec)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            if (pData && pData->uPID == uPID)
                return pNode;
        }
    }
    return NULL;
}


/**
 * Injects input to a specified running process.
 *
 * @return  IPRT status code.
 * @param   uPID                    PID of process to set the input for.
 * @param   fPendingClose           Flag indicating whether this is the last input block sent to the process.
 * @param   pBuf                    Pointer to a buffer containing the actual input data.
 * @param   cbSize                  Size (in bytes) of the input buffer data.
 * @param   pcbWritten              Pointer to number of bytes written to the process.  Optional.
 */
int VBoxServiceControlExecThreadSetInput(uint32_t uPID, bool fPendingClose, uint8_t *pBuf,
                                         uint32_t cbSize, uint32_t *pcbWritten)
{
    AssertPtrReturn(pBuf, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pNode = vboxServiceControlExecThreadGetByPID(uPID);
        if (pNode)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);

            if (VBoxServicePipeBufIsEnabled(&pData->stdIn))
            {
                /*
                 * Feed the data to the pipe.
                 */
                uint32_t cbWritten;
                rc = VBoxServicePipeBufWriteToBuf(&pData->stdIn, pBuf,
                                                  cbSize, fPendingClose, &cbWritten);
                if (pcbWritten)
                    *pcbWritten = cbWritten;
            }
            else
            {
                /* If input buffer is not enabled anymore we cannot handle that data ... */
                rc = VERR_BAD_PIPE;
            }
        }
        else
            rc = VERR_NOT_FOUND; /* PID not found! */
        RTCritSectLeave(&g_GuestControlThreadsCritSect);
    }
    return rc;
}


/**
 * Gets output from stdout/stderr of a specified process.
 *
 * @return  IPRT status code.
 * @param   uPID                    PID of process to retrieve the output from.
 * @param   uHandleId               Stream ID (stdout = 0, stderr = 2) to get the output from.
 * @param   uTimeout                Timeout (in ms) to wait for output becoming available.
 * @param   pBuf                    Pointer to a pre-allocated buffer to store the output.
 * @param   cbSize                  Size (in bytes) of the pre-allocated buffer.
 * @param   pcbRead                 Pointer to number of bytes read.  Optional.
 */
int VBoxServiceControlExecThreadGetOutput(uint32_t uPID, uint32_t uHandleId, uint32_t uTimeout,
                                          uint8_t *pBuf, uint32_t cbSize, uint32_t *pcbRead)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertReturn(cbSize, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        const PVBOXSERVICECTRLTHREAD pThread = vboxServiceControlExecThreadGetByPID(uPID);
        if (pThread)
        {
            const PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
            AssertPtr(pData);

            PVBOXSERVICECTRLEXECPIPEBUF pPipeBuf = NULL;
            switch (uHandleId)
            {
                case OUTPUT_HANDLE_ID_STDERR: /* StdErr */
                    pPipeBuf = &pData->stdErr;
                    break;

                case OUTPUT_HANDLE_ID_STDOUT: /* StdOut */
                default: /* On VBox host < 4.1 this is 0, so default to stdout
                          * to not break things. */
                    pPipeBuf = &pData->stdOut;
                    break;
            }
            AssertPtr(pPipeBuf);

#ifdef DEBUG_andy
            VBoxServiceVerbose(4, "ControlExec: [PID %u]: Getting output from pipe buffer %u ...\n",
                               uPID, pPipeBuf->uPipeId);
#endif
            /* If the stdout pipe buffer is enabled (that is, still could be filled by a running
             * process) wait for the signal to arrive so that we don't return without any actual
             * data read. */
            bool fEnabled = VBoxServicePipeBufIsEnabled(pPipeBuf);
            if (fEnabled)
            {
#ifdef DEBUG_andy
                VBoxServiceVerbose(4, "ControlExec: [PID %u]: Waiting for pipe buffer %u (%ums)\n",
                                   uPID, pPipeBuf->uPipeId, uTimeout);
#endif
                rc = VBoxServicePipeBufWaitForEvent(pPipeBuf, uTimeout);
            }
            if (RT_SUCCESS(rc))
            {
                uint32_t cbRead = cbSize; /* Read as much as possible. */
                rc = VBoxServicePipeBufRead(pPipeBuf, pBuf, cbSize, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    if (   !cbRead
                        && fEnabled)
                    {
                        AssertReleaseMsg(!VBoxServicePipeBufIsEnabled(pPipeBuf),
                                         ("[PID %u]: Waited (%ums) for active pipe buffer %u (%u size, %u bytes left), but nothing read!\n",
                                         uPID, uTimeout, pPipeBuf->uPipeId, pPipeBuf->cbSize, pPipeBuf->cbSize - pPipeBuf->cbOffset));
                    }
                    if (pcbRead)
                        *pcbRead = cbRead;
                }
                else
                    VBoxServiceError("ControlExec: [PID %u]: Unable to read from pipe buffer %u, rc=%Rrc\n",
                                     uPID, pPipeBuf->uPipeId, rc);
            }
        }
        else
            rc = VERR_NOT_FOUND; /* PID not found! */

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


int VBoxServiceControlExecThreadRemove(uint32_t uPID)
{
    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pThread = vboxServiceControlExecThreadGetByPID(uPID);
        if (pThread)
        {
            Assert(pThread->fStarted != pThread->fStopped);
            if (pThread->fStopped) /* Only shut down stopped threads. */
            {
                VBoxServiceVerbose(4, "ControlExec: [PID %u]: Removing thread ... \n",
                                   uPID);

                rc = VBoxServiceControlExecThreadShutdown(pThread);

                RTListNodeRemove(&pThread->Node);
                RTMemFree(pThread);
            }
        }
        else
            rc = VERR_NOT_FOUND;

        int rc2 = RTCritSectLeave(&g_GuestControlThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    return rc;
}

/* Does not do locking, must be done by the caller! */
int VBoxServiceControlExecThreadShutdown(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtrReturn(pThread, VERR_INVALID_POINTER);

    if (pThread->enmType == kVBoxServiceCtrlThreadDataExec)
    {
        PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
        if (!pData) /* Already destroyed execution data. */
            return VINF_SUCCESS;
        if (pThread->fStarted)
        {
            VBoxServiceVerbose(2, "ControlExec: [PID %u]: Shutting down a still running thread without stopping is not possible!\n",
                               pData->uPID);
            return VERR_INVALID_PARAMETER;
        }

        VBoxServiceVerbose(2, "ControlExec: [PID %u]: Shutting down, will not be served anymore\n",
                           pData->uPID);
        VBoxServiceControlExecThreadDataDestroy(pData);
    }

    VBoxServiceControlThreadSignalShutdown(pThread);
    return VBoxServiceControlThreadWaitForShutdown(pThread);
}


int VBoxServiceControlExecThreadStartAllowed(bool *pbAllowed)
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
            /** @todo Put running/stopped (+ memory alloc) stats into global struct! */
            uint32_t uProcsRunning = 0;
            uint32_t uProcsStopped = 0;
            PVBOXSERVICECTRLTHREAD pNode;
            RTListForEach(&g_GuestControlThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
            {
                if (pNode->enmType == kVBoxServiceCtrlThreadDataExec)
                {
                    Assert(pNode->fStarted != pNode->fStopped);
                    if (pNode->fStarted)
                        uProcsRunning++;
                    else if (pNode->fStopped)
                        uProcsStopped++;
                    else
                        AssertMsgFailed(("Process neither started nor stopped!?\n"));
                }
            }

            VBoxServiceVerbose(2, "ControlExec: Maximum served guest processes set to %u, running=%u, stopped=%u\n",
                               g_GuestControlProcsMaxKept, uProcsRunning, uProcsStopped);

            int32_t iProcsLeft = (g_GuestControlProcsMaxKept - uProcsRunning - 1);
            if (iProcsLeft < 0)
            {
                VBoxServiceVerbose(3, "ControlExec: Maximum running guest processes reached (%u)\n",
                                   g_GuestControlProcsMaxKept);
                fLimitReached = true;
            }
            else if (uProcsStopped > (uint32_t)iProcsLeft)
            {
                uint32_t uProcsToKill = uProcsStopped - iProcsLeft;
                Assert(uProcsToKill);
                VBoxServiceVerbose(3, "ControlExec: Shutting down %ld stopped guest processes\n", uProcsToKill);

                RTListForEach(&g_GuestControlThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
                {
                    if (   pNode->enmType == kVBoxServiceCtrlThreadDataExec
                        && pNode->fStopped)
                    {
                        PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICECTRLTHREAD, Node);

                        int rc2 = VBoxServiceControlExecThreadShutdown(pNode);
                        if (RT_FAILURE(rc2))
                        {
                            VBoxServiceError("ControlExec: Unable to shut down thread due to policy, rc=%Rrc\n", rc2);
                            if (RT_SUCCESS(rc))
                                rc = rc2;
                            /* Keep going. */
                        }

                        RTListNodeRemove(&pNode->Node);
                        RTMemFree(pNode);
                        pNode = pNext;

                        Assert(uProcsToKill);
                        uProcsToKill--;
                        if (!uProcsToKill)
                            break;
                    }
                }
                Assert(uProcsToKill == 0);
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
 * Marks an guest execution thread as stopped and cleans up its internal pipe buffers.
 *
 * @param   pThread                 Pointer to guest execution thread.
 */
void VBoxServiceControlExecThreadStop(const PVBOXSERVICECTRLTHREAD pThread)
{
    AssertPtr(pThread);

    int rc = RTCritSectEnter(&g_GuestControlThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        if (pThread->fStarted)
        {
            const PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pThread->pvData;
            if (pData)
            {
                VBoxServiceVerbose(3, "ControlExec: [PID %u]: Marking as stopped\n", pData->uPID);

                VBoxServicePipeBufSetStatus(&pData->stdIn, false /* Disabled */);
                VBoxServicePipeBufSetStatus(&pData->stdOut, false /* Disabled */);
                VBoxServicePipeBufSetStatus(&pData->stdErr, false /* Disabled */);

                /* Since the process is not alive anymore, destroy its local
                 * stdin pipe buffer - it's not used anymore and can eat up quite
                 * a bit of memory. */
                VBoxServicePipeBufDestroy(&pData->stdIn);
            }

            /* Mark as stopped. */
            ASMAtomicXchgBool(&pThread->fStarted, false);
            ASMAtomicXchgBool(&pThread->fStopped, true);
        }

        RTCritSectLeave(&g_GuestControlThreadsCritSect);
    }
}

