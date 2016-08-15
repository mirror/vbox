/* $Id$ */
/** @file
 * NULL audio driver -- also acts as a fallback if no
 * other backend is available.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on: noaudio.c QEMU based code.
 *
 * QEMU Timer based audio emulation
 *
 * Copyright (c) 2004-2005 Vassili Karpov (malc)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/mem.h>
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>

#include "DrvAudio.h"
#include "AudioMixBuffer.h"
#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
typedef struct NULLAUDIOSTREAMOUT
{
    PDMAUDIOSTREAM      Stream;
    uint64_t            u64TicksLast;
    uint64_t            cMaxSamplesInPlayBuffer;
    uint8_t            *pbPlayBuffer;
} NULLAUDIOSTREAMOUT;
typedef NULLAUDIOSTREAMOUT *PNULLAUDIOSTREAMOUT;

typedef struct NULLAUDIOSTREAMIN
{
    /** @note Always must come first! */
    PDMAUDIOSTREAM      Stream;
} NULLAUDIOSTREAMIN;
typedef NULLAUDIOSTREAMIN *PNULLAUDIOSTREAMIN;

/**
 * NULL audio driver instance data.
 * @implements PDMIAUDIOCONNECTOR
 */
typedef struct DRVHOSTNULLAUDIO
{
    /** Pointer to the driver instance structure. */
    PPDMDRVINS          pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO       IHostAudio;
} DRVHOSTNULLAUDIO, *PDRVHOSTNULLAUDIO;



/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostNullAudioGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    pBackendCfg->cbStreamOut    = sizeof(NULLAUDIOSTREAMOUT);
    pBackendCfg->cbStreamIn     = sizeof(NULLAUDIOSTREAMIN);

    /* The NULL backend has exactly one input source and one output sink. */
    pBackendCfg->cSources       = 1;
    pBackendCfg->cSinks         = 1;

    pBackendCfg->cMaxStreamsOut = 1; /* Output */
    pBackendCfg->cMaxStreamsIn  = 2; /* Line input + microphone input. */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvHostNullAudioInit(PPDMIHOSTAUDIO pInterface)
{
    NOREF(pInterface);

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvHostNullAudioShutdown(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostNullAudioGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF2(pvBuf, cbBuf);

    PDRVHOSTNULLAUDIO   pDrv        = RT_FROM_MEMBER(pInterface, DRVHOSTNULLAUDIO, IHostAudio);
    PNULLAUDIOSTREAMOUT pNullStream = RT_FROM_MEMBER(pStream, NULLAUDIOSTREAMOUT, Stream);

    /* Consume as many samples as would be played at the current frequency since last call. */
    uint32_t cLive           = AudioMixBufLive(&pStream->MixBuf);

    uint64_t u64TicksNow     = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
    uint64_t u64TicksElapsed = u64TicksNow  - pNullStream->u64TicksLast;
    uint64_t u64TicksFreq    = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    /* Remember when samples were consumed. */
    pNullStream->u64TicksLast = u64TicksNow;

    /*
     * Minimize the rounding error by adding 0.5: samples = int((u64TicksElapsed * samplesFreq) / u64TicksFreq + 0.5).
     * If rounding is not taken into account then the playback rate will be consistently lower that expected.
     */
    uint64_t cSamplesPlayed = (2 * u64TicksElapsed * pStream->Props.uHz + u64TicksFreq) / u64TicksFreq / 2;

    /* Don't play more than available. */
    if (cSamplesPlayed > cLive)
        cSamplesPlayed = cLive;

    cSamplesPlayed = RT_MIN(cSamplesPlayed, pNullStream->cMaxSamplesInPlayBuffer);

    uint32_t cSamplesToRead = 0;
    AudioMixBufReadCirc(&pStream->MixBuf, pNullStream->pbPlayBuffer,
                        AUDIOMIXBUF_S2B(&pStream->MixBuf, cSamplesPlayed), &cSamplesToRead);
    AudioMixBufFinish(&pStream->MixBuf, cSamplesToRead);

    if (pcbWritten)
        *pcbWritten = cSamplesToRead;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF4(pInterface, pStream, pvBuf, cbBuf);

    /* Never capture anything. */
    if (pcbRead)
        *pcbRead = 0;

    return VINF_SUCCESS;
}


static int nullCreateStreamIn(PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    /* Just adopt the wanted stream configuration. */
    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStream->Props);
    if (RT_SUCCESS(rc))
    {
        if (pCfgAcq)
            pCfgAcq->cSamples = _1K;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


static int nullCreateStreamOut(PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    /* Just adopt the wanted stream configuration. */
    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStream->Props);
    if (RT_SUCCESS(rc))
    {
        PNULLAUDIOSTREAMOUT pNullStream = RT_FROM_MEMBER(pStream, NULLAUDIOSTREAMOUT, Stream);

        pNullStream->u64TicksLast  = 0;
        pNullStream->cMaxSamplesInPlayBuffer = _1K;

        pNullStream->pbPlayBuffer = (uint8_t *)RTMemAlloc(_1K << pStream->Props.cShift);
        if (pNullStream->pbPlayBuffer)
        {
            if (pCfgAcq)
                pCfgAcq->cSamples = pNullStream->cMaxSamplesInPlayBuffer;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
        rc = nullCreateStreamIn( pStream, pCfgReq, pCfgAcq);
    else
        rc = nullCreateStreamOut(pStream, pCfgReq, pCfgAcq);

    LogFlowFunc(("%s: rc=%Rrc\n", pStream->szName, rc));
    return rc;
}


static int nullDestroyStreamIn(void)
{
    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


static int nullDestroyStreamOut(PPDMAUDIOSTREAM pStream)
{
    PNULLAUDIOSTREAMOUT pNullStream = RT_FROM_MEMBER(pStream, NULLAUDIOSTREAMOUT, Stream);
    if (   pNullStream
        && pNullStream->pbPlayBuffer)
    {
        RTMemFree(pNullStream->pbPlayBuffer);
        pNullStream->pbPlayBuffer = NULL;
    }

    LogFlowFuncLeaveRC(VINF_SUCCESS);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
        rc = nullDestroyStreamIn();
    else
        rc = nullDestroyStreamOut(pStream);

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamControl(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(enmStreamCmd);
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    Assert(pStream->enmCtx == PDMAUDIOSTREAMCTX_HOST);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvHostNullAudioStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED
         | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE | PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvHostNullAudioStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostNullAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVHOSTNULLAUDIO pThis   = PDMINS_2_DATA(pDrvIns, PDRVHOSTNULLAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


/**
 * Constructs a Null audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostNullAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(pCfg, fFlags);
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    /* pCfg is optional. */

    PDRVHOSTNULLAUDIO pThis = PDMINS_2_DATA(pDrvIns, PDRVHOSTNULLAUDIO);
    LogRel(("Audio: Initializing NULL driver\n"));

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostNullAudioQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvHostNullAudio);

    return VINF_SUCCESS;
}


/**
 * Char driver registration record.
 */
const PDMDRVREG g_DrvHostNullAudio =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "NullAudio",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "NULL audio host driver",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVHOSTNULLAUDIO),
    /* pfnConstruct */
    drvHostNullAudioConstruct,
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

