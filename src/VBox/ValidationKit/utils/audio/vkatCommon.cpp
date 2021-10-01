/* $Id$ */
/** @file
 * Validation Kit Audio Test (VKAT) - Self test code.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_AUDIO_TEST
#include <iprt/log.h>

#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/errcore.h>
#include <iprt/getopt.h>
#include <iprt/message.h>
#include <iprt/rand.h>
#include <iprt/test.h>

#include "Audio/AudioHlp.h"
#include "Audio/AudioTest.h"
#include "Audio/AudioTestService.h"
#include "Audio/AudioTestServiceClient.h"

#include "vkatInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int audioTestStreamInit(PAUDIOTESTDRVSTACK pDrvStack, PAUDIOTESTSTREAM pStream, PDMAUDIODIR enmDir, PCPDMAUDIOPCMPROPS pProps, bool fWithMixer, uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint);
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream);


/*********************************************************************************************************************************
*   Device enumeration + handling.                                                                                               *
*********************************************************************************************************************************/

/**
 * Enumerates audio devices and optionally searches for a specific device.
 *
 * @returns VBox status code.
 * @param   pDrvStack           Driver stack to use for enumeration.
 * @param   pszDev              Device name to search for. Can be NULL if the default device shall be used.
 * @param   ppDev               Where to return the pointer of the device enumeration of \a pTstEnv when a
 *                              specific device was found.
 */
int audioTestDevicesEnumerateAndCheck(PAUDIOTESTDRVSTACK pDrvStack, const char *pszDev, PPDMAUDIOHOSTDEV *ppDev)
{
    RTTestSubF(g_hTest, "Enumerating audio devices and checking for device '%s'", pszDev && *pszDev ? pszDev : "[Default]");

    if (!pDrvStack->pIHostAudio->pfnGetDevices)
    {
        RTTestSkipped(g_hTest, "Backend does not support device enumeration, skipping");
        return VINF_NOT_SUPPORTED;
    }

    Assert(pszDev == NULL || ppDev);

    if (ppDev)
        *ppDev = NULL;

    int rc = pDrvStack->pIHostAudio->pfnGetDevices(pDrvStack->pIHostAudio, &pDrvStack->DevEnum);
    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOHOSTDEV pDev;
        RTListForEach(&pDrvStack->DevEnum.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
        {
            char szFlags[PDMAUDIOHOSTDEV_MAX_FLAGS_STRING_LEN];
            if (pDev->pszId)
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s' (ID '%s'):\n", pDev->pszName, pDev->pszId);
            else
                RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum: Device '%s':\n", pDev->pszName);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Usage           = %s\n",   PDMAudioDirGetName(pDev->enmUsage));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Flags           = %s\n",   PDMAudioHostDevFlagsToString(szFlags, pDev->fFlags));
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Input channels  = %RU8\n", pDev->cMaxInputChannels);
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Enum:   Output channels = %RU8\n", pDev->cMaxOutputChannels);

            if (   (pszDev && *pszDev)
                && !RTStrCmp(pDev->pszName, pszDev))
            {
                *ppDev = pDev;
            }
        }
    }
    else
        RTTestFailed(g_hTest, "Enumerating audio devices failed with %Rrc", rc);

    if (RT_SUCCESS(rc))
    {
        if (   (pszDev && *pszDev)
            && *ppDev == NULL)
        {
            RTTestFailed(g_hTest, "Audio device '%s' not found", pszDev);
            rc = VERR_NOT_FOUND;
        }
    }

    RTTestSubDone(g_hTest);
    return rc;
}

static int audioTestStreamInit(PAUDIOTESTDRVSTACK pDrvStack, PAUDIOTESTSTREAM pStream,
                               PDMAUDIODIR enmDir, PCPDMAUDIOPCMPROPS pProps, bool fWithMixer,
                               uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint)
{
    int rc;

    if (enmDir == PDMAUDIODIR_IN)
        rc = audioTestDriverStackStreamCreateInput(pDrvStack, pProps, cMsBufferSize,
                                                   cMsPreBuffer, cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    else if (enmDir == PDMAUDIODIR_OUT)
        rc = audioTestDriverStackStreamCreateOutput(pDrvStack, pProps, cMsBufferSize,
                                                    cMsPreBuffer, cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    else
        rc = VERR_NOT_SUPPORTED;

    if (RT_SUCCESS(rc))
    {
        if (!pDrvStack->pIAudioConnector)
        {
            pStream->pBackend = &((PAUDIOTESTDRVSTACKSTREAM)pStream->pStream)->Backend;
        }
        else
            pStream->pBackend = NULL;

        /*
         * Automatically enable the mixer if the PCM properties don't match.
         */
        if (   !fWithMixer
            && !PDMAudioPropsAreEqual(pProps, &pStream->Cfg.Props))
        {
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS,  "Enabling stream mixer\n");
            fWithMixer = true;
        }

        rc = AudioTestMixStreamInit(&pStream->Mix, pDrvStack, pStream->pStream,
                                    fWithMixer ? pProps : NULL, 100 /* ms */); /** @todo Configure mixer buffer? */
    }

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Initializing %s stream failed with %Rrc", enmDir == PDMAUDIODIR_IN ? "input" : "output", rc);

    return rc;
}

/**
 * Destroys an audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment the stream to destroy contains.
 * @param   pStream             Audio stream to destroy.
 */
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream)
{
    int rc = VINF_SUCCESS;
    if (pStream && pStream->pStream)
    {
        /** @todo Anything else to do here, e.g. test if there are left over samples or some such? */

        audioTestDriverStackStreamDestroy(pTstEnv->pDrvStack, pStream->pStream);
        pStream->pStream  = NULL;
        pStream->pBackend = NULL;
    }

    AudioTestMixStreamTerm(&pStream->Mix);

    return rc;
}


