/* $Id$ */
/** @file
 * VBoxServicePipeBuf - Pipe buffering.
 */

/*
 * Copyright (C) 2010-2011 Oracle Corporation
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
#include <iprt/mem.h>
#include <iprt/pipe.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>

#include "VBoxServicePipeBuf.h"

/**
 * Initializes a pipe buffer.
 *
 * @returns IPRT status code.
 * @param   pBuf                    The pipe buffer to initialize.
 * @param   uId                     The pipe's ID handle.
 * @param   fNeedNotificationPipe   Whether the buffer needs a notification
 *                                  pipe or not.
 */
int VBoxServicePipeBufInit(PVBOXSERVICECTRLEXECPIPEBUF pBuf, uint8_t uId, bool fNeedNotificationPipe)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    /** @todo Add allocation size as function parameter! */
    pBuf->uPID = 0; /* We don't have the PID yet. */
    pBuf->uPipeId = uId;
    pBuf->pbData = (uint8_t *)RTMemAlloc(_64K); /* Start with a 64k buffer. */
    AssertReturn(pBuf->pbData, VERR_NO_MEMORY);
    pBuf->cbAllocated = _64K;
    pBuf->cbSize = 0;
    pBuf->cbOffset = 0;
    pBuf->fEnabled = true;
    pBuf->fPendingClose = false;
    pBuf->fNeedNotification = fNeedNotificationPipe;
    pBuf->hNotificationPipeW = NIL_RTPIPE;
    pBuf->hNotificationPipeR = NIL_RTPIPE;
    pBuf->hEventSem = NIL_RTSEMEVENT;

    int rc = RTSemEventCreate(&pBuf->hEventSem);
    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectInit(&pBuf->CritSect);
        if (RT_SUCCESS(rc) && fNeedNotificationPipe)
            rc = RTPipeCreate(&pBuf->hNotificationPipeR, &pBuf->hNotificationPipeW, 0);
    }

    if (RT_FAILURE(rc))
    {
        if (RTCritSectIsInitialized(&pBuf->CritSect))
            RTCritSectDelete(&pBuf->CritSect);
        if (pBuf->hEventSem != NIL_RTSEMEVENT)
            RTSemEventDestroy(pBuf->hEventSem);
    }
    return rc;
}


/**
 * Reads out data from a specififed pipe buffer.
 *
 * @return  IPRT status code.
 * @param   pBuf                        Pointer to pipe buffer to read the data from.
 * @param   pbBuffer                    Pointer to buffer to store the read out data.
 * @param   cbBuffer                    Size (in bytes) of the buffer where to store the data.
 * @param   pcbToRead                   Pointer to desired amount (in bytes) of data to read,
 *                                      will reflect the actual amount read on return.
 */
int VBoxServicePipeBufRead(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                           uint8_t *pbBuffer, uint32_t cbBuffer, uint32_t *pcbToRead)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pbBuffer, VERR_INVALID_POINTER);
    AssertReturn(cbBuffer, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbToRead, VERR_INVALID_POINTER);
    AssertReturn(*pcbToRead > 0, VERR_INVALID_PARAMETER); /* Nothing to read makes no sense ... */

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pBuf->cbSize >= pBuf->cbOffset);
        uint32_t cbToRead = *pcbToRead;

        if (cbToRead > pBuf->cbSize - pBuf->cbOffset)
            cbToRead = pBuf->cbSize - pBuf->cbOffset;

        if (cbToRead > cbBuffer)
            cbToRead = cbBuffer;

        if (cbToRead)
        {
            memcpy(pbBuffer, &pBuf->pbData[pBuf->cbOffset], cbToRead);
            pBuf->cbOffset += cbToRead;

#ifdef DEBUG_andy
            VBoxServiceVerbose(4, "Pipe [%u %u 0x%p %s] read pcbToRead=%u, cbSize=%u, cbAlloc=%u, cbOff=%u\n",
                               pBuf->uPID, pBuf->uPipeId, pBuf, pBuf->fEnabled ? "EN" : "DIS", cbToRead, pBuf->cbSize, pBuf->cbAllocated, pBuf->cbOffset);
#endif
            if (pBuf->hEventSem != NIL_RTSEMEVENT)
            {
                rc = RTSemEventSignal(pBuf->hEventSem);
                AssertRC(rc);
            }

            *pcbToRead = cbToRead;
        }
        else
        {
            *pcbToRead = 0;
        }

        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Peeks for buffer data without moving the buffer's offset
 * or touching any other internal data.
 *
 * @return  IPRT status code.
 * @param   pBuf                        Pointer to pipe buffer to read the data from.
 * @param   pbBuffer                    Pointer to buffer to store the read out data.
 * @param   cbBuffer                    Size (in bytes) of the buffer where to store the data.
 * @param   cbOffset                    Offset (in bytes) where to start reading.
 * @param   pcbRead                     Pointer to desired amount (in bytes) of data to read,
 *                                      will reflect the actual amount read on return.
 * @param   pcbLeft                     Pointer to bytes left in buffer after the current read
 *                                      operation.
 */
