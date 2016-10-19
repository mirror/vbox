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
static DECLCALLBACK(int)
devPlaygroundMap(PPCIDEVICE pPciDev, int iRegion, RTGCPHYS GCPhysAddress, RTGCPHYS cb, PCIADDRESSSPACE enmType)
{
    RT_NOREF(enmType, cb);

    switch (iRegion)
    {
        case 0:
        case 2:
            Assert(enmType == (PCIADDRESSSPACE)(PCI_ADDRESS_SPACE_MEM | PCI_ADDRESS_SPACE_BAR64));
            if (GCPhysAddress == NIL_RTGCPHYS)
                return VINF_SUCCESS; /* We ignore the unmap notification. */
            return PDMDevHlpMMIOExMap(pPciDev->pDevIns, iRegion, GCPhysAddress);

        default:
            /* We should never get here */
            AssertMsgFailedReturn(("Invalid PCI region param in map callback"), VERR_INTERNAL_ERROR);
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) devPlaygroundDestruct(PPDMDEVINS pDevIns)
{
    /*
     * Check the versions here as well since the destructor is *always* called.
     */
    PDMDEV_CHECK_VERSIONS_RETURN_QUIET(pDevIns);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) devPlaygroundConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RT_NOREF(iInstance, pCfg);

    /*
     * Check that the device instance and device helper structures are compatible.
     */
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

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
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "Whatever1|Whatever2", "");

    /*
     * PCI device setup.
     */
    int rc = PDMDevHlpPCIRegister(pDevIns, &pThis->PciDev);
    if (RT_FAILURE(rc))
        return rc;

    /* First region. */
    RTGCPHYS const cbFirst = 8*_1G64;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 0, cbFirst,
                                      (PCIADDRESSSPACE)(PCI_ADDRESS_SPACE_MEM | PCI_ADDRESS_SPACE_BAR64),
                                      devPlaygroundMap);
    AssertLogRelRCReturn(rc, rc);
    rc = PDMDevHlpMMIOExPreRegister(pDevIns, 0, cbFirst, IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU, "PG-BAR0",
                                    NULL /*pvUser*/,  devPlaygroundMMIOWrite, devPlaygroundMMIORead, NULL /*pfnFill*/,
                                    NIL_RTR0PTR /*pvUserR0*/, NULL /*pszWriteR0*/, NULL /*pszReadR0*/, NULL /*pszFillR0*/,
                                    NIL_RTRCPTR /*pvUserRC*/, NULL /*pszWriteRC*/, NULL /*pszReadRC*/, NULL /*pszFillRC*/);
    AssertLogRelRCReturn(rc, rc);

    /* Second region. */
    RTGCPHYS const cbSecond = 256*_1G64;
    rc = PDMDevHlpPCIIORegionRegister(pDevIns, 2, cbSecond,
                                      (PCIADDRESSSPACE)(PCI_ADDRESS_SPACE_MEM | PCI_ADDRESS_SPACE_BAR64),
                                      devPlaygroundMap);
    AssertLogRelRCReturn(rc, rc);
    rc = PDMDevHlpMMIOExPreRegister(pDevIns, 2, cbSecond, IOMMMIO_FLAGS_READ_PASSTHRU | IOMMMIO_FLAGS_WRITE_PASSTHRU, "PG-BAR2",
                                    NULL /*pvUser*/,  devPlaygroundMMIOWrite, devPlaygroundMMIORead, NULL /*pfnFill*/,
                                    NIL_RTR0PTR /*pvUserR0*/, NULL /*pszWriteR0*/, NULL /*pszReadR0*/, NULL /*pszFillR0*/,
                                    NIL_RTRCPTR /*pvUserRC*/, NULL /*pszWriteRC*/, NULL /*pszReadRC*/, NULL /*pszFillRC*/);
    AssertLogRelRCReturn(rc, rc);

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