/*********************************************************************************************************************************
*   Test Primitives                                                                                                              *
*********************************************************************************************************************************/

#if 0 /* Unused */
/**
 * Returns a random scheduling hint (in ms).
 */
DECLINLINE(uint32_t) audioTestEnvGetRandomSchedulingHint(void)
{
    static const unsigned s_aSchedulingHintsMs[] =
    {
        10,
        25,
        50,
        100,
        200,
        250
    };

    return s_aSchedulingHintsMs[RTRandU32Ex(0, RT_ELEMENTS(s_aSchedulingHintsMs) - 1)];
}
#endif

/**
 * Plays a test tone on a specific audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running the test.
 *                              Optional and can be NULL (for simple playback only).
 * @param   pStream             Stream to use for playing the tone.
 * @param   pParms              Tone parameters to use.
 *
 * @note    Blocking function.
 */
int audioTestPlayTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms)
{
    AUDIOTESTTONE TstTone;
    AudioTestToneInit(&TstTone, &pStream->Cfg.Props, pParms->dbFreqHz);

    char const *pcszPathOut = NULL;
    if (pTstEnv)
        pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Playing test tone (tone frequency is %RU16Hz, %RU32ms)\n", (uint16_t)pParms->dbFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using %RU32ms stream scheduling hint\n", pStream->Cfg.Device.cMsSchedulingHint);
    if (pcszPathOut)
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Writing to '%s'\n", pcszPathOut);

    int rc;

    /** @todo Use .WAV here? */
    AUDIOTESTOBJ Obj;
    RT_ZERO(Obj); /* Shut up MSVC. */
    if (pTstEnv)
    {
        rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "guest-tone-play.pcm", &Obj);
        AssertRCReturn(rc, rc);
    }

    rc = AudioTestMixStreamEnable(&pStream->Mix);
    if (   RT_SUCCESS(rc)
        && AudioTestMixStreamIsOkay(&pStream->Mix))
    {
        uint8_t  abBuf[_4K];

        uint32_t cbToPlayTotal  = PDMAudioPropsMilliToBytes(&pStream->Cfg.Props, pParms->msDuration);
        AssertStmt(cbToPlayTotal, rc = VERR_INVALID_PARAMETER);
        uint32_t cbPlayedTotal  = 0;

        /* We play a pre + post beacon before + after the actual test tone.
         * Note that the beacon is *not* part of the written test object, so that we can detect differences between
         * actual played back and serialized (written) data later. */
        uint32_t const cbBeacon       = pTstEnv ? 1024 : 0; /* Only play a beacon if we're running in testing mode. */
        uint32_t       cbBeaconToPlay = cbBeacon;
        uint32_t       cbBeaconPlayed = 0;

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Playing %RU32 bytes total\n", cbToPlayTotal);
        if (cbBeaconToPlay)
        {
            RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Playing 2 x %RU32 bytes pre/post beacons\n", cbBeaconToPlay);
            cbToPlayTotal += cbBeacon * 2 /* Pre + post beacon */;
        }

        if (pTstEnv)
        {
            AudioTestObjAddMetadataStr(Obj, "stream_to_play_bytes=%RU32\n",      cbToPlayTotal);
            AudioTestObjAddMetadataStr(Obj, "stream_period_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesPeriod);
            AudioTestObjAddMetadataStr(Obj, "stream_buffer_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesBufferSize);
            AudioTestObjAddMetadataStr(Obj, "stream_prebuf_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesPreBuffering);
            /* Note: This mostly is provided by backend (e.g. PulseAudio / ALSA / ++) and
             *       has nothing to do with the device emulation scheduling hint. */
            AudioTestObjAddMetadataStr(Obj, "device_scheduling_hint_ms=%RU32\n", pStream->Cfg.Device.cMsSchedulingHint);
        }

        PAUDIOTESTDRVMIXSTREAM pMix = &pStream->Mix;

        uint32_t const  cbPreBuffer        = PDMAudioPropsFramesToBytes(pMix->pProps, pStream->Cfg.Backend.cFramesPreBuffering);
        uint64_t const  nsStarted          = RTTimeNanoTS();
        uint64_t        nsDonePreBuffering = 0;

        uint64_t        offStream          = 0;
        uint64_t        nsTimeout          = RT_MS_5MIN_64 * RT_NS_1MS;
        uint64_t        nsLastMsgCantWrite = 0; /* Timestamp (in ns) when the last message of an unwritable stream was shown. */

        while (cbPlayedTotal < cbToPlayTotal)
        {
            uint64_t const nsNow = RTTimeNanoTS();

            /* Pace ourselves a little. */
            if (offStream >= cbPreBuffer)
            {
                if (!nsDonePreBuffering)
                    nsDonePreBuffering = nsNow;
                uint64_t const cNsWritten = PDMAudioPropsBytesToNano64(pMix->pProps, offStream - cbPreBuffer);
                uint64_t const cNsElapsed = nsNow - nsStarted;
                if (cNsWritten > cNsElapsed + RT_NS_10MS)
                    RTThreadSleep((cNsWritten - cNsElapsed - RT_NS_10MS / 2) / RT_NS_1MS);
            }

            uint32_t       cbPlayed   = 0;
            uint32_t const cbCanWrite = AudioTestMixStreamGetWritable(&pStream->Mix);
            if (cbCanWrite)
            {
                if (g_uVerbosity)
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Stream is writable with %RU32ms (%RU32 bytes)\n",
                                 PDMAudioPropsBytesToMilli(pMix->pProps, cbCanWrite), cbCanWrite);

                uint32_t cbToPlay;

                /* Any beacon to play? */
                if (   cbBeaconToPlay
                    && cbBeaconPlayed < cbBeaconToPlay)
                {
                    /* Limit to exactly one beacon (pre or post). */
                    cbToPlay = RT_MIN(sizeof(abBuf), RT_MIN(cbCanWrite, cbBeaconToPlay - cbBeaconPlayed));
                    memset(abBuf, 0x42 /* Our actual beacon data, hopefully the answer to all ... */, cbToPlay);

                    rc = AudioTestMixStreamPlay(&pStream->Mix, abBuf, cbToPlay, &cbPlayed);
                    if (RT_FAILURE(rc))
                        break;

                    cbBeaconPlayed += cbPlayed;
                    cbPlayedTotal  += cbPlayed;
                    continue;
                }

                /* Start playing the post beacon? */
                if (cbPlayedTotal == cbToPlayTotal - cbBeaconToPlay)
                {
                    cbBeaconPlayed = 0;
                    continue;
                }

                if (RT_FAILURE(rc))
                    break;

                uint32_t const cbToGenerate = RT_MIN(RT_MIN(cbToPlayTotal - cbPlayedTotal - cbBeaconToPlay, sizeof(abBuf)),
                                                     cbCanWrite);
                rc = AudioTestToneGenerate(&TstTone, abBuf, cbToGenerate, &cbToPlay);
                if (RT_SUCCESS(rc))
                {
                    if (pTstEnv)
                    {
                        /* Write stuff to disk before trying to play it. Help analysis later. */
                        rc = AudioTestObjWrite(Obj, abBuf, cbToPlay);
                    }
                    if (RT_SUCCESS(rc))
                    {
                        rc = AudioTestMixStreamPlay(&pStream->Mix, abBuf, cbToPlay, &cbPlayed);
                        if (RT_SUCCESS(rc))
                        {
                            AssertBreakStmt(cbPlayed <= cbToPlay, rc = VERR_TOO_MUCH_DATA);

                            offStream += cbPlayed;

                            if (cbPlayed != cbToPlay)
                                RTTestFailed(g_hTest, "Only played %RU32/%RU32 bytes", cbPlayed, cbToPlay);
                        }
                    }
                }

                if (RT_FAILURE(rc))
                    break;

                nsLastMsgCantWrite = 0;
            }
            else if (AudioTestMixStreamIsOkay(&pStream->Mix))
            {
                RTMSINTERVAL const msSleep = RT_MIN(RT_MAX(1, pStream->Cfg.Device.cMsSchedulingHint), 256);

                if (!nsLastMsgCantWrite || nsNow - nsLastMsgCantWrite > RT_NS_10SEC) /* Don't spam the output too much. */
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Waiting %RU32ms for stream to be writable again ...\n", msSleep);
                    nsLastMsgCantWrite = nsNow;
                }

                RTThreadSleep(msSleep);
            }
            else
                AssertFailedBreakStmt(rc = VERR_AUDIO_STREAM_NOT_READY);

            cbPlayedTotal += cbPlayed;
            AssertBreakStmt(cbPlayedTotal <= cbToPlayTotal, VERR_BUFFER_OVERFLOW);

            /* Fail-safe in case something screwed up while playing back. */
            uint64_t const cNsElapsed = nsNow - nsStarted;
            if (cNsElapsed > nsTimeout)
            {
                RTTestFailed(g_hTest, "Playback took too long (running %RU64 vs. timeout %RU64), aborting\n", cNsElapsed, nsTimeout);
                rc = VERR_TIMEOUT;
            }

            if (RT_FAILURE(rc))
                break;
        }

        if (cbPlayedTotal != cbToPlayTotal)
            RTTestFailed(g_hTest, "Playback ended unexpectedly (%RU32/%RU32 played)\n", cbPlayedTotal, cbToPlayTotal);

        if (RT_SUCCESS(rc))
        {
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Draining stream ...\n");
            rc = AudioTestMixStreamDrain(&pStream->Mix, true /*fSync*/);
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    if (pTstEnv)
    {
        int rc2 = AudioTestObjClose(Obj);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Playing tone failed with %Rrc\n", rc);

    return rc;
}

/**
 * Records a test tone from a specific audio test stream.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for running the test.
 * @param   pStream             Stream to use for recording the tone.
 * @param   pParms              Tone parameters to use.
 *
 * @note    Blocking function.
 */
static int audioTestRecordTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms)
{
    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Recording test tone (tone frequency is %RU16Hz, %RU32ms)\n", (uint16_t)pParms->dbFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    AUDIOTESTOBJ Obj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "guest-tone-rec.pcm", &Obj);
    AssertRCReturn(rc, rc);

    PAUDIOTESTDRVMIXSTREAM pMix = &pStream->Mix;

    rc = AudioTestMixStreamEnable(pMix);
    if (RT_SUCCESS(rc))
    {
        uint64_t cbToRecTotal = PDMAudioPropsMilliToBytes(&pStream->Cfg.Props, pParms->msDuration);

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Recording %RU32 bytes total\n", cbToRecTotal);

        AudioTestObjAddMetadataStr(Obj, "stream_to_record_bytes=%RU32\n", cbToRecTotal);
        AudioTestObjAddMetadataStr(Obj, "stream_buffer_size_ms=%RU32\n", pTstEnv->cMsBufferSize);
        AudioTestObjAddMetadataStr(Obj, "stream_prebuf_size_ms=%RU32\n", pTstEnv->cMsPreBuffer);
        /* Note: This mostly is provided by backend (e.g. PulseAudio / ALSA / ++) and
         *       has nothing to do with the device emulation scheduling hint. */
        AudioTestObjAddMetadataStr(Obj, "device_scheduling_hint_ms=%RU32\n", pTstEnv->cMsSchedulingHint);

        uint8_t         abSamples[16384];
        uint32_t const  cbSamplesAligned = PDMAudioPropsFloorBytesToFrame(pMix->pProps, sizeof(abSamples));
        uint64_t        cbRecTotal  = 0;

        uint64_t const  nsStarted         = RTTimeNanoTS();

        uint64_t        nsTimeout         = RT_MS_5MIN_64 * RT_NS_1MS;
        uint64_t        nsLastMsgCantRead = 0; /* Timestamp (in ns) when the last message of an unreadable stream was shown. */

        while (!g_fTerminate && cbRecTotal < cbToRecTotal)
        {
            uint64_t const nsNow = RTTimeNanoTS();

            /*
             * Anything we can read?
             */
            uint32_t const cbCanRead = AudioTestMixStreamGetReadable(pMix);
            if (cbCanRead)
            {
                if (g_uVerbosity)
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Stream is readable with %RU32ms (%RU32 bytes)\n",
                                 PDMAudioPropsBytesToMilli(pMix->pProps, cbCanRead), cbCanRead);

                uint32_t const cbToRead   = RT_MIN(cbCanRead, cbSamplesAligned);
                uint32_t       cbRecorded = 0;
                rc = AudioTestMixStreamCapture(pMix, abSamples, cbToRead, &cbRecorded);
                if (RT_SUCCESS(rc))
                {
                    if (cbRecorded)
                    {
                        rc = AudioTestObjWrite(Obj, abSamples, cbRecorded);
                        if (RT_SUCCESS(rc))
                        {
                            cbRecTotal += cbRecorded;

                            /** @todo Clamp result? */
                        }
                    }
                }
            }
            else if (AudioTestMixStreamIsOkay(pMix))
            {
                RTMSINTERVAL const msSleep = RT_MIN(RT_MAX(1, pStream->Cfg.Device.cMsSchedulingHint), 256);

                if (!nsLastMsgCantRead || nsNow - nsLastMsgCantRead > RT_NS_10SEC) /* Don't spam the output too much. */
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Waiting %RU32ms for stream to be readable again ...\n", msSleep);
                    nsLastMsgCantRead = nsNow;
                }

                RTThreadSleep(msSleep);
            }

            /* Fail-safe in case something screwed up while playing back. */
            uint64_t const cNsElapsed = nsNow - nsStarted;
            if (cNsElapsed > nsTimeout)
            {
                RTTestFailed(g_hTest, "Recording took too long (running %RU64 vs. timeout %RU64), aborting\n", cNsElapsed, nsTimeout);
                rc = VERR_TIMEOUT;
            }

            if (RT_FAILURE(rc))
                break;
        }

        if (cbRecTotal != cbToRecTotal)
            RTTestFailed(g_hTest, "Recording ended unexpectedly (%RU32/%RU32 recorded)\n", cbRecTotal, cbToRecTotal);

        int rc2 = AudioTestMixStreamDisable(pMix);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }

    int rc2 = AudioTestObjClose(Obj);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Recording tone done failed with %Rrc\n", rc);

    return rc;
}


