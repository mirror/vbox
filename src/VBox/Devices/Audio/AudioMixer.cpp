/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 *
 * This mixer acts as a layer between the audio connector interface and
 * the actual device emulation, providing mechanisms for audio sources (input) and
 * audio sinks (output).
 *
 * As audio driver instances are handled as LUNs on the device level, this
 * audio mixer then can take care of e.g. mixing various inputs/outputs to/from
 * a specific source/sink.
 *
 * How and which audio streams are connected to sinks/sources depends on how
 * the audio mixer has been set up.
 *
 * A sink can connect multiple output streams together, whereas a source
 * does this with input streams. Each sink / source consists of one or more
 * so-called mixer streams, which then in turn have pointers to the actual
 * PDM audio input/output streams.
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

static void audioMixerSinkDestroyInternal(PAUDMIXSINK pSink);
static int audioMixerSinkUpdateVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster);
static void audioMixerSinkRemoveStreamInternal(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream);

static void audioMixerStreamDestroyInternal(PAUDMIXSTREAM pStream);
static void audioMixerStreamFree(PAUDMIXSTREAM pStream);

int AudioMixerCreateSink(PAUDIOMIXER pMixer, const char *pszName, AUDMIXSINKDIR enmDir, PAUDMIXSINK *ppSink)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pszName, VERR_INVALID_POINTER);
    /* ppSink is optional. */

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

void AudioMixerDestroy(PAUDIOMIXER pMixer)
{
    if (!pMixer)
        return;

    LogFlowFunc(("Destroying %s ...\n", pMixer->pszName));

    PAUDMIXSINK pSink, pSinkNext;
    RTListForEachSafe(&pMixer->lstSinks, pSink, pSinkNext, AUDMIXSINK, Node)
        audioMixerSinkDestroyInternal(pSink);

    if (pMixer->pszName)
    {
        RTStrFree(pMixer->pszName);
        pMixer->pszName = NULL;
    }

    RTMemFree(pMixer);
    pMixer = NULL;
}

int AudioMixerGetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,   VERR_INVALID_POINTER);

    /** @todo Perform a deep copy, if needed. */
    *pCfg = pMixer->devFmt;

    return VINF_SUCCESS;
}

void AudioMixerInvalidate(PAUDIOMIXER pMixer)
{
    AssertPtrReturnVoid(pMixer);

    LogFlowFunc(("%s: Invalidating ...\n", pMixer->pszName));

    /* Propagate new master volume to all connected sinks. */
    PAUDMIXSINK pSink;
    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
    {
        int rc2 = audioMixerSinkUpdateVolume(pSink, &pMixer->VolMaster);
        AssertRC(rc2);
    }
}

void AudioMixerRemoveSink(PAUDIOMIXER pMixer, PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pMixer);
    if (!pSink)
        return;

    /** @todo Check if pSink is part of pMixer. */

    AudioMixerSinkRemoveAllStreams(pSink);

    Assert(pSink->cStreams == 0);

    RTListNodeRemove(&pSink->Node);

    Assert(pMixer->cSinks);
    pMixer->cSinks--;

    LogFlowFunc(("%s: pSink=%s, cSinks=%RU8\n",
                 pMixer->pszName, pSink->pszName, pMixer->cSinks));
}

int AudioMixerSetDeviceFormat(PAUDIOMIXER pMixer, PPDMAUDIOSTREAMCFG pCfg)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    /** @todo Perform a deep copy, if needed. */
    pMixer->devFmt = *pCfg;

    return VINF_SUCCESS;
}

/**
 * Sets the mixer's master volume.
 *
 * @returns IPRT status code.
 * @param   pMixer          Mixer to set master volume for.
 * @param   pVol            Volume to set.
 */
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

/*****************************************************************************
 * Mixer Sink implementation.
 *****************************************************************************/

int AudioMixerSinkAddStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    AssertPtrReturn(pSink,   VERR_INVALID_POINTER);
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);

    if (pSink->cStreams == UINT8_MAX) /* 255 streams per sink max. */
        return VERR_NO_MORE_HANDLES;

    /** @todo Check if stream already is assigned to (another) sink. */

    RTListAppend(&pSink->lstStreams, &pStream->Node);
    pSink->cStreams++;

    LogFlowFunc(("%s: cStreams=%RU8\n", pSink->pszName, pSink->cStreams));

    return VINF_SUCCESS;
}

