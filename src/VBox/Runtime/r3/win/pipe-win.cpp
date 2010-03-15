/* $Id$ */
/** @file
 * IPRT - Anonymous Pipes, Windows Implementation.
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
#include <Windows.h>

#include <iprt/pipe.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/process.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTPIPEINTERNAL
{
    /** Magic value (RTPIPE_MAGIC). */
    uint32_t            u32Magic;
    /** The pipe handle. */
    HANDLE              hPipe;
    /** Set if this is the read end, clear if it's the write end. */
    bool                fRead;
    /** Set if there is already pending I/O. */
    bool                fIOPending;
    /** The number of users of the current mode. */
    uint32_t            cModeUsers;
    /** The overlapped I/O structure we use. */
    OVERLAPPED          Overlapped;
    /** Bounce buffer for writes. */
    uint8_t            *pbBounceBuf;
    /** Amount of used buffer space. */
    size_t              cbBounceBufUsed;
    /** Amount of allocated buffer space. */
    size_t              cbBounceBufAlloc;
    /** Critical section protecting the above members.
     * (Taking the lazy/simple approach.) */
    RTCRITSECT          CritSect;
} RTPIPEINTERNAL;


RTDECL(int)  RTPipeCreate(PRTPIPE phPipeRead, PRTPIPE phPipeWrite, uint32_t fFlags)
{
    AssertPtrReturn(phPipeRead, VERR_INVALID_POINTER);
    AssertPtrReturn(phPipeWrite, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~RTPIPE_C_VALID_MASK), VERR_INVALID_PARAMETER);

    /*
     * Create the read end of the pipe.
     */
    DWORD   dwErr;
    HANDLE  hPipeR;
    HANDLE  hPipeW;
    int     rc;
    for (;;)
    {
        static volatile uint32_t    g_iNextPipe = 0;
        char                        szName[128];
        RTStrPrintf(szName, sizeof(szName), "\\\\.\\pipe\\iprt-pipe-%u-%u", RTProcSelf(), ASMAtomicIncU32(&g_iNextPipe));

        SECURITY_ATTRIBUTES  SecurityAttributes;
        PSECURITY_ATTRIBUTES pSecurityAttributes = NULL;
        if (fFlags & RTPIPE_C_INHERIT_READ)
        {
            SecurityAttributes.nLength              = sizeof(SecurityAttributes);
            SecurityAttributes.lpSecurityDescriptor = NULL;
            SecurityAttributes.bInheritHandle       = TRUE;
            pSecurityAttributes = &SecurityAttributes;
        }

        DWORD dwOpenMode = PIPE_ACCESS_INBOUND | FILE_FLAG_OVERLAPPED;
#ifdef FILE_FLAG_FIRST_PIPE_INSTANCE
        dwOpenMode |= FILE_FLAG_FIRST_PIPE_INSTANCE;
#endif

        DWORD dwPipeMode = PIPE_TYPE_BYTE | PIPE_READMODE_BYTE | PIPE_WAIT;
#ifdef PIPE_REJECT_REMOTE_CLIENTS
        dwPipeMode |= PIPE_REJECT_REMOTE_CLIENTS;
#endif

        hPipeR = CreateNamedPipeA(szName, dwOpenMode, dwPipeMode, 1 /*nMaxInstances*/, _64K,  _64K,
                                  NMPWAIT_USE_DEFAULT_WAIT, pSecurityAttributes);
#ifdef PIPE_REJECT_REMOTE_CLIENTS
        if (hPipeR == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            dwPipeMode &= ~PIPE_REJECT_REMOTE_CLIENTS;
            hPipeR = CreateNamedPipeA(szName, dwOpenMode, dwPipeMode, 1 /*nMaxInstances*/, _64K,  _64K,
                                      NMPWAIT_USE_DEFAULT_WAIT, pSecurityAttributes);
        }
#endif
#ifdef FILE_FLAG_FIRST_PIPE_INSTANCE
        if (hPipeR == INVALID_HANDLE_VALUE && GetLastError() == ERROR_INVALID_PARAMETER)
        {
            dwOpenMode &= ~FILE_FLAG_FIRST_PIPE_INSTANCE;
            hPipeR = CreateNamedPipeA(szName, dwOpenMode, dwPipeMode, 1 /*nMaxInstances*/, _64K,  _64K,
                                      NMPWAIT_USE_DEFAULT_WAIT, pSecurityAttributes);
        }
#endif
        if (hPipeR != INVALID_HANDLE_VALUE)
        {
            /*
             * Connect to the pipe (the write end).
             * We add FILE_READ_ATTRIBUTES here to make sure we can query the
             * pipe state later on.
             */
            pSecurityAttributes = NULL;
            if (fFlags & RTPIPE_C_INHERIT_WRITE)
            {
                SecurityAttributes.nLength              = sizeof(SecurityAttributes);
                SecurityAttributes.lpSecurityDescriptor = NULL;
                SecurityAttributes.bInheritHandle       = TRUE;
                pSecurityAttributes = &SecurityAttributes;
            }

            hPipeW = CreateFileA(szName,
                                 GENERIC_WRITE | FILE_READ_ATTRIBUTES /*dwDesiredAccess*/,
                                 0 /*dwShareMode*/,
                                 pSecurityAttributes,
                                 OPEN_EXISTING /* dwCreationDisposition */,
                                 FILE_FLAG_OVERLAPPED /*dwFlagsAndAttributes*/,
                                 NULL /*hTemplateFile*/);
            if (hPipeW != INVALID_HANDLE_VALUE)
                break;
            dwErr = GetLastError();
            CloseHandle(hPipeR);
        }
        else
            dwErr = GetLastError();
        if (   dwErr != ERROR_PIPE_BUSY     /* already exist - compatible */
            && dwErr != ERROR_ACCESS_DENIED /* already exist - incompatible */)
            return RTErrConvertFromWin32(dwErr);
        /* else: try again with a new name */
    }

    /*
     * Create the two handles.
     */
    RTPIPEINTERNAL *pThisR = (RTPIPEINTERNAL *)RTMemAllocZ(sizeof(RTPIPEINTERNAL));
    if (pThisR)
    {
        RTPIPEINTERNAL *pThisW = (RTPIPEINTERNAL *)RTMemAllocZ(sizeof(RTPIPEINTERNAL));
        if (pThisW)
        {
            rc = RTCritSectInit(&pThisR->CritSect);
            if (RT_SUCCESS(rc))
            {
                rc = RTCritSectInit(&pThisW->CritSect);
                if (RT_SUCCESS(rc))
                {
                    pThisR->Overlapped.hEvent = CreateEvent(NULL, TRUE /*fManualReset*/,
                                                            TRUE /*fInitialState*/, NULL /*pName*/);
                    if (pThisR->Overlapped.hEvent != NULL)
                    {
                        pThisW->Overlapped.hEvent = CreateEvent(NULL, TRUE /*fManualReset*/,
                                                                TRUE /*fInitialState*/, NULL /*pName*/);
                        if (pThisW->Overlapped.hEvent != NULL)
                        {
                            pThisR->u32Magic        = RTPIPE_MAGIC;
                            pThisW->u32Magic        = RTPIPE_MAGIC;
                            pThisR->hPipe           = hPipeR;
                            pThisW->hPipe           = hPipeW;
                            pThisR->fRead           = true;
                            pThisW->fRead           = false;
                            pThisR->fIOPending      = false;
                            pThisW->fIOPending      = false;
                            //pThisR->cModeUsers      = 0;
                            //pThisW->cModeUsers      = 0;
                            //pThisR->pbBounceBuf     = NULL;
                            //pThisW->pbBounceBuf     = NULL;
                            //pThisR->cbBounceBufUsed = 0;
                            //pThisW->cbBounceBufUsed = 0;
                            //pThisR->cbBounceBufAlloc= 0;
                            //pThisW->cbBounceBufAlloc= 0;

                            *phPipeRead  = pThisR;
                            *phPipeWrite = pThisW;
                            return VINF_SUCCESS;
                        }
                        CloseHandle(pThisR->Overlapped.hEvent);
                    }
                    RTCritSectDelete(&pThisW->CritSect);
                }
                RTCritSectDelete(&pThisR->CritSect);
            }
            RTMemFree(pThisW);
        }
        else
            rc = VERR_NO_MEMORY;
        RTMemFree(pThisR);
    }
    else
        rc = VERR_NO_MEMORY;

    CloseHandle(hPipeR);
    CloseHandle(hPipeW);
    return rc;
}