/*********************************************************************************************************************************
*   ATS Callback Implementations                                                                                                 *
*********************************************************************************************************************************/

/** @copydoc ATSCALLBACKS::pfnHowdy
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsHowdyCallback(void const *pvUser)
{
    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    AssertReturn(pCtx->cClients <= UINT8_MAX - 1, VERR_BUFFER_OVERFLOW);

    pCtx->cClients++;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "New client connected, now %RU8 total\n", pCtx->cClients);

    return VINF_SUCCESS;
}

/** @copydoc ATSCALLBACKS::pfnBye
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsByeCallback(void const *pvUser)
{
    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    AssertReturn(pCtx->cClients, VERR_WRONG_ORDER);
    pCtx->cClients--;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Client wants to disconnect, %RU8 remaining\n", pCtx->cClients);

    if (0 == pCtx->cClients) /* All clients disconnected? Tear things down. */
    {
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Last client disconnected, terminating server ...\n");
        ASMAtomicWriteBool(&g_fTerminate, true);
    }

    return VINF_SUCCESS;
}

/** @copydoc ATSCALLBACKS::pfnTestSetBegin
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsTestSetBeginCallback(void const *pvUser, const char *pszTag)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Got request for beginning test set '%s' in '%s'\n", pszTag, pTstEnv->szPathTemp);

    return AudioTestSetCreate(&pTstEnv->Set, pTstEnv->szPathTemp, pszTag);
}

/** @copydoc ATSCALLBACKS::pfnTestSetEnd
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsTestSetEndCallback(void const *pvUser, const char *pszTag)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Got request for ending test set '%s'\n", pszTag);

    /* Pack up everything to be ready for transmission. */
    return audioTestEnvPrologue(pTstEnv, true /* fPack */, pCtx->szTestSetArchive, sizeof(pCtx->szTestSetArchive));
}

