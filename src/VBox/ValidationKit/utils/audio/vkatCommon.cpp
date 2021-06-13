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
static int audioTestCreateStreamDefaultIn(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps);
static int audioTestCreateStreamDefaultOut(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps);
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

/**
 * Opens an audio device.
 *
 * @returns VBox status code.
 * @param   pDev                Audio device to open.
 */
int audioTestDeviceOpen(PPDMAUDIOHOSTDEV pDev)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(g_hTest, "Opening audio device '%s' ...", pDev->pszName);

    /** @todo Detect + open device here. */

    RTTestSubDone(g_hTest);

    return rc;
}

/**
 * Closes an audio device.
 *
 * @returns VBox status code.
 * @param   pDev                Audio device to close.
 */
int audioTestDeviceClose(PPDMAUDIOHOSTDEV pDev)
{
    int rc = VINF_SUCCESS;

    RTTestSubF(g_hTest, "Closing audio device '%s' ...", pDev->pszName);

    /** @todo Close device here. */

    RTTestSubDone(g_hTest);

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

    return rc;
}

/**
 * Creates an audio default input (recording) test stream.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pProps              PCM properties to use for creation.
 */
static int audioTestCreateStreamDefaultIn(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps)
{
    pStream->pBackend = NULL;
    int rc = audioTestDriverStackStreamCreateInput(&pTstEnv->DrvStack, pProps, pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer,
                                                   pTstEnv->cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    if (RT_SUCCESS(rc) && !pTstEnv->DrvStack.pIAudioConnector)
        pStream->pBackend = &((PAUDIOTESTDRVSTACKSTREAM)pStream->pStream)->Backend;
    return rc;
}

/**
 * Creates an audio default output (playback) test stream.
 * Convenience function.
 *
 * @returns VBox status code.
 * @param   pTstEnv             Test environment to use for creating the stream.
 * @param   pStream             Audio stream to create.
 * @param   pProps              PCM properties to use for creation.
 */
static int audioTestCreateStreamDefaultOut(PAUDIOTESTENV pTstEnv, PAUDIOTESTSTREAM pStream, PPDMAUDIOPCMPROPS pProps)
{
    pStream->pBackend = NULL;
    int rc = audioTestDriverStackStreamCreateOutput(&pTstEnv->DrvStack, pProps, pTstEnv->cMsBufferSize, pTstEnv->cMsPreBuffer,
                                                    pTstEnv->cMsSchedulingHint, &pStream->pStream, &pStream->Cfg);
    if (RT_SUCCESS(rc) && !pTstEnv->DrvStack.pIAudioConnector)
        pStream->pBackend = &((PAUDIOTESTDRVSTACKSTREAM)pStream->pStream)->Backend;
    return rc;
}


/*********************************************************************************************************************************
*   Test Primitives                                                                                                              *
*********************************************************************************************************************************/

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
    AudioTestToneInit(&TstTone, &pParms->Props, pParms->dbFreqHz);

    const char *pcszPathOut = pTstEnv->Set.szPathAbs;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Playing test tone (tone frequency is %RU16Hz, %RU32ms)\n", (uint16_t)pParms->dbFreqHz, pParms->msDuration);
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG,  "Writing to '%s'\n", pcszPathOut);

    /** @todo Use .WAV here? */
    PAUDIOTESTOBJ pObj;
    int rc = AudioTestSetObjCreateAndRegister(&pTstEnv->Set, "guest-tone-play.pcm", &pObj);
    AssertRCReturn(rc, rc);

    if (audioTestDriverStackStreamIsOkay(&pTstEnv->DrvStack, pStream->pStream))
    {
        uint32_t cbBuf;
        uint8_t  abBuf[_4K];

        const uint32_t cbPerSched = PDMAudioPropsMilliToBytes(&pParms->Props, pTstEnv->cMsSchedulingHint);
        AssertStmt(cbPerSched, rc = VERR_INVALID_PARAMETER);
              uint32_t cbToWrite  = PDMAudioPropsMilliToBytes(&pParms->Props, pParms->msDuration);
        AssertStmt(cbToWrite,  rc = VERR_INVALID_PARAMETER);

        if (RT_SUCCESS(rc))
        {
            AudioTestSetObjAddMetadataStr(pObj, "buffer_size_ms=%RU32\n", pTstEnv->cMsBufferSize);
            AudioTestSetObjAddMetadataStr(pObj, "prebuf_size_ms=%RU32\n", pTstEnv->cMsPreBuffer);
            AudioTestSetObjAddMetadataStr(pObj, "scheduling_hint_ms=%RU32\n", pTstEnv->cMsSchedulingHint);

            while (cbToWrite)
            {
                uint32_t cbWritten    = 0;
                uint32_t cbToGenerate = RT_MIN(cbToWrite, RT_MIN(cbPerSched, sizeof(abBuf)));
                Assert(cbToGenerate);

                rc = AudioTestToneGenerate(&TstTone, abBuf, cbToGenerate, &cbBuf);
                if (RT_SUCCESS(rc))
                {
                    /* Write stuff to disk before trying to play it. Help analysis later. */
                    rc = AudioTestSetObjWrite(pObj, abBuf, cbBuf);
                    if (RT_SUCCESS(rc))
                        rc = audioTestDriverStackStreamPlay(&pTstEnv->DrvStack, pStream->pStream,
                                                            abBuf, cbBuf, &cbWritten);
                }

                if (RT_FAILURE(rc))
                    break;

                RTThreadSleep(pTstEnv->cMsSchedulingHint);

                Assert(cbToWrite >= cbWritten);
                cbToWrite -= cbWritten;
            }
        }
    }
    else
        rc = VERR_AUDIO_STREAM_NOT_READY;

    int rc2 = AudioTestSetObjClose(pObj);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        RTTestFailed(g_hTest, "Playing tone done failed with %Rrc\n", rc);

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

    int rc = audioTestCreateStreamDefaultOut(pTstEnv, pTstStream, &pToneParms->Props);
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

    int rc = audioTestCreateStreamDefaultIn(pTstEnv, pTstStream, &pToneParms->Props);
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