/**
 * Common worker for handling I/O completion.
 *
 * This is used by RTPipeClose, RTPipeWrite and RTPipeWriteBlocking.
 *
 * @returns IPRT status code.
 * @param   pThis               The pipe instance handle.
 */
static int rtPipeWriteCheckCompletion(RTPIPEINTERNAL *pThis)
{
    int rc;
    DWORD dwRc = WaitForSingleObject(pThis->Overlapped.hEvent, 0);
    if (dwRc == WAIT_OBJECT_0)
    {
        DWORD cbWritten = 0;
        if (GetOverlappedResult(pThis->hPipe, &pThis->Overlapped, &cbWritten, TRUE))
        {
            for (;;)
            {
                if (cbWritten >= pThis->cbBounceBufUsed)
                {
                    pThis->fIOPending = false;
                    rc = VINF_SUCCESS;
                    break;
                }

                /* resubmit the remainder of the buffer - can this actually happen? */
                memmove(&pThis->pbBounceBuf[0], &pThis->pbBounceBuf[cbWritten], pThis->cbBounceBufUsed - cbWritten);
                rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                if (!WriteFile(pThis->hPipe, pThis->pbBounceBuf, (DWORD)pThis->cbBounceBufUsed,
                               &cbWritten, &pThis->Overlapped))
                {
                    if (GetLastError() == ERROR_IO_PENDING)
                        rc = VINF_TRY_AGAIN;
                    else
                    {
                        pThis->fIOPending = false;
                        if (GetLastError() == ERROR_NO_DATA)
                            rc = VERR_BROKEN_PIPE;
                        else
                            rc = RTErrConvertFromWin32(GetLastError());
                    }
                    break;
                }
                Assert(cbWritten > 0);
            }
        }
        else
        {
            pThis->fIOPending = false;
            rc = RTErrConvertFromWin32(GetLastError());
        }
    }
    else if (dwRc == WAIT_TIMEOUT)
        rc = VINF_TRY_AGAIN;
    else
    {
        pThis->fIOPending = false;
        if (dwRc == WAIT_ABANDONED)
            rc = VERR_INVALID_HANDLE;
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    return rc;
}



RTDECL(int)  RTPipeClose(RTPIPE hPipe)
{
    RTPIPEINTERNAL *pThis = hPipe;
    if (pThis == NIL_RTPIPE)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Do the cleanup.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, ~RTPIPE_MAGIC, RTPIPE_MAGIC), VERR_INVALID_HANDLE);
    RTCritSectEnter(&pThis->CritSect);
    Assert(pThis->cModeUsers == 0);

    if (!pThis->fRead && pThis->fIOPending)
        rtPipeWriteCheckCompletion(pThis);

    CloseHandle(pThis->hPipe);
    pThis->hPipe = INVALID_HANDLE_VALUE;

    CloseHandle(pThis->Overlapped.hEvent);
    pThis->Overlapped.hEvent = NULL;

    RTMemFree(pThis->pbBounceBuf);
    pThis->pbBounceBuf = NULL;

    RTCritSectLeave(&pThis->CritSect);
    RTCritSectDelete(&pThis->CritSect);

    RTMemFree(pThis);

    return VINF_SUCCESS;
}


