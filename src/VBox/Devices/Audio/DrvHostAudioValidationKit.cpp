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
} VALKITAUDIOSTREAM;
/** Pointer to a Validation Kit stream. */
typedef VALKITAUDIOSTREAM *PVALKITAUDIOSTREAM;

/**
 * Test tone-specific instance data.
 */
typedef struct VALKITTESTTONEDATA
{
    union
    {
        struct
        {
            /** How many bytes to write. */
            uint64_t           cbToWrite;
            /** How many bytes already written. */
            uint64_t           cbWritten;
        } Rec;
        struct
        {
            /** How many bytes to read. */
            uint64_t           cbToRead;
            /** How many bytes already read. */
            uint64_t           cbRead;
        } Play;
    } u;
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
    /** Current test set entry to process. */
    PAUDIOTESTENTRY        pEntry;
    /** Current test object to process. */
    PAUDIOTESTOBJ          pObj;
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
    /** Temporary path to use. */
    char                szPathTemp[RTPATH_MAX];
    /** Output path to use. */
    char                szPathOut[RTPATH_MAX];
    /** Current test set being handled. */
    AUDIOTESTSET        Set;
    /** Number of tests in \a lstTestsRec. */
    uint32_t            cTestsRec;
    /** List keeping the recording tests (FIFO). */
    RTLISTANCHOR        lstTestsRec;
    /** Number of tests in \a lstTestsPlay. */
    uint32_t            cTestsPlay;
    /** List keeping the recording tests (FIFO). */
    RTLISTANCHOR        lstTestsPlay;
    /** Pointer to current test being processed. */
    PVALKITTESTDATA     pTestCur;
    /** Critical section for serializing access across threads. */
    RTCRITSECT          CritSect;
    /** The Audio Test Service (ATS) instance. */
    ATSSERVER           Srv;
} DRVHOSTVALKITAUDIO;
/** Pointer to a Validation Kit host audio driver instance. */
typedef DRVHOSTVALKITAUDIO *PDRVHOSTVALKITAUDIO;


/**
 * Unregisters a ValKit test, common code.
 *
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterTest(PVALKITTESTDATA pTst)
{
    AssertPtrReturnVoid(pTst);

    RTListNodeRemove(&pTst->Node);

    AudioTestSetObjClose(pTst->pObj);
    pTst->pObj = NULL;

    if (pTst->pEntry) /* Set set entry assign? Mark as done. */
    {
        AssertPtrReturnVoid(pTst->pEntry);
        AudioTestSetTestDone(pTst->pEntry);
        pTst->pEntry = NULL;
    }

    RTMemFree(pTst);
    pTst = NULL;
}

/**
 * Unregisters a ValKit recording test.
 *
 * @param   pThis               ValKit audio driver instance.
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterRecTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsRec);
    pThis->cTestsRec--;
}

/**
 * Unregisters a ValKit playback test.
 *
 * @param   pThis               ValKit audio driver instance.
 * @param   pTst                Test to unregister.
 *                              The pointer will be invalid afterwards.
 */
static void drvHostValKiUnregisterPlayTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsPlay);
    pThis->cTestsPlay--;
}

static DECLCALLBACK(int) drvHostValKitTestSetBegin(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    LogRel(("Audio: Validation Kit: Beginning test set '%s'\n", pszTag));

    return AudioTestSetCreate(&pThis->Set, pThis->szPathTemp, pszTag);

}

static DECLCALLBACK(int) drvHostValKitTestSetEnd(void const *pvUser, const char *pszTag)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    const PAUDIOTESTSET pSet  = &pThis->Set;

    LogRel(("Audio: Validation Kit: Ending test set '%s'\n", pszTag));

    /* Close the test set first. */
    AudioTestSetClose(pSet);

    /* Before destroying the test environment, pack up the test set so
     * that it's ready for transmission. */
    char szFileOut[RTPATH_MAX];
    int rc = AudioTestSetPack(pSet, pThis->szPathOut, szFileOut, sizeof(szFileOut));
    if (RT_SUCCESS(rc))
        LogRel(("Audio: Validation Kit: Packed up to '%s'\n", szFileOut));

    int rc2 = AudioTestSetWipe(pSet);
    if (RT_SUCCESS(rc))
        rc = rc2;

    AudioTestSetDestroy(pSet);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Validation Kit: Test set prologue failed with %Rrc\n", rc));

    return rc;
}

/** @copydoc ATSCALLBACKS::pfnTonePlay
 *
 * Creates and registers a new test tone guest recording test.
 */
