/* $Id$ */
/** @file
 * IPRT - File async I/O, native implementation for the Windows host platform.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_DIR

#include <iprt/asm.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include "internal/fileaio.h"

#include <Windows.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Transfer direction.
 */
typedef enum TRANSFERDIRECTION
{
    TRANSFERDIRECTION_INVALID = 0,
    /** Read. */
    TRANSFERDIRECTION_READ,
    /** Write. */
    TRANSFERDIRECTION_WRITE,
    /** The usual 32-bit hack. */
    TRANSFERDIRECTION_32BIT_HACK = 0x7fffffff
} TRANSFERDIRECTION;

/**
 * Async I/O completion context state.
 */
typedef struct RTFILEAIOCTXINTERNAL
{
    /** handle to I/O completion port. */
    HANDLE            hIoCompletionPort;
    /** Current number of requests pending. */
    volatile int32_t  cRequests;
    /** Flag whether the thread was woken up. */
    volatile bool     fWokenUp;
    /** Flag whether the thread is currently waiting. */
    volatile bool     fWaiting;
    /** Magic value (RTFILEAIOCTX_MAGIC). */
    uint32_t          u32Magic;
} RTFILEAIOCTXINTERNAL;
/** Pointer to an internal context structure. */
typedef RTFILEAIOCTXINTERNAL *PRTFILEAIOCTXINTERNAL;

/**
 * Async I/O request state.
 */
typedef struct RTFILEAIOREQINTERNAL
{
    /** Overlapped structure. */
    OVERLAPPED            Overlapped;
    /** The file handle. */
    HANDLE                hFile;
    /** Kind of transfer Read/Write. */
    TRANSFERDIRECTION     enmTransferDirection;
    /** Number of bytes to transfer. */
    size_t                cbTransfer;
    /** Pointer to the buffer. */
    void                 *pvBuf;
    /** Opaque user data. */
    void                 *pvUser;
    /** Flag whether the request completed. */
    bool                  fCompleted;
    /** Number of bytes transfered successfully. */
    size_t                cbTransfered;
    /** Error code of the completed request. */
    int                   Rc;
    /** Completion context we are assigned to. */
    PRTFILEAIOCTXINTERNAL pCtxInt;
    /** Magic value  (RTFILEAIOREQ_MAGIC). */
    uint32_t              u32Magic;
} RTFILEAIOREQINTERNAL;
/** Pointer to an internal request structure. */
typedef RTFILEAIOREQINTERNAL *PRTFILEAIOREQINTERNAL;

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Id for the wakeup event. */
#define AIO_CONTEXT_WAKEUP_EVENT 1
/** Converts a pointer to an OVERLAPPED structure to a internal request. */
#define OVERLAPPED_2_RTFILEAIOREQINTERNAL(pOverlapped) ( (PRTFILEAIOREQINTERNAL)((uintptr_t)(pOverlapped) - RT_OFFSETOF(RTFILEAIOREQINTERNAL, Overlapped)) )

RTR3DECL(int) RTFileAioReqCreate(PRTFILEAIOREQ phReq)
{
    AssertPtrReturn(phReq, VERR_INVALID_POINTER);

    PRTFILEAIOREQINTERNAL pReqInt = (PRTFILEAIOREQINTERNAL)RTMemAllocZ(sizeof(RTFILEAIOREQINTERNAL));
    if (RT_UNLIKELY(!pReqInt))
        return VERR_NO_MEMORY;

    pReqInt->pCtxInt    = NULL;
    pReqInt->fCompleted = false;
    pReqInt->u32Magic   = RTFILEAIOREQ_MAGIC;

    *phReq = (RTFILEAIOREQ)pReqInt;

    return VINF_SUCCESS;
}

RTDECL(void) RTFileAioReqDestroy(RTFILEAIOREQ hReq)
{
    /*
     * Validate the handle and ignore nil.
     */
    if (hReq == NIL_RTFILEAIOREQ)
        return;
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN_VOID(pReqInt);

    /*
     * Trash the magic and free it.
     */
    ASMAtomicUoWriteU32(&pReqInt->u32Magic, ~RTFILEAIOREQ_MAGIC);
    RTMemFree(pReqInt);
}

/**
 * Worker setting up the request.
 */