RTDECL(RTHCINTPTR) RTPipeToNative(RTPIPE hPipe)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, (RTHCINTPTR)(unsigned int)-1);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, (RTHCINTPTR)(unsigned int)-1);

    return (RTHCINTPTR)pThis->hPipe;
}


RTDECL(int) RTPipeRead(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->fRead, VERR_ACCESS_DENIED);
    AssertPtr(pcbRead);
    AssertPtr(pvBuf);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent readers, sorry. */
        if (pThis->cModeUsers == 0)
        {
            pThis->cModeUsers++;

            /*
             * Kick of a an overlapped read.  It should return immedately if
             * there is bytes in the buffer.  If not, we'll cancel it and see
             * what we get back.
             */
            rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
            DWORD cbRead = 0;
            if (   cbToRead == 0
                || ReadFile(pThis->hPipe, pvBuf,
                            cbToRead <= ~(DWORD)0 ? (DWORD)cbToRead : ~(DWORD)0,
                            &cbRead, &pThis->Overlapped))
            {
                *pcbRead = cbRead;
                rc = VINF_SUCCESS;
            }
            else if (GetLastError() == ERROR_IO_PENDING)
            {
                pThis->fIOPending = true;
                RTCritSectLeave(&pThis->CritSect);

                if (!CancelIo(pThis->hPipe))
                    WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
                if (GetOverlappedResult(pThis->hPipe, &pThis->Overlapped, &cbRead, TRUE /*fWait*/))
                {
                    *pcbRead = cbRead;
                    rc = VINF_SUCCESS;
                }
                else if (GetLastError() == ERROR_OPERATION_ABORTED)
                {
                    *pcbRead = 0;
                    rc = VINF_TRY_AGAIN;
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());

                RTCritSectEnter(&pThis->CritSect);
                pThis->fIOPending = false;
            }
            else
                rc = RTErrConvertFromWin32(GetLastError());

            pThis->cModeUsers--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }
    return rc;
}


