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
 * @param   fNeedNotificationPipe   Whether the buffer needs a notification
 *                                  pipe or not.
 */
int VBoxServicePipeBufInit(PVBOXSERVICECTRLEXECPIPEBUF pBuf, bool fNeedNotificationPipe)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);

    /** @todo Add allocation size as function parameter! */
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
        {
            rc = RTPipeCreate(&pBuf->hNotificationPipeR, &pBuf->hNotificationPipeW, 0);
            if (RT_FAILURE(rc))
            {
                RTCritSectDelete(&pBuf->CritSect);
                if (pBuf->hEventSem != NIL_RTSEMEVENT)
                    RTSemEventDestroy(pBuf->hEventSem);
            }
        }
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
        if (*pcbToRead > pBuf->cbSize - pBuf->cbOffset)
            *pcbToRead = pBuf->cbSize - pBuf->cbOffset;

        if (*pcbToRead > cbBuffer)
            *pcbToRead = cbBuffer;

        if (*pcbToRead > 0)
        {
            memcpy(pbBuffer, pBuf->pbData + pBuf->cbOffset, *pcbToRead);
            pBuf->cbOffset += *pcbToRead;

            if (pBuf->hEventSem != NIL_RTSEMEVENT)
            {
                rc = RTSemEventSignal(pBuf->hEventSem);
                AssertRC(rc);
            }

#ifdef DEBUG_andy
            VBoxServiceVerbose(4, "PipeBuf[0x%p]: read=%u, size=%u, alloc=%u, off=%u\n",
                                   pBuf, *pcbToRead, pBuf->cbSize, pBuf->cbAllocated, pBuf->cbOffset);
#endif
        }
        else
        {
            pbBuffer = NULL;
            *pcbToRead = 0;
        }

        int rc2 = RTCritSectLeave(&pBuf->CritSect);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


int VBoxServicePipeBufPeek(PVBOXSERVICECTRLEXECPIPEBUF pBuf,
                           uint8_t *pbBuffer, uint32_t cbBuffer,
                           uint32_t cbOffset,
                           uint32_t *pcbRead, uint32_t *pcbLeft)
{
    AssertPtrReturn(pBuf, VERR_INVALID_POINTER);
    AssertPtrReturn(pbBuffer, VERR_INVALID_POINTER);
    AssertReturn(cbBuffer, VERR_INVALID_PARAMETER);

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        Assert(pBuf->cbSize >= pBuf->cbOffset);
        if (cbOffset > pBuf->cbSize)
            cbOffset = pBuf->cbSize;
        uint32_t cbToRead = pBuf->cbSize - cbOffset;
        if (cbToRead > cbBuffer)
            cbToRead = cbBuffer;
        if (cbToRead)
        {
            memcpy(pbBuffer, pBuf->pbData + cbOffset, cbToRead);
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

    int rc = RTCritSectEnter(&pBuf->CritSect);
    if (RT_SUCCESS(rc))
    {
        AssertPtrReturn(pbData, VERR_INVALID_POINTER);
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

                if (pBuf->hEventSem != NIL_RTSEMEVENT)
                {
                    rc = RTSemEventSignal(pBuf->hEventSem);
                    AssertRC(rc);
                }

#ifdef DEBUG_andy
                VBoxServiceVerbose(4, "PipeBuf[0x%p]: written=%u, size=%u, alloc=%u, off=%u\n",
                                   pBuf, cbData, pBuf->cbSize, pBuf->cbAllocated, pBuf->cbOffset);
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
                pBuf->fEnabled = false;
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
    if (   RTCritSectIsInitialized(&pBuf->CritSect)
        && RT_SUCCESS(RTCritSectEnter(&pBuf->CritSect)))
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

    bool fClosing = false;
    if (   RTCritSectIsInitialized(&pBuf->CritSect)
        && RT_SUCCESS(RTCritSectEnter(&pBuf->CritSect)))
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
        pBuf->fEnabled = fEnabled;
        /* Let waiter know that something has changed ... */
        if (pBuf->hEventSem)
            RTSemEventSignal(pBuf->hEventSem);
        rc = RTCritSectLeave(&pBuf->CritSect);
    }
    return rc;
}


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
    AssertPtr(pBuf);
    if (pBuf->pbData)
    {
        RTMemFree(pBuf->pbData);
        pBuf->pbData = NULL;
        pBuf->cbAllocated = 0;
        pBuf->cbSize = 0;
        pBuf->cbOffset = 0;
    }

    RTPipeClose(pBuf->hNotificationPipeR);
    pBuf->hNotificationPipeR = NIL_RTPIPE;
    RTPipeClose(pBuf->hNotificationPipeW);
    pBuf->hNotificationPipeW = NIL_RTPIPE;

    RTCritSectDelete(&pBuf->CritSect);
    if (pBuf->hEventSem != NIL_RTSEMEVENT)
        RTSemEventDestroy(pBuf->hEventSem);
}

