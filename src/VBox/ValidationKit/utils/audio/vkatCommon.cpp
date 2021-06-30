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
/**
 * Structure for keeping a user context for the test service callbacks.
 */
typedef struct ATSCALLBACKCTX
{
    /** The test environment bound to this context. */
    PAUDIOTESTENV pTstEnv;
    /** Absolute path to the packed up test set archive.
     *  Keep it simple for now and only support one (open) archive at a time. */
    char          szTestSetArchive[RTPATH_MAX];
    /** File handle to the (opened) test set archive for reading. */
    RTFILE        hTestSetArchive;
} ATSCALLBACKCTX;
typedef ATSCALLBACKCTX *PATSCALLBACKCTX;


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int audioTestStreamInit(PAUDIOTESTDRVSTACK pDrvStack, PAUDIOTESTSTREAM pStream, PDMAUDIODIR enmDir, PCPDMAUDIOPCMPROPS pProps, bool fWithMixer, uint32_t cMsBufferSize, uint32_t cMsPreBuffer, uint32_t cMsSchedulingHint);
static int audioTestStreamDestroy(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream);
static int audioTestDevicesEnumerateAndCheck(PAUDIOTESTENV pTstEnv, const char *pszDev, PPDMAUDIOHOSTDEV *ppDev);


/*********************************************************************************************************************************
*   Device enumeration + handling.                                                                                               *
*********************************************************************************************************************************/

/**
 * Enumerates audio devices and optionally searches for a specific device.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test env to use for enumeration.
 * @param   pszDev              Device name to search for. Can be NULL if the default device shall be used.
 * @param   ppDev               Where to return the pointer of the device enumeration of \a pTstEnv when a
 *                              specific device was found.
 */
static int audioTestDevicesEnumerateAndCheck(PAUDIOTESTENV pTstEnv, const char *pszDev, PPDMAUDIOHOSTDEV *ppDev)
{
#ifdef DEBUG_andy
    return VINF_SUCCESS;
#endif

    RTTestSubF(g_hTest, "Enumerating audio devices and checking for device '%s'", pszDev ? pszDev : "<Default>");

    if (!pTstEnv->DrvStack.pIHostAudio->pfnGetDevices)
    {
        RTTestSkipped(g_hTest, "Backend does not support device enumeration, skipping");
        return VINF_NOT_SUPPORTED;
    }

    Assert(pszDev == NULL || ppDev);

    if (ppDev)
        *ppDev = NULL;

    int rc = pTstEnv->DrvStack.pIHostAudio->pfnGetDevices(pTstEnv->DrvStack.pIHostAudio, &pTstEnv->DevEnum);
    if (RT_SUCCESS(rc))
    {
        PPDMAUDIOHOSTDEV pDev;
        RTListForEach(&pTstEnv->DevEnum.LstDevices, pDev, PDMAUDIOHOSTDEV, ListEntry)
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

            if (   pszDev
                && !RTStrCmp(pDev->pszName, pszDev))
            {
                *ppDev = pDev;
            }
        }
    }
    else
        RTTestFailed(g_hTest, "Enumerating audio devices failed with %Rrc", rc);

    RTTestSubDone(g_hTest);

    if (   pszDev
        && *ppDev == NULL)
    {
        RTTestFailed(g_hTest, "Audio device '%s' not found", pszDev);
        return VERR_NOT_FOUND;
    }

    return VINF_SUCCESS;
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

        audioTestDriverStackStreamDestroy(&pTstEnv->DrvStack, pStream->pStream);
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
 * @param   pStream             Stream to use for playing the tone.
 * @param   pParms              Tone parameters to use.
 *
 * @note    Blocking function.
 */
