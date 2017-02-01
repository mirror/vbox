/* $Id$ */
/** @file
 * VRDE audio backend for Main.
 */

/*
 * Copyright (C) 2013-2017 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DRV_VRDE_AUDIO
#include <VBox/log.h>
#include "DrvAudioVRDE.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include "../../Devices/Audio/DrvAudio.h"

#include <iprt/mem.h>
#include <iprt/cdefs.h>
#include <iprt/circbuf.h>

#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmdrv.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Audio VRDE driver instance data.
 */
typedef struct DRVAUDIOVRDE
{
    /** Pointer to audio VRDE object. */
    AudioVRDE           *pAudioVRDE;
    /** Pointer to the driver instance structure. */
    PPDMDRVINS           pDrvIns;
    /** Pointer to host audio interface. */
    PDMIHOSTAUDIO        IHostAudio;
    /** Pointer to the VRDP's console object. */
    ConsoleVRDPServer   *pConsoleVRDPServer;
    /** Pointer to the DrvAudio port interface that is above us. */
    PPDMIAUDIOCONNECTOR  pDrvAudio;
} DRVAUDIOVRDE, *PDRVAUDIOVRDE;

typedef struct VRDESTREAMIN
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    /** Number of samples this stream can handle at once. */
    uint32_t             cSamplesMax;
    /** Circular buffer for holding the recorded audio samples from the host. */
    PRTCIRCBUF           pCircBuf;
} VRDESTREAMIN, *PVRDESTREAMIN;

typedef struct VRDESTREAMOUT
{
    /** Note: Always must come first! */
    PDMAUDIOSTREAM       Stream;
    /** The PCM properties of this stream. */
    PDMAUDIOPCMPROPS     Props;
    uint64_t             old_ticks;
    uint64_t             cSamplesSentPerSec;
} VRDESTREAMOUT, *PVRDESTREAMOUT;



static int vrdeCreateStreamIn(PVRDESTREAMIN pStreamVRDE, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStreamVRDE->Props);
    if (RT_SUCCESS(rc))
    {
        pStreamVRDE->cSamplesMax = _1K; /** @todo Make this configurable. */

        rc = RTCircBufCreate(&pStreamVRDE->pCircBuf, pStreamVRDE->cSamplesMax * (pStreamVRDE->Props.cBits / 8) /* Bytes */);
        if (RT_SUCCESS(rc))
        {
            if (pCfgAcq)
                pCfgAcq->cSampleBufferSize = pStreamVRDE->cSamplesMax;
        }
    }

    return rc;
}


static int vrdeCreateStreamOut(PVRDESTREAMOUT pStreamVRDE, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    int rc = DrvAudioHlpStreamCfgToProps(pCfgReq, &pStreamVRDE->Props);
    if (RT_SUCCESS(rc))
    {
        if (pCfgAcq)
            pCfgAcq->cSampleBufferSize = _4K; /** @todo Make this configurable. */
    }

    return rc;
}


static int vrdeControlStreamOut(PDRVAUDIOVRDE pDrv, PVRDESTREAMOUT pStreamVRDE, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    RT_NOREF(pDrv, pStreamVRDE, enmStreamCmd);

    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    return VINF_SUCCESS;
}


static int vrdeControlStreamIn(PDRVAUDIOVRDE pDrv, PVRDESTREAMIN pVRDEStrmIn, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    LogFlowFunc(("enmStreamCmd=%ld\n", enmStreamCmd));

    if (!pDrv->pConsoleVRDPServer)
        return VINF_SUCCESS;

    int rc;

    /* Initialize only if not already done. */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
        {
            rc = pDrv->pConsoleVRDPServer->SendAudioInputBegin(NULL, pVRDEStrmIn, pVRDEStrmIn->cSamplesMax,
                                                               pVRDEStrmIn->Props.uHz,
                                                               pVRDEStrmIn->Props.cChannels, pVRDEStrmIn->Props.cBits);
            if (rc == VERR_NOT_SUPPORTED)
            {
                LogFunc(("No RDP client connected, so no input recording supported\n"));
                rc = VINF_SUCCESS;
            }
        }

        case PDMAUDIOSTREAMCMD_DISABLE:
        {
            pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL /* pvUserCtx */);
            rc = VINF_SUCCESS;
        }

        case PDMAUDIOSTREAMCMD_PAUSE:
        {
            rc = VINF_SUCCESS;
            break;
        }

        case PDMAUDIOSTREAMCMD_RESUME:
        {
            rc = VINF_SUCCESS;
            break;
        }

        default:
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }

    if (RT_FAILURE(rc))
        LogFunc(("Failed with %Rrc\n", rc));

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnInit}
 */
