/* $Id$ */
/** @file
 * Shared Clipboard: Some helper function for converting between the various eol.
 */

/*
 * Includes contributions from François Revol
 *
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/utf16.h>

#include <iprt/formats/bmp.h>

#include <iprt/errcore.h>
#include <VBox/log.h>
#include <VBox/GuestHost/clipboard-helper.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/
DECLINLINE(PSHCLEVENT) shclEventGet(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent);


/*********************************************************************************************************************************
*   Implementation                                                                                                               *
*********************************************************************************************************************************/

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

    AssertMsgReturnVoid(pEvent->cRefs == 0, ("Event %RU32 still has %RU32 references\n",
                                             pEvent->idEvent, pEvent->cRefs));

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
 * Generates a new event ID for a specific event source and registers it.
 *
 * @returns New event ID generated, or NIL_SHCLEVENTID on error.
 * @param   pSource             Event source to generate event for.
 */
SHCLEVENTID ShClEventIdGenerateAndRegister(PSHCLEVENTSOURCE pSource)
{
    AssertPtrReturn(pSource, NIL_SHCLEVENTID);

    /*
     * Allocate an event.
     */
    PSHCLEVENT pEvent = (PSHCLEVENT)RTMemAllocZ(sizeof(SHCLEVENT));
    AssertReturn(pEvent, NIL_SHCLEVENTID);
    int rc = RTSemEventMultiCreate(&pEvent->hEvtMulSem);
    AssertRCReturnStmt(rc, RTMemFree(pEvent), NIL_SHCLEVENTID);

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
    return NIL_SHCLEVENTID;
}

/**
 * Returns a specific event of a event source. Inlined version.
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
 * Returns a specific event of a event source.
 *
 * @returns Pointer to event if found, or NULL if not found.
 * @param   pSource             Event source to get event from.
 * @param   uID                 Event ID to get.
 */
