/* $Id$ */
/** @file
 * Host audio driver - ValidationKit - For dumping and injecting audio data from/to the device emulation.
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

#include "AudioHlp.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Structure for keeping a validation kit input/output stream.
 */
typedef struct VAKITAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG  pCfg;
    /** Audio file to dump output to or read input from. */
    PAUDIOHLPFILE       pFile;
    /** Text file to store timing of audio buffers submittions. */
    PRTSTREAM           pFileTiming;
    /** Timestamp of the first play or record request. */
    uint64_t            tsStarted;
    /** Total number of frames played or recorded so far. */
    uint32_t            cFramesSinceStarted;
    union
    {
        struct
        {
            /** Timestamp of last captured samples. */
            uint64_t    tsLastCaptured;
        } In;
        struct
        {
            /** Timestamp of last played samples. */
            uint64_t    tsLastPlayed;
            uint8_t    *pbPlayBuffer;
            uint32_t    cbPlayBuffer;
        } Out;
    };
} VAKITAUDIOSTREAM;
/** Pointer to a validation kit stream. */
typedef VAKITAUDIOSTREAM *PVAKITAUDIOSTREAM;

/**
 * Validation kit audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTVAKITAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
} DRVHOSTVAKITAUDIO;
/** Pointer to a validation kit host audio driver instance. */
typedef DRVHOSTVAKITAUDIO *PDRVHOSTVAKITAUDIO;



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
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostValKitAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


static int drvHostValKitAudioCreateStreamIn(PDRVHOSTVAKITAUDIO pThis, PVAKITAUDIOSTREAM pStreamDbg,
                                            PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pThis, pStreamDbg, pCfgReq, pCfgAcq);

    return VINF_SUCCESS;
}


static int drvHostValKitAudioCreateStreamOut(PDRVHOSTVAKITAUDIO pThis, PVAKITAUDIOSTREAM pStreamDbg,
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
    PDRVHOSTVAKITAUDIO pThis       = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = drvHostValKitAudioCreateStreamIn( pThis, pStreamDbg, pCfgReq, pCfgAcq);
    else
        rc = drvHostValKitAudioCreateStreamOut(pThis, pStreamDbg, pCfgReq, pCfgAcq);
    if (RT_SUCCESS(rc))
    {
        pStreamDbg->pCfg = PDMAudioStrmCfgDup(pCfgAcq);
        if (!pStreamDbg->pCfg)
            rc = VERR_NO_MEMORY;
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface); //PDRVHOSTVAKITAUDIO pThis     = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    if (   pStreamDbg->pCfg->enmDir == PDMAUDIODIR_OUT
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

    if (pStreamDbg->pCfg)
    {
        PDMAudioStrmCfgFree(pStreamDbg->pCfg);
        pStreamDbg->pCfg = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                            PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pInterface, enmStreamCmd);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    if (pStreamDbg->pFileTiming)
        RTStrmFlush(pStreamDbg->pFileTiming);

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


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostValKitAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface,
                                                                            PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED | PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostValKitAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    PDRVHOSTVAKITAUDIO pThis      = RT_FROM_MEMBER(pInterface, DRVHOSTVAKITAUDIO, IHostAudio);
    PVAKITAUDIOSTREAM  pStreamDbg = (PVAKITAUDIOSTREAM)pStream;
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
    uint32_t const cFrames = PDMAudioPropsBytesToFrames(&pStreamDbg->pCfg->Props, cbBuf);
    RTStrmPrintf(pStreamDbg->pFileTiming, "%d %d %d %d\n",
                 // Host time elapsed since Guest submitted the first buffer for playback:
                 (uint32_t)(cNsSinceStart / 1000),
                 // how long all the samples submitted previously were played:
                 (uint32_t)(pStreamDbg->cFramesSinceStarted * 1.0E6 / pStreamDbg->pCfg->Props.uHz),
                 // how long a new uSamplesReady samples should/will be played:
                 (uint32_t)(cFrames * 1.0E6 / pStreamDbg->pCfg->Props.uHz),
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
    RT_NOREF(pInterface, pStream, pvBuf, cbBuf);

    /* Never capture anything. */
    *pcbRead = 0;
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
    LogRel(("Audio: Initializing VALKIT driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostValKitAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig          = drvHostValKitAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices         = NULL;
    pThis->IHostAudio.pfnGetStatus          = drvHostValKitAudioHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostValKitAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostValKitAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostValKitAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostValKitAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostValKitAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending   = NULL;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostValKitAudioHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamPlay         = drvHostValKitAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostValKitAudioHA_StreamCapture;

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