static PDMAUDIOSTREAMCMD audioMixerSinkToStreamCmd(AUDMIXSINKCMD enmCmd)
{
    switch (enmCmd)
    {
        case AUDMIXSINKCMD_ENABLE:   return PDMAUDIOSTREAMCMD_ENABLE;
        case AUDMIXSINKCMD_DISABLE:  return PDMAUDIOSTREAMCMD_DISABLE;
        case AUDMIXSINKCMD_PAUSE:    return PDMAUDIOSTREAMCMD_PAUSE;
        case AUDMIXSINKCMD_RESUME:   return PDMAUDIOSTREAMCMD_RESUME;
        default:                     break;
    }

    AssertMsgFailed(("Unsupported sink command %ld\n", enmCmd));
    return PDMAUDIOSTREAMCMD_UNKNOWN;
}

int AudioMixerSinkCtl(PAUDMIXSINK pSink, AUDMIXSINKCMD enmCmd)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);

    PDMAUDIOSTREAMCMD enmCmdStream = audioMixerSinkToStreamCmd(enmCmd);
    if (enmCmdStream == PDMAUDIOSTREAMCMD_UNKNOWN)
        return VERR_NOT_SUPPORTED;

    int rc = VINF_SUCCESS;

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        int rc2 = AudioMixerStreamCtl(pStream, enmCmdStream, AUDMIXSTRMCTL_FLAG_NONE);
        if (RT_SUCCESS(rc))
            rc = rc2;
        /* Keep going. Flag? */
    }

    LogFlowFunc(("Sink=%s, Cmd=%ld, rc=%Rrc\n", pSink->pszName, enmCmd, rc));
    return rc;
}

void AudioMixerSinkDestroy(PAUDMIXSINK pSink)
{
    audioMixerSinkDestroyInternal(pSink);
}

static void audioMixerSinkDestroyInternal(PAUDMIXSINK pSink)
{
    AssertPtrReturnVoid(pSink);
    if (!pSink)
        return;

    LogFunc(("%s\n", pSink->pszName));

    PAUDMIXSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pSink->lstStreams, pStream, pStreamNext, AUDMIXSTREAM, Node)
    {
        audioMixerSinkRemoveStreamInternal(pSink, pStream);
        audioMixerStreamDestroyInternal(pStream);
    }

    Assert(pSink->cStreams == 0);

    if (pSink->pszName)
        RTStrFree(pSink->pszName);

    RTMemFree(pSink);
}

PAUDMIXSTREAM AudioMixerSinkGetStream(PAUDMIXSINK pSink, uint8_t uIndex)
{
    AssertPtrReturn(pSink, NULL);
    AssertMsgReturn(uIndex < pSink->cStreams,
                    ("Index %RU8 exceeds stream count (%RU8)", uIndex, pSink->cStreams), NULL);

    /* Slow lookup, d'oh. */
    PAUDMIXSTREAM pStream = RTListGetFirst(&pSink->lstStreams, AUDMIXSTREAM, Node);
    while (uIndex)
    {
        pStream = RTListGetNext(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node);
        uIndex--;
    }

    AssertPtr(pStream);
    return pStream;
}

uint8_t AudioMixerSinkGetStreamCount(PAUDMIXSINK pSink)
{
    if (!pSink)
        return 0;

    return pSink->cStreams;
}