int audioTestEnvConnectToHostAts(PAUDIOTESTENV pTstEnv,
                                 const char *pszHostTcpAddr, uint32_t uHostTcpPort)
{
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connecting to host ATS at %s:%RU32 ...\n",
                 (pszHostTcpAddr && *pszHostTcpAddr) ? pszHostTcpAddr : ATS_TCP_HOST_DEFAULT_ADDR_STR,
                 uHostTcpPort ? uHostTcpPort : ATS_TCP_HOST_DEFAULT_PORT);

    int rc = AudioTestSvcClientConnect(&pTstEnv->u.Host.AtsClValKit, pszHostTcpAddr, uHostTcpPort);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "Connecting to host ATS failed with %Rrc\n", rc);
        return rc;
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connected to host ATS\n");
    return rc;
}

int audioTestEnvConnectToGuestAts(PAUDIOTESTENV pTstEnv,
                                  const char *pszGuestTcpAddr, uint32_t uGuestTcpPort)
{
    RTTestPrintf(g_hTest, RTTESTLVL_DEBUG, "Connecting to guest ATS at %s:%RU32 ...\n",
                 (pszGuestTcpAddr && *pszGuestTcpAddr) ? pszGuestTcpAddr : "127.0.0.1",
                 uGuestTcpPort ? uGuestTcpPort : ATS_TCP_GUEST_DEFAULT_PORT);

    int rc = AudioTestSvcClientConnect(&pTstEnv->u.Host.AtsClGuest, pszGuestTcpAddr, uGuestTcpPort);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(g_hTest, "Connecting to guest ATS failed with %Rrc\n", rc);
        return rc;
    }

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Connected to guest ATS\n");
    return rc;
}

/**
 * Initializes an audio test environment.
 *
 * @param   pTstEnv             Audio test environment to initialize.
 * @param   pDrvReg             Audio driver to use.
 * @param   fWithDrvAudio       Whether to include DrvAudio in the stack or not.
 * @param   pszHostTcpAddr      Host ATS TCP/IP address to connect to.
 *                              If NULL, ATS_TCP_HOST_DEFAULT_ADDR_STR will be used.
 * @param   uHostTcpPort        Host ATS TCP/IP port to connect to.
 *                              If 0, ATS_TCP_HOST_DEFAULT_PORT will be used.
 * @param   pszGuestTcpAddr     Guest ATS TCP/IP address to connect to.
 *                              If NULL, localhost (127.0.0.1) will be used.
 * @param   uGuestTcpPort       Guest ATS TCP/IP port to connect to.
 *                              If 0, ATS_TCP_GUEST_DEFAULT_PORT will be used.
 */