DECLINLINE(int) rtFileAioReqPrepareTransfer(RTFILEAIOREQ hReq, RTFILE hFile,
                                            TRANSFERDIRECTION enmTransferDirection,
                                            RTFOFF off, void *pvBuf, size_t cbTransfer,
                                            void *pvUser)
{
    /*
     * Validate the input.
     */
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN(pReqInt);
    Assert(hFile != NIL_RTFILE);
    AssertPtr(pvBuf);
    Assert(off >= 0);
    Assert(cbTransfer > 0);

    pReqInt->enmTransferDirection  = enmTransferDirection;
    pReqInt->hFile                 = (HANDLE)hFile;
    pReqInt->Overlapped.Offset     = (DWORD)(off & 0xffffffff);
    pReqInt->Overlapped.OffsetHigh = (DWORD)(off >> 32);
    pReqInt->cbTransfer            = cbTransfer;
    pReqInt->pvBuf                 = pvBuf;
    pReqInt->pvUser                = pvUser;
    pReqInt->fCompleted            = false;

    return VINF_SUCCESS;
}

RTDECL(int) RTFileAioReqPrepareRead(RTFILEAIOREQ hReq, RTFILE hFile, RTFOFF off,
                                    void *pvBuf, size_t cbRead, void *pvUser)
{
    return rtFileAioReqPrepareTransfer(hReq, hFile, TRANSFERDIRECTION_READ,
                                       off, pvBuf, cbRead, pvUser);
}

RTDECL(int) RTFileAioReqPrepareWrite(RTFILEAIOREQ hReq, RTFILE hFile, RTFOFF off,
                                     void *pvBuf, size_t cbWrite, void *pvUser)
{
    return rtFileAioReqPrepareTransfer(hReq, hFile, TRANSFERDIRECTION_WRITE,
                                       off, pvBuf, cbWrite, pvUser);
}

RTDECL(int) RTFileAioReqPrepareFlush(RTFILEAIOREQ hReq, RTFILE hFile, void *pvUser)
{
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN(pReqInt);
    AssertReturn(hFile != NIL_RTFILE, VERR_INVALID_HANDLE);

    /** @todo: Flushing is not available */
#if 0
    return rtFileAsyncPrepareTransfer(pRequest, File, TRANSFERDIRECTION_FLUSH,
                                      0, NULL, 0, pvUser);
#endif
    return VERR_NOT_IMPLEMENTED;
}

RTDECL(void *) RTFileAioReqGetUser(RTFILEAIOREQ hReq)
{
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN_RC(pReqInt, NULL);

    return pReqInt->pvUser;
}

RTDECL(int) RTFileAioReqCancel(RTFILEAIOREQ hReq)
{
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN(pReqInt);

    /**
     * @todo r=aeichner It is not possible to cancel specific
     * requests on Windows before Vista.
     * CancelIo cancels all requests for a file issued by the
     * calling thread and CancelIoEx which does what we need
     * is only available from Vista and up.
     * The solution is to return VERR_FILE_AIO_IN_PROGRESS
     * if the request didn't completed yet.
     * Shouldn't be a big issue because a request is normally
     * only canceled if it exceeds a timeout which is quite huge.
     */
    if (pReqInt->fCompleted)
        return VERR_FILE_AIO_COMPLETED;
    else
        return VERR_FILE_AIO_IN_PROGRESS;
}

RTDECL(int) RTFileAioReqGetRC(RTFILEAIOREQ hReq, size_t *pcbTransfered)
{
    int rc = VINF_SUCCESS;
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN(pReqInt);

    if (pReqInt->fCompleted)
    {
        rc = pReqInt->Rc;
        if (*pcbTransfered)
            *pcbTransfered = pReqInt->cbTransfered;
    }
    else
        rc = VERR_FILE_AIO_IN_PROGRESS;

    return rc;
}