RTDECL(int) RTPipeReadBlocking(RTPIPE hPipe, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->fRead, VERR_ACCESS_DENIED);
    AssertPtr(pvBuf);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent readers, sorry. */
        if (pThis->cModeUsers == 0)
        {
            pThis->cModeUsers++;

            size_t cbTotalRead = 0;
            while (cbToRead > 0)
            {
                /*
                 * Kick of a an overlapped read.  It should return immedately if
                 * there is bytes in the buffer.  If not, we'll cancel it and see
                 * what we get back.
                 */
                rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                DWORD cbRead = 0;
                pThis->fIOPending = true;
                RTCritSectLeave(&pThis->CritSect);

                if (ReadFile(pThis->hPipe, pvBuf,
                             cbToRead <= ~(DWORD)0 ? (DWORD)cbToRead : ~(DWORD)0,
                             &cbRead, &pThis->Overlapped))
                    rc = VINF_SUCCESS;
                else if (GetLastError() == ERROR_IO_PENDING)
                {
                    WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
                    if (GetOverlappedResult(pThis->hPipe, &pThis->Overlapped, &cbRead, TRUE /*fWait*/))
                        rc = VINF_SUCCESS;
                    else
                        rc = RTErrConvertFromWin32(GetLastError());
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());

                RTCritSectEnter(&pThis->CritSect);
                pThis->fIOPending = false;
                if (RT_FAILURE(rc))
                    break;

                /* advance */
                cbToRead    -= cbRead;
                cbTotalRead += cbRead;
                pvBuf        = (uint8_t *)pvBuf + cbRead;
            }

            if (pcbRead)
            {
                *pcbRead = cbTotalRead;
                if (   RT_FAILURE(rc)
                    && cbTotalRead
                    && rc != VERR_INVALID_POINTER)
                    rc = VINF_SUCCESS;
            }

            pThis->cModeUsers--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }
    return rc;
}


