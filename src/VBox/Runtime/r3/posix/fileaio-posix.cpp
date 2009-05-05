/* $Id$ */
/** @file
 * IPRT - File async I/O, native implementation for POSIX compliant host platforms.
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
#include <iprt/thread.h>
#include <iprt/semaphore.h>
#include "internal/fileaio.h"

#include <aio.h>
#include <errno.h>

#define AIO_MAXIMUM_REQUESTS_PER_CONTEXT 64

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Async I/O request state.
 */
typedef struct RTFILEAIOREQINTERNAL
{
    /** The aio control block. FIRST ELEMENT! */
    struct aiocb                 AioCB;
    /** Next element in the chain. */
    struct RTFILEAIOREQINTERNAL *pNext;
    /** Flag whether this is a flush request. */
    bool                         fFlush;
    /** Flag indicating if the request was canceled. */
    volatile bool                fCanceled;
    /** Opaque user data. */
    void                        *pvUser;
    /** Number of bytes actually transfered. */
    size_t                       cbTransfered;
    /** Status code. */
    int                          Rc;
    /** Completion context we are assigned to. */
    struct RTFILEAIOCTXINTERNAL *pCtxInt;
    /** Entry in the waiting list the request is in. */
    unsigned                     iWaitingList;
    /** Magic value  (RTFILEAIOREQ_MAGIC). */
    uint32_t                     u32Magic;
} RTFILEAIOREQINTERNAL, *PRTFILEAIOREQINTERNAL;

/**
 * Async I/O completion context state.
 */
typedef struct RTFILEAIOCTXINTERNAL
{
    /** Current number of requests active on this context. */
    volatile int32_t      cRequests;
    /** Maximum number of requests this context can handle. */
    uint32_t              cMaxRequests;
    /** The ID of the thread which is currently waiting for requests. */
    volatile RTTHREAD     hThreadWait;
    /** Flag whether the thread was woken up. */
    volatile bool         fWokenUp;
    /** Flag whether the thread is currently waiting in the syscall. */
    volatile bool         fWaiting;
    /** Magic value (RTFILEAIOCTX_MAGIC). */
    uint32_t              u32Magic;
    /** Flag whether the thread was woken up due to a internal event. */
    volatile bool         fWokenUpInternal;
    /** List of new requests which needs to be inserted into apReqs by the
     *  waiting thread. */
    volatile PRTFILEAIOREQINTERNAL apReqsNewHead[5];
    /** Special entry for requests which are canceled. Because only one
     * request can be canceled at a time and the thread canceling the request
     * has to wait we need only one entry. */
    volatile PRTFILEAIOREQINTERNAL pReqToCancel;
    /** Event semaphore the canceling thread is waiting for completion of
     * the operation. */
    RTSEMEVENT            SemEventCancel;
    /** Number of elements in the waiting list. */
    unsigned              cReqsWait;
    /** First free slot in the waiting list. */
    unsigned              iFirstFree;
    /** List of requests we are currently waiting on.
     * Size depends on cMaxRequests. */
    volatile PRTFILEAIOREQINTERNAL apReqs[1];
} RTFILEAIOCTXINTERNAL, *PRTFILEAIOCTXINTERNAL;

/**
 * Internal worker for waking up the waiting thread.
 */
static void rtFileAioCtxWakeup(PRTFILEAIOCTXINTERNAL pCtxInt)
{
    /*
     * Read the thread handle before the status flag.
     * If we read the handle after the flag we might
     * end up with an invalid handle because the thread
     * waiting in RTFileAioCtxWakeup() might get scheduled
     * before we read the flag and returns.
     * We can ensure that the handle is valid if fWaiting is true
     * when reading the handle before the status flag.
     */
    RTTHREAD hThread;
    ASMAtomicReadHandle(&pCtxInt->hThreadWait, &hThread);
    bool fWaiting = ASMAtomicReadBool(&pCtxInt->fWaiting);
    if (fWaiting)
    {
        /*
         * If a thread waits the handle must be valid.
         * It is possible that the thread returns from
         * aio_suspend() before the signal is send.
         * This is no problem because we already set fWokenUp
         * to true which will let the thread return VERR_INTERRUPTED
         * and the next call to RTFileAioCtxWait() will not
         * return VERR_INTERRUPTED because signals are not saved
         * and will simply vanish if the destination thread can't
         * receive it.
         */
        Assert(hThread != NIL_RTTHREAD);
        RTThreadPoke(hThread);
    }
}

