/* $Id$ */
/** @file
 * Guest Interface Manager Device.
 */

/*
 * Copyright (C) 2014-2015 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_GIM
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/gim.h>
#include <VBox/vmm/vm.h>

#include "VBoxDD.h"
#include <iprt/uuid.h>

#define GIMDEV_DEBUG_LUN                998

/**
 * GIM device.
 */
typedef struct GIMDEV
{
    /** Pointer to the device instance - R3 Ptr. */
    PPDMDEVINSR3                    pDevInsR3;
    /** Pointer to the device instance - R0 Ptr. */
    PPDMDEVINSR0                    pDevInsR0;
    /** Pointer to the device instance - RC Ptr. */
    PPDMDEVINSRC                    pDevInsRC;
    /** Alignment. */
    RTRCPTR                         Alignment0;

    /** LUN\#998: The debug interface. */
    PDMIBASE                        IDbgBase;
    /** LUN\#998: The stream port interface. */
    PDMISTREAM                      IDbgStreamPort;
    /** Pointer to the attached base debug driver. */
    R3PTRTYPE(PPDMIBASE)            pDbgDrvBase;
    /** Pointer to the attached debug stream driver. */
    R3PTRTYPE(PPDMISTREAM)          pDbgDrvStream;
} GIMDEV;
/** Pointer to the GIM device state. */
typedef GIMDEV *PGIMDEV;
AssertCompileMemberAlignment(GIMDEV, IDbgBase, 8);

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

#ifdef IN_RING3


/* -=-=-=-=-=-=-=-=- PDMIBASE on LUN#GIMDEV_DEBUG_LUN -=-=-=-=-=-=-=-=- */

/**
 * @interface_method_impl{PDMIBASE, pfnQueryInterface}
 */