RTDECL(int) RTPipeWrite(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fRead, VERR_ACCESS_DENIED);
    AssertPtr(pcbWritten);
    AssertPtr(pvBuf);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent readers, sorry. */
        if (pThis->cModeUsers == 0)
        {
            pThis->cModeUsers++;

            /* If I/O is pending, check if it has completed. */
            if (pThis->fIOPending)
                rc = rtPipeWriteCheckCompletion(pThis);
            else
                rc = VINF_SUCCESS;
            if (rc == VINF_SUCCESS)
            {
                Assert(!pThis->fIOPending);

                /* Do the bounce buffering. */
                if (    pThis->cbBounceBufAlloc < cbToWrite
                    &&  pThis->cbBounceBufAlloc < _64K)
                {
                    if (cbToWrite > _64K)
                        cbToWrite = _64K;
                    void *pv = RTMemRealloc(pThis->pbBounceBuf, RT_ALIGN_Z(cbToWrite, _1K));
                    if (pv)
                    {
                        pThis->pbBounceBuf = (uint8_t *)pv;
                        pThis->cbBounceBufAlloc = RT_ALIGN_Z(cbToWrite, _1K);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
                else if (cbToWrite > _64K)
                    cbToWrite = _64K;
                if (RT_SUCCESS(rc) && cbToWrite)
                {
                    memcpy(pThis->pbBounceBuf, pvBuf, cbToWrite);
                    pThis->cbBounceBufUsed = (uint32_t)cbToWrite;

                    /* Submit the write. */
                    rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                    DWORD cbWritten = 0;
                    if (WriteFile(pThis->hPipe, pThis->pbBounceBuf, (DWORD)pThis->cbBounceBufUsed,
                                  &cbWritten, &pThis->Overlapped))
                    {
                        *pcbWritten = cbWritten;
                        rc = VINF_SUCCESS;
                    }
                    else if (GetLastError() == ERROR_IO_PENDING)
                    {
                        *pcbWritten = cbWritten;
                        pThis->fIOPending = true;
                        rc = VINF_SUCCESS;
                    }
                    else if (GetLastError() == ERROR_NO_DATA)
                        rc = VERR_BROKEN_PIPE;
                    else
                        rc = RTErrConvertFromWin32(GetLastError());
                }
                else if (RT_SUCCESS(rc))
                    *pcbWritten = 0;
            }
            else if (RT_SUCCESS(rc))
                *pcbWritten = 0;

            pThis->cModeUsers--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }
    return rc;
}


RTDECL(int) RTPipeWriteBlocking(RTPIPE hPipe, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fRead, VERR_ACCESS_DENIED);
    AssertPtr(pvBuf);
    AssertPtrNull(pcbWritten);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        /* No concurrent readers, sorry. */
        if (pThis->cModeUsers == 0)
        {
            pThis->cModeUsers++;

            /*
             * If I/O is pending, wait for it to complete.
             */
            if (pThis->fIOPending)
            {
                rc = rtPipeWriteCheckCompletion(pThis);
                while (rc == VINF_TRY_AGAIN)
                {
                    Assert(pThis->fIOPending);
                    HANDLE hEvent = pThis->Overlapped.hEvent;
                    RTCritSectLeave(&pThis->CritSect);
                    WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
                    RTCritSectEnter(&pThis->CritSect);
                }
            }
            if (RT_SUCCESS(rc))
            {
                Assert(!pThis->fIOPending);

                /*
                 * Try write everything.
                 * No bounce buffering, cModeUsers protects us.
                 */
                size_t cbTotalWritten = 0;
                while (cbToWrite > 0)
                {
                    rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                    pThis->fIOPending = true;
                    RTCritSectLeave(&pThis->CritSect);

                    DWORD cbWritten = 0;
                    if (WriteFile(pThis->hPipe, pvBuf,
                                  cbToWrite <= ~(DWORD)0 ? (DWORD)cbToWrite : ~(DWORD)0,
                                  &cbWritten, &pThis->Overlapped))
                        rc = VINF_SUCCESS;
                    else if (GetLastError() == ERROR_IO_PENDING)
                    {
                        WaitForSingleObject(pThis->Overlapped.hEvent, INFINITE);
                        if (GetOverlappedResult(pThis->hPipe, &pThis->Overlapped, &cbWritten, TRUE /*fWait*/))
                            rc = VINF_SUCCESS;
                        else
                            rc = RTErrConvertFromWin32(GetLastError());
                    }
                    else if (GetLastError() == ERROR_NO_DATA)
                        rc = VERR_BROKEN_PIPE;
                    else
                        rc = RTErrConvertFromWin32(GetLastError());

                    RTCritSectEnter(&pThis->CritSect);
                    pThis->fIOPending = false;
                    if (RT_FAILURE(rc))
                        break;

                    /* advance */
                    pvBuf           = (char const *)pvBuf + cbWritten;
                    cbTotalWritten += cbWritten;
                    cbToWrite      -= cbWritten;
                }

                if (pcbWritten)
                {
                    *pcbWritten = cbTotalWritten;
                    if (   RT_FAILURE(rc)
                        && cbTotalWritten
                        && rc != VERR_INVALID_POINTER)
                        rc = VINF_SUCCESS;
                }
            }

            pThis->cModeUsers--;
        }
        else
            rc = VERR_WRONG_ORDER;
        RTCritSectLeave(&pThis->CritSect);
    }
    return rc;

#if 1
    return VERR_NOT_IMPLEMENTED;
#else
    int rc = rtPipeTryBlocking(pThis);
    if (RT_SUCCESS(rc))
    {
        size_t cbTotalWritten = 0;
        while (cbToWrite > 0)
        {
            ssize_t cbWritten = write(pThis->fd, pvBuf, RT_MIN(cbToWrite, SSIZE_MAX));
            if (cbWritten < 0)
            {
                rc = RTErrConvertFromErrno(errno);
                break;
            }

            /* advance */
            pvBuf           = (char const *)pvBuf + cbWritten;
            cbTotalWritten += cbWritten;
            cbToWrite      -= cbWritten;
        }

        if (pcbWritten)
        {
            *pcbWritten = cbTotalWritten;
            if (   RT_FAILURE(rc)
                && cbTotalWritten
                && rc != VERR_INVALID_POINTER)
                rc = VINF_SUCCESS;
        }

        ASMAtomicDecU32(&pThis->u32State);
    }
    return rc;
#endif
}