RTDECL(int) RTFileAioCtxCreate(PRTFILEAIOCTX phAioCtx, uint32_t cAioReqsMax)
{
    PRTFILEAIOCTXINTERNAL pCtxInt;
    AssertPtrReturn(phAioCtx, VERR_INVALID_POINTER);

    pCtxInt = (PRTFILEAIOCTXINTERNAL)RTMemAllocZ(sizeof(RTFILEAIOCTXINTERNAL));
    if (RT_UNLIKELY(!pCtxInt))
        return VERR_NO_MEMORY;

    pCtxInt->hIoCompletionPort = CreateIoCompletionPort(INVALID_HANDLE_VALUE,
                                                        NULL,
                                                        0,
                                                        0);
    if (RT_UNLIKELY(!pCtxInt->hIoCompletionPort))
    {
        RTMemFree(pCtxInt);
        return VERR_NO_MEMORY;
    }

     pCtxInt->u32Magic     = RTFILEAIOCTX_MAGIC;

    *phAioCtx = (RTFILEAIOCTX)pCtxInt;

    return VINF_SUCCESS;
}

RTDECL(int) RTFileAioCtxDestroy(RTFILEAIOCTX hAioCtx)
{
    /* Validate the handle and ignore nil. */
    if (hAioCtx == NIL_RTFILEAIOCTX)
        return VINF_SUCCESS;
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);

    /* Cannot destroy a busy context. */
    if (RT_UNLIKELY(pCtxInt->cRequests))
        return VERR_FILE_AIO_BUSY;

    CloseHandle(pCtxInt->hIoCompletionPort);
    ASMAtomicUoWriteU32(&pCtxInt->u32Magic, RTFILEAIOCTX_MAGIC_DEAD);
    RTMemFree(pCtxInt);

    return VINF_SUCCESS;
}

RTDECL(int) RTFileAioCtxAssociateWithFile(RTFILEAIOCTX hAioCtx, RTFILE hFile)
{
    int rc = VINF_SUCCESS;
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);

    HANDLE hTemp = CreateIoCompletionPort((HANDLE)hFile, pCtxInt->hIoCompletionPort, 0, 1);
    if (hTemp != pCtxInt->hIoCompletionPort)
        rc = RTErrConvertFromWin32(GetLastError());

    return rc;
}

RTDECL(uint32_t) RTFileAioCtxGetMaxReqCount(RTFILEAIOCTX hAioCtx)
{
    return RTFILEAIO_UNLIMITED_REQS;
}

RTDECL(int) RTFileAioCtxSubmit(RTFILEAIOCTX hAioCtx, PRTFILEAIOREQ pahReqs, size_t cReqs, size_t *pcReqs)
{
    /*
     * Parameter validation.
     */
    AssertPtrReturn(pcReqs, VERR_INVALID_POINTER);
    *pcReqs = 0;
    int rc = VINF_SUCCESS;
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);
    AssertReturn(cReqs > 0,  VERR_INVALID_PARAMETER);
    AssertPtrReturn(pahReqs, VERR_INVALID_POINTER);
    int i;

    for (i = 0; i < cReqs; i++)
    {
        PRTFILEAIOREQINTERNAL pReqInt = pahReqs[i];
        BOOL fSucceeded;

        if (pReqInt->enmTransferDirection == TRANSFERDIRECTION_READ)
        {
            fSucceeded = ReadFile(pReqInt->hFile, pReqInt->pvBuf,
                                  pReqInt->cbTransfer, NULL,
                                  &pReqInt->Overlapped);
        }
        else if (pReqInt->enmTransferDirection == TRANSFERDIRECTION_WRITE)
        {
            fSucceeded = WriteFile(pReqInt->hFile, pReqInt->pvBuf,
                                   pReqInt->cbTransfer, NULL,
                                   &pReqInt->Overlapped);
        }
        else
            AssertMsgFailed(("Invalid transfer direction\n"));

        if (RT_UNLIKELY(!fSucceeded && GetLastError() != ERROR_IO_PENDING))
        {
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }
    }

    *pcReqs = i;
    ASMAtomicAddS32(&pCtxInt->cRequests, i);

    return rc;
}

