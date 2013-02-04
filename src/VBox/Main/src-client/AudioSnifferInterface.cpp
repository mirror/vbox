/* $Id$ */
/** @file
 * VirtualBox Driver Interface to Audio Sniffer device
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "AudioSnifferInterface.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include <VBox/vmm/pdmdrv.h>
#include <VBox/RemoteDesktop/VRDE.h>
#include <VBox/vmm/cfgm.h>
#include <VBox/err.h>

//
// defines
//


//
// globals
//


/**
 * Audio Sniffer driver instance data.
 *
 * @extends PDMIAUDIOSNIFFERCONNECTOR
 */
typedef struct DRVAUDIOSNIFFER
{
    /** Pointer to the Audio Sniffer object. */
    AudioSniffer                *pAudioSniffer;

    /** Pointer to the driver instance structure. */
    PPDMDRVINS                  pDrvIns;

    /** Pointer to the AudioSniffer port interface of the driver/device above us. */
    PPDMIAUDIOSNIFFERPORT       pUpPort;
    /** Our VMM device connector interface. */
    PDMIAUDIOSNIFFERCONNECTOR   Connector;

} DRVAUDIOSNIFFER, *PDRVAUDIOSNIFFER;

/** Converts PDMIAUDIOSNIFFERCONNECTOR pointer to a DRVAUDIOSNIFFER pointer. */
#define PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface)    RT_FROM_MEMBER(pInterface, DRVAUDIOSNIFFER, Connector)


//
// constructor / destructor
//
AudioSniffer::AudioSniffer(Console *console)
    : mpDrv(NULL),
      mParent(console)
{
}

AudioSniffer::~AudioSniffer()
{
    if (mpDrv)
    {
        mpDrv->pAudioSniffer = NULL;
        mpDrv = NULL;
    }
}

PPDMIAUDIOSNIFFERPORT AudioSniffer::getAudioSnifferPort()
{
    Assert(mpDrv);
    return mpDrv->pUpPort;
}



//
// public methods
//

DECLCALLBACK(void) iface_AudioSamplesOut (PPDMIAUDIOSNIFFERCONNECTOR pInterface, void *pvSamples, uint32_t cSamples,
                                          int samplesPerSec, int nChannels, int bitsPerSample, bool fUnsigned)
{
    PDRVAUDIOSNIFFER pDrv = PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface);

    /*
     * Just call the VRDP server with the data.
     */
    VRDEAUDIOFORMAT format = VRDE_AUDIO_FMT_MAKE(samplesPerSec, nChannels, bitsPerSample, !fUnsigned);
    pDrv->pAudioSniffer->getParent()->consoleVRDPServer()->SendAudioSamples(pvSamples, cSamples, format);
}

DECLCALLBACK(void) iface_AudioVolumeOut (PPDMIAUDIOSNIFFERCONNECTOR pInterface, uint16_t left, uint16_t right)
{
    PDRVAUDIOSNIFFER pDrv = PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface);

    /*
     * Just call the VRDP server with the data.
     */
    pDrv->pAudioSniffer->getParent()->consoleVRDPServer()->SendAudioVolume(left, right);
}

DECLCALLBACK(int) iface_AudioInputBegin (PPDMIAUDIOSNIFFERCONNECTOR pInterface,
                                         void **ppvUserCtx,
                                         void *pvContext,
                                         uint32_t cSamples,
                                         uint32_t iSampleHz,
                                         uint32_t cChannels,
                                         uint32_t cBits)
{
    PDRVAUDIOSNIFFER pDrv = PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface);

    return pDrv->pAudioSniffer->getParent()->consoleVRDPServer()->SendAudioInputBegin(ppvUserCtx,
                                                                                      pvContext,
                                                                                      cSamples,
                                                                                      iSampleHz,
                                                                                      cChannels,
                                                                                      cBits);
}

DECLCALLBACK(void) iface_AudioInputEnd (PPDMIAUDIOSNIFFERCONNECTOR pInterface,
                                        void *pvUserCtx)
{
    PDRVAUDIOSNIFFER pDrv = PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface);

    pDrv->pAudioSniffer->getParent()->consoleVRDPServer()->SendAudioInputEnd(pvUserCtx);
}


/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
DECLCALLBACK(void *) AudioSniffer::drvQueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOSNIFFER pDrv = PDMINS_2_DATA(pDrvIns, PDRVAUDIOSNIFFER);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pDrvIns->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOSNIFFERCONNECTOR, &pDrv->Connector);
    return NULL;
}


/**
 * Destruct a Audio Sniffer driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) AudioSniffer::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVAUDIOSNIFFER pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOSNIFFER);
    LogFlow(("AudioSniffer::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN_VOID(pDrvIns);

    if (pThis->pAudioSniffer)
    {
        pThis->pAudioSniffer->mpDrv = NULL;
    }
}


/**
 * Construct a AudioSniffer driver instance.
 *
 * @copydoc FNPDMDRVCONSTRUCT
 */
DECLCALLBACK(int) AudioSniffer::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfg, uint32_t fFlags)
{
    PDRVAUDIOSNIFFER pThis = PDMINS_2_DATA(pDrvIns, PDRVAUDIOSNIFFER);

    LogFlow(("AudioSniffer::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));
    PDMDRV_CHECK_VERSIONS_RETURN(pDrvIns);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "Object\0"))
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    AssertMsgReturn(PDMDrvHlpNoAttach(pDrvIns) == VERR_PDM_NO_ATTACHED_DRIVER,
                    ("Configuration error: Not possible to attach anything to this driver!\n"),
                    VERR_PDM_DRVINS_NO_ATTACH);

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface            = AudioSniffer::drvQueryInterface;

    /* Audio Sniffer connector. */
    pThis->Connector.pfnAudioSamplesOut         = iface_AudioSamplesOut;
    pThis->Connector.pfnAudioVolumeOut          = iface_AudioVolumeOut;
    pThis->Connector.pfnAudioInputBegin         = iface_AudioInputBegin;
    pThis->Connector.pfnAudioInputEnd           = iface_AudioInputEnd;

    /*
     * Get the Audio Sniffer Port interface of the above driver/device.
     */
    pThis->pUpPort = PDMIBASE_QUERY_INTERFACE(pDrvIns->pUpBase, PDMIAUDIOSNIFFERPORT);
    if (!pThis->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No Audio Sniffer port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Console object pointer and update the mpDrv member.
     */
    void *pv;
    int rc = CFGMR3QueryPtr(pCfg, "Object", &pv);
    if (RT_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Rrc\n", rc));
        return rc;
    }
    pThis->pAudioSniffer = (AudioSniffer *)pv;        /** @todo Check this cast! */
    pThis->pAudioSniffer->mpDrv = pThis;

    return VINF_SUCCESS;
}


/**
 * Audio Sniffer driver registration record.
 */
const PDMDRVREG AudioSniffer::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szName */
    "MainAudioSniffer",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Main Audio Sniffer driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(DRVAUDIOSNIFFER),
    /* pfnConstruct */
    AudioSniffer::drvConstruct,
    /* pfnDestruct */
    AudioSniffer::drvDestruct,
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
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
