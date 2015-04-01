/* $Id$ */
/** @file
 * VBox audio: Mixing routines, mainly used by the various audio device
 *             emulations to achieve proper multiplexing from/to attached
 *             devices LUNs.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
#include "AudioMixBuffer.h"

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


int audioMixerUpdateSinkVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster, const PPDMAUDIOVOLUME pVolSink);


int audioMixerAddSink(PAUDIOMIXER pMixer, const char *pszName, AUDMIXSINKDIR enmDir, PAUDMIXSINK *ppSink)
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

int audioMixerAddStreamIn(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMIN pStream,
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

        if (ppStream)
            *ppStream = pMixStream;

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}

int audioMixerAddStreamOut(PAUDMIXSINK pSink, PPDMIAUDIOCONNECTOR pConnector, PPDMAUDIOGSTSTRMOUT pStream,
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

void audioMixerDestroy(PAUDIOMIXER pMixer)
{
    if (pMixer)
    {
        LogFlowFunc(("Destroying %s ...\n", pMixer->pszName));

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

void audioMixerInvalidate(PAUDIOMIXER pMixer)
{
    AssertPtrReturnVoid(pMixer);

    LogFlowFunc(("%s: Invalidating ...\n", pMixer->pszName));

    /* Propagate new master volume to all connected sinks. */
    PAUDMIXSINK pSink;
    RTListForEach(&pMixer->lstSinks, pSink, AUDMIXSINK, Node)
    {
        int rc2 = audioMixerUpdateSinkVolume(pSink, &pMixer->VolMaster, &pSink->Volume);
        AssertRC(rc2);
    }
}