int AudioMixerSinkRead(PAUDMIXSINK pSink, AUDMIXOP enmOp, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf, VERR_INVALID_POINTER);
    AssertReturn(cbBuf,    VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    /** @todo Handle mixing operation enmOp! */

    uint8_t *pvMixBuf = (uint8_t *)RTMemAlloc(cbBuf);
    if (!pvMixBuf)
        return VERR_NO_MEMORY;

    int rc = VERR_NOT_FOUND;
    uint32_t cbRead = 0;

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        if (!pStream->pConn->pfnIsActiveIn(pStream->pConn, pStream->InOut.pIn))
            continue;

        uint32_t cbTotalRead = 0;
        uint32_t cbToRead = cbBuf;

        while (cbToRead)
        {
            uint32_t cbReadStrm;
            AssertPtr(pStream->pConn);
            rc = pStream->pConn->pfnRead(pStream->pConn, pStream->InOut.pIn,
                                         (uint8_t *)pvMixBuf + cbTotalRead, cbToRead, &cbReadStrm);
            if (   RT_FAILURE(rc)
                || !cbReadStrm)
                break;

            /** @todo Right now we only handle one stream (the last one added in fact). */

            AssertBreakStmt(cbReadStrm <= cbToRead, rc = VERR_BUFFER_OVERFLOW);
            cbToRead    -= cbReadStrm;
            cbTotalRead += cbReadStrm;
        }

        if (RT_FAILURE(rc))
            continue;

        cbRead = RT_MAX(cbRead, cbTotalRead);
    }

    if (RT_SUCCESS(rc))
    {
        memcpy(pvBuf, pvMixBuf, cbRead); /* @todo Use an intermediate mixing buffer per sink! */

        if (pcbRead)
            *pcbRead = cbRead;
    }

    RTMemFree(pvMixBuf);

    Log3Func(("%s: cbRead=%RU32, rc=%Rrc\n", pSink->pszName, cbRead, rc));
    return rc;
}

static void audioMixerSinkRemoveStreamInternal(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    AssertPtrReturnVoid(pSink);
    if (!pStream)
        return;

    /** @todo Check if pStream is part of pSink. */

    Assert(pSink->cStreams);

#ifdef DEBUG
    const char *pszStream = pSink->enmDir == AUDMIXSINKDIR_INPUT
                          ? pStream->InOut.pIn->MixBuf.pszName : pStream->InOut.pOut->MixBuf.pszName;

    LogFlowFunc(("%s: (Stream = %s), cStreams=%RU8\n",
                 pSink->pszName, pszStream ? pszStream : "<Unnamed>", pSink->cStreams));
#endif

    /* Remove stream from sink. */
    RTListNodeRemove(&pStream->Node);

    Assert(pSink->cStreams);
    pSink->cStreams--;
}

void AudioMixerSinkRemoveStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    audioMixerSinkRemoveStreamInternal(pSink, pStream);
}

/**
 * Removes all attached streams from a given sink.
 *
 * @param pSink         Sink to remove attached streams from.
 */
void AudioMixerSinkRemoveAllStreams(PAUDMIXSINK pSink)
{
    if (!pSink)
        return;

    LogFunc(("%s\n", pSink->pszName));

    PAUDMIXSTREAM pStream, pStreamNext;
    RTListForEachSafe(&pSink->lstStreams, pStream, pStreamNext, AUDMIXSTREAM, Node)
        audioMixerSinkRemoveStreamInternal(pSink, pStream);

    Assert(pSink->cStreams == 0);
}

int AudioMixerSinkSetFormat(PAUDMIXSINK pSink, PPDMPCMPROPS pPCMProps)
{
    AssertPtrReturn(pSink,     VERR_INVALID_POINTER);
    AssertPtrReturn(pPCMProps, VERR_INVALID_POINTER);

    memcpy(&pSink->PCMProps, pPCMProps, sizeof(PDMPCMPROPS));

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        /** @todo Invalidate mix buffers! */
    }

    return VINF_SUCCESS;
}

/**
 * Set the volume of an individual sink.
 *
 * @returns IPRT status code.
 * @param   pSink           Sink to set volume for.
 * @param   pVol            Volume to set.
 */
int AudioMixerSinkSetVolume(PAUDMIXSINK pSink, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,  VERR_INVALID_POINTER);
    AssertPtr(pSink->pParent);

    LogFlowFunc(("%s: fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n", pSink->pszName, pVol->fMuted, pVol->uLeft, pVol->uRight));

    pSink->Volume = *pVol;

    return audioMixerSinkUpdateVolume(pSink, &pSink->pParent->VolMaster);
}

int AudioMixerSinkUpdate(PAUDMIXSINK pSink)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        uint32_t cbIn, cbOut;
        uint32_t cSamplesLive;
        int rc2 = pStream->pConn->pfnQueryStatus(pStream->pConn,
                                                 &cbIn, &cbOut, &cSamplesLive);
#ifdef DEBUG
        if (   cbIn
            || cbOut
            || cSamplesLive)
        {
            Log3Func(("cbIn=%RU32, cbOut=%RU32, cSamplesLive=%RU32, rc2=%Rrc\n", cbIn, cbOut, cSamplesLive, rc2));
        }