RTDECL(int) RTFileAioCtxWait(RTFILEAIOCTX hAioCtx, size_t cMinReqs, unsigned cMillisTimeout,
                             PRTFILEAIOREQ pahReqs, size_t cReqs, uint32_t *pcReqs)
{
    /*
     * Validate the parameters, making sure to always set pcReqs.
     */
    AssertPtrReturn(pcReqs, VERR_INVALID_POINTER);
    *pcReqs = 0; /* always set */
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);
    AssertPtrReturn(pahReqs, VERR_INVALID_POINTER);
    AssertReturn(cReqs != 0, VERR_INVALID_PARAMETER);
    AssertReturn(cReqs >= cMinReqs, VERR_OUT_OF_RANGE);

    /*
     * Can't wait if there are no requests around.
     */
    if (RT_UNLIKELY(ASMAtomicUoReadS32(&pCtxInt->cRequests) == 0))
        return VERR_FILE_AIO_NO_REQUEST;

    /* Wait for at least one. */
    if (!cMinReqs)
        cMinReqs = 1;

    /*
     * Loop until we're woken up, hit an error (incl timeout), or
     * have collected the desired number of requests.
     */
    int rc = VINF_SUCCESS;
    int cRequestsCompleted = 0;
    while (   !pCtxInt->fWokenUp
           && (cMinReqs > 0))
    {
        uint64_t     StartNanoTS = 0;
        DWORD        dwTimeout = cMillisTimeout == RT_INDEFINITE_WAIT ? INFINITE : cMillisTimeout;
        DWORD        cbTransfered;
        LPOVERLAPPED pOverlapped;
        ULONG_PTR    lCompletionKey;
        BOOL         fSucceeded;

        if (cMillisTimeout != RT_INDEFINITE_WAIT)
            StartNanoTS = RTTimeNanoTS();

        ASMAtomicXchgBool(&pCtxInt->fWaiting, true);
        fSucceeded = GetQueuedCompletionStatus(pCtxInt->hIoCompletionPort,
                                               &cbTransfered,
                                               &lCompletionKey,
                                               &pOverlapped,
                                               dwTimeout);
        ASMAtomicXchgBool(&pCtxInt->fWaiting, false);
        if (!fSucceeded)
        {
            /* Includes VERR_TIMEOUT */
            rc = RTErrConvertFromWin32(GetLastError());
            break;
        }

        /* Check if we got woken up. */
        if (lCompletionKey == AIO_CONTEXT_WAKEUP_EVENT)
            break;
        else
        {
            /* A request completed. */
            PRTFILEAIOREQINTERNAL pReqInt = OVERLAPPED_2_RTFILEAIOREQINTERNAL(pOverlapped);
            AssertPtr(pReqInt);
            Assert(pReqInt->u32Magic == RTFILEAIOREQ_MAGIC);

            /* Mark the request as finished. */
            pReqInt->fCompleted = true;

            /* completion status. */
            DWORD cbTransfered;
            fSucceeded = GetOverlappedResult(pReqInt->hFile,
                                             &pReqInt->Overlapped,
                                             &cbTransfered,
                                             FALSE);
            pReqInt->cbTransfered = cbTransfered;
            pReqInt->Rc = VINF_SUCCESS;

            pahReqs[cRequestsCompleted++] = (RTFILEAIOREQ)pReqInt;

            /* Update counter. */
            cMinReqs --;

            if (cMillisTimeout != RT_INDEFINITE_WAIT)
            {
                /* Recalculate timeout. */
                uint64_t NanoTS = RTTimeNanoTS();
                uint64_t cMilliesElapsed = (NanoTS - StartNanoTS) / 1000000;
                cMillisTimeout -= cMilliesElapsed;
            }
        }
    }

    /*
     * Update the context state and set the return value.
     */
    *pcReqs = cRequestsCompleted;
    ASMAtomicSubS32(&pCtxInt->cRequests, cRequestsCompleted);

    /*
     * Clear the wakeup flag and set rc.
     */
    if (    pCtxInt->fWokenUp
        &&  RT_SUCCESS(rc))
    {
        ASMAtomicXchgBool(&pCtxInt->fWokenUp, false);
        rc = VERR_INTERRUPTED;
    }

    return rc;
}

RTDECL(int) RTFileAioCtxWakeup(RTFILEAIOCTX hAioCtx)
{
    int rc = VINF_SUCCESS;
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);

    bool fWokenUp = ASMAtomicXchgBool(&pCtxInt->fWokenUp, true);
    bool fWaiting = ASMAtomicReadBool(&pCtxInt->fWaiting);

    if (   !fWokenUp
        && fWaiting)
    {
        BOOL fSucceeded = PostQueuedCompletionStatus(pCtxInt->hIoCompletionPort,
                                                     0, AIO_CONTEXT_WAKEUP_EVENT,
                                                     NULL);

        if (!fSucceeded)
            rc = RTErrConvertFromWin32(GetLastError());
    }

    return rc;
}

