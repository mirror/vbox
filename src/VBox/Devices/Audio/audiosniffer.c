/* $Id$ */
/** @file
 * VBox audio device: Audio sniffer device
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
    PDMIBASE                     IBase;
    /** Audio Sniffer port interface. */
    PDMIAUDIOSNIFFERPORT         IPort;

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

static DECLCALLBACK(int) iface_Setup (PPDMIAUDIOSNIFFERPORT pInterface, bool fEnable, bool fKeepHostAudio)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IPort);

    Assert(g_pData == pThis);

    pThis->fEnabled = fEnable;
    pThis->fKeepHostAudio = fKeepHostAudio;

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMIBASE,pfnQueryInterface}
 */
static DECLCALLBACK(void *) iface_QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    AUDIOSNIFFERSTATE *pThis = RT_FROM_MEMBER(pInterface, AUDIOSNIFFERSTATE, IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIAUDIOSNIFFERPORT, &pThis->IPort);
    return NULL;
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
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    /* Zero the global pointer. */
    g_pData = NULL;

    return VINF_SUCCESS;
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
    int                rc    = VINF_SUCCESS;
    AUDIOSNIFFERSTATE *pThis = PDMINS_2_DATA(pDevIns, AUDIOSNIFFERSTATE *);

    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

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
    pThis->fEnabled = false;
    pThis->fKeepHostAudio = true;
    pThis->pDrv = NULL;

    /*
     * Interfaces
     */
    /* Base */
    pThis->IBase.pfnQueryInterface = iface_QueryInterface;

    /* Audio Sniffer port */
    pThis->IPort.pfnSetup = iface_Setup;

    /*
     * Get the corresponding connector interface
     */
    rc = PDMDevHlpDriverAttach(pDevIns, 0, &pThis->IBase, &pThis->pDrvBase, "Audio Sniffer Port");

    if (RT_SUCCESS(rc))
    {
        pThis->pDrv = PDMIBASE_QUERY_INTERFACE(pThis->pDrvBase, PDMIAUDIOSNIFFERCONNECTOR);
        AssertMsgStmt(pThis->pDrv, ("LUN #0 doesn't have a Audio Sniffer connector interface rc=%Rrc\n", rc),
                      rc = VERR_PDM_MISSING_INTERFACE);
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        Log(("%s/%d: warning: no driver attached to LUN #0.\n", pDevIns->pDevReg->szDeviceName, pDevIns->iInstance));
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Failed to attach LUN #0. rc=%Rrc\n", rc));
    }

    if (RT_SUCCESS (rc))
    {
        /* Save PDM device instance data for future reference. */
        pThis->pDevIns = pDevIns;

        /* Save the pointer to created instance in the global variable, so other
         * functions could reach it.
         */
        g_pData = pThis;
    }

    return rc;
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
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Audio Sniffer device. Redirects audio data to sniffer driver.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
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
    NULL,
    /* pfnPowerOff */
    NULL,
    /* pfnSoftReset */
    NULL,
    PDM_DEVREG_VERSION
};
