/* $Id$ */
/** @file
 * DevPlayground - Device for making PDM/PCI/... experiments.
 *
 * This device uses big PCI BAR64 resources, which needs the ICH9 chipset.
 * The device works without any PCI config (because the default setup with the
 * ICH9 chipset doesn't have anything at bus=0, device=0, function=0.
 *
 * To enable this device for a particular VM:
 * VBoxManage setextradata vmname VBoxInternal/PDM/Devices/playground/Path .../obj/VBoxSampleDevice/VBoxSampleDevice
 * VBoxManage setextradata vmname VBoxInternal/Devices/playground/0/Config/Whatever1 0
 */

/*
 * Copyright (C) 2009-2016 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_MISC
#include <VBox/vmm/pdmdev.h>
#include <VBox/version.h>
#include <VBox/err.h>
#include <VBox/log.h>

#include <iprt/assert.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Device Instance Data.
 */
typedef struct VBOXPLAYGROUNDDEVICE
{
    /** The PCI device. */
    PCIDEVICE           PciDev;
} VBOXPLAYGROUNDDEVICE;
typedef VBOXPLAYGROUNDDEVICE *PVBOXPLAYGROUNDDEVICE;


/*********************************************************************************************************************************
*   Device Functions                                                                                                             *
*********************************************************************************************************************************/
/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) devPlaygroundDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */
    AssertMsgReturn(pDevIns->u32Version            == PDM_DEVINS_VERSION, ("%#x, expected %#x\n", pDevIns->u32Version,            PDM_DEVINS_VERSION), VERR_VERSION_MISMATCH);
    AssertMsgReturn(pDevIns->pHlpR3->u32Version == PDM_DEVHLPR3_VERSION, ("%#x, expected %#x\n", pDevIns->pHlpR3->u32Version, PDM_DEVHLPR3_VERSION), VERR_VERSION_MISMATCH);

    return VINF_SUCCESS;
}


PDMBOTHCBDECL(int) devPlaygroundMMIORead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    NOREF(pDevIns);
    NOREF(pvUser);
    NOREF(GCPhysAddr);
    NOREF(pv);
    NOREF(cb);
    return VINF_SUCCESS;
}


PDMBOTHCBDECL(int) devPlaygroundMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    NOREF(pDevIns);
    NOREF(pvUser);
    NOREF(GCPhysAddr);
    NOREF(pv);
    NOREF(cb);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNPCIIOREGIONMAP}
 */
static DECLCALLBACK(int) devPlaygroundMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    NOREF(enmType);
    int rc;

    switch (iRegion)
    {
        case 0:
            rc = PDMDevHlpMMIORegister(pPciDev->pDevIns, GCPhysAddress, cb, NULL,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                       devPlaygroundMMIOWrite, devPlaygroundMMIORead, "PG-BAR0");
            break;
        case 2:
            rc = PDMDevHlpMMIORegister(pPciDev->pDevIns, GCPhysAddress, cb, NULL,
                                       IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                                       devPlaygroundMMIOWrite, devPlaygroundMMIORead, "PG-BAR2");
            break;
        default:
            /* We should never get here */
            AssertMsgFailed(("Invalid PCI region param in map callback"));
            rc = VERR_INTERNAL_ERROR;
    }
    return rc;

}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) devPlaygroundConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    NOREF(iInstance);

    /*
     * Check that the device instance and device helper structures are compatible.
     */
    AssertLogRelMsgReturn(pDevIns->u32Version            == PDM_DEVINS_VERSION, ("%#x, expected %#x\n", pDevIns->u32Version,            PDM_DEVINS_VERSION), VERR_VERSION_MISMATCH);
    AssertLogRelMsgReturn(pDevIns->pHlpR3->u32Version == PDM_DEVHLPR3_VERSION, ("%#x, expected %#x\n", pDevIns->pHlpR3->u32Version, PDM_DEVHLPR3_VERSION), VERR_VERSION_MISMATCH);

    /*
     * Initialize the instance data so that the destructor won't mess up.
     */
    PVBOXPLAYGROUNDDEVICE pThis = PDMINS_2_DATA(pDevIns, PVBOXPLAYGROUNDDEVICE);
    PCIDevSetVendorId(&pThis->PciDev, 0x80ee);
    PCIDevSetDeviceId(&pThis->PciDev, 0xde4e);
    PCIDevSetClassBase(&pThis->PciDev, 0x07);   /* communications device */
    PCIDevSetClassSub(&pThis->PciDev, 0x80);    /* other communications device */

    /*
     * Validate and read the configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "Whatever1\0"
                              "Whatever2\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /*
     * PCI device setup.
     */
    int rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, 8*_1G64,
                                      (PCIADDRESSSPACE)(PCI_ADDRESS_SPACE_MEM | PCI_ADDRESS_SPACE_BAR64),
                                      devPlaygroundMap);
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, 8*_1G64,
                                      (PCIADDRESSSPACE)(PCI_ADDRESS_SPACE_MEM | PCI_ADDRESS_SPACE_BAR64),
                                      devPlaygroundMap);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}

RT_C_DECLS_BEGIN
extern const PDMDEVREG g_DevicePlayground;
RT_C_DECLS_END

/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePlayground =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "playground",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "VBox Playground Device.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(VBOXPLAYGROUNDDEVICE),
    /* pfnConstruct */
    devPlaygroundConstruct,
    /* pfnDestruct */
    devPlaygroundDestruct,
    /* pfnRelocate */
    NULL,
    /* pfnMemSetup */
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
