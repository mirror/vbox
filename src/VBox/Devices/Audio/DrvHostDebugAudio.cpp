/* $Id$ */
/** @file
 * Debug audio driver -- host backend for dumping and injecting audio data
 * from/to the device emulation.
 */

/*
 * Copyright (C) 2016-2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 */

#include <iprt/alloc.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"
#include "VBoxDD.h"


/**
 * Structure for keeping a debug input/output stream.
 */
typedef struct DEBUGAUDIOSTREAM
{
    /** The stream's acquired configuration. */
    PPDMAUDIOSTREAMCFG pCfg;
    /** Audio file to dump output to or read input from. */
    PDMAUDIOFILE       File;
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
} DEBUGAUDIOSTREAM, *PDEBUGAUDIOSTREAM;

/**
 * Debug audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTDEBUGAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS    pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO IHostAudio;
} DRVHOSTDEBUGAUDIO, *PDRVHOSTDEBUGAUDIO;

/*******************************************PDM_AUDIO_DRIVER******************************/


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostDebugAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    RT_NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    pBackendCfg->cbStreamOut    = sizeof(DEBUGAUDIOSTREAM);
    pBackendCfg->cbStreamIn     = sizeof(DEBUGAUDIOSTREAM);

    pBackendCfg->cMaxStreamsOut = 1; /* Output */
    pBackendCfg->cMaxStreamsIn  = 2; /* Line input + microphone input. */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostDebugAudioInit(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostDebugAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostDebugAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


static int debugCreateStreamIn(PDRVHOSTDEBUGAUDIO pDrv, PDEBUGAUDIOSTREAM pStreamDbg,
                               PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pDrv, pStreamDbg, pCfgReq);

    if (pCfgAcq)
        pCfgAcq->cSampleBufferHint = _1K;

    return VINF_SUCCESS;
}


static int debugCreateStreamOut(PDRVHOSTDEBUGAUDIO pDrv, PDEBUGAUDIOSTREAM pStreamDbg,
                                PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pDrv);

    int rc = VINF_SUCCESS;

    pStreamDbg->Out.tsLastPlayed  = 0;
    pStreamDbg->Out.cbPlayBuffer  = _1K * PDMAUDIOSTREAMCFG_S2B(pCfgReq, 1); /** @todo Make this configurable? */
    pStreamDbg->Out.pu8PlayBuffer = (uint8_t *)RTMemAlloc(pStreamDbg->Out.cbPlayBuffer);
    if (!pStreamDbg->Out.pu8PlayBuffer)
        rc = VERR_NO_MEMORY;

    if (RT_SUCCESS(rc))
    {
        char szTemp[RTPATH_MAX];
        rc = RTPathTemp(szTemp, sizeof(szTemp));
        if (RT_SUCCESS(rc))
        {
            char szFile[RTPATH_MAX];
            rc = DrvAudioHlpGetFileName(szFile, RT_ELEMENTS(szFile), szTemp, NULL, PDMAUDIOFILETYPE_WAV);
            if (RT_SUCCESS(rc))
            {
                LogFlowFunc(("%s\n", szFile));
                rc = DrvAudioHlpWAVFileOpen(&pStreamDbg->File, szFile,
                                            RTFILE_O_WRITE | RTFILE_O_DENY_WRITE | RTFILE_O_CREATE_REPLACE,
                                            &pCfgReq->Props, PDMAUDIOFILEFLAG_NONE);
                if (RT_FAILURE(rc))
                    LogRel(("DebugAudio: Creating output file '%s' failed with %Rrc\n", szFile, rc));
            }
            else
                LogRel(("DebugAudio: Unable to build file name for temp dir '%s': %Rrc\n", szTemp, rc));
        }
        else
            LogRel(("DebugAudio: Unable to retrieve temp dir: %Rrc\n", rc));
    }

    if (RT_SUCCESS(rc))
    {
        if (pCfgAcq)
            pCfgAcq->cSampleBufferHint = PDMAUDIOSTREAMCFG_B2S(pCfgAcq, pStreamDbg->Out.cbPlayBuffer);
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostDebugAudioStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                       PPDMAUDIOBACKENDSTREAM pStream,
                                                       PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    PDRVHOSTDEBUGAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTDEBUGAUDIO, IHostAudio);
    PDEBUGAUDIOSTREAM  pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = debugCreateStreamIn( pDrv, pStreamDbg, pCfgReq, pCfgAcq);
    else
        rc = debugCreateStreamOut(pDrv, pStreamDbg, pCfgReq, pCfgAcq);

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
static DECLCALLBACK(int) drvHostDebugAudioStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                     PPDMAUDIOBACKENDSTREAM pStream, const void *pvBuf, uint32_t cbBuf,
                                                     uint32_t *pcbWritten)
{
    RT_NOREF(pvBuf, cbBuf);

    PDRVHOSTDEBUGAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTDEBUGAUDIO, IHostAudio);
    PDEBUGAUDIOSTREAM  pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;

    /* Consume as many samples as would be played at the current frequency since last call. */
    /*uint32_t cLive           = AudioMixBufLive(&pStream->MixBuf);*/

    uint64_t u64TicksNow     = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
   // uint64_t u64TicksElapsed = u64TicksNow  - pStreamDbg->Out.tsLastPlayed;
   // uint64_t u64TicksFreq    = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    /*
     * Minimize the rounding error by adding 0.5: samples = int((u64TicksElapsed * samplesFreq) / u64TicksFreq + 0.5).
     * If rounding is not taken into account then the playback rate will be consistently lower that expected.
     */
   // uint64_t cSamplesPlayed = (2 * u64TicksElapsed * pStream->Props.uHz + u64TicksFreq) / u64TicksFreq / 2;

    /* Don't play more than available. */
    /*if (cSamplesPlayed > cLive)
        cSamplesPlayed = cLive;*/

    uint32_t cbWritten = 0;

    uint32_t cbAvail  = RT_MIN(cbBuf, pStreamDbg->Out.cbPlayBuffer);
    while (cbAvail)
    {
        uint32_t cbChunk = cbAvail; /** @todo Use chunks? */

        memcpy(pStreamDbg->Out.pu8PlayBuffer, pvBuf, cbChunk);
#if 0
        RTFILE fh;
        RTFileOpen(&fh, "/tmp/AudioDebug-Output.pcm",
                   RTFILE_O_OPEN_CREATE | RTFILE_O_APPEND | RTFILE_O_WRITE | RTFILE_O_DENY_NONE);
        RTFileWrite(fh, pStreamDbg->Out.pu8PlayBuffer, cbChunk, NULL);
        RTFileClose(fh);
#endif
        int rc2 = DrvAudioHlpWAVFileWrite(&pStreamDbg->File,
                                          pStreamDbg->Out.pu8PlayBuffer, cbChunk, 0 /* fFlags */);
        if (RT_FAILURE(rc2))
        {
            LogRel(("DebugAudio: Writing output failed with %Rrc\n", rc2));
            break;
        }

        Assert(cbAvail >= cbAvail);
        cbAvail   -= cbChunk;

        cbWritten += cbChunk;
    }

    /* Remember when samples were consumed. */
    pStreamDbg->Out.tsLastPlayed = u64TicksNow;

    if (pcbWritten)
        *pcbWritten = cbWritten;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostDebugAudioStreamCapture(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOBACKENDSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface, pStream, pvBuf, cbBuf);

    /* Never capture anything. */
    if (pcbRead)
        *pcbRead = 0;

    return VINF_SUCCESS;
}