/**
 * Internal worker processing events and inserting new requests into the waiting list.
 */
static int rtFileAioCtxProcessEvents(PRTFILEAIOCTXINTERNAL pCtxInt)
{
    int rc = VINF_SUCCESS;

    /* Process new requests first. */
    bool fWokenUp = ASMAtomicXchgBool(&pCtxInt->fWokenUpInternal, false);
    if (fWokenUp)
    {
        for (unsigned iSlot = 0; iSlot < RT_ELEMENTS(pCtxInt->apReqsNewHead); iSlot++)
        {
            PRTFILEAIOREQINTERNAL pReqHead = (PRTFILEAIOREQINTERNAL)ASMAtomicXchgPtr((void* volatile*)&pCtxInt->apReqsNewHead[iSlot],
                                                                                     NULL);

            while (pReqHead)
            {
                pCtxInt->apReqs[pCtxInt->iFirstFree] = pReqHead;
                pReqHead->iWaitingList = pCtxInt->iFirstFree;
                pReqHead = pReqHead->pNext;

                /* Clear pointer to next element just for safety. */
                pCtxInt->apReqs[pCtxInt->iFirstFree]->pNext = NULL;
                pCtxInt->iFirstFree++;
                Assert(pCtxInt->iFirstFree <= pCtxInt->cMaxRequests);
            }
        }

        /* Check if a request needs to be canceled. */
        PRTFILEAIOREQINTERNAL pReqToCancel = (PRTFILEAIOREQINTERNAL)ASMAtomicReadPtr((void* volatile*)&pCtxInt->pReqToCancel);
        if (pReqToCancel)
        {
            /* Put it out of the waiting list. */
            pCtxInt->apReqs[pReqToCancel->iWaitingList] = pCtxInt->apReqs[--pCtxInt->iFirstFree];
            pCtxInt->apReqs[pReqToCancel->iWaitingList]->iWaitingList = pReqToCancel->iWaitingList;
            ASMAtomicDecS32(&pCtxInt->cRequests);
            RTSemEventSignal(pCtxInt->SemEventCancel);
        }
    }
    else
    {
        if (ASMAtomicXchgBool(&pCtxInt->fWokenUp, false))
            rc = VERR_INTERRUPTED;
    }

    return rc;
}

RTR3DECL(int) RTFileAioReqCreate(PRTFILEAIOREQ phReq)
{
    AssertPtrReturn(phReq, VERR_INVALID_POINTER);

    PRTFILEAIOREQINTERNAL pReqInt = (PRTFILEAIOREQINTERNAL)RTMemAllocZ(sizeof(RTFILEAIOREQINTERNAL));
    if (RT_UNLIKELY(!pReqInt))
        return VERR_NO_MEMORY;

    pReqInt->pCtxInt  = NULL;
    pReqInt->u32Magic = RTFILEAIOREQ_MAGIC;

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
                                            unsigned uTransferDirection,
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

    memset(&pReqInt->AioCB, 0, sizeof(struct aiocb));
    pReqInt->AioCB.aio_lio_opcode = uTransferDirection;
    pReqInt->AioCB.aio_fildes     = (int)hFile;
    pReqInt->AioCB.aio_offset     = off;
    pReqInt->AioCB.aio_nbytes     = cbTransfer;
    pReqInt->AioCB.aio_buf        = pvBuf;
    pReqInt->pvUser               = pvUser;
    pReqInt->pCtxInt              = NULL;
    pReqInt->Rc                   = VERR_FILE_AIO_IN_PROGRESS;

    return VINF_SUCCESS;
}


RTDECL(int) RTFileAioReqPrepareRead(RTFILEAIOREQ hReq, RTFILE hFile, RTFOFF off,
                                    void *pvBuf, size_t cbRead, void *pvUser)
{
    return rtFileAioReqPrepareTransfer(hReq, hFile, LIO_READ,
                                       off, pvBuf, cbRead, pvUser);
}


RTDECL(int) RTFileAioReqPrepareWrite(RTFILEAIOREQ hReq, RTFILE hFile, RTFOFF off,
                                     void *pvBuf, size_t cbWrite, void *pvUser)
{
    return rtFileAioReqPrepareTransfer(hReq, hFile, LIO_WRITE,
                                       off, pvBuf, cbWrite, pvUser);
}


