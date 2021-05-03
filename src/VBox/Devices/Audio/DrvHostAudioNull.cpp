/* $Id$ */
/** @file
 * Host audio driver - NULL (bitbucket).
 *
 * This also acts as a fallback if no other backend is available.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/uuid.h> /* For PDMIBASE_2_PDMDRV. */

#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO
#include <VBox/log.h>
#include <VBox/vmm/pdmaudioifs.h>
#include <VBox/vmm/pdmaudioinline.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Null audio stream. */
typedef struct NULLAUDIOSTREAM
{
    /** Common part. */
    PDMAUDIOBACKENDSTREAM   Core;
    /** The stream's acquired configuration. */
    PDMAUDIOSTREAMCFG       Cfg;
} NULLAUDIOSTREAM;
/** Pointer to a null audio stream.   */
typedef NULLAUDIOSTREAM *PNULLAUDIOSTREAM;



/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetConfig}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_GetConfig(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDCFG pBackendCfg)
{
    NOREF(pInterface);
    AssertPtrReturn(pBackendCfg, VERR_INVALID_POINTER);

    /*
     * Fill in the config structure.
     */
    RTStrCopy(pBackendCfg->szName, sizeof(pBackendCfg->szName), "NULL audio");
    pBackendCfg->cbStream       = sizeof(NULLAUDIOSTREAM);
    pBackendCfg->fFlags         = 0;
    pBackendCfg->cMaxStreamsOut = 1; /* Output */
    pBackendCfg->cMaxStreamsIn  = 2; /* Line input + microphone input. */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnGetStatus}
 */