/** @copydoc ATSCALLBACKS::pfnTonePlay
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsTonePlayCallback(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Got request for playing test tone #%RU32 (%RU16Hz, %RU32ms) ...\n",
                 pToneParms->Hdr.idxSeq, (uint16_t)pToneParms->dbFreqHz, pToneParms->msDuration);

    char szTimeCreated[RTTIME_STR_LEN];
    RTTimeToString(&pToneParms->Hdr.tsCreated, szTimeCreated, sizeof(szTimeCreated));
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test created (caller UTC): %s\n", szTimeCreated);

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestStreamInit(pTstEnv->pDrvStack, pTstStream, PDMAUDIODIR_OUT, &pTstEnv->Props, false /* fWithMixer */,
                                 pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer, pTstEnv->cMsSchedulingHint);
    if (RT_SUCCESS(rc))
    {
        AUDIOTESTPARMS TstParms;
        RT_ZERO(TstParms);
        TstParms.enmType  = AUDIOTESTTYPE_TESTTONE_PLAY;
        TstParms.enmDir   = PDMAUDIODIR_OUT;
        TstParms.TestTone = *pToneParms;

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Playing test tone", &TstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestPlayTone(pTstEnv, pTstStream, pToneParms);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Playing tone failed");
        }

        int rc2 = audioTestStreamDestroy(pTstEnv, pTstStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    else
        RTTestFailed(g_hTest, "Error creating output stream, rc=%Rrc\n", rc);

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnToneRecord */
static DECLCALLBACK(int) audioTestGstAtsToneRecordCallback(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Got request for recording test tone #%RU32 (%RU32ms) ...\n",
                 pToneParms->Hdr.idxSeq, pToneParms->msDuration);

    char szTimeCreated[RTTIME_STR_LEN];
    RTTimeToString(&pToneParms->Hdr.tsCreated, szTimeCreated, sizeof(szTimeCreated));
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test created (caller UTC): %s\n", szTimeCreated);

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestStreamInit(pTstEnv->pDrvStack, pTstStream, PDMAUDIODIR_IN, &pTstEnv->Props, false /* fWithMixer */,
                                 pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer, pTstEnv->cMsSchedulingHint);
    if (RT_SUCCESS(rc))
    {
        AUDIOTESTPARMS TstParms;
        RT_ZERO(TstParms);
        TstParms.enmType  = AUDIOTESTTYPE_TESTTONE_RECORD;
        TstParms.enmDir   = PDMAUDIODIR_IN;
        TstParms.Props    = pToneParms->Props;
        TstParms.TestTone = *pToneParms;

        PAUDIOTESTENTRY pTst;
        rc = AudioTestSetTestBegin(&pTstEnv->Set, "Recording test tone from host", &TstParms, &pTst);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestRecordTone(pTstEnv, pTstStream, pToneParms);
            if (RT_SUCCESS(rc))
            {
                AudioTestSetTestDone(pTst);
            }
            else
                AudioTestSetTestFailed(pTst, rc, "Recording tone failed");
        }

        int rc2 = audioTestStreamDestroy(pTstEnv, pTstStream);
        if (RT_SUCCESS(rc))
            rc = rc2;
    }
    else
        RTTestFailed(g_hTest, "Error creating input stream, rc=%Rrc\n", rc);

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendBegin */
static DECLCALLBACK(int) audioTestGstAtsTestSetSendBeginCallback(void const *pvUser, const char *pszTag)
{
    RT_NOREF(pszTag);

    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    if (!RTFileExists(pCtx->szTestSetArchive)) /* Has the archive successfully been created yet? */
        return VERR_WRONG_ORDER;

    int rc = RTFileOpen(&pCtx->hTestSetArchive, pCtx->szTestSetArchive, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_SUCCESS(rc))
    {
        uint64_t uSize;
        rc = RTFileQuerySize(pCtx->hTestSetArchive, &uSize);
        if (RT_SUCCESS(rc))
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Sending test set '%s' (%zu bytes)\n", pCtx->szTestSetArchive, uSize);
    }

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendRead */
static DECLCALLBACK(int) audioTestGstAtsTestSetSendReadCallback(void const *pvUser,
                                                                const char *pszTag, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    RT_NOREF(pszTag);

    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    return RTFileRead(pCtx->hTestSetArchive, pvBuf, cbBuf, pcbRead);
}

/** @copydoc ATSCALLBACKS::pfnTestSetSendEnd */
static DECLCALLBACK(int) audioTestGstAtsTestSetSendEndCallback(void const *pvUser, const char *pszTag)
{
    RT_NOREF(pszTag);

    PATSCALLBACKCTX pCtx = (PATSCALLBACKCTX)pvUser;

    int rc = RTFileClose(pCtx->hTestSetArchive);
    if (RT_SUCCESS(rc))
    {
        pCtx->hTestSetArchive = NIL_RTFILE;
    }

    return rc;
}


/*********************************************************************************************************************************
*   Implementation of audio test environment handling                                                                            *
*********************************************************************************************************************************/

/**
 * Connects an ATS client via TCP/IP to a peer.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use.
 * @param   pClient             Client to connect.
 * @param   pszWhat             Hint of what to connect to where.
 * @param   pTcpOpts            Pointer to TCP options to use.
 */
int audioTestEnvConnectViaTcp(PAUDIOTESTENV pTstEnv, PATSCLIENT pClient, const char *pszWhat, PAUDIOTESTENVTCPOPTS pTcpOpts)
{
    RT_NOREF(pTstEnv);

    RTGETOPTUNION Val;
    RT_ZERO(Val);

    Val.u32 = pTcpOpts->enmConnMode;
    int rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_CONN_MODE, &Val);
    AssertRCReturn(rc, rc);

    if (   pTcpOpts->enmConnMode == ATSCONNMODE_BOTH
        || pTcpOpts->enmConnMode == ATSCONNMODE_SERVER)
    {
        Assert(pTcpOpts->uBindPort); /* Always set by the caller. */
        Val.u16 = pTcpOpts->uBindPort;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_BIND_PORT, &Val);
        AssertRCReturn(rc, rc);

        if (pTcpOpts->szBindAddr[0])
        {
            Val.psz = pTcpOpts->szBindAddr;
            rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_BIND_ADDRESS, &Val);
            AssertRCReturn(rc, rc);
        }
        else
        {
            RTTestFailed(g_hTest, "No bind address specified!\n");
            return VERR_INVALID_PARAMETER;
        }

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting %s by listening as server at %s:%RU32 ...\n",
                     pszWhat, pTcpOpts->szBindAddr, pTcpOpts->uBindPort);
    }


    if (   pTcpOpts->enmConnMode == ATSCONNMODE_BOTH
        || pTcpOpts->enmConnMode == ATSCONNMODE_CLIENT)
    {
        Assert(pTcpOpts->uConnectPort); /* Always set by the caller. */
        Val.u16 = pTcpOpts->uConnectPort;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_CONNECT_PORT, &Val);
        AssertRCReturn(rc, rc);

        if (pTcpOpts->szConnectAddr[0])
        {
            Val.psz = pTcpOpts->szConnectAddr;
            rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_CONNECT_ADDRESS, &Val);
            AssertRCReturn(rc, rc);
        }
        else
        {
            RTTestFailed(g_hTest, "No connect address specified!\n");
            return VERR_INVALID_PARAMETER;
        }

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting %s by connecting as client to %s:%RU32 ...\n",
                     pszWhat, pTcpOpts->szConnectAddr, pTcpOpts->uConnectPort);
    }

    rc = AudioTestSvcClientConnect(pClient);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "Connecting %s failed with %Rrc\n", pszWhat, rc);
        return rc;
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Successfully connected %s\n", pszWhat);
    return rc;
}