RTDECL(int) RTFileAioReqPrepareFlush(RTFILEAIOREQ hReq, RTFILE hFile, void *pvUser)
{
    PRTFILEAIOREQINTERNAL pReqInt = (PRTFILEAIOREQINTERNAL)hReq;

    RTFILEAIOREQ_VALID_RETURN(pReqInt);
    Assert(hFile != NIL_RTFILE);

    pReqInt->fFlush           = true;
    pReqInt->AioCB.aio_fildes = (int)hFile;
    pReqInt->pvUser           = pvUser;

    return VINF_SUCCESS;
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

    ASMAtomicXchgBool(&pReqInt->fCanceled, true);

    int rcPosix = aio_cancel(pReqInt->AioCB.aio_fildes, &pReqInt->AioCB);

    if (rcPosix == AIO_CANCELED)
    {
        PRTFILEAIOCTXINTERNAL pCtxInt = pReqInt->pCtxInt;
        /*
         * Notify the waiting thread that the request was canceled.
         */
        AssertMsg(VALID_PTR(pCtxInt),
                  ("Invalid state. Request was canceled but wasn't submitted\n"));

        Assert(!pCtxInt->pReqToCancel);
        ASMAtomicWritePtr((void* volatile*)&pCtxInt->pReqToCancel, pReqInt);
        rtFileAioCtxWakeup(pCtxInt);

        /* Wait for acknowledge. */
        int rc = RTSemEventWait(pCtxInt->SemEventCancel, RT_INDEFINITE_WAIT);
        AssertRC(rc);

        ASMAtomicWritePtr((void* volatile*)&pCtxInt->pReqToCancel, NULL);
        return VINF_SUCCESS;
    }
    else if (rcPosix == AIO_ALLDONE)
        return VERR_FILE_AIO_COMPLETED;
    else if (rcPosix == AIO_NOTCANCELED)
        return VERR_FILE_AIO_IN_PROGRESS;
    else
        return RTErrConvertFromErrno(errno);
}


RTDECL(int) RTFileAioReqGetRC(RTFILEAIOREQ hReq, size_t *pcbTransfered)
{
    PRTFILEAIOREQINTERNAL pReqInt = hReq;
    RTFILEAIOREQ_VALID_RETURN(pReqInt);
    AssertPtrNull(pcbTransfered);

    if (  (pReqInt->Rc != VERR_FILE_AIO_IN_PROGRESS)
        && (pcbTransfered))
        *pcbTransfered = pReqInt->cbTransfered;

    return pReqInt->Rc;
}


RTDECL(int) RTFileAioCtxCreate(PRTFILEAIOCTX phAioCtx, uint32_t cAioReqsMax)
{
    PRTFILEAIOCTXINTERNAL pCtxInt;
    AssertPtrReturn(phAioCtx, VERR_INVALID_POINTER);

    pCtxInt = (PRTFILEAIOCTXINTERNAL)RTMemAllocZ(  sizeof(RTFILEAIOCTXINTERNAL)
                                                 + cAioReqsMax * sizeof(PRTFILEAIOREQINTERNAL));
    if (RT_UNLIKELY(!pCtxInt))
        return VERR_NO_MEMORY;

    /* Create event semaphore. */
    int rc = RTSemEventCreate(&pCtxInt->SemEventCancel);
    if (RT_FAILURE(rc))
    {
        RTMemFree(pCtxInt);
        return rc;
    }

    pCtxInt->u32Magic     = RTFILEAIOCTX_MAGIC;
    pCtxInt->cMaxRequests = cAioReqsMax;
    *phAioCtx = (RTFILEAIOCTX)pCtxInt;

    return VINF_SUCCESS;
}


