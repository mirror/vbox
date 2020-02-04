/* $Id$ */
/** @file
 * ValidationKit audio driver - host backend for dumping and injecting audio data
 *                              from/to the device emulation.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"
#include "VBoxDD.h"


/**
 * Structure for keeping a VAKIT input/output stream.
 */
typedef struct VAKITAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG pCfg;
    /** Audio file to dump output to or read input from. */
    PPDMAUDIOFILE      pFile;
    /** Text file to store timing of audio buffers submittions**/
    RTFILE             hFileTiming;
    /** Timestamp of the first play or record request**/
    uint64_t           tsStarted;
    /** Total number of samples played or recorded so far**/
    uint32_t           uSamplesSinceStarted;
    union
    {
        struct
        {
            /** Timestamp of last captured samples. */
            uint64_t   tsLastCaptured;
        } In;
        struct
        {
            /** Timestamp of last played samples. */
            uint64_t   tsLastPlayed;
            uint8_t   *pu8PlayBuffer;
            uint32_t   cbPlayBuffer;
        } Out;
    };
} VAKITAUDIOSTREAM, *PVAKITAUDIOSTREAM;

/**
 * VAKIT audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTVAKITAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS    pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO IHostAudio;
} DRVHOSTVAKITAUDIO, *PDRVHOSTVAKITAUDIO;

/*******************************************PDM_AUDIO_DRIVER******************************/


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RTStrPrintf2(pBackendCfg->szName, sizeof(pBackendCfg->szName), "Validation Kit");

    pBackendCfg->cbStreamOut    = sizeof(VAKITAUDIOSTREAM);
    pBackendCfg->cbStreamIn     = sizeof(VAKITAUDIOSTREAM);

    pBackendCfg->cMaxStreamsOut = 1; /* Output */
    pBackendCfg->cMaxStreamsIn  = 0; /* No input supported yet. */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_Init(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostValKitAudioHA_Shutdown(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
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


static int drvHostValKitAudioCreateStreamIn(PDRVHOSTVAKITAUDIO pDrv, PVAKITAUDIOSTREAM pStreamDbg,
                                            PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pDrv, pStreamDbg, pCfgReq, pCfgAcq);

    return VINF_SUCCESS;
}


static int drvHostValKitAudioCreateStreamOut(PDRVHOSTVAKITAUDIO pDrv, PVAKITAUDIOSTREAM pStreamDbg,
                                             PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pDrv, pCfgAcq);

    pStreamDbg->tsStarted = 0;
    pStreamDbg->uSamplesSinceStarted = 0;
    pStreamDbg->Out.tsLastPlayed  = 0;
    pStreamDbg->Out.cbPlayBuffer  = DrvAudioHlpFramesToBytes(pCfgReq->Backend.cFramesBufferSize, &pCfgReq->Props);
    pStreamDbg->Out.pu8PlayBuffer = (uint8_t *)RTMemAlloc(pStreamDbg->Out.cbPlayBuffer);
    AssertReturn(pStreamDbg->Out.pu8PlayBuffer, VERR_NO_MEMORY);

    char szTemp[RTPATH_MAX];
    int rc = RTPathTemp(szTemp, sizeof(szTemp));
    if (RT_SUCCESS(rc))
        rc = RTPathAppend(szTemp, sizeof(szTemp), "VBoxTestTmp\\VBoxAudioValKit");
    if (RT_SUCCESS(rc))
    {
        char szFile[RTPATH_MAX];
        rc = DrvAudioHlpFileNameGet(szFile, sizeof(szFile), szTemp, "VaKit",
                                    0 /* Instance */, PDMAUDIOFILETYPE_WAV, PDMAUDIOFILENAME_FLAGS_NONE);
        if (RT_SUCCESS(rc))
        {
            rc = DrvAudioHlpFileCreate(PDMAUDIOFILETYPE_WAV, szFile, PDMAUDIOFILE_FLAGS_NONE, &pStreamDbg->pFile);
            if (RT_SUCCESS(rc))
                rc = DrvAudioHlpFileOpen(pStreamDbg->pFile, PDMAUDIOFILE_DEFAULT_OPEN_FLAGS, &pCfgReq->Props);
        }

        if (RT_FAILURE(rc))
            LogRel(("VaKitAudio: Creating output file '%s' failed with %Rrc\n", szFile, rc));
        else
        {
            size_t cch;
            char szTimingInfo[128];
            cch = RTStrPrintf(szTimingInfo, sizeof(szTimingInfo), "# %uHz %uch %ubit\n",
                              pCfgReq->Props.uHz, pCfgReq->Props.cChannels, pCfgReq->Props.cbSample * 8);

            RTFileWrite(pStreamDbg->hFileTiming, szTimingInfo, cch, NULL);
        }
    }
    else
        LogRel(("VaKitAudio: Unable to retrieve temp dir: %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTVAKITAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = drvHostValKitAudioCreateStreamIn( pDrv, pStreamDbg, pCfgReq, pCfgAcq);
    else
        rc = drvHostValKitAudioCreateStreamOut(pDrv, pStreamDbg, pCfgReq, pCfgAcq);

    if (RT_SUCCESS(rc))
    {
        pStreamDbg->pCfg = DrvAudioHlpStreamCfgDup(pCfgAcq);
        if (!pStreamDbg->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         const void *pvBuf, uint32_t uBufSize, uint32_t *puWritten)
{
    PDRVHOSTVAKITAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;
    RT_NOREF(pDrv);

    uint64_t tsSinceStart;
    size_t cch;
    char szTimingInfo[128];

    if (pStreamDbg->tsStarted == 0)
    {
        pStreamDbg->tsStarted = RTTimeNanoTS();
        tsSinceStart = 0;
    }
    else
    {
        tsSinceStart = RTTimeNanoTS() - pStreamDbg->tsStarted;
    }

    // Microseconds are used everythere below
    uint32_t sBuf = uBufSize >> pStreamDbg->pCfg->Props.cShift;
    cch = RTStrPrintf(szTimingInfo, sizeof(szTimingInfo), "%d %d %d %d\n",
        (uint32_t)(tsSinceStart / 1000), // Host time elapsed since Guest submitted the first buffer for playback
        (uint32_t)(pStreamDbg->uSamplesSinceStarted * 1.0E6 / pStreamDbg->pCfg->Props.uHz), // how long all the samples submitted previously were played
        (uint32_t)(sBuf * 1.0E6 / pStreamDbg->pCfg->Props.uHz), // how long a new uSamplesReady samples should\will be played
        sBuf);
    RTFileWrite(pStreamDbg->hFileTiming, szTimingInfo, cch, NULL);
    pStreamDbg->uSamplesSinceStarted += sBuf;

    /* Remember when samples were consumed. */
   // pStreamDbg->Out.tsLastPlayed = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);;

    int rc2 = DrvAudioHlpFileWrite(pStreamDbg->pFile, pvBuf, uBufSize, 0 /* fFlags */);
    if (RT_FAILURE(rc2))
        LogRel(("VaKitAudio: Writing output failed with %Rrc\n", rc2));

    *puWritten = uBufSize;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                            void *pvBuf, uint32_t uBufSize, uint32_t *puRead)
{
    RT_NOREF(pInterface, pStream, pvBuf, uBufSize);

    /* Never capture anything. */
    if (puRead)
        *puRead = 0;

    return VINF_SUCCESS;
}


static int vakitDestroyStreamIn(PDRVHOSTVAKITAUDIO pDrv, PVAKITAUDIOSTREAM pStreamDbg)
{
    RT_NOREF(pDrv, pStreamDbg);
    return VINF_SUCCESS;
}


static int vakitDestroyStreamOut(PDRVHOSTVAKITAUDIO pDrv, PVAKITAUDIOSTREAM pStreamDbg)
{
    RT_NOREF(pDrv);

    if (pStreamDbg->Out.pu8PlayBuffer)
    {
        RTMemFree(pStreamDbg->Out.pu8PlayBuffer);
        pStreamDbg->Out.pu8PlayBuffer = NULL;
    }

    if (pStreamDbg->pFile)
    {
        size_t cbDataSize = DrvAudioHlpFileGetDataSize(pStreamDbg->pFile);
        if (cbDataSize)
            LogRel(("VaKitAudio: Created output file '%s' (%zu bytes)\n", pStreamDbg->pFile->szName, cbDataSize));

        DrvAudioHlpFileDestroy(pStreamDbg->pFile);
        pStreamDbg->pFile = NULL;
    }

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    PDRVHOSTVAKITAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;

    if (!pStreamDbg->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamDbg->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = vakitDestroyStreamIn (pDrv, pStreamDbg);
    else
        rc = vakitDestroyStreamOut(pDrv, pStreamDbg);

    if (RT_SUCCESS(rc))
    {
        DrvAudioHlpStreamCfgFree(pStreamDbg->pCfg);
        pStreamDbg->pCfg = NULL;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostValKitAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                            PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(enmStreamCmd);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    return VINF_SUCCESS;
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

static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostValKitAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface,
                                                                            PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);

    return PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED | PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
}

static DECLCALLBACK(int) drvHostValKitAudioHA_StreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostValKitAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS         pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTVAKITAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTVAKITAUDIO);

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
    PDRVHOSTVAKITAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTVAKITAUDIO);
    LogRel(("Audio: Initializing VAKIT driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostValKitAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnInit               = drvHostValKitAudioHA_Init;
    pThis->IHostAudio.pfnShutdown           = drvHostValKitAudioHA_Shutdown;
    pThis->IHostAudio.pfnGetConfig          = drvHostValKitAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetStatus          = drvHostValKitAudioHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostValKitAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostValKitAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostValKitAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostValKitAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostValKitAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostValKitAudioHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamIterate      = drvHostValKitAudioHA_StreamIterate;
    pThis->IHostAudio.pfnStreamPlay         = drvHostValKitAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostValKitAudioHA_StreamCapture;
    pThis->IHostAudio.pfnSetCallback        = NULL;
    pThis->IHostAudio.pfnGetDevices         = NULL;
    pThis->IHostAudio.pfnStreamGetPending   = NULL;
    pThis->IHostAudio.pfnStreamPlayBegin    = NULL;
    pThis->IHostAudio.pfnStreamPlayEnd      = NULL;
    pThis->IHostAudio.pfnStreamCaptureBegin = NULL;
    pThis->IHostAudio.pfnStreamCaptureEnd   = NULL;

    return VINF_SUCCESS;
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
    sizeof(DRVHOSTVAKITAUDIO),
    /* pfnConstruct */
    drvHostValKitAudioConstruct,
    /* pfnDestruct */
    NULL,
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

