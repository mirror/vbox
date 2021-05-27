/* $Id$ */
/** @file
 * Host audio driver - ValidationKit - For dumping and injecting audio data from/to the device emulation.
 */

/*
 * Copyright (C) 2016-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <iprt/env.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/stream.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "VBoxDD.h"
#include "AudioHlp.h"
#include "AudioTest.h"
#include "AudioTestService.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Structure for keeping a Validation Kit input/output stream.
 */
typedef struct VALKITAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** Audio file to dump output to or read input from. */
    PAUDIOHLPFILE           pFile;
    /** Text file to store timing of audio buffers submittions. */
    PRTSTREAM               pFileTiming;
    /** Timestamp of the first play or record request. */
    uint64_t                tsStarted;
    /** Total number of frames played or recorded so far. */
    uint32_t                cFramesSinceStarted;
    union
    {
        struct
        {
            /** Timestamp of last captured samples. */
            uint64_t        tsLastCaptured;
        } In;
        struct
        {
            /** Timestamp of last played samples. */
            uint64_t        tsLastPlayed;
            uint8_t        *pbPlayBuffer;
            uint32_t        cbPlayBuffer;
        } Out;
    };
} VALKITAUDIOSTREAM;
/** Pointer to a Validation Kit stream. */
typedef VALKITAUDIOSTREAM *PVALKITAUDIOSTREAM;

/**
 * Test tone-specific instance data.
 */
typedef struct VALKITTESTTONEDATA
{
    /** How many bytes to write. */
    uint64_t           cbToWrite;
    /** How many bytes already written. */
    uint64_t           cbWritten;
    /** The test tone instance to use. */
    AUDIOTESTTONE      Tone;
    /** The test tone parameters to use. */
    AUDIOTESTTONEPARMS Parms;
} VALKITTESTTONEDATA;

/**
 * Structure keeping a single Validation Kit test.
 */
typedef struct VALKITTESTDATA
{
    /** The list node. */
    RTLISTNODE             Node;
    /** Stream configuration to use for this test. */
    PDMAUDIOSTREAMCFG      StreamCfg;
    union
    {
        /** Test tone-specific data. */
        VALKITTESTTONEDATA TestTone;
    } t;
    /** Time stamp (real, in ms) when test started. */
    uint64_t               msStartedTS;
} VALKITTESTDATA;
/** Pointer to Validation Kit test data. */
typedef VALKITTESTDATA *PVALKITTESTDATA;

/**
 * Validation Kit audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTVALKITAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
    /** Number of tests in \a lstTestsRec. */
    uint32_t            cTestsRec;
    /** List keeping the recording tests (FIFO). */
    RTLISTANCHOR        lstTestsRec;
    /** Pointer to current recording test being processed. */
    PVALKITTESTDATA     pTestRecCur;
    /** Critical section for serializing access across threads. */
    RTCRITSECT          CritSect;
    /** The Audio Test Service (ATS) instance. */
    ATSSERVER           Srv;
} DRVHOSTVALKITAUDIO;
/** Pointer to a Validation Kit host audio driver instance. */
typedef DRVHOSTVALKITAUDIO *PDRVHOSTVALKITAUDIO;


/**
 * Note: Called within server (client serving) thread.
 */
static DECLCALLBACK(int) drvHostValKitAudioSvcTonePlayCallback(void const *pvUser, PPDMAUDIOSTREAMCFG pStreamCfg, PAUDIOTESTTONEPARMS pToneParms)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    PVALKITTESTDATA pTestData = (PVALKITTESTDATA)RTMemAllocZ(sizeof(VALKITTESTDATA));
    AssertPtrReturn(pTestData, VERR_NO_MEMORY);

    memcpy(&pTestData->StreamCfg,        pStreamCfg, sizeof(PDMAUDIOSTREAMCFG));
    memcpy(&pTestData->t.TestTone.Parms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    AudioTestToneInit(&pTestData->t.TestTone.Tone, &pStreamCfg->Props, pTestData->t.TestTone.Parms.dbFreqHz);

    pTestData->t.TestTone.cbToWrite = PDMAudioPropsMilliToBytes(&pStreamCfg->Props,
                                                                pTestData->t.TestTone.Parms.msDuration);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        RTListAppend(&pThis->lstTestsRec, &pTestData->Node);

        pThis->cTestsRec++;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "Validation Kit");
    pBackendCfg->cbStream       = sizeof(VALKITAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsOut = 1; /* Output (Playback). */
    pBackendCfg->cMaxStreamsIn  = 1; /* Input (Recording). */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostValKitAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


static int drvHostValKitAudioCreateStreamIn(PDRVHOSTVALKITAUDIO pThis, PVALKITAUDIOSTREAM pStreamDbg,
                                            PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pThis, pStreamDbg, pCfgReq, pCfgAcq);

    return VINF_SUCCESS;
}


