/** @file
 *
 * VBox audio device:
 * Audio sniffer device
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define LOG_GROUP LOG_GROUP_DEV_AUDIO
#define AUDIO_CAP "sniffer"
#include <VBox/pdm.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/uuid.h>
#include <iprt/string.h>
#include <iprt/alloc.h>

#include "Builtins.h"
#include "../../vl_vbox.h"

#include "audio.h"
#include "audio_int.h"

typedef struct _AUDIOSNIFFERSTATE
{
    /** If the device is enabled. */
    bool fEnabled;

    /** Whether audio should reach the host driver too. */
    bool fKeepHostAudio;

    /** Pointer to device instance. */
    PPDMDEVINS                   pDevIns;

    /** Audio Sniffer port base interface. */
    PDMIBASE                     Base;
    /** Audio Sniffer port interface. */
    PDMIAUDIOSNIFFERPORT         Port;

    /** Pointer to base interface of the driver. */
    PPDMIBASE                    pDrvBase;
    /** Audio Sniffer connector interface */
    PPDMIAUDIOSNIFFERCONNECTOR   pDrv;

} AUDIOSNIFFERSTATE;

static AUDIOSNIFFERSTATE *g_pData = NULL;

/*
 * Public sniffer callbacks to be called from audio driver.
 */

/* *** Subject to change ***
 * Process audio output. The function is called when an audio output
 * driver is about to play audio samples.
 *
 * It is expected that there is only one audio data flow,
 * i.e. one voice.
 *
 * @param hw           Audio samples information.
 * @param pvSamples    Pointer to audio samples.
 * @param cSamples     Number of audio samples in the buffer.
 * @returns     'true' if audio also to be played back by the output driver.
 *              'false' if audio should not be played.
 */
DECLCALLBACK(bool) sniffer_run_out (HWVoiceOut *hw, void *pvSamples, unsigned cSamples)
{
    int  samplesPerSec;
    int  nChannels;
    int  bitsPerSample;
    bool fUnsigned;

    if (!g_pData || !g_pData->pDrv || !g_pData->fEnabled)
    {
        return true;
    }

    samplesPerSec = hw->info.freq;
    nChannels     = hw->info.nchannels;
    bitsPerSample = hw->info.bits;
    fUnsigned     = (hw->info.sign == 0);

    g_pData->pDrv->pfnAudioSamplesOut (g_pData->pDrv, pvSamples, cSamples,
                                       samplesPerSec, nChannels, bitsPerSample, fUnsigned);

    return g_pData->fKeepHostAudio;
}


/*
 * Audio Sniffer PDM device.
 */

/* Converts a Audio Sniffer port interface pointer to a Audio Sniffer state pointer. */
#define IAUDIOSNIFFERPORT_2_AUDIOSNIFFERSTATE(pInterface) ((AUDIOSNIFFERSTATE *)((uintptr_t)pInterface - RT_OFFSETOF(AUDIOSNIFFERSTATE, Port)))

static DECLCALLBACK(int) iface_Setup (PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio)
{
    AUDIOSNIFFERSTATE *pData = IAUDIOSNIFFERPORT_2_AUDIOSNIFFERSTATE(pInterface);

    Assert(g_pData == pData);

    pData->fEnabled = fEnable;
    pData->fKeepHostAudio = fKeepHostAudio;

    return VINF_SUCCESS;
}

/**
 * Queries an interface to the device.
 *
 * @returns Pointer to interface.
 * @returns NULL if the interface was not supported by the driver.
 * @param   pInterface          Pointer to this interface structure.
 * @param   enmInterface        The requested interface identification.
 */
static DECLCALLBACK(void *) iface_QueryInterface(PPDMIBASE pInterface, PDMINTERFACE enmInterface)
{
    AUDIOSNIFFERSTATE *pData = (AUDIOSNIFFERSTATE *)((uintptr_t)pInterface - RT_OFFSETOF(AUDIOSNIFFERSTATE, Base));

    switch (enmInterface)
    {
        case PDMINTERFACE_BASE:
            return &pData->Base;
        case PDMINTERFACE_AUDIO_SNIFFER_PORT:
            return &pData->Port;
        default:
            return NULL;
    }
}

/**
 * Construct a device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int) audioSnifferR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int rc = VINF_SUCCESS;

    AUDIOSNIFFERSTATE *pData = PDMINS2DATA(pDevIns, AUDIOSNIFFERSTATE *);

    Assert(iInstance == 0);

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "\0"))
    {
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;
    }

    /*
     * Initialize data.
     */
    pData->fEnabled = false;
    pData->fKeepHostAudio = true;
    pData->pDrv = NULL;

    /*
     * Interfaces
     */
    /* Base */
    pData->Base.pfnQueryInterface = iface_QueryInterface;

    /* Audio Sniffer port */
    pData->Port.pfnSetup = iface_Setup;

    /*
     * Get the corresponding connector interface
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pData->Base, &pData->pDrvBase, "Audio Sniffer Port");

    if (VBOX_SUCCESS(rc))
    {
        pData->pDrv = (PPDMIAUDIOSNIFFERCONNECTOR)pData->pDrvBase->pfnQueryInterface(pData->pDrvBase, PDMINTERFACE_AUDIO_SNIFFER_CONNECTOR);

        if (!pData->pDrv)
        {
            AssertMsgFailed(("LUN #0 doesn't have a Audio Sniffer connector interface rc=%Vrc\n", rc));
            rc = VERR_PDM_MISSING_INTERFACE;
        }
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s/%d: warning: no driver attached to LUN #0.\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Failed to attach LUN #0. rc=%Vrc\n", rc));
    }

    if (VBOX_SUCCESS (rc))
    {
        /* Save PDM device instance data for future reference. */
        pData->pDevIns = pDevIns;

        /* Save the pointer to created instance in the global variable, so other
         * functions could reach it.
         */
        g_pData = pData;
    }

    return rc;
}

/**
 * Destruct a device instance.
 *
 * Most VM resources are freed by the VM. This callback is provided so that any non-VM
 * resources can be freed correctly.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(int) audioSnifferR3Destruct(PPDMDEVINS pDevIns)
{
    /* Zero the global pointer. */
    g_pData = NULL;

    return VINF_SUCCESS;
}

/**
 * The Audio Sniffer device registration structure.
 */
const PDMDEVREG g_DeviceAudioSniffer =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "AudioSniffer",
    /* szGCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio Sniffer device. Redirects audio data to sniffer driver.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_DEFAULT,
    /* fClass */
    PDM_DEVREG_CLASS_AUDIO,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(AUDIOSNIFFERSTATE),
    /* pfnConstruct */
    audioSnifferR3Construct,
    /* pfnDestruct */
    audioSnifferR3Destruct,
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
    /* pfnQueryInterface */
    NULL,
    /* pfnInitComplete */
    NULL
};