#endif
        if (pSink->enmDir == AUDMIXSINKDIR_OUTPUT)
        {
            rc2 = pStream->pConn->pfnPlayOut(pStream->pConn, NULL /* pcSamplesPlayed */);
            if (RT_FAILURE(rc2))
                Log3Func(("rc2=%Rrc\n", rc2));
        }
        else if (pSink->enmDir == AUDMIXSINKDIR_INPUT)
        {
            //int rc2 = pStream->pConn->pfnCaptureIn(pStream->pConn, NULL /* pcSamplesCaptured */);
            //Log3Func(("rc2=%Rrc\n", rc2));
        }
        else
            AssertMsgFailed(("Direction not implemented\n"));
    }

    return VINF_SUCCESS;
}

static int audioMixerSinkUpdateVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster)
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
            AudioMixBufSetVolume(&pStream->InOut.pOut->MixBuf, &volSink);
        else
            AudioMixBufSetVolume(&pStream->InOut.pIn->MixBuf,  &volSink);
    }

    return VINF_SUCCESS;
}

int AudioMixerSinkWrite(PAUDMIXSINK pSink, AUDMIXOP enmOp, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    if (!pvBuf || !cbBuf)
    {
        if (pcbWritten)
            *pcbWritten = 0;
        return VINF_SUCCESS;
    }

    uint32_t cbProcessed = 0;

    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        int rc2 = pStream->pConn->pfnWrite(pStream->pConn, pStream->InOut.pOut, pvBuf, cbBuf, &cbProcessed);
        if (   RT_FAILURE(rc2)
            || cbProcessed < cbBuf)
        {
            LogFlowFunc(("rc=%Rrc, cbBuf=%RU32, cbProcessed=%RU32\n", rc2, cbBuf, cbProcessed));
        }
    }

    if (pcbWritten)
        *pcbWritten = cbBuf; /* Always report back a complete write for now. */

    return VINF_SUCCESS;
}

/*****************************************************************************
 * Mixer Stream implementation.
 *****************************************************************************/