static DECLCALLBACK(PDMAUDIOBACKENDSTS) drvHostNullAudioHA_GetStatus(PPDMIHOSTAUDIO pInterface, PDMAUDIODIR enmDir)
{
    RT_NOREF(pInterface, enmDir);
    return PDMAUDIOBACKENDSTS_RUNNING;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCreate}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamCreate(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                         PPDMAUDIOSTREAMCFG pCfgReq, PPDMAUDIOSTREAMCFG pCfgAcq)
{
    RT_NOREF(pInterface);
    PNULLAUDIOSTREAM pStreamNull = (PNULLAUDIOSTREAM)pStream;
    AssertPtrReturn(pStreamNull, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgReq, VERR_INVALID_POINTER);
    AssertPtrReturn(pCfgAcq, VERR_INVALID_POINTER);

    PDMAudioStrmCfgCopy(&pStreamNull->Cfg, pCfgAcq);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamDestroy}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamDestroy(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @ interface_method_impl{PDMIHOSTAUDIO,pfnStreamEnable}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamControlStub(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamControl}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamControl(PPDMIHOSTAUDIO pInterface,
                                                           PPDMAUDIOBACKENDSTREAM pStream, PDMAUDIOSTREAMCMD enmStreamCmd)
{
    /** @todo r=bird: I'd like to get rid of this pfnStreamControl method,
     *        replacing it with individual StreamXxxx methods.  That would save us
     *        potentally huge switches and more easily see which drivers implement
     *        which operations (grep for pfnStreamXxxx). */
    switch (enmStreamCmd)
    {
        case PDMAUDIOSTREAMCMD_ENABLE:
            return drvHostNullAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DISABLE:
            return drvHostNullAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_PAUSE:
            return drvHostNullAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_RESUME:
            return drvHostNullAudioHA_StreamControlStub(pInterface, pStream);
        case PDMAUDIOSTREAMCMD_DRAIN:
            return drvHostNullAudioHA_StreamControlStub(pInterface, pStream);

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
static DECLCALLBACK(uint32_t) drvHostNullAudioHA_StreamGetReadable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    /** @todo rate limit this?   */
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetWritable}
 */
static DECLCALLBACK(uint32_t) drvHostNullAudioHA_StreamGetWritable(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return UINT32_MAX;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetPending}
 */
static DECLCALLBACK(uint32_t) drvHostNullAudioHA_StreamGetPending(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return 0;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamGetStatus}
 */
static DECLCALLBACK(uint32_t) drvHostNullAudioHA_StreamGetStatus(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream)
{
    RT_NOREF(pInterface, pStream);
    return PDMAUDIOSTREAM_STS_INITIALIZED | PDMAUDIOSTREAM_STS_ENABLED;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamPlay}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamPlay(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                       const void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten)
{
    RT_NOREF(pInterface, pStream, pvBuf);

    /* The bitbucket never overflows. */
    *pcbWritten = cbBuf;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMIHOSTAUDIO,pfnStreamCapture}
 */
static DECLCALLBACK(int) drvHostNullAudioHA_StreamCapture(PPDMIHOSTAUDIO pInterface, PPDMAUDIOBACKENDSTREAM pStream,
                                                          void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead)
{
    RT_NOREF(pInterface);
    PNULLAUDIOSTREAM pStreamNull = (PNULLAUDIOSTREAM)pStream;

    /** @todo rate limit this? */

    /* Return silence. */
    PDMAudioPropsClearBuffer(&pStreamNull->Cfg.Props, pvBuf, cbBuf,
                             PDMAudioPropsBytesToFrames(&pStreamNull->Cfg.Props, cbBuf));
    *pcbRead = cbBuf;
    return VINF_SUCCESS;
}


/**
 * This is used directly by DrvAudio when a backend fails to initialize in a
 * non-fatal manner.
 */
DECL_HIDDEN_CONST(PDMIHOSTAUDIO) const g_DrvHostAudioNull =
{
    /* .pfnGetConfig                 =*/ drvHostNullAudioHA_GetConfig,
    /* .pfnGetDevices                =*/ NULL,
    /* .pfnGetStatus                 =*/ drvHostNullAudioHA_GetStatus,
    /* .pfnDoOnWorkerThread          =*/ NULL,
    /* .pfnStreamConfigHint          =*/ NULL,
    /* .pfnStreamCreate              =*/ drvHostNullAudioHA_StreamCreate,
    /* .pfnStreamInitAsync           =*/ NULL,
    /* .pfnStreamDestroy             =*/ drvHostNullAudioHA_StreamDestroy,
    /* .pfnStreamNotifyDeviceChanged =*/ NULL,
    /* .pfnStreamControl             =*/ drvHostNullAudioHA_StreamControl,
    /* .pfnStreamGetReadable         =*/ drvHostNullAudioHA_StreamGetReadable,
    /* .pfnStreamGetWritable         =*/ drvHostNullAudioHA_StreamGetWritable,
    /* .pfnStreamGetPending          =*/ drvHostNullAudioHA_StreamGetPending,
    /* .pfnStreamGetStatus           =*/ drvHostNullAudioHA_StreamGetStatus,
    /* .pfnStreamPlay                =*/ drvHostNullAudioHA_StreamPlay,
    /* .pfnStreamCapture             =*/ drvHostNullAudioHA_StreamCapture,
};


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) drvHostNullAudioQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS        pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PPDMIHOSTAUDIO    pThis   = PDMINS_2_DATA(pDrvIns, PPDMIHOSTAUDIO);

    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIHOSTAUDIO, pThis);
    return NULL;
}


/**
 * Constructs a Null audio driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
static DECLCALLBACK(int) drvHostNullAudioConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);
    PPDMIHOSTAUDIO pThis = PDMINS_2_DATA(pDrvIns, PPDMIHOSTAUDIO);
    RT_NOREF(pCfg, fFlags);
    LogRel(("Audio: Initializing NULL driver\n"));

    /*
     * Init the static parts.
     */
    /* IBase */
    pDrvIns->IBase.pfnQueryInterface = drvHostNullAudioQueryInterface;
    /* IHostAudio */
    *pThis = g_DrvHostAudioNull;

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
    sizeof(PDMIHOSTAUDIO),
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