RTDECL(int) RTFileAioCtxDestroy(RTFILEAIOCTX hAioCtx)
{
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;

    AssertPtrReturn(pCtxInt, VERR_INVALID_HANDLE);

    if (RT_UNLIKELY(pCtxInt->cRequests))
        return VERR_FILE_AIO_BUSY;

    RTSemEventDestroy(pCtxInt->SemEventCancel);
    RTMemFree(pCtxInt);

    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTFileAioCtxGetMaxReqCount(RTFILEAIOCTX hAioCtx)
{
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;

    if (hAioCtx == NIL_RTFILEAIOCTX)
        return RTFILEAIO_UNLIMITED_REQS;
    else
        return pCtxInt->cMaxRequests;
}

RTDECL(int) RTFileAioCtxAssociateWithFile(RTFILEAIOCTX hAioCtx, RTFILE hFile)
{
    return VINF_SUCCESS;
}

RTDECL(int) RTFileAioCtxSubmit(RTFILEAIOCTX hAioCtx, PRTFILEAIOREQ phReqs, size_t cReqs, size_t *pcReqs)
{
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;

    /* Parameter checks */
    AssertPtrReturn(pCtxInt, VERR_INVALID_HANDLE);
    AssertReturn(cReqs != 0, VERR_INVALID_POINTER);
    AssertPtrReturn(phReqs,  VERR_INVALID_PARAMETER);

    /* Check that we don't exceed the limit */
    if (ASMAtomicUoReadS32(&pCtxInt->cRequests) + cReqs > pCtxInt->cMaxRequests)
        return VERR_FILE_AIO_LIMIT_EXCEEDED;

    PRTFILEAIOREQINTERNAL pHead = NULL;
    for (size_t i = 0; i < cReqs; i++)
    {
        PRTFILEAIOREQINTERNAL pReqInt = phReqs[i];

        pReqInt->pCtxInt = pCtxInt;
        /* Link them together. */
        pReqInt->pNext = pHead;
        pHead = pReqInt;
    }

    int rcPosix = lio_listio(LIO_NOWAIT, (struct aiocb **)phReqs, cReqs, NULL);
    if (RT_UNLIKELY(rcPosix < 0))
        return RTErrConvertFromErrno(errno);

    ASMAtomicAddS32(&pCtxInt->cRequests, cReqs);
    *pcReqs = cReqs;

    /*
     * Forward them to the thread waiting for requests.
     * We search for a free slot first and if we don't find one
     * we will grab the first one and append our list to the existing entries.
     */
    unsigned iSlot = 0;
    while (  (iSlot < RT_ELEMENTS(pCtxInt->apReqsNewHead))
           && !ASMAtomicCmpXchgPtr((void * volatile *)&pCtxInt->apReqsNewHead[iSlot], pHead, NULL))
        iSlot++;

    if (iSlot == RT_ELEMENTS(pCtxInt->apReqsNewHead))
    {
        /* Nothing found. */
        PRTFILEAIOREQINTERNAL pOldHead = (PRTFILEAIOREQINTERNAL)ASMAtomicXchgPtr((void * volatile *)&pCtxInt->apReqsNewHead[0],
                                                                                  NULL);

        /* Find the end of the current head and link the old list to the current. */
        PRTFILEAIOREQINTERNAL pTail = pHead;
        while (pTail->pNext)
            pTail = pTail->pNext;

        pTail->pNext = pOldHead;

        ASMAtomicXchgPtr((void * volatile *)&pCtxInt->apReqsNewHead[0], pHead);
    }

    /* Set the internal wakeup flag and wakeup the thread if possible. */
    bool fWokenUp = ASMAtomicXchgBool(&pCtxInt->fWokenUpInternal, true);
    if (!fWokenUp)
        rtFileAioCtxWakeup(pCtxInt);

    return VINF_SUCCESS;
}


RTDECL(int) RTFileAioCtxWait(RTFILEAIOCTX hAioCtx, size_t cMinReqs, unsigned cMillisTimeout,
                             PRTFILEAIOREQ pahReqs, size_t cReqs, uint32_t *pcReqs)
{
    int rc = VINF_SUCCESS;
    int cRequestsCompleted = 0;
    PRTFILEAIOCTXINTERNAL pCtxInt = (PRTFILEAIOCTXINTERNAL)hAioCtx;
    struct timespec Timeout;
    struct timespec *pTimeout = NULL;
    uint64_t         StartNanoTS = 0;

    /* Check parameters. */
    AssertPtrReturn(pCtxInt, VERR_INVALID_HANDLE);
    AssertPtrReturn(pcReqs, VERR_INVALID_POINTER);
    AssertPtrReturn(pahReqs, VERR_INVALID_POINTER);
    AssertReturn(cReqs != 0, VERR_INVALID_PARAMETER);
    AssertReturn(cReqs >= cMinReqs, VERR_OUT_OF_RANGE);

    if (RT_UNLIKELY(ASMAtomicReadS32(&pCtxInt->cRequests) == 0))
        return VERR_FILE_AIO_NO_REQUEST;

    if (cMillisTimeout != RT_INDEFINITE_WAIT)
    {
        Timeout.tv_sec  = cMillisTimeout / 1000;
        Timeout.tv_nsec = (cMillisTimeout % 1000) * 1000000;
        pTimeout = &Timeout;
        StartNanoTS = RTTimeNanoTS();
    }

    /* Wait for at least one. */
    if (!cMinReqs)
        cMinReqs = 1;

    /* For the wakeup call. */
    Assert(pCtxInt->hThreadWait == NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pCtxInt->hThreadWait, RTThreadSelf());

    /* Update the waiting list once before we enter the loop. */
    rc = rtFileAioCtxProcessEvents(pCtxInt);

    while (   cMinReqs
           && RT_SUCCESS_NP(rc))
    {
        ASMAtomicXchgBool(&pCtxInt->fWaiting, true);
        int rcPosix = aio_suspend((const struct aiocb * const *)pCtxInt->apReqs,
                                  pCtxInt->iFirstFree, pTimeout);
        ASMAtomicXchgBool(&pCtxInt->fWaiting, false);
        if (rcPosix < 0)
        {
            /* Check that this is an external wakeup event. */
            if (errno == EINTR)
                rc = rtFileAioCtxProcessEvents(pCtxInt);
            else
                rc = RTErrConvertFromErrno(errno);
        }
        else
        {
            /* Requests finished. */
            unsigned iReqCurr = 0;
            int cDone = 0;

            /* Remove completed requests from the waiting list. */
            while (iReqCurr < pCtxInt->iFirstFree)
            {
                PRTFILEAIOREQINTERNAL pReq = pCtxInt->apReqs[iReqCurr];
                int rcReq = aio_error(&pReq->AioCB);

                if (rcReq != EINPROGRESS)
                {
                    /* Completed store the return code. */
                    if (rcReq == 0)
                    {
                        pReq->Rc = VINF_SUCCESS;
                        /* Call aio_return() to free ressources. */
                        pReq->cbTransfered = aio_return(&pReq->AioCB);
                    }
                    else
                        pReq->Rc = RTErrConvertFromErrno(rcReq);

                    cDone++;

                    /*
                     * Move the last entry into the current position to avoid holes
                     * but only if it is not the last element already.
                     */
                    if (pReq->iWaitingList < pCtxInt->iFirstFree - 1)
                    {
                        pCtxInt->apReqs[pReq->iWaitingList] = pCtxInt->apReqs[--pCtxInt->iFirstFree];
                        pCtxInt->apReqs[pReq->iWaitingList]->iWaitingList = pReq->iWaitingList;
                        pCtxInt->apReqs[pCtxInt->iFirstFree] = NULL;
                    }
                    else
                        pCtxInt->iFirstFree--;

                    /* Put the request into the completed list. */
                    pahReqs[cRequestsCompleted++] = pReq;
                }
                else
                    iReqCurr++;
            }

            cReqs    -= cDone;
            cMinReqs -= cDone;
            ASMAtomicSubS32(&pCtxInt->cRequests, cDone);

            if ((cMillisTimeout != RT_INDEFINITE_WAIT) && (cMinReqs > 0))
            {
                uint64_t TimeDiff;

                /* Recalculate the timeout. */
                TimeDiff = RTTimeSystemNanoTS() - StartNanoTS;
                Timeout.tv_sec = Timeout.tv_sec - (TimeDiff / 1000000);
                Timeout.tv_nsec = Timeout.tv_nsec - (TimeDiff % 1000000);
            }

            /* Check for new elements. */
            rc = rtFileAioCtxProcessEvents(pCtxInt);
        }
    }

    *pcReqs = cRequestsCompleted;
    Assert(pCtxInt->hThreadWait == RTThreadSelf());
    ASMAtomicWriteHandle(&pCtxInt->hThreadWait, NIL_RTTHREAD);

    return rc;
}


RTDECL(int) RTFileAioCtxWakeup(RTFILEAIOCTX hAioCtx)
{
    PRTFILEAIOCTXINTERNAL pCtxInt = hAioCtx;
    RTFILEAIOCTX_VALID_RETURN(pCtxInt);

    /** @todo r=bird: Define the protocol for how to resume work after calling
     *        this function. */

    bool fWokenUp = ASMAtomicXchgBool(&pCtxInt->fWokenUp, true);
    if (!fWokenUp)
        rtFileAioCtxWakeup(pCtxInt);

    return VINF_SUCCESS;
}

