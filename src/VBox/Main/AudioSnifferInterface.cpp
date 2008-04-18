/** @file
 *
 * VirtualBox Driver Interface to Audio Sniffer device
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "AudioSnifferInterface.h"
#include "ConsoleImpl.h"
#include "ConsoleVRDPServer.h"

#include "Logging.h"

#include <VBox/pdmdrv.h>
#include <VBox/vrdpapi.h>
#include <VBox/cfgm.h>
#include <VBox/err.h>

//
// defines
//


//
// globals
//


/**
 * Audio Sniffer driver instance data.
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
#define PDMIAUDIOSNIFFERCONNECTOR_2_MAINAUDIOSNIFFER(pInterface) ( (PDRVAUDIOSNIFFER) ((uintptr_t)pInterface - RT_OFFSETOF(DRVAUDIOSNIFFER, Connector)) )


//
// constructor / destructor
//
AudioSniffer::AudioSniffer(Console *console) : mpDrv(NULL)
{
    mParent = console;
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
    VRDPAUDIOFORMAT format = VRDP_AUDIO_FMT_MAKE(samplesPerSec, nChannels, bitsPerSample, !fUnsigned);
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


/**
 * Queries an interface to the driver.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
DECLCALLBACK(void *) AudioSniffer::drvQueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    PPDMDRVINS pDrvIns = PDMIBASE_2_PDMDRV(pInterface);
    PDRVAUDIOSNIFFER pDrv = PDMINS2DATA(pDrvIns, PDRVAUDIOSNIFFER);
    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pDrvIns->IBase;
        case PDMINTERFACE_AUDIO_SNIFFER_CONNECTOR:
            return &pDrv->Connector;
        default:
            return NULL;
    }
}


/**
 * Destruct a Audio Sniffer driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 */
DECLCALLBACK(void) AudioSniffer::drvDestruct(PPDMDRVINS pDrvIns)
{
    PDRVAUDIOSNIFFER pData = PDMINS2DATA(pDrvIns, PDRVAUDIOSNIFFER);
    LogFlow(("AudioSniffer::drvDestruct: iInstance=%d\n", pDrvIns->iInstance));
    if (pData->pAudioSniffer)
    {
        pData->pAudioSniffer->mpDrv = NULL;
    }
}


/**
 * Construct a AudioSniffer driver instance.
 *
 * @returns VBox status.
 * @param   pDrvIns     The driver instance data.
 *                      If the registration structure is needed, pDrvIns->pDrvReg points to it.
 * @param   pCfgHandle  Configuration node handle for the driver. Use this to obtain the configuration
 *                      of the driver instance. It's also found in pDrvIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
DECLCALLBACK(int) AudioSniffer::drvConstruct(PPDMDRVINS pDrvIns, PCFGMNODE pCfgHandle)
{
    PDRVAUDIOSNIFFER pData = PDMINS2DATA(pDrvIns, PDRVAUDIOSNIFFER);

    LogFlow(("AudioSniffer::drvConstruct: iInstance=%d\n", pDrvIns->iInstance));

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "Object\0"))
    {
        return VERR_PDM_DRVINS_UNKNOWN_CFG_VALUES;
    }

    PPDMIBASE pBaseIgnore;
    int rc = pDrvIns->pDrvHlp->pfnAttach(pDrvIns, &pBaseIgnore);
    if (rc != VERR_PDM_NO_ATTACHED_DRIVER)
    {
        AssertMsgFailed(("Configuration error: Not possible to attach anything to this driver!\n"));
        return VERR_PDM_DRVINS_NO_ATTACH;
    }

    /*
     * IBase.
     */
    pDrvIns->IBase.pfnQueryInterface            = AudioSniffer::drvQueryInterface;

    /* Audio Sniffer connector. */
    pData->Connector.pfnAudioSamplesOut         = iface_AudioSamplesOut;
    pData->Connector.pfnAudioVolumeOut          = iface_AudioVolumeOut;

    /*
     * Get the Audio Sniffer Port interface of the above driver/device.
     */
    pData->pUpPort = (PPDMIAUDIOSNIFFERPORT)pDrvIns->pUpBase->pfnQueryInterface(pDrvIns->pUpBase, PDMINTERFACE_AUDIO_SNIFFER_PORT);
    if (!pData->pUpPort)
    {
        AssertMsgFailed(("Configuration error: No Audio Sniffer port interface above!\n"));
        return VERR_PDM_MISSING_INTERFACE_ABOVE;
    }

    /*
     * Get the Console object pointer and update the mpDrv member.
     */
    void *pv;
    rc = CFGMR3QueryPtr(pCfgHandle, "Object", &pv);
    if (VBOX_FAILURE(rc))
    {
        AssertMsgFailed(("Configuration error: No/bad \"Object\" value! rc=%Vrc\n", rc));
        return rc;
    }
    pData->pAudioSniffer = (AudioSniffer *)pv;        /** @todo Check this cast! */
    pData->pAudioSniffer->mpDrv = pData;

    return VINF_SUCCESS;
}


/**
 * Audio Sniffer driver registration record.
 */
const PDMDRVREG AudioSniffer::DrvReg =
{
    /* u32Version */
    PDM_DRVREG_VERSION,
    /* szDriverName */
    "MainAudioSniffer",
    /* pszDescription */
    "Main Audio Sniffer driver (Main as in the API).",
    /* fFlags */
    PDM_DRVREG_FLAGS_HOST_BITS_DEFAULT,
    /* fClass. */
    PDM_DRVREG_CLASS_AUDIO,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(DRVAUDIOSNIFFER),
    /* pfnConstruct */
    AudioSniffer::drvConstruct,
    /* pfnDestruct */
    AudioSniffer::drvDestruct,
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
    /* pfnDetach */
    NULL
};