int VBoxServicePipeBufPeek(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                           uint8_t *pbBuffer, uint32_t cbBuffer,
                           uint32_t cbOffset,
                           uint32_t *pcbRead, uint32_t *pcbLeft)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pbBuffer, VERR_INVALID_POINTER);
    AssertReturn(cbBuffer, VERR_INVALID_PARAMETER);
    AssertReturn(pBuf->cbSize >= pBuf->cbOffset, VERR_BUFFER_OVERFLOW);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (cbOffset > pBuf->cbSize)
            cbOffset = pBuf->cbSize;
        uint32_t cbToRead = pBuf->cbSize - cbOffset;
        if (cbToRead > cbBuffer)
            cbToRead = cbBuffer;
        if (cbToRead)
        {
            memcpy(pbBuffer, &pBuf->pbData[cbOffset], cbToRead);
            pbBuffer[cbBuffer - 1] = '\0';
        }
        if (pcbRead)
            *pcbRead = cbToRead;
        if (pcbLeft)
            *pcbLeft = pBuf->cbSize - (cbOffset + cbToRead);

        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Writes data into a specififed pipe buffer.
 *
 * @return  IPRT status code.
 * @param   pBuf                        Pointer to pipe buffer to write data into.
 * @param   pbData                      Pointer to byte data to write.
 * @param   cbData                      Data size (in bytes) to write.
 * @param   fPendingClose               Needs the pipe (buffer) to be closed next time we have the chance to?
 * @param   pcbWritten                  Pointer to where the amount of written bytes get stored. Optional.
 */
