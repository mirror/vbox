/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AudioMixer.h"

#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/alloc.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#include <VBox/log.h>

int audioMixerAddSink(PAUDIOMIXER pMixer, const char *pszName, PAUDMIXSINK *ppSink)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    /** ppSink is optional. */

    int rc = VINF_SUCCESS;

    PAUDMIXSINK pSink = (PAUDMIXSINK)RTMemAllocZ(sizeof(AUDMIXSINK));
    if (pSink)
    {
        pSink->pszName = RTStrDup(pszName);
        if (!pSink->pszName)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            pSink->pParent = pMixer;
            pSink->cStreams = 0;
            RTListInit(&pSink->lstStreams);

            RTListAppend(&pMixer->lstSinks, &pSink->Node);
            pMixer->cSinks++;

            LogFlowFunc(("pMixer=%p, pSink=%p, cSinks=%RU8\n",
                         pMixer, pSink, pMixer->cSinks));

            if (ppSink)
                *ppSink = pSink;
        }
        else
            RTMemFree(pSink);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int audioMixerAddStreamIn(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMIN pStream,
                          uint32_t uFlags, PAUDMIXSTREAM *ppStream)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /** @todo Add flag validation. */
    /* ppStream is optional. */

    int rc;

    if (pSink->cStreams == INT8_MAX) /* 255 streams per sink max. */
        return VERR_TOO_MUCH_DATA;

    PAUDMIXSTREAM pMixStream
        = (PAUDMIXSTREAM)RTMemAllocZ(sizeof(AUDMIXSTREAM));
    if (pMixStream)
    {
        pMixStream->pConn = pConnector;
        pMixStream->pStrm = pStream;
        /** @todo Process flags. */

        RTListAppend(&pSink->lstStreams, &pMixStream->Node);
        pSink->cStreams++;

        LogFlowFunc(("pSink=%p, pStream=%p, cStreams=%RU8\n",
                     pSink, pMixStream, pSink->cStreams));

        if (ppStream)
            *ppStream = pMixStream;

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int audioMixerControlStream(PAUDIOMIXER pMixer, PAUDMIXSTREAM pHandle)
{
    return VERR_NOT_IMPLEMENTED;
}

int audioMixerCreate(const char *pszName, uint32_t uFlags, PAUDIOMIXER *ppMixer)
{
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    /** @todo Add flag validation. */
    AssertPtrReturn(ppMixer, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    PAUDIOMIXER pMixer = (PAUDIOMIXER)RTMemAllocZ(sizeof(AUDIOMIXER));
    if (pMixer)
    {
        pMixer->pszName = RTStrDup(pszName);
        if (!pMixer->pszName)
            rc = VERR_NO_MEMORY;

        if (RT_SUCCESS(rc))
        {
            pMixer->cSinks = 0;
            RTListInit(&pMixer->lstSinks);

            LogFlowFunc(("Created %p ...\n", pMixer));

            *ppMixer = pMixer;
        }
        else
            RTMemFree(pMixer);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

void audioMixerDestroy(PAUDIOMIXER pMixer)
{
    if (pMixer)
    {
        LogFlowFunc(("Destroying %p ...\n", pMixer));

        PAUDMIXSINK pSink = RTListGetFirst(&pMixer->lstSinks, AUDMIXSINK, Node);
        while (pSink)
        {
            PAUDMIXSINK pNext = RTListNodeGetNext(&pSink->Node, AUDMIXSINK, Node);
            bool fLast = RTListNodeIsLast(&pMixer->lstSinks, &pSink->Node);

            audioMixerRemoveSink(pMixer, pSink);

            if (fLast)
                break;

            pSink = pNext;
        }

        Assert(pMixer->cSinks == 0);

        RTStrFree(pMixer->pszName);

        RTMemFree(pMixer);
    }
}

static void audioMixerDestroySink(PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pSink);
    if (!pSink)
        return;

    RTStrFree(pSink->pszName);

    RTMemFree(pSink);
}

static void audioMixerDestroyStream(PAUDMIXSTREAM pStream)
{
    AssertPtrReturnVoid(pStream);
    if (!pStream)
        return;

    RTMemFree(pStream);
}

uint32_t audioMixerGetStreamCount(PAUDIOMIXER pMixer)
{
    AssertPtrReturn(pMixer, 0);

    uint32_t cStreams = 0;

    PAUDMIXSINK pSink;
    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
        cStreams += pSink->cStreams;

    return cStreams;
}

int audioMixerProcessSinkIn(PAUDMIXSINK pSink, void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbProcessed is optional. */

    uint8_t *pvMixBuf = (uint8_t *)RTMemAlloc(cbBuf);
    if (!pvMixBuf)
        return VERR_NO_MEMORY;

    int rc = VERR_NOT_FOUND;
    uint32_t cbProcessed = 0;

    LogFlowFunc(("%s: pvBuf=%p, cbBuf=%zu\n", pSink->pszName, pvBuf, cbBuf));

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        /** @todo Support output sinks as well! */
        if (!pStream->pConn->pfnIsActiveIn(pStream->pConn, pStream->pStrm))
            continue;

        uint32_t cbTotalRead = 0;
        size_t cbToRead = cbBuf;

        while (cbToRead)
        {
            uint32_t cbRead;
            AssertPtr(pStream->pConn);
            rc = pStream->pConn->pfnRead(pStream->pConn, pStream->pStrm,
                                         (uint8_t *)pvMixBuf + cbTotalRead, cbToRead, &cbRead);
            if (   RT_FAILURE(rc)
                || !cbRead)
                break;

            AssertBreakStmt(cbRead <= cbToRead, rc = VERR_BUFFER_OVERFLOW);
            cbToRead -= cbRead;
            cbTotalRead += cbRead;
        }

        if (RT_FAILURE(rc))
            continue;

        cbProcessed = RT_MAX(cbProcessed, cbTotalRead);
    }

    if (RT_SUCCESS(rc))
    {
        memcpy(pvBuf, pvMixBuf, cbProcessed); /* @todo Use an intermediate mixing buffer per sink! */

        if (pcbProcessed)
            *pcbProcessed = cbProcessed;
    }

    RTMemFree(pvMixBuf);

    LogFlowFunc(("cbProcessed=%RU32, rc=%Rrc\n", cbProcessed, rc));
    return rc;
}

void audioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pMixer);
    if (!pSink)
        return;

    PAUDMIXSTREAM pStream = RTListGetFirst(&pSink->lstStreams, AUDMIXSTREAM, Node);
    while (pStream)
    {
        PAUDMIXSTREAM pNext = RTListNodeGetNext(&pStream->Node, AUDMIXSTREAM, Node);
        bool fLast = RTListNodeIsLast(&pSink->lstStreams, &pStream->Node);

        audioMixerRemoveStream(pSink, pStream);

        if (fLast)
            break;

        pStream = pNext;
    }

    Assert(pSink->cStreams == 0);

    RTListNodeRemove(&pSink->Node);
    Assert(pMixer->cSinks);
    pMixer->cSinks--;

    LogFlowFunc(("pMixer=%p, pSink=%p, cSinks=%RU8\n",
                 pMixer, pSink, pMixer->cSinks));

    audioMixerDestroySink(pSink);
}

void audioMixerRemoveStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    AssertPtrReturnVoid(pSink);
    if (!pStream)
        return;

    RTListNodeRemove(&pStream->Node);
    Assert(pSink->cStreams);
    pSink->cStreams--;

    LogFlowFunc(("pSink=%p, pStream=%p, cStreams=%RU8\n",
                 pSink, pStream, pSink->cStreams));

    audioMixerDestroyStream(pStream);
}

int audioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    /** @todo Perform a deep copy, if needed. */
    pMixer->devFmt = *pCfg;

    return VINF_SUCCESS;
}