static int audioTestPlayTone(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PAUDIOTESTTONEPARMS pParms)
{
    AUDIOTESTTONE TstTone;
    AudioTestToneInit(&TstTone, &pStream->Cfg.Props, pParms->dbFreqHz);

    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Playing test tone (tone frequency is %RU16Hz, %RU32ms)\n", (uint16_t)pParms->dbFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using %RU32ms stream scheduling hint\n", pStream->Cfg.Device.cMsSchedulingHint);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "guest-tone-play.pcm", &pObj);
    AssertRCReturn(rc, rc);

    rc = AudioTestMixStreamEnable(&pStream->Mix);
    if (   RT_SUCCESS(rc)
        && AudioTestMixStreamIsOkay(&pStream->Mix))
    {
        uint8_t  abBuf[_4K];

        uint32_t cbToPlayTotal  = PDMAudioPropsMilliToBytes(&pStream->Cfg.Props, pParms->msDuration);
        AssertStmt(cbToPlayTotal, rc = VERR_INVALID_PARAMETER);

        RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Playing %RU32 bytes total\n", cbToPlayTotal);

        AudioTestSetObjAddMetadataStr(pObj, "stream_to_play_bytes=%RU32\n",      cbToPlayTotal);
        AudioTestSetObjAddMetadataStr(pObj, "stream_period_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesPeriod);
        AudioTestSetObjAddMetadataStr(pObj, "stream_buffer_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesBufferSize);
        AudioTestSetObjAddMetadataStr(pObj, "stream_prebuf_size_frames=%RU32\n", pStream->Cfg.Backend.cFramesPreBuffering);
        /* Note: This mostly is provided by backend (e.g. PulseAudio / ALSA / ++) and
         *       has nothing to do with the device emulation scheduling hint. */
        AudioTestSetObjAddMetadataStr(pObj, "device_scheduling_hint_ms=%RU32\n", pStream->Cfg.Device.cMsSchedulingHint);

        while (cbToPlayTotal)
        {
            uint32_t       cbPlayed   = 0;
            uint32_t const cbCanWrite = AudioTestMixStreamGetWritable(&pStream->Mix);
            if (cbCanWrite)
            {
                uint32_t const cbToGenerate = RT_MIN(RT_MIN(cbToPlayTotal, sizeof(abBuf)), cbCanWrite);
                uint32_t       cbToPlay;
                rc = AudioTestToneGenerate(&TstTone, abBuf, cbToGenerate, &cbToPlay);
                if (RT_SUCCESS(rc))
                {
                    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Playing %RU32 bytes ...\n", cbToPlay);

                    /* Write stuff to disk before trying to play it. Help analysis later. */
                    rc = AudioTestSetObjWrite(pObj, abBuf, cbToPlay);
                    if (RT_SUCCESS(rc))
                        rc = AudioTestMixStreamPlay(&pStream->Mix, abBuf, cbToPlay, &cbPlayed);
                }

                if (RT_FAILURE(rc))
                    break;
            }
            else if (AudioTestMixStreamIsOkay(&pStream->Mix))
                RTThreadSleep(RT_MIN(RT_MAX(1, pStream->Cfg.Device.cMsSchedulingHint), 256));
            else
                AssertFailedBreakStmt(rc = VERR_AUDIO_STREAM_NOT_READY);

            Assert(cbToPlayTotal >= cbPlayed);
            cbToPlayTotal -= cbPlayed;
        }

        if (cbToPlayTotal != 0)
            RTTestFailed(g_hTest, "Playback ended unexpectedly (%RU32 bytes left)\n", cbToPlayTotal);
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

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

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Recording test tone (for %RU32ms)\n", pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "guest-tone-rec.pcm", &pObj);
    AssertRCReturn(rc, rc);

    if (audioTestDriverStackStreamIsOkay(&pTstEnv->DrvStack, pStream->pStream))
    {
        const uint32_t cbPerSched = PDMAudioPropsMilliToBytes(&pParms->Props, pTstEnv->cMsSchedulingHint);
        AssertStmt(cbPerSched, rc = VERR_INVALID_PARAMETER);
              uint32_t cbToRead   = PDMAudioPropsMilliToBytes(&pParms->Props, pParms->msDuration);
        AssertStmt(cbToRead,   rc = VERR_INVALID_PARAMETER);

        if (RT_SUCCESS(rc))
        {
            AudioTestSetObjAddMetadataStr(pObj, "buffer_size_ms=%RU32\n", pTstEnv->cMsBufferSize);
            AudioTestSetObjAddMetadataStr(pObj, "prebuf_size_ms=%RU32\n", pTstEnv->cMsPreBuffer);
            AudioTestSetObjAddMetadataStr(pObj, "scheduling_hint_ms=%RU32\n", pTstEnv->cMsSchedulingHint);

            uint8_t abBuf[_4K];

            while (cbToRead)
            {
                const uint32_t cbChunk = RT_MIN(cbToRead, RT_MIN(cbPerSched, sizeof(abBuf)));

                uint32_t cbRead = 0;
                rc = audioTestDriverStackStreamCapture(&pTstEnv->DrvStack, pStream->pStream, (void *)abBuf, cbChunk, &cbRead);
                if (RT_SUCCESS(rc))
                    rc = AudioTestSetObjWrite(pObj, abBuf, cbRead);

                if (RT_FAILURE(rc))
                    break;

                RTThreadSleep(pTstEnv->cMsSchedulingHint);

                Assert(cbToRead >= cbRead);
                cbToRead -= cbRead;
            }
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Recording tone done failed with %Rrc\n", rc);

    return rc;
}


/*********************************************************************************************************************************
*   ATS Callback Implementations                                                                                                 *
*********************************************************************************************************************************/

/** @copydoc ATSCALLBACKS::pfnTestSetBegin
 *
 *  @note Runs as part of the guest ATS.
 */
static DECLCALLBACK(int) audioTestGstAtsTestSetBeginCallback(void const *pvUser, const char *pszTag)
{
    PATSCALLBACKCTX pCtx    = (PATSCALLBACKCTX)pvUser;
    PAUDIOTESTENV   pTstEnv = pCtx->pTstEnv;

    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Beginning test set '%s' in '%s'\n", pszTag, pTstEnv->szPathTemp);

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

    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Ending test set '%s'\n", pszTag);

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

    AUDIOTESTTONE TstTone;
    AudioTestToneInitRandom(&TstTone, &pToneParms->Props);

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestStreamInit(&pTstEnv->DrvStack, pTstStream, PDMAUDIODIR_OUT, &pTstEnv->Props, false /* fWithMixer */,
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

    const PAUDIOTESTSTREAM pTstStream = &pTstEnv->aStreams[0]; /** @todo Make this dynamic. */

    int rc = audioTestStreamInit(&pTstEnv->DrvStack, pTstStream, PDMAUDIODIR_IN, &pTstEnv->Props, false /* fWithMixer */,
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
 * @param   pszTcpBindAddr      TCP/IP bind address. Optionl and can be NULL.
 *                              Server mode will be disabled then.
 * @param   uTcpBindPort        TCP/IP bind port. Optionl and can be 0.
 *                              Server mode will be disabled then. *
 * @param   pszTcpConnectAddr   TCP/IP connect address. Optionl and can be NULL.
 *                              Client mode will be disabled then.
 * @param   uTcpConnectPort     TCP/IP connect port. Optionl and can be 0.
 *                              Client mode will be disabled then.
 */
int audioTestEnvConnectViaTcp(PAUDIOTESTENV pTstEnv, PATSCLIENT pClient, const char *pszWhat,
                              const char *pszTcpBindAddr, uint16_t uTcpBindPort,
                              const char *pszTcpConnectAddr, uint16_t uTcpConnectPort)
{
    RT_NOREF(pTstEnv);

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting %s ...\n", pszWhat);

    RTGETOPTUNION Val;
    RT_ZERO(Val);

    int rc;

    if (   !pszTcpBindAddr
        || !uTcpBindPort)
    {
        Val.psz = "client";
    }
    else if (   !pszTcpConnectAddr
             || !uTcpConnectPort)
    {
        Val.psz = "server";
    }
    else
        Val.psz = "both";

    rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_MODE, &Val);
    AssertRCReturn(rc, rc);

    if (   !RTStrCmp(Val.psz, "client")
        || !RTStrCmp(Val.psz, "both"))
           RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting at %s:%RU32\n", pszTcpConnectAddr, uTcpConnectPort);

    if (   !RTStrCmp(Val.psz, "server")
        || !RTStrCmp(Val.psz, "both"))
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Listening at %s:%RU32\n", pszTcpBindAddr ? pszTcpBindAddr : "<None>", uTcpBindPort);

    if (pszTcpBindAddr)
    {
        Val.psz = pszTcpBindAddr;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_BIND_ADDRESS, &Val);
        AssertRCReturn(rc, rc);
    }

    if (uTcpBindPort)
    {
        Val.u16 = uTcpBindPort;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_BIND_PORT, &Val);
        AssertRCReturn(rc, rc);
    }

    if (pszTcpConnectAddr)
    {
        Val.psz = pszTcpConnectAddr;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_CONNECT_ADDRESS, &Val);
        AssertRCReturn(rc, rc);
    }

    if (uTcpConnectPort)
    {
        Val.u16 = uTcpConnectPort;
        rc = AudioTestSvcClientHandleOption(pClient, ATSTCPOPT_CONNECT_PORT, &Val);
        AssertRCReturn(rc, rc);
    }

    rc = AudioTestSvcClientConnect(pClient);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "Connecting %s failed with %Rrc\n", pszWhat, rc);
        return rc;
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connected %s\n", pszWhat);
    return rc;
}

/**
 * Configures and starts an ATS TCP/IP server.
 *
 * @returns VBox status code.
 * @param   pSrv                ATS server instance to configure and start.
 * @param   pCallbacks          ATS callback table to use.
 * @param   pszDesc             Hint of server type which is being started.
 * @param   pszTcpBindAddr      TCP/IP bind address. Optionl and can be NULL.
 *                              Server mode will be disabled then.
 * @param   uTcpBindPort        TCP/IP bind port. Optionl and can be 0.
 *                              Server mode will be disabled then. *
 * @param   pszTcpConnectAddr   TCP/IP connect address. Optionl and can be NULL.
 *                              Client mode will be disabled then.
 * @param   uTcpConnectPort     TCP/IP connect port. Optionl and can be 0.
 *                              Client mode will be disabled then.
 */
int audioTestEnvConfigureAndStartTcpServer(PATSSERVER pSrv, PCATSCALLBACKS pCallbacks, const char *pszDesc,
                                           const char *pszTcpBindAddr, uint16_t uTcpBindPort,
                                           const char *pszTcpConnectAddr, uint16_t uTcpConnectPort)
{
    RTGETOPTUNION Val;
    RT_ZERO(Val);

    if (pszTcpBindAddr)
    {
        Val.psz = pszTcpBindAddr;
        AudioTestSvcHandleOption(pSrv, ATSTCPOPT_BIND_ADDRESS, &Val);
    }

    if (uTcpBindPort)
    {
        Val.u16 = uTcpBindPort;
        AudioTestSvcHandleOption(pSrv, ATSTCPOPT_BIND_PORT, &Val);
    }

    if (pszTcpConnectAddr)
    {
        Val.psz = pszTcpConnectAddr;
        AudioTestSvcHandleOption(pSrv, ATSTCPOPT_CONNECT_ADDRESS, &Val);
    }

    if (uTcpConnectPort)
    {
        Val.u16 = uTcpConnectPort;
        AudioTestSvcHandleOption(pSrv, ATSTCPOPT_CONNECT_PORT, &Val);
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Starting server for %s at %s:%RU32 ...\n",
                 pszDesc, pszTcpBindAddr[0] ? pszTcpBindAddr : "0.0.0.0", uTcpBindPort);
    if (pszTcpConnectAddr[0])
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Trying %s to connect as client to %s:%RU32 ...\n",
                     pszDesc, pszTcpConnectAddr[0] ? pszTcpConnectAddr : "0.0.0.0", uTcpConnectPort);

    int rc = AudioTestSvcInit(pSrv, pCallbacks);
    if (RT_SUCCESS(rc))
        rc = AudioTestSvcStart(pSrv);

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Starting server for %s failed with %Rrc\n", pszDesc, rc);

    return rc;
}

/**
 * Initializes an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvReg             Audio driver to use.
 * @param   fWithDrvAudio       Whether to include DrvAudio in the stack or not.
 */
int audioTestEnvInit(PAUDIOTESTENV pTstEnv,
                     PCPDMDRVREG pDrvReg, bool fWithDrvAudio)
{
    int rc = VINF_SUCCESS;

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

    /* Go with the platform's default backend if nothing else is set. */
    if (!pDrvReg)
        pDrvReg = AudioTestGetDefaultBackend();

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Initializing environment for mode '%s'\n", pTstEnv->enmMode == AUDIOTESTMODE_HOST ? "host" : "guest");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using backend '%s'\n", pDrvReg->szName);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pTstEnv->szTag);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", pTstEnv->szPathOut);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Temp directory is '%s'\n", pTstEnv->szPathTemp);

    if (!pTstEnv->cMsBufferSize)
        pTstEnv->cMsBufferSize     = UINT32_MAX;
    if (!pTstEnv->cMsPreBuffer)
        pTstEnv->cMsPreBuffer      = UINT32_MAX;
    if (!pTstEnv->cMsSchedulingHint)
        pTstEnv->cMsSchedulingHint = UINT32_MAX;

    PDMAudioHostEnumInit(&pTstEnv->DevEnum);

    bool fUseDriverStack = false; /* Whether to init + use the audio driver stack or not. */

    /* In regular testing mode only the guest mode needs initializing the driver stack. */
    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
        fUseDriverStack = true;

    /* When running in self-test mode, the host mode also needs to initialize the stack in order to
     * to run the Valdation Kit audio driver ATS (no "real" VBox involved). */
    if (pTstEnv->enmMode == AUDIOTESTMODE_HOST && pTstEnv->fSelftest)
        fUseDriverStack = true;

    if (fUseDriverStack)
    {
        rc = audioTestDriverStackInitEx(&pTstEnv->DrvStack, pDrvReg,
                                        true /* fEnabledIn */, true /* fEnabledOut */, fWithDrvAudio);
        if (RT_FAILURE(rc))
            return rc;

        PPDMAUDIOHOSTDEV pDev;
        rc = audioTestDevicesEnumerateAndCheck(pTstEnv, pTstEnv->szDev, &pDev);
        if (RT_FAILURE(rc))
            return rc;
    }

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

    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        ATSCALLBACKCTX Ctx;
        Ctx.pTstEnv = pTstEnv;

        ATSCALLBACKS Callbacks;
        RT_ZERO(Callbacks);
        Callbacks.pfnTestSetBegin     = audioTestGstAtsTestSetBeginCallback;
        Callbacks.pfnTestSetEnd       = audioTestGstAtsTestSetEndCallback;
        Callbacks.pfnTonePlay         = audioTestGstAtsTonePlayCallback;
        Callbacks.pfnToneRecord       = audioTestGstAtsToneRecordCallback;
        Callbacks.pfnTestSetSendBegin = audioTestGstAtsTestSetSendBeginCallback;
        Callbacks.pfnTestSetSendRead  = audioTestGstAtsTestSetSendReadCallback;
        Callbacks.pfnTestSetSendEnd   = audioTestGstAtsTestSetSendEndCallback;
        Callbacks.pvUser              = &Ctx;

        if (!pTstEnv->u.Guest.TcpOpts.uTcpBindPort)
            pTstEnv->u.Guest.TcpOpts.uTcpBindPort = ATS_TCP_DEF_BIND_PORT_GUEST;

        if (!pTstEnv->u.Guest.TcpOpts.szTcpBindAddr[0])
            RTStrCopy(pTstEnv->u.Guest.TcpOpts.szTcpBindAddr, sizeof(pTstEnv->u.Guest.TcpOpts.szTcpBindAddr), "0.0.0.0");

        if (!pTstEnv->u.Guest.TcpOpts.uTcpConnectPort)
            pTstEnv->u.Guest.TcpOpts.uTcpConnectPort = ATS_TCP_DEF_CONNECT_PORT_GUEST;

        if (!pTstEnv->u.Guest.TcpOpts.szTcpConnectAddr[0])
            RTStrCopy(pTstEnv->u.Guest.TcpOpts.szTcpConnectAddr, sizeof(pTstEnv->u.Guest.TcpOpts.szTcpConnectAddr), "10.0.2.2");

        /*
         * Start the ATS (Audio Test Service) on the guest side.
         * That service then will perform playback and recording operations on the guest, triggered from the host.
         *
         * When running this in self-test mode, that service also can be run on the host if nothing else is specified.
         * Note that we have to bind to "0.0.0.0" by default so that the host can connect to it.
         */
        rc = audioTestEnvConfigureAndStartTcpServer(&pTstEnv->u.Guest.Srv, &Callbacks, "Guest ATS",
                                                    pTstEnv->u.Guest.TcpOpts.szTcpBindAddr, pTstEnv->u.Guest.TcpOpts.uTcpBindPort,
                                                    pTstEnv->u.Guest.TcpOpts.szTcpConnectAddr, pTstEnv->u.Guest.TcpOpts.uTcpConnectPort);

    }
    else /* Host mode */
    {

        ATSCALLBACKCTX Ctx;
        Ctx.pTstEnv = pTstEnv;

        ATSCALLBACKS Callbacks;
        RT_ZERO(Callbacks);
        Callbacks.pvUser              = &Ctx;

        if (!pTstEnv->u.Host.TcpOpts.uTcpBindPort)
            pTstEnv->u.Host.TcpOpts.uTcpBindPort = ATS_TCP_DEF_BIND_PORT_HOST;

        if (!pTstEnv->u.Host.TcpOpts.szTcpBindAddr[0])
            RTStrCopy(pTstEnv->u.Host.TcpOpts.szTcpBindAddr, sizeof(pTstEnv->u.Host.TcpOpts.szTcpBindAddr), "0.0.0.0");

        if (!pTstEnv->u.Host.TcpOpts.uTcpConnectPort)
            pTstEnv->u.Host.TcpOpts.uTcpConnectPort = ATS_TCP_DEF_CONNECT_PORT_HOST_PORT_FWD;

        if (!pTstEnv->u.Host.TcpOpts.szTcpConnectAddr[0])
            RTStrCopy(pTstEnv->u.Host.TcpOpts.szTcpConnectAddr, sizeof(pTstEnv->u.Host.TcpOpts.szTcpConnectAddr),
                      ATS_TCP_DEF_CONNECT_HOST_ADDR_STR); /** @todo Get VM IP? Needs port forwarding. */

        /* We need to start a server on the host so that VMs configured with NAT networking
         * can connect to it as well. */
        rc = AudioTestSvcClientCreate(&pTstEnv->u.Host.AtsClGuest);
        if (RT_SUCCESS(rc))
            rc = audioTestEnvConnectViaTcp(pTstEnv, &pTstEnv->u.Host.AtsClGuest,
                                           "Host -> Guest ATS",
                                           pTstEnv->u.Host.TcpOpts.szTcpBindAddr, pTstEnv->u.Host.TcpOpts.uTcpBindPort,
                                           pTstEnv->u.Host.TcpOpts.szTcpConnectAddr, pTstEnv->u.Host.TcpOpts.uTcpConnectPort);
        if (RT_SUCCESS(rc))
        {
            if (!pTstEnv->ValKitTcpOpts.uTcpConnectPort)
                pTstEnv->ValKitTcpOpts.uTcpConnectPort = ATS_TCP_DEF_CONNECT_PORT_VALKIT;

            if (!pTstEnv->ValKitTcpOpts.szTcpConnectAddr[0])
                RTStrCopy(pTstEnv->ValKitTcpOpts.szTcpConnectAddr, sizeof(pTstEnv->ValKitTcpOpts.szTcpConnectAddr),
                          ATS_TCP_DEF_CONNECT_HOST_ADDR_STR);

            rc = AudioTestSvcClientCreate(&pTstEnv->u.Host.AtsClValKit);
            if (RT_SUCCESS(rc))
                rc = audioTestEnvConnectViaTcp(pTstEnv, &pTstEnv->u.Host.AtsClValKit,
                                               "Host -> Validation Kit Host Audio Driver ATS",
                                               pTstEnv->ValKitTcpOpts.szTcpBindAddr, pTstEnv->ValKitTcpOpts.uTcpBindPort,
                                               pTstEnv->ValKitTcpOpts.szTcpConnectAddr, pTstEnv->ValKitTcpOpts.uTcpConnectPort);
        }
    }

    if (   RT_FAILURE(rc)
        && fUseDriverStack)
        audioTestDriverStackDelete(&pTstEnv->DrvStack);

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

    PDMAudioHostEnumDelete(&pTstEnv->DevEnum);

    for (unsigned i = 0; i < RT_ELEMENTS(pTstEnv->aStreams); i++)
    {
        int rc2 = audioTestStreamDestroy(pTstEnv, &pTstEnv->aStreams[i]);
        if (RT_FAILURE(rc2))
            RTTestFailed(g_hTest, "Stream destruction for stream #%u failed with %Rrc\n", i, rc2);
    }

    /* Try cleaning up a bit. */
    RTDirRemove(pTstEnv->szPathTemp);
    RTDirRemove(pTstEnv->szPathOut);

    audioTestDriverStackDelete(&pTstEnv->DrvStack);
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

