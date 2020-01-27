/* $Id$ */
/** @file
 * Shared Clipboard: Some helper function for converting between the various eol.
 */

/*
 * Includes contributions from François Revol
 *
 * Copyright (C) 2006-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/semaphore.h>
#include <iprt/path.h>
#include <iprt/rand.h>

#include <iprt/errcore.h>
#include <VBox/log.h>
#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>


/**
 * Allocates a new event payload.
 *
 * @returns VBox status code.
 * @param   uID                 Payload ID to set for this payload. Useful for consequtive payloads.
 * @param   pvData              Data block to associate to this payload.
 * @param   cbData              Size (in bytes) of data block to associate.
 * @param   ppPayload           Where to store the allocated event payload on success.
 */
int ShClPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData,
                     PSHCLEVENTPAYLOAD *ppPayload)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData > 0, VERR_INVALID_PARAMETER);

    PSHCLEVENTPAYLOAD pPayload = (PSHCLEVENTPAYLOAD)RTMemAlloc(sizeof(SHCLEVENTPAYLOAD));
    if (pPayload)
    {
        pPayload->pvData = RTMemDup(pvData, cbData);
        if (pPayload->pvData)
        {
            pPayload->cbData = cbData;
            pPayload->uID    = uID;

            *ppPayload = pPayload;
            return VINF_SUCCESS;
        }

        RTMemFree(pPayload);
    }
    return VERR_NO_MEMORY;
}

/**
 * Frees an event payload.
 *
 * @returns VBox status code.
 * @param   pPayload            Event payload to free.
 */
void ShClPayloadFree(PSHCLEVENTPAYLOAD pPayload)
{
    if (!pPayload)
        return;

    if (pPayload->pvData)
    {
        Assert(pPayload->cbData);
        RTMemFree(pPayload->pvData);
        pPayload->pvData = NULL;
    }

    pPayload->cbData = 0;
    pPayload->uID = UINT32_MAX;

    RTMemFree(pPayload);
}

/**
 * Destroys an event, but doesn't free the memory.
 *
 * @param   pEvent              Event to destroy.
 */
static void shClEventTerm(PSHCLEVENT pEvent)
{
    if (!pEvent)
        return;

    LogFlowFunc(("Event %RU32\n", pEvent->idEvent));

    if (pEvent->hEvtMulSem != NIL_RTSEMEVENT)
    {
        RTSemEventMultiDestroy(pEvent->hEvtMulSem);
        pEvent->hEvtMulSem = NIL_RTSEMEVENT;
    }

    ShClPayloadFree(pEvent->pPayload);

    pEvent->idEvent = 0;
}

/**
 * Creates a new event source.
 *
 * @returns VBox status code.
 * @param   pSource             Event source to create.
 * @param   uID                 ID to use for event source.
 */
int ShClEventSourceCreate(PSHCLEVENTSOURCE pSource, SHCLEVENTSOURCEID uID)
{
    LogFlowFunc(("pSource=%p, uID=%RU16\n", pSource, uID));
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);

    RTListInit(&pSource->lstEvents);

    pSource->uID          = uID;
    /* Choose a random event ID starting point. */
    pSource->idNextEvent  = RTRandU32Ex(1, VBOX_SHCL_MAX_EVENTS - 1);

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

/**
 * Destroys an event source.
 *
 * @param   pSource             Event source to destroy.
 */
void ShClEventSourceDestroy(PSHCLEVENTSOURCE pSource)
{
    if (!pSource)
        return;

    LogFlowFunc(("ID=%RU32\n", pSource->uID));

    ShClEventSourceReset(pSource);

    pSource->uID          = UINT16_MAX;
    pSource->idNextEvent  = UINT32_MAX;
}

/**
 * Resets an event source.
 *
 * @param   pSource             Event source to reset.
 */
void ShClEventSourceReset(PSHCLEVENTSOURCE pSource)
{
    LogFlowFunc(("ID=%RU32\n", pSource->uID));

    PSHCLEVENT pEvIt;
    PSHCLEVENT pEvItNext;
    RTListForEachSafe(&pSource->lstEvents, pEvIt, pEvItNext, SHCLEVENT, Node)
    {
        RTListNodeRemove(&pEvIt->Node);

        shClEventTerm(pEvIt);

        RTMemFree(pEvIt);
        pEvIt = NULL;
    }
}