/**
 * Configures and starts an ATS TCP/IP server.
 *
 * @returns VBox status code.
 * @param   pSrv                ATS server instance to configure and start.
 * @param   pCallbacks          ATS callback table to use.
 * @param   pszDesc             Hint of server type which is being started.
 * @param   pTcpOpts            TCP options to use.
 */
int audioTestEnvConfigureAndStartTcpServer(PATSSERVER pSrv, PCATSCALLBACKS pCallbacks, const char *pszDesc,
                                           PAUDIOTESTENVTCPOPTS pTcpOpts)
{
    RTGETOPTUNION Val;
    RT_ZERO(Val);

    int rc = AudioTestSvcInit(pSrv, pCallbacks);
    if (RT_FAILURE(rc))
        return rc;

    Val.u32 = pTcpOpts->enmConnMode;
    rc = AudioTestSvcHandleOption(pSrv, ATSTCPOPT_CONN_MODE, &Val);
    AssertRCReturn(rc, rc);

    if (   pTcpOpts->enmConnMode == ATSCONNMODE_BOTH
        || pTcpOpts->enmConnMode == ATSCONNMODE_SERVER)
    {
        Assert(pTcpOpts->uBindPort); /* Always set by the caller. */
        Val.u16 = pTcpOpts->uBindPort;
        rc = AudioTestSvcHandleOption(pSrv, ATSTCPOPT_BIND_PORT, &Val);
        AssertRCReturn(rc, rc);

        if (pTcpOpts->szBindAddr[0])
        {
            Val.psz = pTcpOpts->szBindAddr;
            rc = AudioTestSvcHandleOption(pSrv, ATSTCPOPT_BIND_ADDRESS, &Val);
            AssertRCReturn(rc, rc);
        }
        else
        {
            RTTestFailed(g_hTest, "No bind address specified!\n");
            return VERR_INVALID_PARAMETER;
        }

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Starting server for %s at %s:%RU32 ...\n",
                     pszDesc, pTcpOpts->szBindAddr, pTcpOpts->uBindPort);
    }


    if (   pTcpOpts->enmConnMode == ATSCONNMODE_BOTH
        || pTcpOpts->enmConnMode == ATSCONNMODE_CLIENT)
    {
        Assert(pTcpOpts->uConnectPort); /* Always set by the caller. */
        Val.u16 = pTcpOpts->uConnectPort;
        rc = AudioTestSvcHandleOption(pSrv, ATSTCPOPT_CONNECT_PORT, &Val);
        AssertRCReturn(rc, rc);

        if (pTcpOpts->szConnectAddr[0])
        {
            Val.psz = pTcpOpts->szConnectAddr;
            rc = AudioTestSvcHandleOption(pSrv, ATSTCPOPT_CONNECT_ADDRESS, &Val);
            AssertRCReturn(rc, rc);
        }
        else
        {
            RTTestFailed(g_hTest, "No connect address specified!\n");
            return VERR_INVALID_PARAMETER;
        }

        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Starting server for %s by connecting as client to %s:%RU32 ...\n",
                     pszDesc, pTcpOpts->szConnectAddr, pTcpOpts->uConnectPort);
    }

    if (RT_SUCCESS(rc))
    {
        rc = AudioTestSvcStart(pSrv);
        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "Starting server for %s failed with %Rrc\n", pszDesc, rc);
    }

    return rc;
}