int VBoxServicePipeBufWriteToBuf(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                                 uint8_t *pbData, uint32_t cbData, bool fPendingClose,
                                 uint32_t *pcbWritten)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pbData, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (pBuf->fEnabled)
        {
            /* Rewind the buffer if it's empty. */
            size_t     cbInBuf   = pBuf->cbSize - pBuf->cbOffset;
            bool const fAddToSet = cbInBuf == 0;
            if (fAddToSet)
                pBuf->cbSize = pBuf->cbOffset = 0;

            /* Try and see if we can simply append the data. */
            if (cbData + pBuf->cbSize <= pBuf->cbAllocated)
            {
                memcpy(&pBuf->pbData[pBuf->cbSize], pbData, cbData);
                pBuf->cbSize += cbData;
            }
            else
            {
                /* Move any buffered data to the front. */
                cbInBuf = pBuf->cbSize - pBuf->cbOffset;
                if (cbInBuf == 0)
                    pBuf->cbSize = pBuf->cbOffset = 0;
                else if (pBuf->cbOffset) /* Do we have something to move? */
                {
                    memmove(pBuf->pbData, &pBuf->pbData[pBuf->cbOffset], cbInBuf);
                    pBuf->cbSize = cbInBuf;
                    pBuf->cbOffset = 0;
                }

                /* Do we need to grow the buffer? */
                if (cbData + pBuf->cbSize > pBuf->cbAllocated)
                {
                    size_t cbAlloc = pBuf->cbSize + cbData;
                    cbAlloc = RT_ALIGN_Z(cbAlloc, _64K);
                    void *pvNew = RTMemRealloc(pBuf->pbData, cbAlloc);
                    if (pvNew)
                    {
                        pBuf->pbData = (uint8_t *)pvNew;
                        pBuf->cbAllocated = cbAlloc;
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }

                /* Finally, copy the data. */
                if (RT_SUCCESS(rc))
                {
                    if (cbData + pBuf->cbSize <= pBuf->cbAllocated)
                    {
                        memcpy(&pBuf->pbData[pBuf->cbSize], pbData, cbData);
                        pBuf->cbSize += cbData;
                    }
                    else
                        rc = VERR_BUFFER_OVERFLOW;
                }
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * Was this the final read/write to do on this buffer? Then close it
                 * next time we have the chance to.
                 */
                if (fPendingClose)
                    pBuf->fPendingClose = fPendingClose;

                /*
                 * Wake up the thread servicing the process so it can feed it
                 * (if we have a notification helper pipe).
                 */
                if (pBuf->fNeedNotification)
                {
                    size_t cbWritten;
                    int rc2 = RTPipeWrite(pBuf->hNotificationPipeW, "i", 1, &cbWritten);

                    /* Disable notification until it is set again on successful write. */
                    pBuf->fNeedNotification = !RT_SUCCESS(rc2);
                }

                /* Report back written bytes (if wanted). */
                if (pcbWritten)
                    *pcbWritten = cbData;

                /* Only trigger signal if we really wrote something. */
                if (   cbData
                    && pBuf->hEventSem != NIL_RTSEMEVENT)
                {
                    rc = RTSemEventSignal(pBuf->hEventSem);
                    AssertRC(rc);
                }

#ifdef DEBUG_andy
                VBoxServiceVerbose(4, "Pipe [%u %u 0x%p %s] written cbData=%u, cbSize=%u, cbAlloc=%u, cbOff=%u\n",
                                   pBuf->uPID, pBuf->uPipeId, pBuf, pBuf->fEnabled ? "EN" : "DIS", cbData, pBuf->cbSize, pBuf->cbAllocated, pBuf->cbOffset);
#endif
            }
        }
        else
            rc = VERR_BAD_PIPE;

        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


int VBoxServicePipeBufWriteToPipe(PVBOXSERVICECTRLEXECPIPEBUF pBuf, RTPIPE hPipe,
                                  size_t *pcbWritten, size_t *pcbLeft)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbWritten, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbLeft, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pBuf->cbSize >= pBuf->cbOffset);
        size_t cbToWrite = pBuf->cbSize - pBuf->cbOffset;
        cbToWrite = RT_MIN(cbToWrite, _64K);

        /* Set current bytes left in pipe buffer. It's okay
         * to have no data in yet ... */
        *pcbLeft = pBuf->cbSize - pBuf->cbOffset;

        if (   pBuf->fEnabled
            && cbToWrite)
        {
            rc = RTPipeWrite(hPipe, &pBuf->pbData[pBuf->cbOffset], cbToWrite, pcbWritten);
            if (RT_SUCCESS(rc))
            {
                pBuf->fNeedNotification = true;
                if (rc != VINF_TRY_AGAIN)
                    pBuf->cbOffset += *pcbWritten;

                /* Did somebody tell us that we should come to an end,
                 * e.g. no more data coming in? */
                if (pBuf->fPendingClose)
                {
                    /* Is there more data to write out? */
                    if (pBuf->cbSize > pBuf->cbOffset)
                    {
                        /* We have a pending close, but there's more data that we can write out
                         * from buffer to pipe ... */
                        if (pBuf->fNeedNotification)
                        {
                            /* Still data to push out - so we need another
                             * poll round! Write something into the notification pipe. */
                            size_t cbWrittenIgnore;
                            int rc2 = RTPipeWrite(pBuf->hNotificationPipeW, "i", 1, &cbWrittenIgnore);
                            AssertRC(rc2);

                            /* Disable notification until it is set again on successful write. */
                            pBuf->fNeedNotification = !RT_SUCCESS(rc2);
                        }
                    }
                }

                /* Set new bytes left in pipe buffer afer we wrote to the pipe above. */
                *pcbLeft = pBuf->cbSize - pBuf->cbOffset;
            }
            else
            {
                *pcbWritten = 0;
                /* Don't set pBuf->fEnabled to false here! We just didn't write
                 * anything -- that doesn't mean this buffer is disabled (again). */
            }
        }
        else
        {
            *pcbWritten = 0;
            pBuf->fNeedNotification = pBuf->fEnabled;
        }

        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Returns whether a pipe buffer is active or not.
 *
 * @return  bool            True if pipe buffer is active, false if not.
 * @param   pBuf            The pipe buffer.
 */
