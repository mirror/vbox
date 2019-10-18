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
    AssertReturn   (cbData, VERR_INVALID_PARAMETER);

    PSHCLEVENTPAYLOAD pPayload =
        (PSHCLEVENTPAYLOAD)RTMemAlloc(sizeof(SHCLEVENTPAYLOAD));
    if (!pPayload)
        return VERR_NO_MEMORY;

    pPayload->pvData = RTMemAlloc(cbData);
    if (pPayload->pvData)
    {
        memcpy(pPayload->pvData, pvData, cbData);

        pPayload->cbData = cbData;
        pPayload->uID    = uID;

        *ppPayload = pPayload;

        return VINF_SUCCESS;
    }

    RTMemFree(pPayload);
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

    RTMemFree(pPayload);
    pPayload = NULL;
}

/**
 * Creates (initializes) an event.
 *
 * @returns VBox status code.
 * @param   pEvent              Event to initialize.
 * @param   uID                 Event ID to use.
 */
int ShClEventCreate(PSHCLEVENT pEvent, SHCLEVENTID uID)
{
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    LogFlowFunc(("Event %RU32\n", uID));

    int rc = RTSemEventCreate(&pEvent->hEventSem);
    if (RT_SUCCESS(rc))
    {
        pEvent->uID      = uID;
        pEvent->pPayload = NULL;
    }

    return rc;
}

/**
 * Destroys an event.
 *
 * @param   pEvent              Event to destroy.
 */
void ShClEventDestroy(PSHCLEVENT pEvent)
{
    if (!pEvent)
        return;

    LogFlowFunc(("Event %RU32\n", pEvent->uID));

    if (pEvent->hEventSem != NIL_RTSEMEVENT)
    {
        RTSemEventDestroy(pEvent->hEventSem);
        pEvent->hEventSem = NIL_RTSEMEVENT;
    }

    ShClPayloadFree(pEvent->pPayload);

    pEvent->uID = 0;
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
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);

    LogFlowFunc(("pSource=%p, uID=%RU32\n", pSource, uID));

    int rc = VINF_SUCCESS;

    RTListInit(&pSource->lstEvents);

    pSource->uID          = uID;
    /* Choose a random event ID starting point. */
    pSource->uEventIDNext = RTRandU32() % VBOX_SHCL_MAX_EVENTS;

    LogFlowFuncLeaveRC(rc);
    return rc;
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

    PSHCLEVENT pEvIt;
    PSHCLEVENT pEvItNext;
    RTListForEachSafe(&pSource->lstEvents, pEvIt, pEvItNext, SHCLEVENT, Node)
    {
        RTListNodeRemove(&pEvIt->Node);

        ShClEventDestroy(pEvIt);

        RTMemFree(pEvIt);
        pEvIt = NULL;
    }

    pSource->uID          = 0;
    pSource->uEventIDNext = 0;
}

/**
 * Generates a new event ID for a specific event source.
 *
 * @returns New event ID generated, or 0 on error.
 * @param   pSource             Event source to generate event for.
 */
SHCLEVENTID ShClEventIDGenerate(PSHCLEVENTSOURCE pSource)
{
    AssertPtrReturn(pSource, 0);

    LogFlowFunc(("uSource=%RU16: New event: %RU32\n", pSource->uID, pSource->uEventIDNext));

    pSource->uEventIDNext++;
    if (pSource->uEventIDNext == VBOX_SHCL_MAX_EVENTS)
        pSource->uEventIDNext = 0;

    return pSource->uEventIDNext;
}

/**
 * Returns a specific event of a event source.
 *
 * @returns Pointer to event if found, or NULL if not found.
 * @param   pSource             Event source to get event from.
 * @param   uID                 Event ID to get.
 */
inline PSHCLEVENT shclEventGet(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID)
{
    PSHCLEVENT pEvIt;
    RTListForEach(&pSource->lstEvents, pEvIt, SHCLEVENT, Node)
    {
        if (pEvIt->uID == uID)
            return pEvIt;
    }

    return NULL;
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
        return pEvent->uID;

    return 0;
}

/**
 * Detaches a payload from an event, internal version.
 *
 * @param   pEvent              Event to detach payload for.
 */
static void shclEventPayloadDetachInternal(PSHCLEVENT pEvent)
{
    AssertPtrReturnVoid(pEvent);

    pEvent->pPayload = NULL;
}

/**
 * Registers an event.
 *
 * @returns VBox status code.
 * @param   pSource             Event source to register event for.
 * @param   uID                 Event ID to register.
 */
