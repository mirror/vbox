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

#include <iprt/rand.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define _USE_MATH_DEFINES
#include <math.h> /* sin, M_PI */

#include "AudioHlp.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Debug host audio stream.
 */
typedef struct DEBUGAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG   Cfg;
    /** Audio file to dump output to or read input from. */
    PAUDIOHLPFILE       pFile;
    union
    {
        struct
        {
            /** Current sample index for generate the sine wave. */
            uint64_t    uSample;
            /** The fixed portion of the sin() input. */
            double      rdFixed;
            /** Timestamp of last captured samples. */
            uint64_t    tsLastCaptured;
            /** Frequency (in Hz) of the sine wave to generate. */
            double      rdFreqHz;
        } In;
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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Frequency selection for input streams. */
static const double s_ardInputFreqsHz[] =
{
     349.2282 /*F4*/,
     440.0000 /*A4*/,
     523.2511 /*C5*/,
     698.4565 /*F5*/,
     880.0000 /*A5*/,
    1046.502  /*C6*/,
    1174.659  /*D6*/,
    1396.913  /*F6*/,
    1760.0000 /*A6*/
};


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "DebugAudio");

    pBackendCfg->cbStreamOut    = sizeof(DEBUGAUDIOSTREAM);
    pBackendCfg->cbStreamIn     = sizeof(DEBUGAUDIOSTREAM);

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
    {
        /* Pick a frequency from our selection, so that every time a recording starts
           we'll hopfully generate a different note. */
        pStreamDbg->In.rdFreqHz = s_ardInputFreqsHz[RTRandU32Ex(0, RT_ELEMENTS(s_ardInputFreqsHz) - 1)];
        pStreamDbg->In.rdFixed  = 2.0 * M_PI * pStreamDbg->In.rdFreqHz / PDMAudioPropsHz(&pStreamDbg->Cfg.Props);
        pStreamDbg->In.uSample  = 0;
    }

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
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface);
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
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostDebugAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                           PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pInterface, pStream, enmStreamCmd);
    return VINF_SUCCESS;
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
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(PDMAUDIOSTREAMSTS) drvHostDebugAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return PDMAUDIOSTREAMSTS_FLAGS_INITIALIZED | PDMAUDIOSTREAMSTS_FLAGS_ENABLED;
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

    /*
     * Clear the buffer first so we don't need to thing about additional channels.
     */
    uint32_t cFrames = PDMAudioPropsBytesToFrames(&pStreamDbg->Cfg.Props, cbBuf);
    PDMAudioPropsClearBuffer(&pStreamDbg->Cfg.Props, pvBuf, cbBuf, cFrames);

    /*
     * Generate the select sin wave in the first channel:
     */
    uint32_t const cbFrame   = PDMAudioPropsFrameSize(&pStreamDbg->Cfg.Props);
    double const   rdFixed   = pStreamDbg->In.rdFixed;
    uint64_t       iSrcFrame = pStreamDbg->In.uSample;
    switch (PDMAudioPropsSampleSize(&pStreamDbg->Cfg.Props))
    {
        case 1:
            /* untested */
            if (PDMAudioPropsIsSigned(&pStreamDbg->Cfg.Props))
            {
                int8_t *piSample = (int8_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = 126 /*Amplitude*/ * sin(rdFixed * iSrcFrame);
                    iSrcFrame++;
                    piSample += cbFrame;
                }
            }
            else
            {
                /* untested */
                uint16_t *pbSample = (uint16_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *pbSample = 126 /*Amplitude*/ * sin(rdFixed * iSrcFrame) + 0x80;
                    iSrcFrame++;
                    pbSample += cbFrame;
                }
            }
            break;

        case 2:
            if (PDMAudioPropsIsSigned(&pStreamDbg->Cfg.Props))
            {
                int16_t *piSample = (int16_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = 32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame);
                    iSrcFrame++;
                    piSample = (int16_t *)((uint8_t *)piSample + cbFrame);
                }
            }
            else
            {
                /* untested */
                uint16_t *puSample = (uint16_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *puSample = 32760 /*Amplitude*/ * sin(rdFixed * iSrcFrame) + 0x8000;
                    iSrcFrame++;
                    puSample = (uint16_t *)((uint8_t *)puSample + cbFrame);
                }
            }
            break;

        case 4:
            /* untested */
            if (PDMAudioPropsIsSigned(&pStreamDbg->Cfg.Props))
            {
                int32_t *piSample = (int32_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *piSample = (32760 << 16) /*Amplitude*/ * sin(rdFixed * iSrcFrame);
                    iSrcFrame++;
                    piSample = (int32_t *)((uint8_t *)piSample + cbFrame);
                }
            }
            else
            {
                uint32_t *puSample = (uint32_t *)pvBuf;
                while (cFrames-- > 0)
                {
                    *puSample = (32760 << 16) /*Amplitude*/ * sin(rdFixed * iSrcFrame) + UINT32_C(0x80000000);
                    iSrcFrame++;
                    puSample = (uint32_t *)((uint8_t *)puSample + cbFrame);
                }
            }
            break;

        default:
            AssertFailed();
    }
    pStreamDbg->In.uSample = iSrcFrame;

    /*
     * Write it.
     */
    int rc = AudioHlpFileWrite(pStreamDbg->pFile, pvBuf, cbBuf, 0 /* fFlags */);
    if (RT_SUCCESS(rc))
        *pcbRead = cbBuf;
    else
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
    pThis->IHostAudio.pfnGetConfig          = drvHostDebugAudioHA_GetConfig;
    pThis->IHostAudio.pfnGetDevices         = NULL;
    pThis->IHostAudio.pfnGetStatus          = drvHostDebugAudioHA_GetStatus;
    pThis->IHostAudio.pfnStreamCreate       = drvHostDebugAudioHA_StreamCreate;
    pThis->IHostAudio.pfnStreamDestroy      = drvHostDebugAudioHA_StreamDestroy;
    pThis->IHostAudio.pfnStreamControl      = drvHostDebugAudioHA_StreamControl;
    pThis->IHostAudio.pfnStreamGetReadable  = drvHostDebugAudioHA_StreamGetReadable;
    pThis->IHostAudio.pfnStreamGetWritable  = drvHostDebugAudioHA_StreamGetWritable;
    pThis->IHostAudio.pfnStreamGetPending   = drvHostDebugAudioHA_StreamGetPending;
    pThis->IHostAudio.pfnStreamGetStatus    = drvHostDebugAudioHA_StreamGetStatus;
    pThis->IHostAudio.pfnStreamPlay         = drvHostDebugAudioHA_StreamPlay;
    pThis->IHostAudio.pfnStreamCapture      = drvHostDebugAudioHA_StreamCapture;

#ifdef VBOX_AUDIO_DEBUG_DUMP_PCM_DATA
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

