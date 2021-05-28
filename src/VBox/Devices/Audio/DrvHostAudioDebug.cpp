/* $Id$ */
/** @file
 * Host audio driver - Debug - For dumping and injecting audio data from/to the device emulation.
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
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#include "AudioHlp.h"
#include "AudioTest.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Debug host audio stream.
 */
typedef struct DEBUGAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
    /** Audio file to dump output to or read input from. */
    PAUDIOHLPFILE           pFile;
    union
    {
        AUDIOTESTTONE       In;
    };
} DEBUGAUDIOSTREAM;
/** Pointer to a debug host audio stream. */
typedef DEBUGAUDIOSTREAM *PDEBUGAUDIOSTREAM;

/**
 * Debug audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTDEBUGAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS      pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO   IHostAudio;
} DRVHOSTDEBUGAUDIO;
/** Pointer to a debug host audio driver. */
typedef DRVHOSTDEBUGAUDIO *PDRVHOSTDEBUGAUDIO;


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "DebugAudio");
    pBackendCfg->cbStream       = sizeof(DEBUGAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsOut = 1; /* Output; writing to a file. */
    pBackendCfg->cMaxStreamsIn  = 1; /* Input; generates a sine wave. */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostDebugAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(pInterface, enmDir);
    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    PDRVHOSTDEBUGAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTDEBUGAUDIO, IHostAudio);
    PDEBUGAUDIOSTREAM  pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    PDMAudioStrmCfgCopy(&pStreamDbg->Cfg, pCfgAcq);

    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        AudioTestToneInitRandom(&pStreamDbg->In, &pStreamDbg->Cfg.Props);

    int rc = AudioHlpFileCreateAndOpenEx(&pStreamDbg->pFile, AUDIOHLPFILETYPE_WAV, NULL /*use temp dir*/,
                                         pCfgReq->enmDir == PDMAUDIODIR_IN ? "DebugAudioIn" : "DebugAudioOut",
                                         pDrv->pDrvIns->iInstance, AUDIOHLPFILENAME_FLAGS_NONE, AUDIOHLPFILE_FLAGS_NONE,
                                         &pCfgReq->Props, RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE);
    if (RT_FAILURE(rc))
        LogRel(("DebugAudio: Failed to creating debug file for %s stream '%s' in the temp directory: %Rrc\n",
                pCfgReq->enmDir == PDMAUDIODIR_IN ? "input" : "output", pCfgReq->szName, rc));

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           bool fImmediate)
{
    RT_NOREF(pInterface, fImmediate);
    PDEBUGAUDIOSTREAM pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamDbg, VERR_INVALID_POINTER);

    if (pStreamDbg->pFile)
    {
        AudioHlpFileDestroy(pStreamDbg->pFile);
        pStreamDbg->pFile = NULL;
    }

    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamControlStub(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                           PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostDebugAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostDebugAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostDebugAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostDebugAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostDebugAudioHA_StreamControlStub(pInterface, pStream);

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
static DECLCALLBACK(uint32_t) drvHostDebugAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    PDEBUGAUDIOSTREAM pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;

    return PDMAudioPropsMilliToBytes(&pStreamDbg->Cfg.Props, 10 /*ms*/);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostDebugAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetPending}
 */
static DECLCALLBACK(uint32_t) drvHostDebugAudioHA_StreamGetPending(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetState}
 */
static DECLCALLBACK(PDMHOSTAUDIOSTREAMSTATE) drvHostDebugAudioHA_StreamGetState(PPDMIHOSTAUDIO pInterface,
                                                                                PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pStream, PDMHOSTAUDIOSTREAMSTATE_INVALID);
    return PDMHOSTAUDIOSTREAMSTATE_OKAY;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                        const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pInterface);
    PDEBUGAUDIOSTREAM pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;

    int rc = AudioHlpFileWrite(pStreamDbg->pFile, pvBuf, cbBuf, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
        *pcbWritten = cbBuf;
    else
        LogRelMax(32, ("DebugAudio: Writing output failed with %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                           void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface);
    PDEBUGAUDIOSTREAM pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;
/** @todo rate limit this?  */

    uint32_t cbWritten;
    int rc = AudioTestToneGenerate(&pStreamDbg->In, pvBuf, cbBuf, &cbWritten);
    if (RT_SUCCESS(rc))
    {
        /*
         * Write it.
         */
        rc = AudioHlpFileWrite(pStreamDbg->pFile, pvBuf, cbWritten, 0 /* fFlags */);
        if (RT_SUCCESS(rc))
            *pcbRead = cbWritten;
    }

    if (RT_FAILURE(rc))
        LogRelMax(32, ("DebugAudio: Writing input failed with %Rrc\n", rc));

    return rc;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostDebugAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS         pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTDEBUGAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTDEBUGAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/**
 * Constructs a Null audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostDebugAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVHOSTDEBUGAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTDEBUGAUDIO);
    LogRel(("Audio: Initializing DEBUG driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostDebugAudioQueryInterface;
    /* IHostAudio */
    pThis->IHostAudio.pfnGetConfig                  = drvHostDebugAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices                 = NULL;
    pThis->IHostAudio.pfnSetDevice                  = NULL;
    pThis->IHostAudio.pfnGetStatus                  = drvHostDebugAudioHA_GetStatus;
    pThis->IHostAudio.pfnDoOnWorkerThread           = NULL;
    pThis->IHostAudio.pfnStreamConfigHint           = NULL;
    pThis->IHostAudio.pfnStreamCreate               = drvHostDebugAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamInitAsync            = NULL;
    pThis->IHostAudio.pfnStreamDestroy              = drvHostDebugAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamNotifyDeviceChanged  = NULL;
    pThis->IHostAudio.pfnStreamControl              = drvHostDebugAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable          = drvHostDebugAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable          = drvHostDebugAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending           = drvHostDebugAudioHA_StreamGetPending;
    pThis->IHostAudio.pfnStreamGetState             = drvHostDebugAudioHA_StreamGetState;
    pThis->IHostAudio.pfnStreamPlay                 = drvHostDebugAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture              = drvHostDebugAudioHA_StreamCapture;

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH
    RTFileDelete(VBOX_AUDIO_DEBUG_DUMP_PCM_DATA_PATH "AudioDebugOutput.pcm");
#endif

    return VINF_SUCCESS;
}

/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostDebugAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "DebugAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Debug audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTDEBUGAUDIO),
    /* pfnConstruct */
    drvHostDebugAudioConstruct,
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