static int drvHostValKitAudioCreateStreamOut(PDRVHOSTVALKITAUDIO pThis, PVALKITAUDIOSTREAM pStreamDbg,
                                             PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pThis, pCfgAcq);

    /* Use the test box scratch dir if we're running in such an
       environment, otherwise just dump the output in the temp
       directory. */
    char szTemp[RTPATH_MAX];
    int rc = RTEnvGetEx(RTENV_DEFAULT, "TESTBOX_PATH_SCRATCH", szTemp, sizeof(szTemp), NULL);
    if (RT_FAILURE(rc))
    {
        rc = RTPathTemp(szTemp, sizeof(szTemp));
        if (RT_SUCCESS(rc))
            rc = RTPathAppend(szTemp, sizeof(szTemp), "VBoxAudioValKit");
        AssertRCReturn(rc, rc);
    }

    /* Get down to things that may fail and need cleanup. */
    pStreamDbg->tsStarted           = 0;
    pStreamDbg->cFramesSinceStarted = 0;
    pStreamDbg->Out.tsLastPlayed    = 0;
    pStreamDbg->Out.cbPlayBuffer    = PDMAudioPropsFramesToBytes(&pCfgReq->Props, pCfgReq->Backend.cFramesBufferSize);
    pStreamDbg->Out.pbPlayBuffer    = (uint8_t *)RTMemAlloc(pStreamDbg->Out.cbPlayBuffer);
    AssertReturn(pStreamDbg->Out.pbPlayBuffer, VERR_NO_MEMORY);

    rc = AudioHlpFileCreateAndOpenEx(&pStreamDbg->pFile, AUDIOHLPFILETYPE_WAV, szTemp, "ValKit",
                                     pThis->pDrvIns->iInstance, AUDIOHLPFILENAME_FLAGS_NONE, AUDIOHLPFILE_FLAGS_NONE,
                                     &pCfgReq->Props, AUDIOHLPFILE_DEFAULT_OPEN_FLAGS);
    if (RT_SUCCESS(rc))
    {
        rc = RTPathAppend(szTemp, sizeof(szTemp), "ValKitTimings.txt");
        if (RT_SUCCESS(rc))
        {
            rc = RTStrmOpen(szTemp, "w", &pStreamDbg->pFileTiming);
            if (RT_SUCCESS(rc))
            {
                RTStrmPrintf(pStreamDbg->pFileTiming, "# %uHz %uch %ubit\n",
                             PDMAudioPropsHz(&pCfgReq->Props),
                             PDMAudioPropsChannels(&pCfgReq->Props),
                             PDMAudioPropsSampleBits(&pCfgReq->Props));
                return VINF_SUCCESS;
            }

            LogRel(("ValKitAudio: Opening output file '%s' failed: %Rrc\n", szTemp, rc));
        }
        else
            LogRel(("ValKitAudio: Constructing timing file path: %Rrc\n", rc));

        AudioHlpFileDestroy(pStreamDbg->pFile);
        pStreamDbg->pFile = NULL;
    }
    else
        LogRel(("ValKitAudio: Creating output file 'ValKit' in '%s' failed: %Rrc\n", szTemp, rc));

    RTMemFree(pStreamDbg->Out.pbPlayBuffer);
    pStreamDbg->Out.pbPlayBuffer = NULL;
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTVALKITAUDIO pThis       = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = drvHostValKitAudioCreateStreamIn( pThis, pStreamDbg, pCfgReq, pCfgAcq);
    else
        rc = drvHostValKitAudioCreateStreamOut(pThis, pStreamDbg, pCfgReq, pCfgAcq);
    PDMAudioStrmCfgCopy(&pStreamDbg->Cfg, pCfgAcq);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                            bool fImmediate)
{
    RT_NOREF(pInterface, fImmediate);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    if (   pStreamDbg->Cfg.enmDir == PDMAUDIODIR_OUT
        && pStreamDbg->Out.pbPlayBuffer)
    {
        RTMemFree(pStreamDbg->Out.pbPlayBuffer);
        pStreamDbg->Out.pbPlayBuffer = NULL;
    }

    if (pStreamDbg->pFile)
    {
        size_t cbDataSize = AudioHlpFileGetDataSize(pStreamDbg->pFile);
        if (cbDataSize)
            LogRel(("ValKitAudio: Created output file '%s' (%zu bytes)\n", pStreamDbg->pFile->szName, cbDataSize));

        AudioHlpFileDestroy(pStreamDbg->pFile);
        pStreamDbg->pFile = NULL;
    }

    if (pStreamDbg->pFileTiming)
    {
        RTStrmClose(pStreamDbg->pFileTiming);
        pStreamDbg->pFileTiming = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamControlStub(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDisableOrPauseOrDrain(PPDMIHOSTAUDIO pInterface,
                                                                          PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PVALKITAUDIOSTREAM pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    if (pStreamDbg->pFileTiming)
        RTStrmFlush(pStreamDbg->pFileTiming);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                           PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostValKitAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostValKitAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostValKitAudioHA_StreamDisableOrPauseOrDrain(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostValKitAudioHA_StreamDisableOrPauseOrDrain(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostValKitAudioHA_StreamDisableOrPauseOrDrain(pInterface, pStream);

        case PDMAUDIOSTREAMCMD_END:
        case PDMAUDIOSTREAMCMD_32BIT_HACK:
        case PDMAUDIOSTREAMCMD_INVALID:
            /* no default*/
            break;
    }
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostValKitAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostValKitAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostValKitAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                                 PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMHOSTAUDIOSTREAMSTATE_INVALID);
    return PDMHOSTAUDIOSTREAMSTATE_OKAY;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVHOSTVALKITAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    RT_NOREF(pThis);

    uint64_t cNsSinceStart;
    if (pStreamDbg->tsStarted != 0)
        cNsSinceStart = RTTimeNanoTS() - pStreamDbg->tsStarted;
    else
    {
        pStreamDbg->tsStarted = RTTimeNanoTS();
        cNsSinceStart = 0;
    }

    // Microseconds are used everythere below
    uint32_t const cFrames = PDMAudioPropsBytesToFrames(&pStreamDbg->Cfg.Props, cbBuf);
    RTStrmPrintf(pStreamDbg->pFileTiming, "%d %d %d %d\n",
                 // Host time elapsed since Guest submitted the first buffer for playback:
                 (uint32_t)(cNsSinceStart / 1000),
                 // how long all the samples submitted previously were played:
                 (uint32_t)(pStreamDbg->cFramesSinceStarted * 1.0E6 / pStreamDbg->Cfg.Props.uHz),
                 // how long a new uSamplesReady samples should/will be played:
                 (uint32_t)(cFrames * 1.0E6 / pStreamDbg->Cfg.Props.uHz),
                 cFrames);

    pStreamDbg->cFramesSinceStarted += cFrames;

    /* Remember when samples were consumed. */
    // pStreamDbg->Out.tsLastPlayed = PDMDrvHlpTMGetVirtualTime(pThis->pDrvIns);

    int rc2 = AudioHlpFileWrite(pStreamDbg->pFile, pvBuf, cbBuf, 0 /* fFlags */);
    if (RT_FAILURE(rc2))
        LogRel(("ValKitAudio: Writing output failed with %Rrc\n", rc2));

    *pcbWritten = cbBuf;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                            void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pStream);

    PDRVHOSTVALKITAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        pThis->pTestRecCur = RTListGetFirst(&pThis->lstTestsRec, VALKITTESTDATA, Node);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pThis->pTestRecCur == NULL) /* Empty list? */
    {
        *pcbRead = 0;
        return VINF_SUCCESS;
    }

    PVALKITTESTDATA pTst = pThis->pTestRecCur;

    if (pTst->t.TestTone.cbWritten == 0)
    {
        pTst->msStartedTS = RTTimeMilliTS();

        LogRel(("Audio: Validation Kit: Injecting input tone (%RU16Hz, %RU32ms)\n",
                (uint16_t)pTst->t.TestTone.Tone.rdFreqHz,
                pTst->t.TestTone.Parms.msDuration));
    }

    uint32_t cbToWrite = pTst->t.TestTone.cbToWrite - pTst->t.TestTone.cbWritten;
    uint32_t cbRead    = 0;
    if (cbToWrite)
        rc = AudioTestToneGenerate(&pTst->t.TestTone.Tone, pvBuf, RT_MIN(cbToWrite, cbBuf), &cbRead);

    pTst->t.TestTone.cbWritten += cbRead;
    Assert(pTst->t.TestTone.cbWritten <= pTst->t.TestTone.cbToWrite);

    const bool fComplete = pTst->t.TestTone.cbToWrite == pTst->t.TestTone.cbWritten;

    if (fComplete)
    {
        LogRel(("Audio: Validation Kit: Injection done (took %RU32ms)\n",
                RTTimeMilliTS() - pTst->msStartedTS));

        rc = RTCritSectEnter(&pThis->CritSect);
        if (RT_SUCCESS(rc))
        {
            RTListNodeRemove(&pTst->Node);

            RTMemFree(pTst);
            pTst = NULL;

            Assert(pThis->cTestsRec);
            pThis->cTestsRec--;

            int rc2 = RTCritSectLeave(&pThis->CritSect);
            AssertRC(rc2);
        }
    }

    *pcbRead = cbRead;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostValKitAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS         pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTVALKITAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/**
 * Constructs a VaKit audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostValKitAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTVALKITAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);
    LogRel(("Audio: Initializing VALKIT driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostValKitAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig                  = drvHostValKitAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvHostValKitAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostValKitAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostValKitAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostValKitAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostValKitAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostValKitAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostValKitAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostValKitAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostValKitAudioHA_StreamCapture;

    RTListInit(&pThis->lstTestsRec);
    pThis->cTestsRec = 0;

    ATSCALLBACKS Callbacks;
    Callbacks.pfnTonePlay = drvHostValKitAudioSvcTonePlayCallback;
    Callbacks.pvUser      = pThis;

    LogRel(("Audio: Validation Kit: Starting Audio Test Service (ATS) ...\n"));

    int rc = AudioTestSvcInit(&pThis->Srv, &Callbacks);
    if (RT_SUCCESS(rc))
        rc = AudioTestSvcStart(&pThis->Srv);

    if (RT_FAILURE(rc))
    {
        LogRel(("Audio: Validation Kit: Starting Audio Test Service (ATS) failed, rc=%Rrc\n", rc));
    }
    else
        LogRel(("Audio: Validation Kit: Audio Test Service (ATS) running\n"));

    if (RT_SUCCESS(rc))
        rc = RTCritSectInit(&pThis->CritSect);

    return rc;
}

static DECLCALLBACK(void) drvHostValKitAudioDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVHOSTVALKITAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTVALKITAUDIO);

    LogRel(("Audio: Validation Kit: Shutting down Audio Test Service (ATS) ...\n"));

    int rc = AudioTestSvcShutdown(&pThis->Srv);
    if (RT_SUCCESS(rc))
        rc = AudioTestSvcDestroy(&pThis->Srv);

    if (RT_SUCCESS(rc))
    {
        LogRel(("Audio: Validation Kit: Shutdown of Audio Test Service complete\n"));

        PVALKITTESTDATA pData, pDataNext;
        RTListForEachSafe(&pThis->lstTestsRec, pData, pDataNext, VALKITTESTDATA, Node)
        {
            RTListNodeRemove(&pData->Node);

            RTMemFree(pData);

            Assert(pThis->cTestsRec);
            pThis->cTestsRec--;
        }

        Assert(pThis->cTestsRec == 0);
    }
    else
        LogRel(("Audio: Validation Kit: Shutdown of Audio Test Service failed, rc=%Rrc\n", rc));

    rc = RTCritSectDelete(&pThis->CritSect);
    AssertRC(rc);
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostValidationKitAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "ValidationKitAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "ValidationKitAudio audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTVALKITAUDIO),
    /* pfnConstruct */
    drvHostValKitAudioConstruct,
    /* pfnDestruct */
    drvHostValKitAudioDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    NULL,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    /* u32EndVersion */
    PDM_DRVREG_VERSION
};