RTDECL(int) RTPipeFlush(RTPIPE hPipe)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!pThis->fRead, VERR_ACCESS_DENIED);

    if (!FlushFileBuffers(pThis->hPipe))
        return RTErrConvertFromWin32(GetLastError());
    return VINF_SUCCESS;
}


RTDECL(int) RTPipeSelectOne(RTPIPE hPipe, RTMSINTERVAL cMillies)
{
    RTPIPEINTERNAL *pThis = hPipe;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTPIPE_MAGIC, VERR_INVALID_HANDLE);

    uint64_t const StartMsTS = RTTimeMilliTS();

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_FAILURE(rc))
        return rc;
    for (unsigned iLoop = 0;; iLoop++)
    {
        uint8_t abBuf[4];
        bool    fPendingRead  = false;
        HANDLE  hWait         = INVALID_HANDLE_VALUE;
        if (pThis->fRead)
        {
            if (pThis->fIOPending)
                hWait = pThis->Overlapped.hEvent;
            else
            {
                /* Peek at the pipe buffer and see how many bytes it contains. */
                DWORD cbAvailable;
                if (   PeekNamedPipe(pThis->hPipe, NULL, 0, NULL, &cbAvailable, NULL)
                    && cbAvailable > 0)
                {
                    rc = VINF_SUCCESS;
                    break;
                }

                /* Start a zero byte read operation that we can wait on. */
                if (cMillies == 0)
                {
                    rc = VERR_TIMEOUT;
                    break;
                }
                AssertBreakStmt(pThis->cModeUsers == 0, rc = VERR_INTERNAL_ERROR_5);
                rc = ResetEvent(pThis->Overlapped.hEvent); Assert(rc == TRUE);
                DWORD cbRead = 0;
                if (ReadFile(pThis->hPipe, &abBuf[0], 0, &cbRead, &pThis->Overlapped))
                {
                    rc = VINF_SUCCESS;
                    if (iLoop > 10)
                        RTThreadYield();
                }
                else if (GetLastError() == ERROR_IO_PENDING)
                {
                    pThis->cModeUsers++;
                    pThis->fIOPending = true;
                    fPendingRead = true;
                    hWait = pThis->Overlapped.hEvent;
                }
                else
                    rc = RTErrConvertFromWin32(GetLastError());
            }
        }
        else
        {
            if (pThis->fIOPending)
                hWait = pThis->Overlapped.hEvent;
            else
            {
                /* If nothing pending, the next write will succeed because
                   we buffer it and pretend that it does... */
                rc = VINF_SUCCESS;
                break;
            }
        }
        if (RT_FAILURE(rc))
            break;

        /*
         * Check for timeout.
         */
        DWORD cMsMaxWait = INFINITE;
        if (   cMillies != RT_INDEFINITE_WAIT
            && (   hWait != INVALID_HANDLE_VALUE
                || iLoop > 10)
           )
        {
            uint64_t cElapsed = RTTimeMilliTS() - StartMsTS;
            if (cElapsed >= cMillies)
            {
                rc = VERR_TIMEOUT;
                break;
            }
            cMsMaxWait = cMillies - (uint32_t)cElapsed;
        }

        /*
         * Wait.
         */
        if (hWait != INVALID_HANDLE_VALUE)
        {
            RTCritSectLeave(&pThis->CritSect);

            DWORD dwRc = WaitForSingleObject(hWait, cMsMaxWait);
            if (dwRc == WAIT_OBJECT_0)
                rc = VINF_SUCCESS;
            else if (dwRc == WAIT_TIMEOUT)
                rc = VERR_TIMEOUT;
            else if (dwRc == WAIT_ABANDONED)
                rc = VERR_INVALID_HANDLE;
            else
                rc = RTErrConvertFromWin32(GetLastError());
            if (   RT_FAILURE(rc)
                && pThis->u32Magic != RTPIPE_MAGIC)
                return rc;

            RTCritSectEnter(&pThis->CritSect);
            if (fPendingRead)
            {
                pThis->cModeUsers--;
                pThis->fIOPending = false;
                if (rc != VINF_SUCCESS)
                    CancelIo(pThis->hPipe);
                DWORD cbRead = 0;
                GetOverlappedResult(pThis->hPipe, &pThis->Overlapped, &cbRead, TRUE /*fWait*/);
            }
            if (RT_FAILURE(rc))
                break;
        }
    }

    RTCritSectLeave(&pThis->CritSect);
    return rc;
}