/**
 * Initializes an audio test environment.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvStack           Driver stack to use.
 */
int audioTestEnvInit(PAUDIOTESTENV pTstEnv, PAUDIOTESTDRVSTACK pDrvStack)
{
    int rc = VINF_SUCCESS;

    pTstEnv->pDrvStack = pDrvStack;

    /*
     * Set sane defaults if not already set.
     */
    if (!RTStrNLen(pTstEnv->szTag, sizeof(pTstEnv->szTag)))
    {
        rc = AudioTestGenTag(pTstEnv->szTag, sizeof(pTstEnv->szTag));
        AssertRCReturn(rc, rc);
    }

    if (!RTStrNLen(pTstEnv->szPathTemp, sizeof(pTstEnv->szPathTemp)))
    {
        rc = AudioTestPathGetTemp(pTstEnv->szPathTemp, sizeof(pTstEnv->szPathTemp));
        AssertRCReturn(rc, rc);
    }

    if (!RTStrNLen(pTstEnv->szPathOut, sizeof(pTstEnv->szPathOut)))
    {
        rc = RTPathJoin(pTstEnv->szPathOut, sizeof(pTstEnv->szPathOut), pTstEnv->szPathTemp, "vkat-temp");
        AssertRCReturn(rc, rc);
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Initializing environment for mode '%s'\n", pTstEnv->enmMode == AUDIOTESTMODE_HOST ? "host" : "guest");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pTstEnv->szTag);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", pTstEnv->szPathOut);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Temp directory is '%s'\n", pTstEnv->szPathTemp);

    if (!pTstEnv->cMsBufferSize)
        pTstEnv->cMsBufferSize     = UINT32_MAX;
    if (!pTstEnv->cMsPreBuffer)
        pTstEnv->cMsPreBuffer      = UINT32_MAX;
    if (!pTstEnv->cMsSchedulingHint)
        pTstEnv->cMsSchedulingHint = UINT32_MAX;

    char szPathTemp[RTPATH_MAX];
    if (   !strlen(pTstEnv->szPathTemp)
        || !strlen(pTstEnv->szPathOut))
        rc = RTPathTemp(szPathTemp, sizeof(szPathTemp));

    if (   RT_SUCCESS(rc)
        && !strlen(pTstEnv->szPathTemp))
        rc = RTPathJoin(pTstEnv->szPathTemp, sizeof(pTstEnv->szPathTemp), szPathTemp, "vkat-temp");

    if (RT_SUCCESS(rc))
    {
        rc = RTDirCreate(pTstEnv->szPathTemp, RTFS_UNIX_IRWXU, 0 /* fFlags */);
        if (rc == VERR_ALREADY_EXISTS)
            rc = VINF_SUCCESS;
    }

    if (   RT_SUCCESS(rc)
        && !strlen(pTstEnv->szPathOut))
        rc = RTPathJoin(pTstEnv->szPathOut, sizeof(pTstEnv->szPathOut), szPathTemp, "vkat");

    if (RT_SUCCESS(rc))
    {
        rc = RTDirCreate(pTstEnv->szPathOut, RTFS_UNIX_IRWXU, 0 /* fFlags */);
        if (rc == VERR_ALREADY_EXISTS)
            rc = VINF_SUCCESS;
    }

    if (RT_FAILURE(rc))
        return rc;

    /**
     * For NAT'ed VMs we use (default):
     *     - client mode (uConnectAddr / uConnectPort) on the guest.
     *     - server mode (uBindAddr / uBindPort) on the host.
     */
    if (   !pTstEnv->TcpOpts.szConnectAddr[0]
        && !pTstEnv->TcpOpts.szBindAddr[0])
            RTStrCopy(pTstEnv->TcpOpts.szBindAddr, sizeof(pTstEnv->TcpOpts.szBindAddr), "0.0.0.0");

    /*
     * Determine connection mode based on set variables.
     */
    if (   pTstEnv->TcpOpts.szBindAddr[0]
        && pTstEnv->TcpOpts.szConnectAddr[0])
    {
        pTstEnv->TcpOpts.enmConnMode = ATSCONNMODE_BOTH;
    }
    else if (pTstEnv->TcpOpts.szBindAddr[0])
        pTstEnv->TcpOpts.enmConnMode = ATSCONNMODE_SERVER;
    else /* "Reversed mode", i.e. used for NATed VMs. */
        pTstEnv->TcpOpts.enmConnMode = ATSCONNMODE_CLIENT;

    /* Set a back reference to the test environment for the callback context. */
    pTstEnv->CallbackCtx.pTstEnv = pTstEnv;

    ATSCALLBACKS Callbacks;
    RT_ZERO(Callbacks);
    Callbacks.pvUser = &pTstEnv->CallbackCtx;

    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        Callbacks.pfnHowdy            = audioTestGstAtsHowdyCallback;
        Callbacks.pfnBye              = audioTestGstAtsByeCallback;
        Callbacks.pfnTestSetBegin     = audioTestGstAtsTestSetBeginCallback;
        Callbacks.pfnTestSetEnd       = audioTestGstAtsTestSetEndCallback;
        Callbacks.pfnTonePlay         = audioTestGstAtsTonePlayCallback;
        Callbacks.pfnToneRecord       = audioTestGstAtsToneRecordCallback;
        Callbacks.pfnTestSetSendBegin = audioTestGstAtsTestSetSendBeginCallback;
        Callbacks.pfnTestSetSendRead  = audioTestGstAtsTestSetSendReadCallback;
        Callbacks.pfnTestSetSendEnd   = audioTestGstAtsTestSetSendEndCallback;

        if (!pTstEnv->TcpOpts.uBindPort)
            pTstEnv->TcpOpts.uBindPort = ATS_TCP_DEF_BIND_PORT_GUEST;

        if (!pTstEnv->TcpOpts.uConnectPort)
            pTstEnv->TcpOpts.uConnectPort = ATS_TCP_DEF_CONNECT_PORT_GUEST;

        pTstEnv->pSrv = (PATSSERVER)RTMemAlloc(sizeof(ATSSERVER));
        AssertPtrReturn(pTstEnv->pSrv, VERR_NO_MEMORY);

        /*
         * Start the ATS (Audio Test Service) on the guest side.
         * That service then will perform playback and recording operations on the guest, triggered from the host.
         *
         * When running this in self-test mode, that service also can be run on the host if nothing else is specified.
         * Note that we have to bind to "0.0.0.0" by default so that the host can connect to it.
         */
        rc = audioTestEnvConfigureAndStartTcpServer(pTstEnv->pSrv, &Callbacks, "guest", &pTstEnv->TcpOpts);
    }
    else /* Host mode */
    {
        if (!pTstEnv->TcpOpts.uBindPort)
            pTstEnv->TcpOpts.uBindPort = ATS_TCP_DEF_BIND_PORT_HOST;

        if (!pTstEnv->TcpOpts.uConnectPort)
            pTstEnv->TcpOpts.uConnectPort = ATS_TCP_DEF_CONNECT_PORT_HOST_PORT_FWD;

        /**
         * Note: Don't set pTstEnv->TcpOpts.szTcpConnectAddr by default here, as this specifies what connection mode
         *       (client / server / both) we use on the host.
         */

        /* We need to start a server on the host so that VMs configured with NAT networking
         * can connect to it as well. */
        rc = AudioTestSvcClientCreate(&pTstEnv->u.Host.AtsClGuest);
        if (RT_SUCCESS(rc))
            rc = audioTestEnvConnectViaTcp(pTstEnv, &pTstEnv->u.Host.AtsClGuest,
                                           "host -> guest", &pTstEnv->TcpOpts);
        if (RT_SUCCESS(rc))
        {
            AUDIOTESTENVTCPOPTS ValKitTcpOpts;
            RT_ZERO(ValKitTcpOpts);

            /* We only connect as client to the Validation Kit audio driver ATS. */
            ValKitTcpOpts.enmConnMode = ATSCONNMODE_CLIENT;

            /* For now we ASSUME that the Validation Kit audio driver ATS runs on the same host as VKAT (this binary) runs on. */
            ValKitTcpOpts.uConnectPort = ATS_TCP_DEF_CONNECT_PORT_VALKIT; /** @todo Make this dynamic. */
            RTStrCopy(ValKitTcpOpts.szConnectAddr, sizeof(ValKitTcpOpts.szConnectAddr), ATS_TCP_DEF_CONNECT_HOST_ADDR_STR); /** @todo Ditto. */

            rc = AudioTestSvcClientCreate(&pTstEnv->u.Host.AtsClValKit);
            if (RT_SUCCESS(rc))
            {
                rc = audioTestEnvConnectViaTcp(pTstEnv, &pTstEnv->u.Host.AtsClValKit,
                                               "host -> valkit", &ValKitTcpOpts);
                if (RT_FAILURE(rc))
                    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Unable to connect to the Validation Kit audio driver!\n"
                                                            "There could be multiple reasons:\n\n"
                                                            "    - Wrong host being used\n"
                                                            "    - VirtualBox host version is too old\n"
                                                            "    - Audio debug mode is not enabled\n"
                                                            "    - Support for Validation Kit audio driver is not included\n"
                                                            "    - Firewall / network configuration problem\n");
            }
        }
    }

    return rc;
}