int audioMixerProcessSinkIn(PAUDMIXSINK pSink, AUDMIXOP enmOp, void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed)
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
        /** @todo Support output sinks as well! */
        if (!pStream->pConn->pfnIsActiveIn(pStream->pConn, pStream->pIn))
            continue;

        uint32_t cbTotalRead = 0;
        size_t cbToRead = cbBuf;

        while (cbToRead)
        {
            uint32_t cbRead;
            AssertPtr(pStream->pConn);
            rc = pStream->pConn->pfnRead(pStream->pConn, pStream->pIn,
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

int audioMixerProcessSinkOut(PAUDMIXSINK pSink, AUDMIXOP enmOp, const void *pvBuf, size_t cbBuf, uint32_t *pcbProcessed)
{
    return VERR_NOT_IMPLEMENTED;
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

    LogFlowFunc(("%s: pSink=%s, cSinks=%RU8\n",
                 pMixer->pszName, pSink->pszName, pMixer->cSinks));

    audioMixerDestroySink(pSink);
}

void audioMixerRemoveStream(PAUDMIXSINK pSink, PAUDMIXSTREAM pStream)
{
    AssertPtrReturnVoid(pSink);
    if (!pStream)
        return;

    Assert(pSink->cStreams);
    RTListNodeRemove(&pStream->Node);
    pSink->cStreams--;

    const char *pszStream = pSink->enmDir == AUDMIXSINKDIR_INPUT
                          ? pStream->pIn->MixBuf.pszName : pStream->pOut->MixBuf.pszName;

    LogFlowFunc(("%s: pStream=%s, cStreams=%RU8\n",
                 pSink->pszName, pszStream, pSink->cStreams));

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

static inline PDMAUDIOVOLUME audioMixerVolCalc(PPDMAUDIOVOLUME pVol)
{
    uint32_t u32VolumeLeft  = pVol->uLeft;
    uint32_t u32VolumeRight = pVol->uRight;
    /* 0x00..0xff => 0x01..0x100 */
    if (u32VolumeLeft)
        u32VolumeLeft++;
    if (u32VolumeRight)
        u32VolumeRight++;

    PDMAUDIOVOLUME volOut;
    volOut.fMuted = pVol->fMuted;
    volOut.uLeft  = u32VolumeLeft  * 0x80000000; /* Maximum is 0x80000000 */
    volOut.uRight = u32VolumeRight * 0x80000000; /* Maximum is 0x80000000 */

    return volOut;
}

static inline PDMAUDIOVOLUME audioMixerVolMix(const PPDMAUDIOVOLUME pVolMaster, PPDMAUDIOVOLUME pVol)
{
    PDMAUDIOVOLUME volOut;
    volOut.fMuted = pVolMaster->fMuted || pVol->fMuted;
    volOut.uLeft  = ASMMultU64ByU32DivByU32(pVolMaster->uLeft,  pVol->uLeft,  0x80000000U); /* Maximum is 0x80000000U */
    volOut.uRight = ASMMultU64ByU32DivByU32(pVolMaster->uRight, pVol->uRight, 0x80000000U); /* Maximum is 0x80000000U */

    LogFlowFunc(("pMaster=%p, fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                 pVolMaster, pVolMaster->fMuted, pVolMaster->uLeft, pVolMaster->uRight));
    LogFlowFunc(("pVol=%p, fMuted=%RTbool, lVol=%RU32, rVol=%RU32 => fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                 pVol, pVol->fMuted, pVol->uLeft, pVol->uRight, volOut.fMuted, volOut.uLeft, volOut.uRight));

    return volOut;
}

int audioMixerUpdateSinkVolume(PAUDMIXSINK pSink, const PPDMAUDIOVOLUME pVolMaster, const PPDMAUDIOVOLUME pVolSink)
{
    AssertPtrReturn(pSink,      VERR_INVALID_POINTER);
    AssertPtrReturn(pVolMaster, VERR_INVALID_POINTER);
    AssertPtrReturn(pVolSink,   VERR_INVALID_POINTER);

    LogFlowFunc(("Master fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  pVolMaster->fMuted, pVolMaster->uLeft, pVolMaster->uRight));
    LogFlowFunc(("%s: fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  pSink->pszName, pSink->Volume.fMuted, pSink->Volume.uLeft, pSink->Volume.uRight));

    /** @todo Very crude implementation for now -- needs more work! */

    PDMAUDIOVOLUME volSink;
    volSink.fMuted  = pVolMaster->fMuted || pVolSink->fMuted;
    volSink.uLeft   = (pVolSink->uLeft  * pVolMaster->uLeft)  / UINT8_MAX;
    volSink.uRight  = (pVolSink->uRight * pVolMaster->uRight) / UINT8_MAX;

    LogFlowFunc(("\t-> fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                  volSink.fMuted, volSink.uLeft, volSink.uRight));

    bool fOut = pSink->enmDir == AUDMIXSINKDIR_OUTPUT;

    /* Propagate new sink volume to all streams in the sink. */
    PAUDMIXSTREAM pStream;
    RTListForEach(&pSink->lstStreams, pStream, AUDMIXSTREAM, Node)
    {
        if (fOut)
            audioMixBufSetVolume(&pStream->pOut->MixBuf, &volSink);
        else
            audioMixBufSetVolume(&pStream->pIn->MixBuf,  &volSink);
    }

    return VINF_SUCCESS;
}

int audioMixerSetMasterVolume(PAUDIOMIXER pMixer, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pMixer, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,   VERR_INVALID_POINTER);

    pMixer->VolMaster = *pVol; //= audioMixerVolCalc(pVol);

    LogFlowFunc(("%s: lVol=%RU32, rVol=%RU32 => fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n",
                 pMixer->pszName, pVol->uLeft, pVol->uRight,
                 pMixer->VolMaster.fMuted, pMixer->VolMaster.uLeft, pMixer->VolMaster.uRight));

    audioMixerInvalidate(pMixer);
    return VINF_SUCCESS;
}

int audioMixerSetSinkVolume(PAUDMIXSINK pSink, PPDMAUDIOVOLUME pVol)
{
    AssertPtrReturn(pSink, VERR_INVALID_POINTER);
    AssertPtrReturn(pVol,  VERR_INVALID_POINTER);

    LogFlowFunc(("%s: fMuted=%RTbool, lVol=%RU32, rVol=%RU32\n", pSink->pszName, pVol->fMuted, pVol->uLeft, pVol->uRight));

    AssertPtr(pSink->pParent);
    return audioMixerUpdateSinkVolume(pSink, &pSink->pParent->VolMaster, pVol);
}