static int debugDestroyStreamIn(PDRVHOSTDEBUGAUDIO pDrv, PDEBUGAUDIOSTREAM pStreamDbg)
{
    RT_NOREF(pDrv, pStreamDbg);
    return VINF_SUCCESS;
}


static int debugDestroyStreamOut(PDRVHOSTDEBUGAUDIO pDrv, PDEBUGAUDIOSTREAM pStreamDbg)
{
    RT_NOREF(pDrv);

    if (pStreamDbg->Out.pu8PlayBuffer)
    {
        RTMemFree(pStreamDbg->Out.pu8PlayBuffer);
        pStreamDbg->Out.pu8PlayBuffer = NULL;
    }

    size_t cbDataSize = DrvAudioHlpWAVFileGetDataSize(&pStreamDbg->File);

    int rc = DrvAudioHlpWAVFileClose(&pStreamDbg->File);
    if (RT_SUCCESS(rc))
    {
        /* Delete the file again if nothing but the header was written to it. */
        bool fDeleteEmptyFiles = true; /** @todo Make deletion configurable? */

        if (   !cbDataSize
            && fDeleteEmptyFiles)
        {
            rc = RTFileDelete(pStreamDbg->File.szName);
        }
        else
            LogRel(("DebugAudio: Created output file '%s' (%zu bytes)\n", pStreamDbg->File.szName, cbDataSize));
    }

    return rc;
}


static DECLCALLBACK(int) drvHostDebugAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);

    PDRVHOSTDEBUGAUDIO pDrv       = RT_FROM_MEMBER(pInterface, DRVHOSTDEBUGAUDIO, IHostAudio);
    PDEBUGAUDIOSTREAM  pStreamDbg = (PDEBUGAUDIOSTREAM)pStream;

    if (!pStreamDbg->pCfg) /* Not (yet) configured? Skip. */
        return VINF_SUCCESS;

    int rc;
    if (pStreamDbg->pCfg->enmDir == PDMAUDIODIR_IN)
        rc = debugDestroyStreamIn (pDrv, pStreamDbg);
    else
        rc = debugDestroyStreamOut(pDrv, pStreamDbg);

    if (RT_SUCCESS(rc))
    {
        DrvAudioHlpStreamCfgFree(pStreamDbg->pCfg);
        pStreamDbg->pCfg = NULL;
    }

    return rc;
}

static DECLCALLBACK(int) drvHostDebugAudioStreamControl(PPDMIHOSTAUDIO pInterface,
                                                        PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(enmStreamCmd);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    return VINF_SUCCESS;
}

static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostDebugAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);

    return (  PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED
            | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE | PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE);
}

static DECLCALLBACK(int) drvHostDebugAudioStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
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
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostDebugAudio);

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