/**
 * Destroys an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to destroy.
 */
void audioTestEnvDestroy(PAUDIOTESTENV pTstEnv)
{
    if (!pTstEnv)
        return;

    /* When in host mode, we need to destroy our ATS clients in order to also let
     * the ATS server(s) know we're going to quit. */
    if (pTstEnv->enmMode == AUDIOTESTMODE_HOST)
    {
        AudioTestSvcClientDestroy(&pTstEnv->u.Host.AtsClValKit);
        AudioTestSvcClientDestroy(&pTstEnv->u.Host.AtsClGuest);
    }

    if (pTstEnv->pSrv)
    {
        int rc2 = AudioTestSvcDestroy(pTstEnv->pSrv);
        AssertRC(rc2);

        RTMemFree(pTstEnv->pSrv);
        pTstEnv->pSrv = NULL;
    }

    for (unsigned i = 0; i < RT_ELEMENTS(pTstEnv->aStreams); i++)
    {
        int rc2 = audioTestStreamDestroy(pTstEnv, &pTstEnv->aStreams[i]);
        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Stream destruction for stream #%u failed with %Rrc\n", i, rc2);
    }

    /* Try cleaning up a bit. */
    RTDirRemove(pTstEnv->szPathTemp);
    RTDirRemove(pTstEnv->szPathOut);

    pTstEnv->pDrvStack = NULL;
}

