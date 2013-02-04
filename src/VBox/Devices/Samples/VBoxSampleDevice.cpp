/* $Id$ */
/** @file
 * VBox Sample Device.
 */

/*
 * Copyright (C) 2009-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_MISC
#include <VBox/vmm/pdmdev.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Device Instance Data.
 */
typedef struct VBOXSAMPLEDEVICE
{
    uint32_t    Whatever;
} VBOXSAMPLEDEVICE;
typedef VBOXSAMPLEDEVICE *PVBOXSAMPLEDEVICE;




    FNPDMDEVCONSTRUCT  pfnConstruct;
    /** Destruct instance - optional. */
    FNPDMDEVDESTRUCT   pfnDestruct;

static DECLCALLBACK(int) devSampleDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */
    AssertMsgReturn(pDevIns->u32Version            == PDM_DEVINS_VERSION, ("%#x, expected %#x\n", pDevIns->u32Version,            PDM_DEVINS_VERSION), VERR_VERSION_MISMATCH);
    AssertMsgReturn(pDevIns->pHlpR3->u32Version == PDM_DEVHLPR3_VERSION, ("%#x, expected %#x\n", pDevIns->pHlpR3->u32Version, PDM_DEVHLPR3_VERSION), VERR_VERSION_MISMATCH);

    return VINF_SUCCESS;
}


static DECLCALLBACK(int) devSampleConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    /*
     * Check that the device instance and device helper structures are compatible.
     */
    AssertLogRelMsgReturn(pDevIns->u32Version            == PDM_DEVINS_VERSION, ("%#x, expected %#x\n", pDevIns->u32Version,            PDM_DEVINS_VERSION), VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pDevIns->pHlpR3->u32Version == PDM_DEVHLPR3_VERSION, ("%#x, expected %#x\n", pDevIns->pHlpR3->u32Version, PDM_DEVHLPR3_VERSION), VERR_VERSION_MISMATCH);

    /*
     * Initialize the instance data so that the destructure won't mess up.
     */
    PVBOXSAMPLEDEVICE pThis = PDMINS_2_DATA(pDevIns, PVBOXSAMPLEDEVICE);
    pThis->Whatever = 0;

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "Whatever1\0"
                              "Whatever2\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;


    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
static const PDMDEVREG g_DeviceSample =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "sample",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "VBox Sample Device.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(VBOXSAMPLEDEVICE),
    /* pfnConstruct */
    devSampleConstruct,
    /* pfnDestruct */
    devSampleDestruct,
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
    /* u32VersionEnd */
    PDM_DEVREG_VERSION
};


/**
 * Register builtin devices.
 *
 * @returns VBox status code.
 * @param   pCallbacks      Pointer to the callback table.
 * @param   u32Version      VBox version number.
 */
extern "C" DECLEXPORT(int) VBoxDevicesRegister(PPDMDEVREGCB pCallbacks, uint32_t u32Version)
{
    LogFlow(("VBoxSampleDevice::VBoxDevicesRegister: u32Version=%#x pCallbacks->u32Version=%#x\n", u32Version, pCallbacks->u32Version));

    AssertLogRelMsgReturn(pCallbacks->u32Version == PDM_DEVREG_CB_VERSION,
                          ("%#x, expected %#x\n", pCallbacks->u32Version, PDM_DEVREG_CB_VERSION),
                          VERR_VERSION_MISMATCH);

    return pCallbacks->pfnRegister(pCallbacks, &g_DeviceSample);
}