/**
 * Returns a specific event of a event source.
 *
 * @returns Pointer to event if found, or NULL if not found.
 * @param   pSource             Event source to get event from.
 * @param   uID                 Event ID to get.
 */
DECLINLINE(PSHCLEVENT) shclEventGet(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent)
{
    PSHCLEVENT pEvent;
    RTListForEach(&pSource->lstEvents, pEvent, SHCLEVENT, Node)
    {
        if (pEvent->idEvent == idEvent)
            return pEvent;
    }

    return NULL;
}

/**
 * Generates a new event ID for a specific event source and registers it.
 *
 * @returns New event ID generated, or 0 on error.
 * @param   pSource             Event source to generate event for.
 */
SHCLEVENTID ShClEventIdGenerateAndRegister(PSHCLEVENTSOURCE pSource)
{
    AssertPtrReturn(pSource, 0);

    /*
     * Allocate an event.
     */
    PSHCLEVENT pEvent = (PSHCLEVENT)RTMemAllocZ(sizeof(SHCLEVENT));
    AssertReturn(pEvent, 0);
    int rc = RTSemEventMultiCreate(&pEvent->hEvtMulSem);
    AssertRCReturnStmt(rc, RTMemFree(pEvent), 0);

    /*
     * Allocate an unique event ID.
     */
    for (uint32_t cTries = 0;; cTries++)
    {
        SHCLEVENTID idEvent = ++pSource->idNextEvent;
        if (idEvent < VBOX_SHCL_MAX_EVENTS)
        { /* likely */ }
        else
            pSource->idNextEvent = idEvent = 1; /* zero == error, remember! */

        if (shclEventGet(pSource, idEvent) == NULL)
        {
            pEvent->idEvent = idEvent;
            RTListAppend(&pSource->lstEvents, &pEvent->Node);

            LogFlowFunc(("uSource=%RU16: New event: %#x\n", pSource->uID, idEvent));
            return idEvent;
        }

        AssertBreak(cTries < 4096);
    }

    AssertMsgFailed(("Unable to register a new event ID for event source %RU16\n", pSource->uID));

    RTMemFree(pEvent);
    return 0;
}

/**
 * Returns the last (newest) event ID which has been registered for an event source.
 *
 * @returns Last registered event ID, or 0 if not found.
 * @param   pSource             Event source to get last registered event from.
 */
SHCLEVENTID ShClEventGetLast(PSHCLEVENTSOURCE pSource)
{
    AssertPtrReturn(pSource, 0);
    PSHCLEVENT pEvent = RTListGetLast(&pSource->lstEvents, SHCLEVENT, Node);
    if (pEvent)
        return pEvent->idEvent;

    return 0;
}

/**
 * Detaches a payload from an event, internal version.
 *
 * @param   pEvent              Event to detach payload for.
 */
static void shclEventPayloadDetachInternal(PSHCLEVENT pEvent)
{
    /** @todo r=bird: This should return pPayload.  It should also not need
     *        assert the validity of pEvent in non-strict builds, given that this
     *        is an static + internal function, that's a complete waste of time. */
    AssertPtrReturnVoid(pEvent);

    pEvent->pPayload = NULL;
}

/**
 * Unregisters an event.
 *
 * @returns VBox status code.
 * @param   pSource             Event source to unregister event for.
 * @param   uID                 Event ID to unregister.
 */