int ShClEventRegister(PSHCLEVENTSOURCE pSource, SHCLEVENTID uID)
{
    AssertPtrReturn(pSource, VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("uSource=%RU16, uEvent=%RU32\n", pSource->uID, uID));

    if (shclEventGet(pSource, uID) == NULL)
    {
        PSHCLEVENT pEvent
            = (PSHCLEVENT)RTMemAllocZ(sizeof(SHCLEVENT));
        if (pEvent)
        {
            rc = ShClEventCreate(pEvent, uID);
            if (RT_SUCCESS(rc))
            {
                RTListAppend(&pSource->lstEvents, &pEvent->Node);

                LogFlowFunc(("Event %RU32\n", uID));
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_ALREADY_EXISTS;

#ifdef DEBUG_andy
    AssertRC(rc);
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
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
        LogFlowFunc(("Event %RU32\n", pEvent->uID));

        RTListNodeRemove(&pEvent->Node);

        ShClEventDestroy(pEvent);

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
        rc = RTSemEventWait(pEvent->hEventSem, uTimeoutMs);
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
 * Signals an event.
 *
 * @returns VBox status code.
 * @param   pSource             Event source of event to signal.
 * @param   uID                 Event ID to signal.
 * @param   pPayload            Event payload to associate. Takes ownership. Optional.
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

        rc = RTSemEventSignal(pEvent->hEventSem);
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

/** @todo use const where appropriate; delinuxify the code (*Lin* -> *Host*); use AssertLogRel*. */

int ShClUtf16GetWinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest, i;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    AssertLogRelMsgReturn(pwszSrc != NULL, ("vboxClipboardUtf16GetWinSize: received a null Utf16 string, returning VERR_INVALID_PARAMETER\n"), VERR_INVALID_PARAMETER);
    if (cwSrc == 0)
    {
        *pcwDest = 0;
        LogFlowFunc(("empty source string, returning\n"));
        return VINF_SUCCESS;
    }
/** @todo convert the remainder of the Assert stuff to AssertLogRel. */
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetWinSize: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    cwDest = 0;
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0); i < cwSrc; ++i, ++cwDest)
    {
        /* Check for a single line feed */
        if (pwszSrc[i] == LINEFEED)
            ++cwDest;
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        if (pwszSrc[i] == CARRIAGERETURN)
            ++cwDest;
#endif
        if (pwszSrc[i] == 0)
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

int ShClUtf16LinToWin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest)
{
    size_t i, j;
    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16LinToWin: received an invalid pointer, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
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
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16LinToWin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Don't copy the endian marker. */
    for (i = (pwszSrc[0] == UTF16LEMARKER ? 1 : 0), j = 0; i < cwSrc; ++i, ++j)
    {
        /* Don't copy the null byte, as we add it below. */
        if (pwszSrc[i] == 0)
            break;
        if (j == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (pwszSrc[i] == LINEFEED)
        {
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
        }
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        else if (pwszSrc[i] == CARRIAGERETURN)
        {
            /* set cr */
            pu16Dest[j] = CARRIAGERETURN;
            ++j;
            if (j == cwDest)
            {
                LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
                return VERR_BUFFER_OVERFLOW;
            }
            /* add the lf */
            pu16Dest[j] = LINEFEED;
            continue;
        }
#endif
        pu16Dest[j] = pwszSrc[i];
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

int ShClUtf16GetLinSize(PRTUTF16 pwszSrc, size_t cwSrc, size_t *pcwDest)
{
    size_t cwDest;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u\n", cwSrc, pwszSrc, cwSrc));
    if (!VALID_PTR(pwszSrc))
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received an invalid Utf16 string %p.  Returning VERR_INVALID_PARAMETER.\n", pwszSrc));
        AssertReturn(VALID_PTR(pwszSrc), VERR_INVALID_PARAMETER);
    }
    if (cwSrc == 0)
    {
        LogFlowFunc(("empty source string, returning VINF_SUCCESS\n"));
        *pcwDest = 0;
        return VINF_SUCCESS;
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16GetLinSize: received a big endian Utf16 string.  Returning VERR_INVALID_PARAMETER.\n"));
        AssertReturn(pwszSrc[0] != UTF16BEMARKER, VERR_INVALID_PARAMETER);
    }
    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pwszSrc[0] == UTF16LEMARKER)
        cwDest = 0;
    else
        cwDest = 1;
    for (size_t i = 0; i < cwSrc; ++i, ++cwDest)
    {
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        if (pwszSrc[i] == 0)
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

int ShClUtf16WinToLin(PRTUTF16 pwszSrc, size_t cwSrc, PRTUTF16 pu16Dest, size_t cwDest)
{
    size_t cwDestPos;

    LogFlowFunc(("pwszSrc=%.*ls, cwSrc=%u, pu16Dest=%p, cwDest=%u\n",
                 cwSrc, pwszSrc, cwSrc, pu16Dest, cwDest));
    /* A buffer of size 0 may not be an error, but it is not a good idea either. */
    Assert(cwDest > 0);
    if (!VALID_PTR(pwszSrc) || !VALID_PTR(pu16Dest))
    {
        LogRel(("vboxClipboardUtf16WinToLin: received an invalid ptr, pwszSrc=%p, pu16Dest=%p, returning VERR_INVALID_PARAMETER\n", pwszSrc, pu16Dest));
        AssertReturn(VALID_PTR(pwszSrc) && VALID_PTR(pu16Dest), VERR_INVALID_PARAMETER);
    }
    /* We only take little endian Utf16 */
    if (pwszSrc[0] == UTF16BEMARKER)
    {
        LogRel(("vboxClipboardUtf16WinToLin: received a big endian Utf16 string, returning VERR_INVALID_PARAMETER\n"));
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
    if (pwszSrc[0] == UTF16LEMARKER)
    {
        cwDestPos = 0;
    }
    else
    {
        pu16Dest[0] = UTF16LEMARKER;
        cwDestPos = 1;
    }
    for (size_t i = 0; i < cwSrc; ++i, ++cwDestPos)
    {
        if (pwszSrc[i] == 0)
        {
            break;
        }
        if (cwDestPos == cwDest)
        {
            LogFlowFunc(("returning VERR_BUFFER_OVERFLOW\n"));
            return VERR_BUFFER_OVERFLOW;
        }
        if (   (i + 1 < cwSrc)
            && (pwszSrc[i] == CARRIAGERETURN)
            && (pwszSrc[i + 1] == LINEFEED))
        {
            ++i;
        }
        pu16Dest[cwDestPos] = pwszSrc[i];
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
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) < sizeof(BMINFOHEADER)
        || RT_LE2H_U32(pBitmapInfoHeader->u32Size) != sizeof(BMINFOHEADER))
    {
        Log(("vboxClipboardDibToBmp: invalid or unsupported bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    offPixel = sizeof(BMFILEHEADER)
                + RT_LE2H_U32(pBitmapInfoHeader->u32Size)
                + RT_LE2H_U32(pBitmapInfoHeader->u32ClrUsed) * sizeof(uint32_t);
    if (cbSrc < offPixel)
    {
        Log(("vboxClipboardDibToBmp: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    pvDest = RTMemAlloc(cb);
    if (!pvDest)
    {
        Log(("writeToPasteboard: cannot allocate memory for bitmap.\n"));
        return VERR_NO_MEMORY;
    }

    pFileHeader = (PBMFILEHEADER)pvDest;
    pFileHeader->u16Type        = BITMAPHEADERMAGIC;
    pFileHeader->u32Size        = (uint32_t)RT_H2LE_U32(cb);
    pFileHeader->u16Reserved1   = pFileHeader->u16Reserved2 = 0;
    pFileHeader->u32OffBits     = (uint32_t)RT_H2LE_U32(offPixel);
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
        || pFileHeader->u16Type != BITMAPHEADERMAGIC
        || RT_LE2H_U32(pFileHeader->u32Size) != cbSrc)
    {
        Log(("vboxClipboardBmpGetDib: invalid bitmap data.\n"));
        return VERR_INVALID_PARAMETER;
    }

    *ppvDest = ((uint8_t *)pvSrc) + sizeof(BMFILEHEADER);
    *pcbDest = cbSrc - sizeof(BMFILEHEADER);
    return VINF_SUCCESS;
}

#ifdef LOG_ENABLED
int ShClDbgDumpHtml(const char *pszSrc, size_t cbSrc)
{
    size_t cchIgnored = 0;
    int rc = RTStrNLenEx(pszSrc, cbSrc, &cchIgnored);
    if (RT_SUCCESS(rc))
    {
        char *pszBuf = (char *)RTMemAllocZ(cbSrc + 1);
        if (pszBuf)
        {
            rc = RTStrCopy(pszBuf, cbSrc + 1, (const char *)pszSrc);
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

void ShClDbgDumpData(const void *pv, size_t cb, SHCLFORMAT u32Format)
{
    if (u32Format & VBOX_SHCL_FMT_UNICODETEXT)
    {
        LogFunc(("VBOX_SHCL_FMT_UNICODETEXT:\n"));
        if (pv && cb)
            LogFunc(("%ls\n", pv));
        else
            LogFunc(("%p %zu\n", pv, cb));
    }
    else if (u32Format & VBOX_SHCL_FMT_BITMAP)
        LogFunc(("VBOX_SHCL_FMT_BITMAP\n"));
    else if (u32Format & VBOX_SHCL_FMT_HTML)
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
        LogFunc(("Invalid format %02X\n", u32Format));
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
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_GET_HOST_MSG_OLD);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_FORMATS_REPORT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_DATA_READ);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_DATA_WRITE);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_CONNECT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_MSG_GET);
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
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_CANCEL);
        RT_CASE_RET_STR(VBOX_SHCL_GUEST_FN_ERROR);
    }
    return "Unknown";
}