static DECLCALLBACK(int) drvHostValKitRegisterGuestRecTest(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    PVALKITTESTDATA pTestData = (PVALKITTESTDATA)RTMemAllocZ(sizeof(VALKITTESTDATA));
    AssertPtrReturn(pTestData, VERR_NO_MEMORY);

    memcpy(&pTestData->t.TestTone.Parms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    AudioTestToneInit(&pTestData->t.TestTone.Tone, &pToneParms->Props, pTestData->t.TestTone.Parms.dbFreqHz);

    pTestData->t.TestTone.u.Rec.cbToWrite = PDMAudioPropsMilliToBytes(&pToneParms->Props,
                                                                      pTestData->t.TestTone.Parms.msDuration);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogRel(("Audio: Validation Kit: Registered guest recording test (%RU32ms, %RU64 bytes)\n",
                pTestData->t.TestTone.Parms.msDuration, pTestData->t.TestTone.u.Rec.cbToWrite));

        RTListAppend(&pThis->lstTestsRec, &pTestData->Node);

        pThis->cTestsRec++;

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    return VINF_SUCCESS;
}

/** @copydoc ATSCALLBACKS::pfnToneRecord
 *
 * Creates and registers a new test tone guest playback test.
 */
static DECLCALLBACK(int) drvHostValKitRegisterGuestPlayTest(void const *pvUser, PAUDIOTESTTONEPARMS pToneParms)
{
    PDRVHOSTVALKITAUDIO pThis = (PDRVHOSTVALKITAUDIO)pvUser;

    PVALKITTESTDATA pTestData = (PVALKITTESTDATA)RTMemAllocZ(sizeof(VALKITTESTDATA));
    AssertPtrReturn(pTestData, VERR_NO_MEMORY);

    memcpy(&pTestData->t.TestTone.Parms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    pTestData->t.TestTone.u.Rec.cbToWrite = PDMAudioPropsMilliToBytes(&pToneParms->Props,
                                                                      pTestData->t.TestTone.Parms.msDuration);
    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        LogRel(("Audio: Validation Kit: Registered guest playback test (%RU32ms, %RU64 bytes)\n",
                pTestData->t.TestTone.Parms.msDuration, pTestData->t.TestTone.u.Rec.cbToWrite));

        RTListAppend(&pThis->lstTestsPlay, &pTestData->Node);

        pThis->cTestsPlay++;

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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           PCPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTVALKITAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
    PVALKITAUDIOSTREAM  pStreamDbg = (PVALKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);
    RT_NOREF(pThis);

    int rc = VINF_SUCCESS;
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

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamEnable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDisable}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDisable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPause}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPause(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamResume}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamResume(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDrain}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDrain(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetReadable}
 */
static DECLCALLBACK(uint32_t) drvHostValKitAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pStream);

    PDRVHOSTVALKITAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        pThis->pTestCur = RTListGetFirst(&pThis->lstTestsPlay, VALKITTESTDATA, Node);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pThis->pTestCur == NULL) /* Empty list? */
        return 0;

    PVALKITTESTDATA const pTst = pThis->pTestCur;

    Assert(pTst->t.TestTone.u.Rec.cbToWrite >= pTst->t.TestTone.u.Rec.cbWritten);
    return pTst->t.TestTone.u.Rec.cbToWrite - pTst->t.TestTone.u.Rec.cbWritten;
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
    RT_NOREF(pStream);

    PDRVHOSTVALKITAUDIO pThis = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);

    int rc = RTCritSectEnter(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        pThis->pTestCur = RTListGetFirst(&pThis->lstTestsPlay, VALKITTESTDATA, Node);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pThis->pTestCur == NULL) /* Empty list? */
    {
        //LogRelMax(64, ("Audio: Validation Kit: Warning: Guest is playing back data when no playback test is active\n"));

        *pcbWritten = cbBuf; /* Report all data as being written. */
        return VINF_SUCCESS;
    }

    PVALKITTESTDATA pTst = pThis->pTestCur;

    if (pTst->t.TestTone.u.Play.cbRead == 0)
    {
        AUDIOTESTPARMS Parms;
        RT_ZERO(Parms);
        Parms.TestTone = pTst->t.TestTone.Parms;

        Assert(pTst->pEntry == NULL);
        rc = AudioTestSetTestBegin(&pThis->Set, "Recording audio data from guest",
                                    &Parms, &pTst->pEntry);
        if (RT_SUCCESS(rc))
            rc = AudioTestSetObjCreateAndRegister(&pThis->Set, "guest-output.pcm", &pTst->pObj);

        if (RT_SUCCESS(rc))
        {
            pTst->msStartedTS = RTTimeMilliTS();
            LogRel(("Audio: Validation Kit: Recording audio data (%RU16Hz, %RU32ms) started\n",
                    (uint16_t)pTst->t.TestTone.Tone.rdFreqHz,
                    pTst->t.TestTone.Parms.msDuration));
        }
    }

    uint32_t cbWritten = 0;

    if (RT_SUCCESS(rc))
    {
        uint32_t cbToRead = RT_MIN(cbBuf,
                                   pTst->t.TestTone.u.Play.cbToRead- pTst->t.TestTone.u.Play.cbRead);

        rc = AudioTestSetObjWrite(pTst->pObj, pvBuf, cbToRead);
        if (RT_SUCCESS(rc))
        {
            pTst->t.TestTone.u.Play.cbRead += cbToRead;

            Assert(pTst->t.TestTone.u.Play.cbRead <= pTst->t.TestTone.u.Play.cbToRead);

            const bool fComplete = pTst->t.TestTone.u.Play.cbToRead == pTst->t.TestTone.u.Play.cbRead;

            if (fComplete)
            {
                LogRel(("Audio: Validation Kit: Recording audio data done (took %RU32ms)\n",
                        RTTimeMilliTS() - pTst->msStartedTS));

                rc = RTCritSectEnter(&pThis->CritSect);
                if (RT_SUCCESS(rc))
                {
                    drvHostValKiUnregisterPlayTest(pThis, pTst);

                    int rc2 = RTCritSectLeave(&pThis->CritSect);
                    AssertRC(rc2);
                }
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTst->pEntry)
            AudioTestSetTestFailed(pTst->pEntry, rc, "Recording audio data failed");
        LogRel(("Audio: Validation Kit: Recording audio data failed with %Rrc\n", rc));
    }

    *pcbWritten = cbWritten;

    return VINF_SUCCESS; /** @todo Return rc here? */
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
        pThis->pTestCur = RTListGetFirst(&pThis->lstTestsRec, VALKITTESTDATA, Node);

        int rc2 = RTCritSectLeave(&pThis->CritSect);
        AssertRC(rc2);
    }

    if (pThis->pTestCur == NULL) /* Empty list? */
    {
        LogRelMax(64, ("Audio: Validation Kit: Warning: Guest is recording audio data when no recording test is active\n"));

        *pcbRead = 0;
        return VINF_SUCCESS;
    }

    PVALKITTESTDATA pTst = pThis->pTestCur;

    if (pTst->t.TestTone.u.Rec.cbWritten == 0)
    {
        AUDIOTESTPARMS Parms;
        RT_ZERO(Parms);
        Parms.TestTone = pTst->t.TestTone.Parms;

        Assert(pTst->pEntry == NULL);
        rc = AudioTestSetTestBegin(&pThis->Set, "Injecting audio input data to guest",
                                    &Parms, &pTst->pEntry);
        if (RT_SUCCESS(rc))
            rc = AudioTestSetObjCreateAndRegister(&pThis->Set, "host-input.pcm", &pTst->pObj);

        if (RT_SUCCESS(rc))
        {
            pTst->msStartedTS = RTTimeMilliTS();
            LogRel(("Audio: Validation Kit: Injecting audio input data (%RU16Hz, %RU32ms) started\n",
                    (uint16_t)pTst->t.TestTone.Tone.rdFreqHz,
                    pTst->t.TestTone.Parms.msDuration));
        }
    }

    uint32_t cbRead = 0;

    if (RT_SUCCESS(rc))
    {
        uint32_t cbToWrite = pTst->t.TestTone.u.Rec.cbToWrite - pTst->t.TestTone.u.Rec.cbWritten;
        if (cbToWrite)
            rc = AudioTestToneGenerate(&pTst->t.TestTone.Tone, pvBuf, RT_MIN(cbToWrite, cbBuf), &cbRead);
        if (   RT_SUCCESS(rc)
            && cbToWrite)
        {
            rc = AudioTestSetObjWrite(pTst->pObj, pvBuf, cbToWrite);
            if (RT_SUCCESS(rc))
            {
                pTst->t.TestTone.u.Rec.cbWritten += cbRead;
                Assert(pTst->t.TestTone.u.Rec.cbWritten <= pTst->t.TestTone.u.Rec.cbToWrite);

                const bool fComplete = pTst->t.TestTone.u.Rec.cbToWrite == pTst->t.TestTone.u.Rec.cbWritten;

                if (fComplete)
                {
                    LogRel(("Audio: Validation Kit: Injecting audio input data done (took %RU32ms)\n",
                            RTTimeMilliTS() - pTst->msStartedTS));

                    rc = RTCritSectEnter(&pThis->CritSect);
                    if (RT_SUCCESS(rc))
                    {
                        drvHostValKiUnregisterRecTest(pThis, pTst);

                        int rc2 = RTCritSectLeave(&pThis->CritSect);
                        AssertRC(rc2);
                    }
                }
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTst->pEntry)
            AudioTestSetTestFailed(pTst->pEntry, rc, "Injecting audio input data failed");
        LogRel(("Audio: Validation Kit: Injecting audio input data failed with %Rrc\n", rc));
    }

    *pcbRead = cbRead;

    return VINF_SUCCESS; /** @todo Return rc here? */
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
    pThis->IHostAudio.pfnStreamEnable               = drvHostValKitAudioHA_StreamEnable;
    pThis->IHostAudio.pfnStreamDisable              = drvHostValKitAudioHA_StreamDisable;
    pThis->IHostAudio.pfnStreamPause                = drvHostValKitAudioHA_StreamPause;
    pThis->IHostAudio.pfnStreamResume               = drvHostValKitAudioHA_StreamResume;
    pThis->IHostAudio.pfnStreamDrain                = drvHostValKitAudioHA_StreamDrain;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostValKitAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostValKitAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = NULL;
    pThis->IHostAudio.pfnStreamGetState             = drvHostValKitAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostValKitAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostValKitAudioHA_StreamCapture;

    RTListInit(&pThis->lstTestsRec);
    pThis->cTestsRec  = 0;
    RTListInit(&pThis->lstTestsPlay);
    pThis->cTestsPlay = 0;

    ATSCALLBACKS Callbacks;
    Callbacks.pfnTestSetBegin = drvHostValKitTestSetBegin;
    Callbacks.pfnTestSetEnd   = drvHostValKitTestSetEnd;
    Callbacks.pfnTonePlay     = drvHostValKitRegisterGuestRecTest;
    Callbacks.pfnToneRecord   = drvHostValKitRegisterGuestPlayTest;
    Callbacks.pvUser          = pThis;

    /** @todo Make this configurable via CFGM. */
    const char *pszTcpAddr = "127.0.0.1";
       uint32_t uTcpPort   = ATS_TCP_DEFAULT_PORT;

    LogRel(("Audio: Validation Kit: Starting Audio Test Service (ATS) at %s:%RU32...\n",
            pszTcpAddr, uTcpPort));

    int rc = AudioTestSvcInit(&pThis->Srv,
                              /* We only allow connections from localhost for now. */
                              pszTcpAddr, uTcpPort, &Callbacks);
    if (RT_SUCCESS(rc))
        rc = AudioTestSvcStart(&pThis->Srv);

    if (RT_SUCCESS(rc))
    {
        LogRel(("Audio: Validation Kit: Audio Test Service (ATS) running\n"));

        /** @todo Let the following be customizable by CFGM later. */
        rc = AudioTestPathCreateTemp(pThis->szPathTemp, sizeof(pThis->szPathTemp), "ValKitAudio");
        if (RT_SUCCESS(rc))
        {
            LogRel(("Audio: Validation Kit: Using temp dir '%s'\n", pThis->szPathTemp));
            rc = AudioTestPathGetTemp(pThis->szPathOut, sizeof(pThis->szPathOut));
            if (RT_SUCCESS(rc))
                LogRel(("Audio: Validation Kit: Using output dir '%s'\n", pThis->szPathOut));
        }
    }

    if (RT_SUCCESS(rc))
        rc = RTCritSectInit(&pThis->CritSect);

    if (RT_FAILURE(rc))
        LogRel(("Audio: Validation Kit: Initialization failed, rc=%Rrc\n", rc));

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

        if (pThis->cTestsRec)
            LogRel(("Audio: Validation Kit: Warning: %RU32 guest recording tests still outstanding\n", pThis->cTestsRec));
        if (pThis->cTestsPlay)
            LogRel(("Audio: Validation Kit: Warning: %RU32 guest playback tests still outstanding\n", pThis->cTestsPlay));

        PVALKITTESTDATA pTst, pTstNext;
        RTListForEachSafe(&pThis->lstTestsRec, pTst, pTstNext, VALKITTESTDATA, Node)
            drvHostValKiUnregisterRecTest(pThis, pTst);

        RTListForEachSafe(&pThis->lstTestsPlay, pTst, pTstNext, VALKITTESTDATA, Node)
            drvHostValKiUnregisterPlayTest(pThis, pTst);

        Assert(pThis->cTestsRec == 0);
        Assert(pThis->cTestsPlay == 0);
    }
    else
        LogRel(("Audio: Validation Kit: Shutdown of Audio Test Service failed, rc=%Rrc\n", rc));

    int rc2 = RTCritSectDelete(&pThis->CritSect);
    if (RT_SUCCESS(rc))
        rc = rc2;

    if (RT_FAILURE(rc))
        LogRel(("Audio: Validation Kit: Destruction failed, rc=%Rrc\n", rc));
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

