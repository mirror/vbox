/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 */

/*
 * Copyright (C) 2014-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#define LOG_GROUP LOG_GROUP_AUDIO_MIXER
#include <VBox/log.h>
#include "AudioMixer.h"
#include "AudioMixBuffer.h"

#include <VBox/vmm/pdm.h>
#include <VBox/err.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmaudioifs.h>

#include <iprt/alloc.h>
#include <iprt/asm-math.h>
#include <iprt/assert.h>
#include <iprt/string.h>

static int audioMixerUpdateSinkVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster);


int AudioMixerAddSink(PAUDIOMIXER pMixer, const char *pszName, AUDMIXSINKDIR enmDir, PAUDMIXSINK *ppSink)
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
            pSink->pParent  = pMixer;
            pSink->cStreams = 0;
            pSink->enmDir   = enmDir;
            RTListInit(&pSink->lstStreams);

            /* Set initial volume to max. */
            pSink->Volume.fMuted = false;
            pSink->Volume.uLeft  = 0x7F;
            pSink->Volume.uRight = 0x7F;

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

int AudioMixerAddStreamIn(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMIN pStream,
                          uint32_t uFlags, PAUDMIXSTREAM *ppStream)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /** @todo Add flag validation. */
    /* ppStream is optional. */

    int rc;

    if (pSink->cStreams == UINT8_MAX) /* 255 streams per sink max. */
        return VERR_TOO_MUCH_DATA;

    PAUDMIXSTREAM pMixStream
        = (PAUDMIXSTREAM)RTMemAllocZ(sizeof(AUDMIXSTREAM));
    if (pMixStream)
    {
        pMixStream->pConn = pConnector;
        pMixStream->pIn   = pStream;
        /** @todo Process flags. */

        RTListAppend(&pSink->lstStreams, &pMixStream->Node);
        pSink->cStreams++;

        LogFlowFunc(("%s: pStream=%p, cStreams=%RU8\n",
                     pSink->pszName, pMixStream, pSink->cStreams));

        /* Increase the stream's reference count to let others know
         * we're reyling on it to be around now. */
        pStream->State.cRefs++;

        if (ppStream)
            *ppStream = pMixStream;

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int AudioMixerAddStreamOut(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMOUT pStream,
                           uint32_t uFlags, PAUDMIXSTREAM *ppStream)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /** @todo Add flag validation. */
    /* ppStream is optional. */

    int rc;

    if (pSink->cStreams == UINT8_MAX) /* 255 streams per sink max. */
        return VERR_TOO_MUCH_DATA;

    PAUDMIXSTREAM pMixStream
        = (PAUDMIXSTREAM)RTMemAllocZ(sizeof(AUDMIXSTREAM));
    if (pMixStream)
    {
        pMixStream->pConn = pConnector;
        pMixStream->pOut  = pStream;
        /** @todo Process flags. */

        RTListAppend(&pSink->lstStreams, &pMixStream->Node);
        pSink->cStreams++;

        LogFlowFunc(("%s: pStream=%p, cStreams=%RU8\n",
                     pSink->pszName, pMixStream, pSink->cStreams));

        /* Increase the stream's reference count to let others know
         * we're reyling on it to be around now. */
        pStream->State.cRefs++;

        if (ppStream)
            *ppStream = pMixStream;

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int AudioMixerControlStream(PAUDIOMIXER pMixer, PAUDMIXSTREAM pHandle)
{
    return VERR_NOT_IMPLEMENTED;
}

int AudioMixerCreate(const char *pszName, uint32_t uFlags, PAUDIOMIXER *ppMixer)
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

            pMixer->VolMaster.fMuted = false;
            pMixer->VolMaster.uLeft  = UINT32_MAX;
            pMixer->VolMaster.uRight = UINT32_MAX;

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

void AudioMixerDestroy(PAUDIOMIXER pMixer)
{
    if (!pMixer)
        return;

    LogFlowFunc(("Destroying %s ...\n", pMixer->pszName));

    PAUDMIXSINK pSink, pSinkNext;
    RTListForEachSafe(&pMixer->lstSinks, pSink, pSinkNext, AUDMIXSINK, Node)
        AudioMixerRemoveSink(pMixer, pSink);

    Assert(pMixer->cSinks == 0);

    if (pMixer->pszName)
    {
        RTStrFree(pMixer->pszName);
        pMixer->pszName = NULL;
    }

    RTMemFree(pMixer);
}

static void audioMixerDestroySink(PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pSink);
    if (!pSink)
        return;

    if (pSink->pszName)
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

int AudioMixerGetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    /** @todo Perform a deep copy, if needed. */
    *pCfg = pMixer->devFmt;

    return VINF_SUCCESS;
}

uint32_t AudioMixerGetStreamCount(PAUDIOMIXER pMixer)
{
    AssertPtrReturn(pMixer, 0);

    uint32_t cStreams = 0;

    PAUDMIXSINK pSink;
    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
        cStreams += pSink->cStreams;

    return cStreams;
}

void AudioMixerInvalidate(PAUDIOMIXER pMixer)
{
    AssertPtrReturnVoid(pMixer);

    LogFlowFunc(("%s: Invalidating ...\n", pMixer->pszName));

    /* Propagate new master volume to all connected sinks. */
    PAUDMIXSINK pSink;
    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
    {
        int rc2 = audioMixerUpdateSinkVolume(pSink, &pMixer->VolMaster);
        AssertRC(rc2);
    }
}

int AudioMixerProcessSinkIn(PAUDMIXSINK pSink, AUDMIXOP enmOp, void *pvBuf, uint32_t cbBuf, uint32_t *pcbProcessed)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf, VERR_INVALID_PARAMETER);
    /* pcbProcessed is optional. */

    /** @todo Handle mixing operation enmOp! */

    uint8_t *pvMixBuf = (uint8_t *)RTMemAlloc(cbBuf);
    if (!pvMixBuf)
        return VERR_NO_MEMORY;

    int rc = VERR_NOT_FOUND;
    uint32_t cbProcessed = 0;

    LogFlowFunc(("%s: pvBuf=%p, cbBuf=%zu\n", pSink->pszName, pvBuf, cbBuf));

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        if (!pStream->pConn->pfnIsActiveIn(pStream->pConn, pStream->pIn))
            continue;

        uint32_t cbTotalRead = 0;
        uint32_t cbToRead = cbBuf;

        while (cbToRead)
        {
            uint32_t cbRead;
            AssertPtr(pStream->pConn);
            rc = pStream->pConn->pfnRead(pStream->pConn, pStream->pIn,
                                         (uint8_t *)pvMixBuf + cbTotalRead, cbToRead, &cbRead);
            if (   RT_FAILURE(rc)
                || !cbRead)
                break;

            /** @todo Right now we only handle one stream (the last one added in fact). */

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

int AudioMixerProcessSinkOut(PAUDMIXSINK pSink, AUDMIXOP enmOp, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbProcessed)
{
    return VERR_NOT_IMPLEMENTED;
}

void AudioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pMixer);
    if (!pSink)
        return;

    PAUDMIXSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pSink->lstStreams, pStream, pStreamNext, AUDMIXSTREAM, Node)
        AudioMixerRemoveStream(pSink, pStream);

    Assert(pSink->cStreams == 0);

    RTListNodeRemove(&pSink->Node);
    Assert(pMixer->cSinks);
    pMixer->cSinks--;

    LogFlowFunc(("%s: pSink=%s, cSinks=%RU8\n",
                 pMixer->pszName, pSink->pszName, pMixer->cSinks));

    audioMixerDestroySink(pSink);
}

void AudioMixerRemoveStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    AssertPtrReturnVoid(pSink);
    if (!pStream)
        return;

    Assert(pSink->cStreams);
    RTListNodeRemove(&pStream->Node);
    pSink->cStreams--;

#ifdef DEBUG
    const char *pszStream = pSink->enmDir == AUDMIXSINKDIR_INPUT
                          ? pStream->pIn->MixBuf.pszName : pStream->pOut->MixBuf.pszName;

    LogFlowFunc(("%s: pStream=%s, cStreams=%RU8\n",
                 pSink->pszName, pszStream ? pszStream : "<Unnamed>", pSink->cStreams));
#endif

    /* Decrease the reference count again. */
    switch (pSink->enmDir)
    {
        case AUDMIXSINKDIR_INPUT:
        {
            Assert(pStream->pIn->State.cRefs);
            pStream->pIn->State.cRefs--;
            break;
        }

        case AUDMIXSINKDIR_OUTPUT:
        {
            Assert(pStream->pOut->State.cRefs);
            pStream->pOut->State.cRefs--;
            break;
        }

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    audioMixerDestroyStream(pStream);
}

int AudioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    /** @todo Perform a deep copy, if needed. */
    pMixer->devFmt = *pCfg;

    return VINF_SUCCESS;
}

static int audioMixerUpdateSinkVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster)
{
    AssertPtrReturn(pSink,      VERR_INVALID_POINTER);
    AssertPtrReturn(pVolMaster, VERR_INVALID_POINTER);

    LogFlowFunc(("Master fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  pVolMaster->fMuted, pVolMaster->uLeft, pVolMaster->uRight));
    LogFlowFunc(("%s: fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  pSink->pszName, pSink->Volume.fMuted, pSink->Volume.uLeft, pSink->Volume.uRight));

    /** @todo Very crude implementation for now -- needs more work! */

    PDMAUDIOVOLUME volSink;
    volSink.fMuted  = pVolMaster->fMuted || pSink->Volume.fMuted;
    volSink.uLeft   = (pSink->Volume.uLeft  * pVolMaster->uLeft)  / UINT8_MAX;
    volSink.uRight  = (pSink->Volume.uRight * pVolMaster->uRight) / UINT8_MAX;

    LogFlowFunc(("\t-> fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  volSink.fMuted, volSink.uLeft, volSink.uRight));

    bool fOut = pSink->enmDir == AUDMIXSINKDIR_OUTPUT;

    /* Propagate new sink volume to all streams in the sink. */
    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        if (fOut)
            AudioMixBufSetVolume(&pStream->pOut->MixBuf, &volSink);
        else
            AudioMixBufSetVolume(&pStream->pIn->MixBuf,  &volSink);
    }

    return VINF_SUCCESS;
}

/** Set the master volume of the mixer. */
int AudioMixerSetMasterVolume(PAUDIOMIXER pMixer, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,   VERR_INVALID_POINTER);

    pMixer->VolMaster = *pVol;

    LogFlowFunc(("%s: lVol=%RU32, rVol=%RU32 => fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                 pMixer->pszName, pVol->uLeft, pVol->uRight,
                 pMixer->VolMaster.fMuted, pMixer->VolMaster.uLeft, pMixer->VolMaster.uRight));

    AudioMixerInvalidate(pMixer);
    return VINF_SUCCESS;
}

/** Set the volume of an individual sink. */
int AudioMixerSetSinkVolume(PAUDMIXSINK pSink, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,  VERR_INVALID_POINTER);
    AssertPtr(pSink->pParent);

    LogFlowFunc(("%s: fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n", pSink->pszName, pVol->fMuted, pVol->uLeft, pVol->uRight));

    pSink->Volume = *pVol;

    return audioMixerUpdateSinkVolume(pSink, &pSink->pParent->VolMaster);
}

void AudioMixerDebug(PAUDIOMIXER pMixer, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PAUDMIXSINK pSink;
    unsigned    iSink = 0;

    pHlp->pfnPrintf(pHlp, "[Master] %s: lVol=%u, rVol=%u, fMuted=%RTbool\n", pMixer->pszName,
                    pMixer->VolMaster.uLeft, pMixer->VolMaster.uRight, pMixer->VolMaster.fMuted);

    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
    {
        pHlp->pfnPrintf(pHlp, "[Sink %u] %s: lVol=%u, rVol=%u, fMuted=%RTbool\n", iSink, pSink->pszName,
                        pSink->Volume.uLeft, pSink->Volume.uRight, pSink->Volume.fMuted);
        ++iSink;
    }
}