int AudioMixerStreamCtl(PAUDMIXSTREAM pStream, PDMAUDIOSTREAMCMD enmCmd, uint32_t fCtl)
{
    AssertPtrReturn(pStream, VERR_INVALID_POINTER);
    /** @todo Validate fCtl. */

    int rc;
    switch (pStream->enmDir)
    {
        case PDMAUDIODIR_IN:
        {
            if (   enmCmd == PDMAUDIOSTREAMCMD_ENABLE
                || enmCmd == PDMAUDIOSTREAMCMD_DISABLE)
            {
                rc = pStream->pConn->pfnEnableIn(pStream->pConn, pStream->InOut.pIn,
                                                 enmCmd == PDMAUDIOSTREAMCMD_ENABLE);
            }
            else
                AssertFailed();
            break;
        }

        case PDMAUDIODIR_OUT:
        {
            if (   enmCmd == PDMAUDIOSTREAMCMD_ENABLE
                || enmCmd == PDMAUDIOSTREAMCMD_DISABLE)
            {
                rc = pStream->pConn->pfnEnableOut(pStream->pConn, pStream->InOut.pOut,
                                                  enmCmd == PDMAUDIOSTREAMCMD_ENABLE);
            }
            else
                AssertFailed();
            break;
        }

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    return rc;
}

int AudioMixerStreamCreate(PPDMIAUDIOCONNECTOR pConn, PPDMAUDIOSTREAMCFG pCfg, uint32_t fFlags, PAUDMIXSTREAM *ppStream)
{
    AssertPtrReturn(pConn, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg,  VERR_INVALID_POINTER);
    /** @todo Validate fFlags. */
    /* ppStream is optional. */

    PAUDMIXSTREAM pMixStream = (PAUDMIXSTREAM)RTMemAllocZ(sizeof(AUDMIXSTREAM));
    if (!pMixStream)
        return VERR_NO_MEMORY;

    pMixStream->pszName = RTStrDup(pCfg->szName);
    if (!pMixStream->pszName)
    {
        RTMemFree(pMixStream);
        return VERR_NO_MEMORY;
    }

    int rc;
    if (pCfg->enmDir == PDMAUDIODIR_IN)
    {
        PPDMAUDIOGSTSTRMIN pGstStrm;
        rc = pConn->pfnCreateIn(pConn, pCfg, &pGstStrm);
        if (RT_SUCCESS(rc))
        {
            pMixStream->InOut.pIn = pGstStrm;

            /* Increase the stream's reference count to let others know
             * we're reyling on it to be around now. */
            pConn->pfnAddRefIn(pConn, pGstStrm);
        }
    }
    else if (pCfg->enmDir == PDMAUDIODIR_OUT)
    {
        PPDMAUDIOGSTSTRMOUT pGstStrm;
        rc = pConn->pfnCreateOut(pConn, pCfg, &pGstStrm);
        if (RT_SUCCESS(rc))
        {
            pMixStream->InOut.pOut = pGstStrm;

            /* Increase the stream's reference count to let others know
             * we're reyling on it to be around now. */
            pConn->pfnAddRefOut(pConn, pGstStrm);
        }
    }
    else
        rc = VERR_NOT_SUPPORTED;

    if (RT_SUCCESS(rc))
    {
        pMixStream->fFlags = fFlags;
        pMixStream->enmDir = pCfg->enmDir;
        pMixStream->pConn  = pConn;

        if (ppStream)
            *ppStream = pMixStream;
    }
    else if (pMixStream)
    {
        RTStrFree(pMixStream->pszName);
        pMixStream->pszName = NULL;

        RTMemFree(pMixStream);
        pMixStream = NULL;
    }

    return rc;
}

static void audioMixerStreamDestroyInternal(PAUDMIXSTREAM pStream)
{
    if (!pStream)
        return;

    LogFunc(("%s\n", pStream->pszName));

    if (pStream->pConn) /* Stream has a connector interface present? */
    {
        if (   pStream->enmDir == PDMAUDIODIR_IN
            && pStream->pConn->pfnDestroyIn)
        {
            if (pStream->InOut.pIn)
            {
                pStream->pConn->pfnReleaseIn(pStream->pConn, pStream->InOut.pIn);
                pStream->pConn->pfnDestroyIn(pStream->pConn, pStream->InOut.pIn);
            }
        }
        else if (   pStream->enmDir == PDMAUDIODIR_OUT
                 && pStream->pConn->pfnDestroyOut)
        {
            if (pStream->InOut.pOut)
            {
                pStream->pConn->pfnReleaseOut(pStream->pConn, pStream->InOut.pOut);
                pStream->pConn->pfnDestroyOut(pStream->pConn, pStream->InOut.pOut);
            }
        }
        else
            AssertFailed();
    }

    audioMixerStreamFree(pStream);
}

void AudioMixerStreamDestroy(PAUDMIXSTREAM pStream)
{
    audioMixerStreamDestroyInternal(pStream);
}

static void audioMixerStreamFree(PAUDMIXSTREAM pStream)
{
    if (pStream)
    {
        LogFunc(("%s\n", pStream->pszName));

        if (pStream->pszName)
        {
            RTStrFree(pStream->pszName);
            pStream->pszName = NULL;
        }

        RTMemFree(pStream);
        pStream = NULL;
    }
}

bool AudioMixerStreamIsActive(PAUDMIXSTREAM pStream)
{
    if (!pStream)
        return false;

    bool fIsValid;
    switch (pStream->enmDir)
    {
        case PDMAUDIODIR_IN:
        {
            fIsValid = pStream->pConn->pfnIsActiveIn(pStream->pConn, pStream->InOut.pIn);
            break;
        }

        case PDMAUDIODIR_OUT:
        {
            fIsValid = pStream->pConn->pfnIsActiveOut(pStream->pConn, pStream->InOut.pOut);
            break;
        }

        default:
            fIsValid = false;
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    return fIsValid;
}

bool AudioMixerStreamIsValid(PAUDMIXSTREAM pStream)
{
    if (!pStream)
        return false;

    bool fIsValid;
    switch (pStream->enmDir)
    {
        case PDMAUDIODIR_IN:
        {
            fIsValid = pStream->pConn->pfnIsValidIn(pStream->pConn, pStream->InOut.pIn);
            break;
        }

        case PDMAUDIODIR_OUT:
        {
            fIsValid = pStream->pConn->pfnIsValidOut(pStream->pConn, pStream->InOut.pOut);
            break;
        }

        default:
            fIsValid = false;
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    return fIsValid;
}