int ShClEventUnregister(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID)
{
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("uSource=%RU16, uEvent=%RU32\n", pSource->uID, uID));

    PSHCLEVENT pEvent = shclEventGet(pSource, uID);
    if (pEvent)
    {
        LogFlowFunc(("Event %RU32\n", pEvent->idEvent));

        RTListNodeRemove(&pEvent->Node);

        shClEventTerm(pEvent);

        RTMemFree(pEvent);
        pEvent = NULL;

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Waits for an event to get signalled.
 *
 * @returns VBox status code.
 * @param   pSource             Event source that contains the event to wait for.
 * @param   uID                 Event ID to wait for.
 * @param   uTimeoutMs          Timeout (in ms) to wait.
 * @param   ppPayload           Where to store the (allocated) event payload on success. Needs to be free'd with
 *                              SharedClipboardPayloadFree(). Optional.
 */
int ShClEventWait(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID, RTMSINTERVAL uTimeoutMs,
                  PSHCLEVENTPAYLOAD* ppPayload)
{
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);
    /** ppPayload is optional. */

    LogFlowFuncEnter();

    int rc;

    PSHCLEVENT pEvent = shclEventGet(pSource, uID);
    if (pEvent)
    {
        rc = RTSemEventMultiWait(pEvent->hEvtMulSem, uTimeoutMs);
        if (RT_SUCCESS(rc))
        {
            if (ppPayload)
            {
                *ppPayload = pEvent->pPayload;

                /* Make sure to detach payload here, as the caller now owns the data. */
                shclEventPayloadDetachInternal(pEvent);
            }
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Retains an event by increasing its reference count.
 *
 * @returns New reference count, or UINT32_MAX if failed.
 * @param   pSource             Event source of event to retain.
 * @param   idEvent             ID of event to retain.
 */
uint32_t ShClEventRetain(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent)
{
    PSHCLEVENT pEvent = shclEventGet(pSource, idEvent);
    if (!pEvent)
    {
        AssertFailed();
        return UINT32_MAX;
    }

    AssertReturn(pEvent->cRefs < 64, UINT32_MAX); /* Sanity. Yeah, not atomic. */

    return ASMAtomicIncU32(&pEvent->cRefs);
}

/**
 * Releases an event by decreasing its reference count.
 *
 * @returns New reference count, or UINT32_MAX if failed.
 * @param   pSource             Event source of event to release.
 * @param   idEvent             ID of event to release.
 */
uint32_t ShClEventRelease(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent)
{
    PSHCLEVENT pEvent = shclEventGet(pSource, idEvent);
    if (!pEvent)
    {
        AssertFailed();
        return UINT32_MAX;
    }

    AssertReturn(pEvent->cRefs, UINT32_MAX); /* Sanity. Yeah, not atomic. */

    return ASMAtomicDecU32(&pEvent->cRefs);
}

/**
 * Signals an event.
 *
 * @returns VBox status code.
 * @param   pSource             Event source of event to signal.
 * @param   uID                 Event ID to signal.
 * @param   pPayload            Event payload to associate. Takes ownership. Optional.
 *
 * @note    Caller must enter crit sect protecting the event source!
 */
int ShClEventSignal(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID,
                    PSHCLEVENTPAYLOAD pPayload)
{
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("uSource=%RU16, uEvent=%RU32\n", pSource->uID, uID));

    PSHCLEVENT pEvent = shclEventGet(pSource, uID);
    if (pEvent)
    {
        Assert(pEvent->pPayload == NULL);

        pEvent->pPayload = pPayload;

        rc = RTSemEventMultiSignal(pEvent->hEvtMulSem);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Detaches a payload from an event.
 *
 * @returns VBox status code.
 * @param   pSource             Event source of event to detach payload for.
 * @param   uID                 Event ID to detach payload for.
 */
void ShClEventPayloadDetach(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID)
{
    /** @todo r=bird: This API is not needed, it either is a no-op as it
     *        replicates work done by ShClEventWait or it leaks the payload as
     *        ShClEventWait is the only way to get it as far as I can tell. */

    AssertPtrReturnVoid(pSource);

    LogFlowFunc(("uSource=%RU16, uEvent=%RU32\n", pSource->uID, uID));

    PSHCLEVENT pEvent = shclEventGet(pSource, uID);
    if (pEvent)
    {
        shclEventPayloadDetachInternal(pEvent);
    }
#ifdef DEBUG_andy
    else
        AssertMsgFailed(("uSource=%RU16, uEvent=%RU32\n", pSource->uID, uID));
#endif
}

/** @todo Delinuxify the code (*Lin* -> *Host*); use AssertLogRel*. */

int ShClUtf16GetWinSize(PCRTUTF16 pcwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest, i;

    LogFlowFunc(("pcwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pcwszSrc, cwSrc));
    AssertLogRelMsgReturn(pcwszSrc != NULL, ("vboxClipboardUtf16GetWinSize: received a null Utf16 string, returning VERR_INVALID_PARAMETER\n"), VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        *pcwDest = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
/** @todo convert the remainder of the Assert stuff to AssertLogRel. */
    /* We only take little endian Utf16 */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16BEMARKER)
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16GetWinSize: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    cwDest = 0;
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    for (i = (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER ? 1 : 0); i < cwSrc; ++i, ++cwDest)
    {
        /* Check for a single line feed */
        if (pcwszSrc[i] == VBOX_SHCL_LINEFEED)
            ++cwDest;
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        if (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
            ++cwDest;
#endif
        if (pcwszSrc[i] == 0)
        {
            /* Don't count this, as we do so below. */
            break;
        }
    }
    /* Count the terminating null byte. */
    ++cwDest;
    LogFlowFunc(("returning VINF_SUCCESS, %d 16bit words\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int ShClUtf16LinToWin(PCRTUTF16 pcwszSrc, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest)
{
    size_t i, j;
    LogFlowFunc(("pcwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pcwszSrc, cwSrc));
    if (!VALID_PTR(pcwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16LinToWin: received an invalid pointer, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(VALID_PTR(pcwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        if (cwDest == 0)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        pu16Dest[0] = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16BEMARKER)
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16LinToWin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Don't copy the endian marker. */
    for (i = (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER ? 1 : 0), j = 0; i < cwSrc; ++i, ++j)
    {
        /* Don't copy the null byte, as we add it below. */
        if (pcwszSrc[i] == 0)
            break;
        if (j == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (pcwszSrc[i] == VBOX_SHCL_LINEFEED)
        {
            pu16Dest[j] = VBOX_SHCL_CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
        }
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        else if (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
        {
            /* set cr */
            pu16Dest[j] = VBOX_SHCL_CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
            /* add the lf */
            pu16Dest[j] = VBOX_SHCL_LINEFEED;
            continue;
        }
#endif
        pu16Dest[j] = pcwszSrc[i];
    }
    /* Add the trailing null. */
    if (j == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[j] = 0;
    LogFlowFunc(("rc=VINF_SUCCESS, pu16Dest=%ls\n", pu16Dest));
    return VINF_SUCCESS;
}

int ShClUtf16GetLinSize(PCRTUTF16 pcwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest;

    LogFlowFunc(("pcwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pcwszSrc, cwSrc));
    if (!VALID_PTR(pcwszSrc))
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16GetLinSize: received an invalid Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(VALID_PTR(pcwszSrc), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        LogFlowFunc(("empty source string, returning VINF_SUCCESS\n"));
        *pcwDest = 0;
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16BEMARKER)
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16GetLinSize: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER)
        cwDest = 0;
    else
        cwDest = 1;
    for (size_t i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (   (i + 1 < cwSrc)
            && (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
            && (pcwszSrc[i + 1] == VBOX_SHCL_LINEFEED))
        {
            ++i;
        }
        if (pcwszSrc[i] == 0)
        {
            break;
        }
    }
    /* Terminating zero */
    ++cwDest;
    LogFlowFunc(("returning %d\n", cwDest));
    *pcwDest = cwDest;
    return VINF_SUCCESS;
}

int ShClUtf16WinToLin(PCRTUTF16 pcwszSrc, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest)
{
    size_t cwDestPos;

    LogFlowFunc(("pcwszSrc=%.*ls, cwSrc=%u, pu16Dest=%p, cwDest=%u\n",
                 cwSrc, pcwszSrc, cwSrc, pu16Dest, cwDest));
    /* A buffer of size 0 may not be an error, but it is not a good idea either. */
    Assert(cwDest > 0);
    if (!VALID_PTR(pcwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16WinToLin: received an invalid pointer, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(VALID_PTR(pcwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    /* We only take little endian Utf16 */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16BEMARKER)
    {
        LogRel(("Shared Clipboard: vboxClipboardUtf16WinToLin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertMsgFailedReturn(("received a big endian string\n"), VERR_INVALID_PARAMETER);
    }
    if (cwDest == 0)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    if (cwSrc == 0)
    {
        pu16Dest[0] = 0;
        LogFlowFunc(("received empty string.  Returning VINF_SUCCESS\n"));
        return VINF_SUCCESS;
    }
    /* Prepend the Utf16 byte order marker if it is missing. */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER)
    {
        cwDestPos = 0;
    }
    else
    {
        pu16Dest[0] = VBOX_SHCL_UTF16LEMARKER;
        cwDestPos = 1;
    }
    for (size_t i = 0; i < cwSrc; ++i, ++cwDestPos)
    {
        if (pcwszSrc[i] == 0)
        {
            break;
        }
        if (cwDestPos == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (   (i + 1 < cwSrc)
            && (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
            && (pcwszSrc[i + 1] == VBOX_SHCL_LINEFEED))
        {
            ++i;
        }
        pu16Dest[cwDestPos] = pcwszSrc[i];
    }
    /* Terminating zero */
    if (cwDestPos == cwDest)
    {
        LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
        return VERR_BUFFER_OVERFLOW;
    }
    pu16Dest[cwDestPos] = 0;
    LogFlowFunc(("set string %ls.  Returning\n", pu16Dest + 1));
    return VINF_SUCCESS;
}

int ShClDibToBmp(const void *pvSrc, size_t cbSrc, void **ppvDest, size_t *pcbDest)
{
    size_t        cb            = sizeof(BMFILEHEADER) + cbSrc;
    PBMFILEHEADER pFileHeader   = NULL;
    void         *pvDest        = NULL;
    size_t        offPixel      = 0;

    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMINFOHEADER pBitmapInfoHeader = (PBMINFOHEADER)pvSrc;
    /** @todo Support all the many versions of the DIB headers. */
    if (   cbSrc < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->uSize) < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->uSize) != sizeof(BMINFOHEADER))
    {
        Log(("vboxClipboardDibToBmp: invalid or unsupported bitmap data\n"));
        return VERR_INVALID_PARAMETER;
    }

    offPixel = sizeof(BMFILEHEADER)
                + RT_LE2H_U32(pBitmapInfoHeader->uSize)
                + RT_LE2H_U32(pBitmapInfoHeader->uClrUsed) * sizeof(uint32_t);
    if (cbSrc < offPixel)
    {
        Log(("vboxClipboardDibToBmp: invalid bitmap data\n"));
        return VERR_INVALID_PARAMETER;
    }

    pvDest = RTMemAlloc(cb);
    if (!pvDest)
    {
        Log(("writeToPasteboard: cannot allocate memory for bitmap\n"));
        return VERR_NO_MEMORY;
    }

    pFileHeader = (PBMFILEHEADER)pvDest;
    pFileHeader->uType        = BITMAPHEADERMAGIC;
    pFileHeader->uSize        = (uint32_t)RT_H2LE_U32(cb);
    pFileHeader->uReserved1   = pFileHeader->uReserved2 = 0;
    pFileHeader->uOffBits     = (uint32_t)RT_H2LE_U32(offPixel);
    memcpy((uint8_t *)pvDest + sizeof(BMFILEHEADER), pvSrc, cbSrc);
    *ppvDest = pvDest;
    *pcbDest = cb;
    return VINF_SUCCESS;
}

int ShClBmpGetDib(const void *pvSrc, size_t cbSrc, const void **ppvDest, size_t *pcbDest)
{
    AssertPtrReturn(pvSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcbDest, VERR_INVALID_PARAMETER);

    PBMFILEHEADER pFileHeader = (PBMFILEHEADER)pvSrc;
    if (   cbSrc < sizeof(BMFILEHEADER)
        || pFileHeader->uType != BITMAPHEADERMAGIC
        || RT_LE2H_U32(pFileHeader->uSize) != cbSrc)
    {
        Log(("vboxClipboardBmpGetDib: invalid bitmap data\n"));
        return VERR_INVALID_PARAMETER;
    }

    *ppvDest = ((uint8_t *)pvSrc) + sizeof(BMFILEHEADER);
    *pcbDest = cbSrc - sizeof(BMFILEHEADER);
    return VINF_SUCCESS;
}

#ifdef LOG_ENABLED
int ShClDbgDumpHtml(const char *pcszSrc, size_t cbSrc)
{
    size_t cchIgnored = 0;
    int rc = RTStrNLenEx(pcszSrc, cbSrc, &cchIgnored);
    if (RT_SUCCESS(rc))
    {
        char *pszBuf = (char *)RTMemAllocZ(cbSrc + 1);
        if (pszBuf)
        {
            rc = RTStrCopy(pszBuf, cbSrc + 1, (const char *)pcszSrc);
            if (RT_SUCCESS(rc))
            {
                for (size_t i = 0; i < cbSrc; ++i)
                    if (pszBuf[i] == '\n' || pszBuf[i] == '\r')
                        pszBuf[i] = ' ';
            }
            else
                LogFunc(("Error in copying string\n"));
            LogFunc(("Removed \\r\\n: %s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

void ShClDbgDumpData(const void *pv, size_t cb, SHCLFORMAT uFormat)
{
    if (uFormat & VBOX_SHCL_FMT_UNICODETEXT)
    {
        LogFunc(("VBOX_SHCL_FMT_UNICODETEXT:\n"));
        if (pv && cb)
            LogFunc(("%ls\n", pv));
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else if (uFormat & VBOX_SHCL_FMT_BITMAP)
        LogFunc(("VBOX_SHCL_FMT_BITMAP\n"));
    else if (uFormat & VBOX_SHCL_FMT_HTML)
    {
        LogFunc(("VBOX_SHCL_FMT_HTML:\n"));
        if (pv && cb)
        {
            LogFunc(("%s\n", pv));

            //size_t cb = RTStrNLen(pv, );
            char *pszBuf = (char *)RTMemAllocZ(cb + 1);
            RTStrCopy(pszBuf, cb + 1, (const char *)pv);
            for (size_t off = 0; off < cb; ++off)
            {
                if (pszBuf[off] == '\n' || pszBuf[off] == '\r')
                    pszBuf[off] = ' ';
            }

            LogFunc(("%s\n", pszBuf));
            RTMemFree(pszBuf);
        }
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else
        LogFunc(("Invalid format %02X\n", uFormat));
}
#endif /* LOG_ENABLED */

/**
 * Translates a Shared Clipboard host function number to a string.
 *
 * @returns Function ID string name.
 * @param   uFn                 The function to translate.
 */
const char *ShClHostFunctionToStr(uint32_t uFn)
{
    switch (uFn)
    {
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_SET_MODE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_SET_TRANSFER_MODE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_SET_HEADLESS);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_CANCEL);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_ERROR);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_AREA_REGISTER);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_AREA_UNREGISTER);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_AREA_ATTACH);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_FN_AREA_DETACH);
    }
    return "Unknown";
}

/**
 * Translates a Shared Clipboard host message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *ShClHostMsgToStr(uint32_t uMsg)
{
    switch (uMsg)
    {
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_QUIT);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_READ_DATA);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_FORMATS_REPORT);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_STATUS);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_ROOT_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_OPEN);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_CLOSE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_OPEN);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_CLOSE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_READ);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_OBJ_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_CANCEL);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_TRANSFER_ERROR);
    }
    return "Unknown";
}

/**
 * Translates a Shared Clipboard guest message enum to a string.
 *
 * @returns Message ID string name.
 * @param   uMsg                The message to translate.
 */
const char *ShClGuestMsgToStr(uint32_t uMsg)
{
    switch (uMsg)
    {
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_OLD_GET_WAIT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_REPORT_FORMATS);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_DATA_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_DATA_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_CONNECT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_REPORT_FEATURES);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_QUERY_FEATURES);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_GET);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_CANCEL);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_REPLY);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_OPEN);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_CLOSE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_HDR_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_OBJ_OPEN);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_OBJ_CLOSE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_OBJ_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_OBJ_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ERROR);
    }
    return "Unknown";
}
