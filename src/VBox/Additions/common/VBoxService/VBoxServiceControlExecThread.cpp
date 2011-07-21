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

extern RTLISTNODE g_GuestControlExecThreads;
extern RTCRITSECT g_GuestControlExecThreadsCritSect;


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
        rc = VBoxServicePipeBufInit(&pData->stdOut, false /*fNeedNotificationPipe*/);
        if (RT_SUCCESS(rc))
        {
            rc = VBoxServicePipeBufInit(&pData->stdErr, false /*fNeedNotificationPipe*/);
            if (RT_SUCCESS(rc))
                rc = VBoxServicePipeBufInit(&pData->stdIn, true /*fNeedNotificationPipe*/);
        }
    }

    if (RT_FAILURE(rc))
    {
        VBoxServiceControlExecThreadDestroy(pData);
    }
    else
    {
        pThread->enmType = kVBoxServiceCtrlThreadDataExec;
        pThread->pvData = pData;
    }
    return rc;
}


/**
 *  Frees an allocated thread data structure along with all its allocated parameters.
 *
 * @param   pData          Pointer to thread data to free.
 */
void VBoxServiceControlExecThreadDestroy(PVBOXSERVICECTRLTHREADDATAEXEC pData)
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
    RTListForEach(&g_GuestControlExecThreads, pNode, VBOXSERVICECTRLTHREAD, Node)
    {
        if (   pNode->fStarted
            && pNode->enmType == kVBoxServiceCtrlThreadDataExec)
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

    int rc = RTCritSectEnter(&g_GuestControlExecThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pNode = vboxServiceControlExecThreadGetByPID(uPID);
        if (pNode)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);

            if (VBoxServicePipeBufIsEnabled(&pData->stdIn))
            {
                uint32_t cbWritten;
                /*
                 * Feed the data to the pipe.
                 */
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
        RTCritSectLeave(&g_GuestControlExecThreadsCritSect);
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

    int rc = RTCritSectEnter(&g_GuestControlExecThreadsCritSect);
    if (RT_SUCCESS(rc))
    {
        PVBOXSERVICECTRLTHREAD pNode = vboxServiceControlExecThreadGetByPID(uPID);
        if (pNode)
        {
            PVBOXSERVICECTRLTHREADDATAEXEC pData = (PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData;
            AssertPtr(pData);

            PVBOXSERVICECTRLEXECPIPEBUF pPipeBuf;
            switch (uHandleId)
            {
                case OUTPUT_HANDLE_ID_STDERR: /* StdErr */
                    pPipeBuf = &pData->stdErr;
                    break;

                case OUTPUT_HANDLE_ID_STDOUT: /* StdOut */
                default:
                    pPipeBuf = &pData->stdOut;
                    break;
            }
            AssertPtr(pPipeBuf);

            /* If the stdout pipe buffer is enabled (that is, still could be filled by a running
             * process) wait for the signal to arrive so that we don't return without any actual
             * data read. */
            if (VBoxServicePipeBufIsEnabled(pPipeBuf))
                rc = VBoxServicePipeBufWaitForEvent(pPipeBuf, uTimeout);

            if (RT_SUCCESS(rc))
            {
                uint32_t cbRead = cbSize;
                rc = VBoxServicePipeBufRead(pPipeBuf, pBuf, cbSize, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    if (pcbRead)
                        *pcbRead = cbRead;
                }
            }
        }
        else
            rc = VERR_NOT_FOUND; /* PID not found! */

        int rc2 = RTCritSectLeave(&g_GuestControlExecThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Gracefully shuts down all process execution threads.
 *
 */
void VBoxServiceControlExecThreadsShutdown(void)
{
    int rc = RTCritSectEnter(&g_GuestControlExecThreadsCritSect);
    if (RT_SUCCESS(rc))
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
                /* Wait a bit ... */
                int rc2 = RTThreadWait(pNode->Thread, 30 * 1000 /* Wait 30 seconds max. */, NULL);
                if (RT_FAILURE(rc2))
                    VBoxServiceError("Control: Thread failed to stop; rc2=%Rrc\n", rc2);
            }

            /* Destroy thread specific data. */
            switch (pNode->enmType)
            {
                case kVBoxServiceCtrlThreadDataExec:
                    VBoxServiceControlExecThreadDestroy((PVBOXSERVICECTRLTHREADDATAEXEC)pNode->pvData);
                    break;

                default:
                    break;
            }
        }

        /* Finally destroy thread list. */
        pNode = RTListGetFirst(&g_GuestControlExecThreads, VBOXSERVICECTRLTHREAD, Node);
        while (pNode)
        {
            PVBOXSERVICECTRLTHREAD pNext = RTListNodeGetNext(&pNode->Node, VBOXSERVICECTRLTHREAD, Node);
            bool fLast = RTListNodeIsLast(&g_GuestControlExecThreads, &pNode->Node);

            RTListNodeRemove(&pNode->Node);
            RTMemFree(pNode);

            if (fLast)
                break;

            pNode = pNext;
        }

        int rc2 = RTCritSectLeave(&g_GuestControlExecThreadsCritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    RTCritSectDelete(&g_GuestControlExecThreadsCritSect);
}