static DECLCALLBACK(int) drvAudioVRDEInit(PPDMIHOSTAUDIO pInterface)
{
    RT_NOREF(pInterface);
    LogFlowFuncEnter();

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamCapture(PPDMIHOSTAUDIO pInterface,
                                                   PPDMAUDIOSTREAM pStream, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    PVRDESTREAMIN pStreamVRDE = (PVRDESTREAMIN)pStream;

    size_t cbData = 0;

    if (RTCircBufUsed(pStreamVRDE->pCircBuf))
    {
        void *pvData;

        RTCircBufAcquireReadBlock(pStreamVRDE->pCircBuf, cbBuf, &pvData, &cbData);

        if (cbData)
            memcpy(pvBuf, pvData, cbData);

        RTCircBufReleaseReadBlock(pStreamVRDE->pCircBuf, cbData);
    }

    if (pcbRead)
        *pcbRead = (uint32_t)cbData;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamPlay(PPDMIHOSTAUDIO pInterface,
                                                PPDMAUDIOSTREAM pStream, const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,      VERR_INVALID_POINTER);
    AssertReturn(cbBuf,         VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    PDRVAUDIOVRDE  pDrv        = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    PVRDESTREAMOUT pStreamVRDE = (PVRDESTREAMOUT)pStream;

    uint32_t cbLive           = cbBuf;

    uint64_t now              = PDMDrvHlpTMGetVirtualTime(pDrv->pDrvIns);
    uint64_t ticks            = now  - pStreamVRDE->old_ticks;
    uint64_t ticks_per_second = PDMDrvHlpTMGetVirtualFreq(pDrv->pDrvIns);

    /* Minimize the rounding error: samples = int((ticks * freq) / ticks_per_second + 0.5). */
    uint32_t cbToWrite = (int)((2 * ticks * pStreamVRDE->Props.uHz + ticks_per_second) / ticks_per_second / 2);

    /* Remember when samples were consumed. */
    pStreamVRDE->old_ticks = now;

    VRDEAUDIOFORMAT format = VRDE_AUDIO_FMT_MAKE(pStreamVRDE->Props.uHz,
                                                 pStreamVRDE->Props.cChannels,
                                                 pStreamVRDE->Props.cBits,
                                                 pStreamVRDE->Props.fSigned);

    Log2Func(("uFreq=%RU32, cChan=%RU8, cBits=%RU8, fSigned=%RTbool, enmFormat=%ld, cbLive=%RU32, cbToWrite=%RU32\n",
              pStreamVRDE->Props.uHz,   pStreamVRDE->Props.cChannels,
              pStreamVRDE->Props.cBits, pStreamVRDE->Props.fSigned,
              format, cbLive, cbToWrite));

    /* Don't play more than available. */
    if (cbToWrite > cbLive)
        cbToWrite = cbLive;

    int rc = VINF_SUCCESS;

    /*
     * Call the VRDP server with the data.
     */
    uint32_t cbWritten = 0;

    while (cbToWrite)
    {
        uint32_t cbChunk = cbToWrite; /** @todo For now write all at once. */

        pDrv->pConsoleVRDPServer->SendAudioSamples((uint8_t *)pvBuf + cbWritten, cbChunk, format);

        cbWritten += cbChunk;
        Assert(cbWritten <= cbBuf);

        Assert(cbToWrite >= cbChunk);
        cbToWrite -= cbChunk;
    }

    if (RT_SUCCESS(rc))
    {
        /*
         * Always report back all samples acquired, regardless of whether the
         * VRDP server actually did process those.
         */
        if (pcbWritten)
            *pcbWritten = cbToWrite;
    }

    return rc;
}


static int vrdeDestroyStreamIn(PDRVAUDIOVRDE pDrv, PVRDESTREAMIN pStreamVRDE)
{
    if (pDrv->pConsoleVRDPServer)
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);

    if (pStreamVRDE->pCircBuf)
    {
        RTCircBufDestroy(pStreamVRDE->pCircBuf);
        pStreamVRDE->pCircBuf = NULL;
    }

    return VINF_SUCCESS;
}


static int vrdeDestroyStreamOut(PDRVAUDIOVRDE pDrv, PVRDESTREAMOUT pStreamVRDE)
{
    RT_NOREF(pDrv, pStreamVRDE);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvAudioVRDEGetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    pBackendCfg->cbStreamOut    = sizeof(VRDESTREAMOUT);
    pBackendCfg->cbStreamIn     = sizeof(VRDESTREAMIN);
    pBackendCfg->cMaxStreamsIn  = UINT32_MAX;
    pBackendCfg->cMaxStreamsOut = UINT32_MAX;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnShutdown}
 */
static DECLCALLBACK(void) drvAudioVRDEShutdown(PPDMIHOSTAUDIO pInterface)
{
    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);
    AssertPtrReturnVoid(pDrv);

    if (pDrv->pConsoleVRDPServer)
        pDrv->pConsoleVRDPServer->SendAudioInputEnd(NULL);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvAudioVRDEGetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(enmDir);
    AssertPtrReturn(pInterface, PDMAUDIOBACKENDSTS_UNKNOWN);

    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamCreate(PPDMIHOSTAUDIO pInterface,
                                                  PPDMAUDIOSTREAM pStream, PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq,    VERR_INVALID_POINTER);

    RT_NOREF(pInterface);

    int rc;
    if (pCfgReq->enmDir == PDMAUDIODIR_IN)
    {
        PVRDESTREAMIN pStreamVRDE = (PVRDESTREAMIN)pStream;
        rc = vrdeCreateStreamIn(pStreamVRDE, pCfgReq, pCfgAcq);
    }
    else
    {
        PVRDESTREAMOUT pStreamVRDE = (PVRDESTREAMOUT)pStream;
        rc = vrdeCreateStreamOut(pStreamVRDE, pCfgReq, pCfgAcq);
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
        PVRDESTREAMIN pStreamVRDE = (PVRDESTREAMIN)pStream;
        rc = vrdeDestroyStreamIn(pDrv, pStreamVRDE);
    }
    else
    {
        PVRDESTREAMOUT pStreamVRDE = (PVRDESTREAMOUT)pStream;
        rc = vrdeDestroyStreamOut(pDrv, pStreamVRDE);
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamControl(PPDMIHOSTAUDIO pInterface,
                                                   PPDMAUDIOSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    PDRVAUDIOVRDE pDrv = RT_FROM_MEMBER(pInterface, DRVAUDIOVRDE, IHostAudio);

    int rc;
    if (pStream->enmDir == PDMAUDIODIR_IN)
    {
        PVRDESTREAMIN pStreamVRDE = (PVRDESTREAMIN)pStream;
        rc = vrdeControlStreamIn(pDrv, pStreamVRDE, enmStreamCmd);
    }
    else
    {
        PVRDESTREAMOUT pStreamVRDE = (PVRDESTREAMOUT)pStream;
        rc = vrdeControlStreamOut(pDrv, pStreamVRDE, enmStreamCmd);
    }

    return rc;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(PDMAUDIOSTRMSTS) drvAudioVRDEStreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    NOREF(pInterface);
    NOREF(pStream);

    return (  PDMAUDIOSTRMSTS_FLAG_INITIALIZED | PDMAUDIOSTRMSTS_FLAG_ENABLED
            | PDMAUDIOSTRMSTS_FLAG_DATA_READABLE | PDMAUDIOSTRMSTS_FLAG_DATA_WRITABLE);
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamIterate}
 */
static DECLCALLBACK(int) drvAudioVRDEStreamIterate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOSTREAM pStream)
{
    AssertPtrReturn(pInterface, VERR_INVALID_POINTER);
    AssertPtrReturn(pStream,    VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* Nothing to do here for VRDE. */
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvAudioVRDEQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, &pThis->IHostAudio);
    return NULL;
}


AudioVRDE::AudioVRDE(Console *pConsole)
    : mpDrv(NULL),
      mParent(pConsole)
{
}


AudioVRDE::~AudioVRDE(void)
{
    if (mpDrv)
    {
        mpDrv->pAudioVRDE = NULL;
        mpDrv = NULL;
    }
}


int AudioVRDE::onVRDEControl(bool fEnable, uint32_t uFlags)
{
    RT_NOREF(fEnable, uFlags);
    LogFlowThisFunc(("fEnable=%RTbool, uFlags=0x%x\n", fEnable, uFlags));

    if (mpDrv == NULL)
        return VERR_INVALID_STATE;

    return VINF_SUCCESS; /* Never veto. */
}


/**
 * Marks the beginning of sending captured audio data from a connected
 * RDP client.
 *
 * @return  IPRT status code.
 * @param   pvContext               The context; in this case a pointer to a
 *                                  VRDESTREAMIN structure.
 * @param   pVRDEAudioBegin         Pointer to a VRDEAUDIOINBEGIN structure.
 */
int AudioVRDE::onVRDEInputBegin(void *pvContext, PVRDEAUDIOINBEGIN pVRDEAudioBegin)
{
    AssertPtrReturn(pvContext, VERR_INVALID_POINTER);
    AssertPtrReturn(pVRDEAudioBegin, VERR_INVALID_POINTER);

    PVRDESTREAMIN pVRDEStrmIn = (PVRDESTREAMIN)pvContext;
    AssertPtrReturn(pVRDEStrmIn, VERR_INVALID_POINTER);

    VRDEAUDIOFORMAT audioFmt = pVRDEAudioBegin->fmt;

    int iSampleHz  = VRDE_AUDIO_FMT_SAMPLE_FREQ(audioFmt);     NOREF(iSampleHz);
    int cChannels  = VRDE_AUDIO_FMT_CHANNELS(audioFmt);        NOREF(cChannels);
    int cBits      = VRDE_AUDIO_FMT_BITS_PER_SAMPLE(audioFmt); NOREF(cBits);
    bool fUnsigned = VRDE_AUDIO_FMT_SIGNED(audioFmt);          NOREF(fUnsigned);

    LogFlowFunc(("cbSample=%RU32, iSampleHz=%d, cChannels=%d, cBits=%d, fUnsigned=%RTbool\n",
                 VRDE_AUDIO_FMT_BYTES_PER_SAMPLE(audioFmt), iSampleHz, cChannels, cBits, fUnsigned));

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputData(void *pvContext, const void *pvData, uint32_t cbData)
{
    PVRDESTREAMIN pStreamVRDE = (PVRDESTREAMIN)pvContext;
    AssertPtrReturn(pStreamVRDE, VERR_INVALID_POINTER);

    void  *pvBuf;
    size_t cbBuf;

    RTCircBufAcquireWriteBlock(pStreamVRDE->pCircBuf, cbData, &pvBuf, &cbBuf);

    if (cbBuf)
        memcpy(pvBuf, pvData, cbBuf);

    RTCircBufReleaseWriteBlock(pStreamVRDE->pCircBuf, cbBuf);

    if (cbBuf < cbData)
        LogRel(("VRDE: Capturing audio data lost %zu bytes\n", cbData - cbBuf)); /** @todo Use an error counter. */

    return VINF_SUCCESS; /** @todo r=andy How to tell the caller if we were not able to handle *all* input data? */
}


int AudioVRDE::onVRDEInputEnd(void *pvContext)
{
    NOREF(pvContext);

    return VINF_SUCCESS;
}


int AudioVRDE::onVRDEInputIntercept(bool fEnabled)
{
    RT_NOREF(fEnabled);
    return VINF_SUCCESS; /* Never veto. */
}


/**
 * Construct a VRDE audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
/* static */
DECLCALLBACK(int) AudioVRDE::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    RT_NOREF(fFlags);

    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);

    AssertPtrReturn(pDrvIns, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfg, VERR_INVALID_POINTER);

    LogRel(("Audio: Initializing VRDE driver\n"));
    LogFlowFunc(("fFlags=0x%x\n", fFlags));

    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * Init the static parts.
     */
    pThis->pDrvIns                   = pDrvIns;
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvAudioVRDEQueryInterface;
    /* IHostAudio */
    PDMAUDIO_IHOSTAUDIO_CALLBACKS(drvAudioVRDE);

    /*
     * Get the ConsoleVRDPServer object pointer.
     */
    void *pvUser;
    int rc = CFGMR3QueryPtr(pCfg, "ObjectVRDPServer", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"ObjectVRDPServer\" value, rc=%Rrc\n", rc), rc);

    /* CFGM tree saves the pointer to ConsoleVRDPServer in the Object node of AudioVRDE. */
    pThis->pConsoleVRDPServer = (ConsoleVRDPServer *)pvUser;

    /*
     * Get the AudioVRDE object pointer.
     */
    pvUser = NULL;
    rc = CFGMR3QueryPtr(pCfg, "Object", &pvUser); /** @todo r=andy Get rid of this hack and use IHostAudio::SetCallback. */
    AssertMsgRCReturn(rc, ("Confguration error: No/bad \"Object\" value, rc=%Rrc\n", rc), rc);

    pThis->pAudioVRDE = (AudioVRDE *)pvUser;
    pThis->pAudioVRDE->mpDrv = pThis;

    /*
     * Get the interface for the above driver (DrvAudio) to make mixer/conversion calls.
     * Described in CFGM tree.
     */
    pThis->pDrvAudio = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOCONNECTOR);
    AssertMsgReturn(pThis->pDrvAudio, ("Configuration error: No upper interface specified!\n"), VERR_PDM_MISSING_INTERFACE_ABOVE);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDRVREG,pfnDestruct}
 */
/* static */
DECLCALLBACK(void) AudioVRDE::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);
    PDRVAUDIOVRDE pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOVRDE);
    LogFlowFuncEnter();

    /*
     * If the AudioVRDE object is still alive, we must clear it's reference to
     * us since we'll be invalid when we return from this method.
     */
    if (pThis->pAudioVRDE)
    {
        pThis->pAudioVRDE->mpDrv = NULL;
        pThis->pAudioVRDE = NULL;
    }
}


/**
 * VRDE audio driver registration record.
 */
const PDMDRVREG AudioVRDE::DrvReg =
{
    PDM_DRVREG_VERSION,
    /* szName */
    "AudioVRDE",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio driver for VRDE backend",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOVRDE),
    /* pfnConstruct */
    AudioVRDE::drvConstruct,
    /* pfnDestruct */
    AudioVRDE::drvDestruct,
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