/**
 * Closes, packs up and destroys a test environment.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to handle.
 * @param   fPack               Whether to pack the test set up before destroying / wiping it.
 * @param   pszPackFile         Where to store the packed test set file on success. Can be NULL if \a fPack is \c false.
 * @param   cbPackFile          Size (in bytes) of \a pszPackFile. Can be 0 if \a fPack is \c false.
 */
int audioTestEnvPrologue(PAUDIOTESTENV pTstEnv, bool fPack, char *pszPackFile, size_t cbPackFile)
{
    /* Close the test set first. */
    AudioTestSetClose(&pTstEnv->Set);

    int rc = VINF_SUCCESS;

    if (fPack)
    {
        /* Before destroying the test environment, pack up the test set so
         * that it's ready for transmission. */
        rc = AudioTestSetPack(&pTstEnv->Set, pTstEnv->szPathOut, pszPackFile, cbPackFile);
        if (RT_SUCCESS(rc))
            RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test set packed up to '%s'\n", pszPackFile);
    }

    if (!g_fDrvAudioDebug) /* Don't wipe stuff when debugging. Can be useful for introspecting data. */
        /* ignore rc */ AudioTestSetWipe(&pTstEnv->Set);

    AudioTestSetDestroy(&pTstEnv->Set);

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Test set prologue failed with %Rrc\n", rc);

    return rc;
}

/**
 * Initializes an audio test parameters set.
 *
 * @param   pTstParms           Test parameters set to initialize.
 */
void audioTestParmsInit(PAUDIOTESTPARMS pTstParms)
{
    RT_ZERO(*pTstParms);
}

/**
 * Destroys an audio test parameters set.
 *
 * @param   pTstParms           Test parameters set to destroy.
 */
void audioTestParmsDestroy(PAUDIOTESTPARMS pTstParms)
{
    if (!pTstParms)
        return;

    return;
}