static DECLCALLBACK(void *) gimdevR3QueryInterface(PPDMIBASE pInterface, const char *pszIID)
{
    PGIMDEV pThis = RT_FROM_MEMBER(pInterface, GIMDEV, IDbgBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMIBASE, &pThis->IDbgBase);
    PDMIBASE_RETURN_INTERFACE(pszIID, PDMISTREAM, &pThis->IDbgStreamPort);
    return NULL;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) gimdevR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    Assert(iInstance == 0);
    PGIMDEV pThis = PDMINS_2_DATA(pDevIns, PGIMDEV);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Initialize relevant state bits.
     */
    pThis->pDevInsR3  = pDevIns;
    pThis->pDevInsR0  = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC  = PDMDEVINS_2_RCPTR(pDevIns);

    /*
     * Attach the stream driver for the debug connection.
     */
    pThis->IDbgBase.pfnQueryInterface = gimdevR3QueryInterface;
    int rc = PDMDevHlpDriverAttach(pDevIns, GIMDEV_DEBUG_LUN, &pThis->IDbgBase, &pThis->pDbgDrvBase, "GIM Debug Port");
    if (RT_SUCCESS(rc))
    {
        pThis->pDbgDrvStream = PDMIBASE_QUERY_INTERFACE(pThis->pDbgDrvBase, PDMISTREAM);
        if (pThis->pDbgDrvStream)
            LogRel(("GIMDev: LUN#%u: Debug port configured\n", GIMDEV_DEBUG_LUN));
        else
            LogRel(("GIMDev: LUN#%u: No unit\n", GIMDEV_DEBUG_LUN));
    }
    else if (rc == VERR_PDM_NO_ATTACHED_DRIVER)
    {
        pThis->pDbgDrvBase   = NULL;
        pThis->pDbgDrvStream = NULL;
        LogRel(("GIMDev: LUN#%u: No debug port configured\n", GIMDEV_DEBUG_LUN));
    }
    else
    {
        AssertLogRelMsgFailed(("GIMDev: LUN#%u: Failed to attach to driver on debug port. rc=%Rrc\n", GIMDEV_DEBUG_LUN, rc));
        /* Don't call VMSetError here as we assume that the driver already set an appropriate error */
        return rc;
    }

    /*
     * Register ourselves with the GIM VMM component.
     */
    PVM pVM = PDMDevHlpGetVM(pDevIns);
    GIMR3GimDeviceRegister(pVM, pDevIns, pThis->pDbgDrvStream);

    /*
     * Get the MMIO2 regions from the GIM provider.
     */
    uint32_t cRegions = 0;
    PGIMMMIO2REGION pRegionsR3 = GIMR3GetMmio2Regions(pVM, &cRegions);
    if (   cRegions
        && pRegionsR3)
    {
        /*
         * Register the MMIO2 regions.
         */
        PGIMMMIO2REGION pCur = pRegionsR3;
        for (uint32_t i = 0; i < cRegions; i++, pCur++)
        {
            Assert(!pCur->fRegistered);
            rc = PDMDevHlpMMIO2Register(pDevIns, pCur->iRegion, pCur->cbRegion, 0 /* fFlags */, &pCur->pvPageR3,
                                            pCur->szDescription);
            if (RT_FAILURE(rc))
                return rc;

            pCur->fRegistered = true;

#if defined(VBOX_WITH_2X_4GB_ADDR_SPACE)
            RTR0PTR pR0Mapping = 0;
            rc = PDMDevHlpMMIO2MapKernel(pDevIns, pCur->iRegion, 0 /* off */, pCur->cbRegion, pCur->szDescription,
                                         &pR0Mapping);
            AssertLogRelMsgRCReturn(rc, ("PDMDevHlpMapMMIO2IntoR0(%#x,) -> %Rrc\n", pCur->cbRegion, rc), rc);
            pCur->pvPageR0 = pR0Mapping;
#else
            pCur->pvPageR0 = (RTR0PTR)pCur->pvPageR3;
#endif

            /*
             * Map into RC if required.
             */
            if (pCur->fRCMapping)
            {
                RTRCPTR pRCMapping = 0;
                rc = PDMDevHlpMMHyperMapMMIO2(pDevIns, pCur->iRegion, 0 /* off */, pCur->cbRegion, pCur->szDescription,
                                              &pRCMapping);
                AssertLogRelMsgRCReturn(rc, ("PDMDevHlpMMHyperMapMMIO2(%#x,) -> %Rrc\n", pCur->cbRegion, rc), rc);
                pCur->pvPageRC = pRCMapping;
            }
            else
                pCur->pvPageRC = NIL_RTRCPTR;

            LogRel(("GIMDev: Registered %s\n", pCur->szDescription));
        }
    }

    /** @todo Register SSM: PDMDevHlpSSMRegister(). */
    /** @todo Register statistics: STAM_REG(). */
    /** @todo Register DBGFInfo: PDMDevHlpDBGFInfoRegister(). */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) gimdevR3Destruct(PPDMDEVINS pDevIns)
{
    PGIMDEV  pThis    = PDMINS_2_DATA(pDevIns, PGIMDEV);
    PVM      pVM      = PDMDevHlpGetVM(pDevIns);
    uint32_t cRegions = 0;

    PGIMMMIO2REGION pCur = GIMR3GetMmio2Regions(pVM, &cRegions);
    for (uint32_t i = 0; i < cRegions; i++, pCur++)
    {
        int rc = PDMDevHlpMMIO2Deregister(pDevIns, pCur->iRegion);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) gimdevR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    NOREF(pDevIns);
    NOREF(offDelta);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) gimdevR3Reset(PPDMDEVINS pDevIns)
{
    NOREF(pDevIns);
    /* We do not deregister any MMIO2 regions as the regions are expected to be static. */
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceGIMDev =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "GIMDev",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "VirtualBox GIM Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_R0 | PDM_DEVREG_FLAGS_RC,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(GIMDEV),
    /* pfnConstruct */
    gimdevR3Construct,
    /* pfnDestruct */
    gimdevR3Destruct,
    /* pfnRelocate */
    gimdevR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    gimdevR3Reset,
    /* pfnSuspend */
    NULL,
    /* pfnResume */
    NULL,
    /* pfnAttach */
    NULL,
    /* pfnDetach */
    NULL,
    /* pfnQueryInterface. */
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
#endif /* IN_RING3 */

#endif  /* VBOX_DEVICE_STRUCT_TESTCASE */