PSHCLEVENT ShClEventGet(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent)
{
    return shclEventGet(pSource, idEvent);
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
 * Returns the current reference count for a specific event.
 *
 * @returns Reference count.
 * @param   pSource             Event source the specific event is part of.
 * @param   idEvent             Event ID to return reference count for.
 */
uint32_t ShClEventGetRefs(PSHCLEVENTSOURCE pSource, SHCLEVENTID idEvent)
{
    PSHCLEVENT pEvent = shclEventGet(pSource, idEvent);
    if (pEvent)
        return pEvent->cRefs;

    AssertMsgFailed(("No event with %RU32\n", idEvent));
    return 0;
}

/**
 * Detaches a payload from an event, internal version.
 *
 * @returns Pointer to the detached payload. Can be NULL if the payload has no payload.
 * @param   pEvent              Event to detach payload for.
 */
static PSHCLEVENTPAYLOAD shclEventPayloadDetachInternal(PSHCLEVENT pEvent)
{
#ifdef VBOX_STRICT
    AssertPtrReturn(pEvent, NULL);
#endif

    PSHCLEVENTPAYLOAD pPayload = pEvent->pPayload;

    pEvent->pPayload = NULL;

    return pPayload;
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
                /* Make sure to detach payload here, as the caller now owns the data. */
                *ppPayload = shclEventPayloadDetachInternal(pEvent);
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

int ShClUtf16LenUtf8(PCRTUTF16 pcwszSrc, size_t cwcSrc, size_t *pchLen)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(pchLen, VERR_INVALID_POINTER);

    size_t chLen = 0;
    int rc = RTUtf16CalcUtf8LenEx(pcwszSrc, cwcSrc, &chLen);
    if (RT_SUCCESS(rc))
        *pchLen = chLen;
    return rc;
}

int ShClConvUtf16CRLFToUtf8LF(PCRTUTF16 pcwszSrc, size_t cwcSrc,
                              char *pszBuf, size_t cbBuf, size_t *pcbLen)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertReturn   (cwcSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszBuf,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcbLen,   VERR_INVALID_POINTER);

    int rc;

    PRTUTF16 pwszTmp = NULL;
    size_t   cchTmp  = 0;

    size_t   cbLen = 0;

    /* How long will the converted text be? */
    rc = ShClUtf16CRLFLenUtf8(pcwszSrc, cwcSrc, &cchTmp);
    if (RT_SUCCESS(rc))
    {
        cchTmp++; /* Add space for terminator. */

        pwszTmp = (PRTUTF16)RTMemAlloc(cchTmp * sizeof(RTUTF16));
        if (pwszTmp)
        {
            rc = ShClConvUtf16CRLFToLF(pcwszSrc, cwcSrc, pwszTmp, cchTmp);
            if (RT_SUCCESS(rc))
                rc = RTUtf16ToUtf8Ex(pwszTmp + 1, cchTmp - 1, &pszBuf, cbBuf, &cbLen);

            RTMemFree(reinterpret_cast<void *>(pwszTmp));
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *pcbLen = cbLen;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClConvUtf16LFToCRLFA(PCRTUTF16 pcwszSrc, size_t cwcSrc,
                           PRTUTF16 *ppwszDst, size_t *pcwDst)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(ppwszDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pcwDst,   VERR_INVALID_POINTER);

    PRTUTF16 pwszDst = NULL;
    size_t   cchDst;

    int rc = ShClUtf16LFLenUtf8(pcwszSrc, cwcSrc, &cchDst);
    if (RT_SUCCESS(rc))
    {
        pwszDst = (PRTUTF16)RTMemAlloc((cchDst + 1 /* Leave space for terminator */) * sizeof(RTUTF16));
        if (pwszDst)
        {
            rc = ShClConvUtf16LFToCRLF(pcwszSrc, cwcSrc, pwszDst, cchDst + 1 /* Include terminator */);
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        *ppwszDst = pwszDst;
        *pcwDst   = cchDst;
    }
    else
        RTMemFree(pwszDst);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClConvUtf8LFToUtf16CRLF(const char *pcszSrc, size_t cbSrc,
                              PRTUTF16 *ppwszDst, size_t *pcwDst)
{
    AssertPtrReturn(pcszSrc,  VERR_INVALID_POINTER);
    AssertReturn(cbSrc,       VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppwszDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pcwDst,   VERR_INVALID_POINTER);

    /* Intermediate conversion to UTF-16. */
    size_t   cwcTmp;
    PRTUTF16 pwcTmp = NULL;
    int rc = RTStrToUtf16Ex(pcszSrc, cbSrc, &pwcTmp, 0, &cwcTmp);
    if (RT_SUCCESS(rc))
    {
        rc = ShClConvUtf16LFToCRLFA(pwcTmp, cwcTmp, ppwszDst, pcwDst);
        RTUtf16Free(pwcTmp);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClConvLatin1LFToUtf16CRLF(const char *pcszSrc, size_t cbSrc,
                                PRTUTF16 *ppwszDst, size_t *pcwDst)
{
    AssertPtrReturn(pcszSrc,  VERR_INVALID_POINTER);
    AssertReturn(cbSrc,       VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppwszDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pcwDst,   VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PRTUTF16 pwszDst = NULL;

    /* Calculate the space needed. */
    unsigned cwDst = 0;
    for (unsigned i = 0; i < cbSrc && pcszSrc[i] != '\0'; ++i)
    {
        if (pcszSrc[i] == VBOX_SHCL_LINEFEED)
            cwDst += 2; /* Space for VBOX_SHCL_CARRIAGERETURN + VBOX_SHCL_LINEFEED. */
        else
            ++cwDst;
    }

    pwszDst = (PRTUTF16)RTMemAlloc((cwDst + 1 /* Leave space for the terminator */) * sizeof(RTUTF16));
    if (!pwszDst)
        rc = VERR_NO_MEMORY;

    /* Do the conversion, bearing in mind that Latin-1 expands "naturally" to UTF-16. */
    if (RT_SUCCESS(rc))
    {
        for (unsigned i = 0, j = 0; i < cbSrc; ++i, ++j)
        {
            if (pcszSrc[i] != VBOX_SHCL_LINEFEED)
                pwszDst[j] = pcszSrc[i];
            else
            {
                pwszDst[j]     = VBOX_SHCL_CARRIAGERETURN;
                pwszDst[j + 1] = VBOX_SHCL_LINEFEED;
                ++j;
            }
        }

        pwszDst[cwDst] = '\0';  /* Make sure we are zero-terminated. */
    }

    if (RT_SUCCESS(rc))
    {
        *ppwszDst = pwszDst;
        *pcwDst   = cwDst;
    }
    else
        RTMemFree(pwszDst);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClConvUtf16ToUtf8HTML(PCRTUTF16 pcwszSrc, size_t cwcSrc, char **ppszDst, size_t *pcbDst)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertReturn   (cwcSrc,   VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppszDst,  VERR_INVALID_POINTER);
    AssertPtrReturn(pcbDst,   VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    size_t    cwTmp = cwcSrc;
    PCRTUTF16 pwTmp = pcwszSrc;

    char  *pchDst = NULL;
    size_t cbDst  = 0;

    size_t i = 0;
    while (i < cwTmp)
    {
        /* Find  zero symbol (end of string). */
        for (; i < cwTmp && pcwszSrc[i] != 0; i++)
            ;

        /* Convert found string. */
        char  *psz = NULL;
        size_t cch = 0;
        rc = RTUtf16ToUtf8Ex(pwTmp, cwTmp, &psz, pwTmp - pcwszSrc, &cch);
        if (RT_FAILURE(rc))
            break;

        /* Append new substring. */
        char *pchNew = (char *)RTMemRealloc(pchDst, cbDst + cch + 1);
        if (!pchNew)
        {
            RTStrFree(psz);
            rc = VERR_NO_MEMORY;
            break;
        }

        pchDst = pchNew;
        memcpy(pchDst + cbDst, psz, cch + 1);

        RTStrFree(psz);

        cbDst += cch + 1;

        /* Skip zero symbols. */
        for (; i < cwTmp && pcwszSrc[i] == 0; i++)
            ;

        /* Remember start of string. */
        pwTmp += i;
    }

    if (RT_SUCCESS(rc))
    {
        *ppszDst = pchDst;
        *pcbDst  = cbDst;

        return VINF_SUCCESS;
    }

    RTMemFree(pchDst);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClUtf16LFLenUtf8(PCRTUTF16 pcwszSrc, size_t cwSrc, size_t *pchLen)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(pchLen, VERR_INVALID_POINTER);

    AssertMsgReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER,
                    ("Big endian UTF-16 not supported yet\n"), VERR_NOT_SUPPORTED);

    size_t cLen = 0;

    /* Don't copy the endian marker. */
    size_t i = pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER ? 1 : 0;

    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    for (; i < cwSrc; ++i, ++cLen)
    {
        /* Check for a single line feed */
        if (pcwszSrc[i] == VBOX_SHCL_LINEFEED)
            ++cLen;
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        if (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
            ++cLen;
#endif
        if (pcwszSrc[i] == 0)
        {
            /* Don't count this, as we do so below. */
            break;
        }
    }

    *pchLen = cLen;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

int ShClUtf16CRLFLenUtf8(PCRTUTF16 pcwszSrc, size_t cwSrc, size_t *pchLen)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertReturn(cwSrc, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pchLen, VERR_INVALID_POINTER);

    AssertMsgReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER,
                    ("Big endian UTF-16 not supported yet\n"), VERR_NOT_SUPPORTED);

    size_t cLen = 0;

    /* Calculate the size of the destination text string. */
    /* Is this Utf16 or Utf16-LE? */
    if (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER)
        cLen = 0;
    else
        cLen = 1;

    for (size_t i = 0; i < cwSrc; ++i, ++cLen)
    {
        if (   (i + 1 < cwSrc)
            && (pcwszSrc[i]     == VBOX_SHCL_CARRIAGERETURN)
            && (pcwszSrc[i + 1] == VBOX_SHCL_LINEFEED))
        {
            ++i;
        }
        if (pcwszSrc[i] == 0)
            break;
    }

    *pchLen = cLen;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

int ShClConvUtf16LFToCRLF(PCRTUTF16 pcwszSrc, size_t cwcSrc, PRTUTF16 pu16Dst, size_t cwDst)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertPtrReturn(pu16Dst, VERR_INVALID_POINTER);
    AssertReturn(cwDst, VERR_INVALID_PARAMETER);

    AssertMsgReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER,
                    ("Big endian UTF-16 not supported yet\n"), VERR_NOT_SUPPORTED);

    int rc = VINF_SUCCESS;

    /* Don't copy the endian marker. */
    size_t i = pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER ? 1 : 0;
    size_t j = 0;

    for (; i < cwcSrc; ++i, ++j)
    {
        /* Don't copy the null byte, as we add it below. */
        if (pcwszSrc[i] == 0)
            break;

        /* Not enough space in destination? */
        if (j == cwDst)
        {
            rc = VERR_BUFFER_OVERFLOW;
            break;
        }

        if (pcwszSrc[i] == VBOX_SHCL_LINEFEED)
        {
            pu16Dst[j] = VBOX_SHCL_CARRIAGERETURN;
            ++j;

            /* Not enough space in destination? */
            if (j == cwDst)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }
        }
#ifdef RT_OS_DARWIN
        /* Check for a single carriage return (MacOS) */
        else if (pcwszSrc[i] == VBOX_SHCL_CARRIAGERETURN)
        {
            /* Set CR.r */
            pu16Dst[j] = VBOX_SHCL_CARRIAGERETURN;
            ++j;

            /* Not enough space in destination? */
            if (j == cwDst)
            {
                rc = VERR_BUFFER_OVERFLOW;
                break;
            }

            /* Add line feed. */
            pu16Dst[j] = VBOX_SHCL_LINEFEED;
            continue;
        }
#endif
        pu16Dst[j] = pcwszSrc[i];
    }

    if (j == cwDst)
        rc = VERR_BUFFER_OVERFLOW;

    if (RT_SUCCESS(rc))
    {
        /* Add terminator. */
        pu16Dst[j] = 0;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int ShClConvUtf16CRLFToLF(PCRTUTF16 pcwszSrc, size_t cwcSrc, PRTUTF16 pu16Dst, size_t cwDst)
{
    AssertPtrReturn(pcwszSrc, VERR_INVALID_POINTER);
    AssertReturn(cwcSrc,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(pu16Dst,  VERR_INVALID_POINTER);
    AssertReturn(cwDst,       VERR_INVALID_PARAMETER);

    AssertMsgReturn(pcwszSrc[0] != VBOX_SHCL_UTF16BEMARKER,
                    ("Big endian UTF-16 not supported yet\n"), VERR_NOT_SUPPORTED);

    /* Prepend the Utf16 byte order marker if it is missing. */
    size_t cwDstPos;
    if (pcwszSrc[0] == VBOX_SHCL_UTF16LEMARKER)
    {
        cwDstPos = 0;
    }
    else
    {
        pu16Dst[0] = VBOX_SHCL_UTF16LEMARKER;
        cwDstPos = 1;
    }

    for (size_t i = 0; i < cwcSrc; ++i, ++cwDstPos)
    {
        if (pcwszSrc[i] == 0)
            break;

        if (cwDstPos == cwDst)
            return VERR_BUFFER_OVERFLOW;

        if (   (i + 1 < cwcSrc)
            && (pcwszSrc[i]     == VBOX_SHCL_CARRIAGERETURN)
            && (pcwszSrc[i + 1] == VBOX_SHCL_LINEFEED))
        {
            ++i;
        }

        pu16Dst[cwDstPos] = pcwszSrc[i];
    }

    if (cwDstPos == cwDst)
        return VERR_BUFFER_OVERFLOW;

    /* Add terminating zero. */
    pu16Dst[cwDstPos] = 0;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

int ShClDibToBmp(const void *pvSrc, size_t cbSrc, void **ppvDest, size_t *pcbDest)
{
    AssertPtrReturn(pvSrc,   VERR_INVALID_POINTER);
    AssertReturn(cbSrc,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbDest, VERR_INVALID_POINTER);

    PBMPWIN3XINFOHDR coreHdr = (PBMPWIN3XINFOHDR)pvSrc;
    /** @todo Support all the many versions of the DIB headers. */
    if (   cbSrc < sizeof(BMPWIN3XINFOHDR)
        || RT_LE2H_U32(coreHdr->cbSize) < sizeof(BMPWIN3XINFOHDR)
        || RT_LE2H_U32(coreHdr->cbSize) != sizeof(BMPWIN3XINFOHDR))
    {
        return VERR_INVALID_PARAMETER;
    }

    size_t offPixel = sizeof(BMPFILEHDR)
                    + RT_LE2H_U32(coreHdr->cbSize)
                    + RT_LE2H_U32(coreHdr->cClrUsed) * sizeof(uint32_t);
    if (cbSrc < offPixel)
        return VERR_INVALID_PARAMETER;

    size_t cbDst = sizeof(BMPFILEHDR) + cbSrc;

    void *pvDest = RTMemAlloc(cbDst);
    if (!pvDest)
        return VERR_NO_MEMORY;

    PBMPFILEHDR fileHdr = (PBMPFILEHDR)pvDest;

    fileHdr->uType       = BMP_HDR_MAGIC;
    fileHdr->cbFileSize  = (uint32_t)RT_H2LE_U32(cbDst);
    fileHdr->Reserved1   = 0;
    fileHdr->Reserved2   = 0;
    fileHdr->offBits     = (uint32_t)RT_H2LE_U32(offPixel);

    memcpy((uint8_t *)pvDest + sizeof(BMPFILEHDR), pvSrc, cbSrc);

    *ppvDest = pvDest;
    *pcbDest = cbDst;

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}

int ShClBmpGetDib(const void *pvSrc, size_t cbSrc, const void **ppvDest, size_t *pcbDest)
{
    AssertPtrReturn(pvSrc,   VERR_INVALID_POINTER);
    AssertReturn(cbSrc,      VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppvDest, VERR_INVALID_POINTER);
    AssertPtrReturn(pcbDest, VERR_INVALID_POINTER);

    PBMPFILEHDR pBmpHdr = (PBMPFILEHDR)pvSrc;
    if (   cbSrc < sizeof(BMPFILEHDR)
        || pBmpHdr->uType != BMP_HDR_MAGIC
        || RT_LE2H_U32(pBmpHdr->cbFileSize) != cbSrc)
    {
        return VERR_INVALID_PARAMETER;
    }

    *ppvDest = ((uint8_t *)pvSrc) + sizeof(BMPFILEHDR);
    *pcbDest = cbSrc - sizeof(BMPFILEHDR);

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
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_CANCELED);
        RT_CASE_RET_STR(VBOX_SHCL_HOST_MSG_READ_DATA_CID);
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