int audioTestEnvInit(PAUDIOTESTENV pTstEnv,
                     PCPDMDRVREG pDrvReg, bool fWithDrvAudio,
                     const char *pszHostTcpAddr, uint32_t uHostTcpPort,
                     const char *pszGuestTcpAddr, uint32_t uGuestTcpPort)
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

    if (pDrvReg == NULL)
    {
        if (pTstEnv->fSelftest)
            pDrvReg = &g_DrvHostValidationKitAudio;
        else /* Go with the platform's default backend if nothing else is set. */
            pDrvReg = AudioTestGetDefaultBackend();
    }

    if (!uHostTcpPort)
        uHostTcpPort = ATS_TCP_HOST_DEFAULT_PORT;

    if (!uGuestTcpPort)
        uGuestTcpPort = ATS_TCP_GUEST_DEFAULT_PORT;

    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Test mode is '%s'\n", pTstEnv->enmMode == AUDIOTESTMODE_HOST ? "host" : "guest");
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Using tag '%s'\n", pTstEnv->szTag);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Output directory is '%s'\n", pTstEnv->szPathOut);
    RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Temp directory is '%s'\n", pTstEnv->szPathTemp);

    PDMAudioHostEnumInit(&pTstEnv->DevEnum);

    pTstEnv->cMsBufferSize     = 300; /* ms */ /** @todo Randomize this also? */
    pTstEnv->cMsPreBuffer      = 150; /* ms */ /** @todo Ditto. */
    pTstEnv->cMsSchedulingHint = RTRandU32Ex(10, 80); /* Choose a random scheduling (in ms). */

    /* Only the guest mode needs initializing the driver stack. */
    const bool fUseDriverStack = pTstEnv->enmMode == AUDIOTESTMODE_GUEST;
    if (fUseDriverStack)
    {
        rc = audioTestDriverStackInitEx(&pTstEnv->DrvStack, pDrvReg,
                                        true /* fEnabledIn */, true /* fEnabledOut */, fWithDrvAudio);
        if (RT_FAILURE(rc))
            return rc;

        PPDMAUDIOHOSTDEV pDev;
        rc = audioTestDevicesEnumerateAndCheck(pTstEnv, NULL /* pszDevice */, &pDev); /** @todo Implement device checking. */
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

    if (   RT_SUCCESS(rc)
        && !strlen(pTstEnv->szPathOut))
        rc = RTPathJoin(pTstEnv->szPathOut, sizeof(pTstEnv->szPathOut), szPathTemp, "vkat");

    if (RT_FAILURE(rc))
        return rc;

    /** @todo Implement NAT mode like we do for TxS later? */
    if (pTstEnv->enmMode == AUDIOTESTMODE_GUEST)
    {
        ATSCALLBACKCTX Ctx;
        Ctx.pTstEnv = pTstEnv;

        ATSCALLBACKS Callbacks;
        Callbacks.pfnTestSetBegin     = audioTestGstAtsTestSetBeginCallback;
        Callbacks.pfnTestSetEnd       = audioTestGstAtsTestSetEndCallback;
        Callbacks.pfnTonePlay         = audioTestGstAtsTonePlayCallback;
        Callbacks.pfnToneRecord       = audioTestGstAtsToneRecordCallback;
        Callbacks.pfnTestSetSendBegin = audioTestGstAtsTestSetSendBeginCallback;
        Callbacks.pfnTestSetSendRead  = audioTestGstAtsTestSetSendReadCallback;
        Callbacks.pfnTestSetSendEnd   = audioTestGstAtsTestSetSendEndCallback;
        Callbacks.pvUser              = &Ctx;

        /*
         * Start the ATS (Audio Test Service) on the guest side.
         * That service then will perform playback and recording operations on the guest, triggered from the host.
         *
         * When running this in self-test mode, that service also will be run on the host.
         */
        RTTestPrintf(g_hTest, RTTESTLVL_ALWAYS, "Starting guest ATS at %s:%RU32...\n",
                     (pszGuestTcpAddr && *pszGuestTcpAddr) ? pszGuestTcpAddr : "127.0.0.1",
                     uGuestTcpPort ? uGuestTcpPort : ATS_TCP_GUEST_DEFAULT_PORT);
        rc = AudioTestSvcInit(&pTstEnv->u.Guest.Srv, pszGuestTcpAddr, uGuestTcpPort, &Callbacks);
        if (RT_SUCCESS(rc))
            rc = AudioTestSvcStart(&pTstEnv->u.Guest.Srv);

        if (RT_FAILURE(rc))
            RTTestFailed(g_hTest, "Starting ATS failed with %Rrc\n", rc);
    }
    else /* Host mode */
    {
        rc = audioTestEnvConnectToHostAts(pTstEnv, pszHostTcpAddr, uHostTcpPort);
        if (RT_SUCCESS(rc))
            rc = audioTestEnvConnectToGuestAts(pTstEnv, pszGuestTcpAddr, uGuestTcpPort);
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

