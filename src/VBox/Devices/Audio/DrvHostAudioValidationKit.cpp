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



static void drvHostValKiUnregisterTest(PVALKITTESTDATA pTst)
{
    RTListNodeRemove(&pTst->Node);

    AudioTestSetObjClose(pTst->pObj);
    pTst->pObj = NULL;
    AudioTestSetTestDone(pTst->pEntry);
    pTst->pEntry = NULL;

    RTMemFree(pTst);
    pTst = NULL;
}

static void drvHostValKiUnregisterRecTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsRec);
    pThis->cTestsRec--;
}

static void drvHostValKiUnregisterPlayTest(PDRVHOSTVALKITAUDIO pThis, PVALKITTESTDATA pTst)
{
    drvHostValKiUnregisterTest(pTst);

    Assert(pThis->cTestsPlay);
    pThis->cTestsPlay--;
}

/** @copydoc ATSCALLBACKS::pfnTonePlay
 *
 * Creates and registers a new test tone recording test
 * which later then gets recorded by the guest side.
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
 * Creates and registers a new test tone playback test
 * which later then records audio played back by the guest side.
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


static int drvHostValKitAudioCreateStreamIn(PDRVHOSTVALKITAUDIO pThis, PVALKITAUDIOSTREAM pStreamDbg,
                                            PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pThis, pStreamDbg, pCfgReq, pCfgAcq);

    return VINF_SUCCESS;
}


static int drvHostValKitAudioCreateStreamOut(PDRVHOSTVALKITAUDIO pThis, PVALKITAUDIOSTREAM pStreamDbg,
                                             PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pThis, pStreamDbg, pCfgReq, pCfgAcq);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTVALKITAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTVALKITAUDIO, IHostAudio);
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
        LogRelMax(64, ("Audio: Validation Kit: Warning: Guest is playing back data when no playback test is active\n"));
#ifdef DEBUG_andy
        AssertFailed();
#endif
        *pcbWritten = 0;
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
#ifdef DEBUG_andy
        AssertFailed();
#endif
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
    pThis->IHostAudio.pfnStreamControl              = drvHostValKitAudioHA_StreamControl;
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
    Callbacks.pfnTonePlay   = drvHostValKitRegisterGuestRecTest;
    Callbacks.pfnToneRecord = drvHostValKitRegisterGuestPlayTest;
    Callbacks.pvUser        = pThis;

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