bool VBoxServicePipeBufIsEnabled(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    AssertPtrReturn(pBuf, false);

    bool fEnabled = false;
    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        fEnabled = pBuf->fEnabled;
        RTCritSectLeave(&pBuf->CritSect);
    }
    return fEnabled;
}


/**
 * Returns whether a pipe buffer is in a pending close state or
 * not. This means that someone has written the last chunk into
 * the pipe buffer with the fPendingClose flag set.
 *
 * @return  bool            True if pipe buffer is in pending
 *                          close state, false if not.
 * @param   pBuf            The pipe buffer.
 */
bool VBoxServicePipeBufIsClosing(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    AssertPtrReturn(pBuf, false);

    if (!RTCritSectIsInitialized(&pBuf->CritSect))
        return false;

    bool fClosing = false;
    if (RT_SUCCESS(RTCritSectEnter(&pBuf->CritSect)))
    {
        fClosing = pBuf->fPendingClose;
        RTCritSectLeave(&pBuf->CritSect);
    }
    return fClosing;
}


/**
 * Sets the current status (enabled/disabled) of a pipe buffer.
 *
 * @return  IPRT status code.
 * @param   pBuf            The pipe buffer.
 * @param   fEnabled        Pipe buffer status to set.
 */
int VBoxServicePipeBufSetStatus(PVBOXSERVICECTRLEXECPIPEBUF pBuf, bool fEnabled)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        bool fEnabledOld = pBuf->fEnabled;
        pBuf->fEnabled = fEnabled;
        if (   pBuf->fEnabled != fEnabledOld
            && pBuf->hEventSem)
        {
            /* Let waiter know that something has changed ... */
            RTSemEventSignal(pBuf->hEventSem);
        }
        rc = RTCritSectLeave(&pBuf->CritSect);
    }
    return rc;
}


/**
 * Assigns a PID to the specified pipe buffer. Does not allow
 * setting a new PID after a PID already was set.
 *
 * @return  IPRT status code.
 * @param   pBuf            The pipe buffer.
 * @param   uPID            PID to assign.
 */
int VBoxServicePipeBufSetPID(PVBOXSERVICECTRLEXECPIPEBUF pBuf, uint32_t uPID)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        if (!pBuf->uPID)
            pBuf->uPID = uPID;
        rc = RTCritSectLeave(&pBuf->CritSect);
    }
    return rc;
}


/**
 * Waits for the pipe buffer to get notified when something changed, like
 * new data was written to or the buffer was disabled, i.e. not used anymore.
 *
 * @return  IPRT status code.
 * @param   pBuf            The pipe buffer.
 * @param   cMillies        The maximum time (in ms) to wait for the event.
 */
int VBoxServicePipeBufWaitForEvent(PVBOXSERVICECTRLEXECPIPEBUF pBuf, RTMSINTERVAL cMillies)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    /* Don't enter the critical section here; someone else could signal the event semaphore
     * and this will deadlock then ... */
    return RTSemEventWait(pBuf->hEventSem, cMillies);
}


/**
 * Deletes a pipe buffer.
 *
 * @note    Not thread safe -- only call this when nobody is relying on the
 *          data anymore!
 *
 * @param   pBuf            The pipe buffer.
 */
void VBoxServicePipeBufDestroy(PVBOXSERVICECTRLEXECPIPEBUF pBuf)
{
    AssertPtrReturnVoid(pBuf);

    if (pBuf->pbData)
    {
        pBuf->uPID = 0;
        pBuf->uPipeId = 0;
        pBuf->cbAllocated = 0;
        pBuf->cbSize = 0;
        pBuf->cbOffset = 0;

        RTPipeClose(pBuf->hNotificationPipeR);
        pBuf->hNotificationPipeR = NIL_RTPIPE;
        RTPipeClose(pBuf->hNotificationPipeW);
        pBuf->hNotificationPipeW = NIL_RTPIPE;

        if (RTCritSectIsInitialized(&pBuf->CritSect))
            RTCritSectDelete(&pBuf->CritSect);
        if (pBuf->hEventSem != NIL_RTSEMEVENT)
            RTSemEventDestroy(pBuf->hEventSem);

        RTMemFree(pBuf->pbData);
        pBuf->pbData = NULL;
    }
}

