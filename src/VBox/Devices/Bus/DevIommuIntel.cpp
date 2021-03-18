/* $Id$ */
/** @file
 * IOMMU - Input/Output Memory Management Unit - Intel implementation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_IOMMU
#include <VBox/vmm/pdmdev.h>
#include <VBox/iommu-intel.h>

#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Release log prefix string. */
#define IOMMU_LOG_PFX                               "Intel-IOMMU"
/** The current saved state version. */
#define IOMMU_SAVED_STATE_VERSION                   1


/**
 * The shared IOMMU device state.
 */
typedef struct IOMMU
{
    /** IOMMU device index (0 is at the top of the PCI tree hierarchy). */
    uint32_t                    idxIommu;
    /** IOMMU magic. */
    uint32_t                    u32Magic;

    /** The MMIO handle. */
    IOMMMIOHANDLE               hMmio;

    /** IOMMU registers (group 0). */
    uint8_t                     abRegs0[VTD_MMIO_GROUP_0_SIZE];
    /** IOMMU registers (group 1). */
    uint8_t                     abRegs1[VTD_MMIO_GROUP_1_SIZE];
} IOMMU;
/** Pointer to the IOMMU device state. */
typedef IOMMU *PIOMMU;
/** Pointer to the const IOMMU device state. */
typedef const IOMMU *PCIOMMU;

/**
 * The ring-3 IOMMU device state.
 */
typedef struct IOMMUR3
{
    /** Device instance. */
    PPDMDEVINSR3                pDevInsR3;
    /** The IOMMU helper. */
    R3PTRTYPE(PCPDMIOMMUHLPR3)  pIommuHlpR3;
} IOMMUR3;
/** Pointer to the ring-3 IOMMU device state. */
typedef IOMMUR3 *PIOMMUR3;
/** Pointer to the const ring-3 IOMMU device state. */
typedef const IOMMUR3 *PCIOMMUR3;

/**
 * The ring-0 IOMMU device state.
 */
typedef struct IOMMUR0
{
    /** Device instance. */
    PPDMDEVINSR0                pDevInsR0;
    /** The IOMMU helper. */
    R0PTRTYPE(PCPDMIOMMUHLPR0)  pIommuHlpR0;
} IOMMUR0;
/** Pointer to the ring-0 IOMMU device state. */
typedef IOMMUR0 *PIOMMUR0;
/** Pointer to the const ring-0 IOMMU device state. */
typedef const IOMMUR0 *PCIOMMUR0;

/**
 * The raw-mode IOMMU device state.
 */
typedef struct IOMMURC
{
    /** Device instance. */
    PPDMDEVINSRC                pDevInsRC;
    /** The IOMMU helper. */
    RCPTRTYPE(PCPDMIOMMUHLPRC)  pIommuHlpRC;
} IOMMURC;
/** Pointer to the raw-mode IOMMU device state. */
typedef IOMMURC *PIOMMURC;
/** Pointer to the const raw-mode IOMMU device state. */
typedef const IOMMURC *CPIOMMURC;

/** The IOMMU device state for the current context. */
typedef CTX_SUFF(IOMMU)  IOMMUCC;
/** Pointer to the IOMMU device state for the current context. */
typedef CTX_SUFF(PIOMMU) PIOMMUCC;

/**
 * IOMMU register attributes.
 */
typedef struct IOMMUREGATTR
{
    /** Name of the register. */
    const char   *pszName;
    /** RW: Read/write mask. */
    uint64_t      fRwMask;
    /** RO: Read-only mask. */
    uint64_t      fRoMask;
    /** WO: Write-only mask. */
    uint64_t      fWoMask;
    /** RW1C: Read-only Status, Write-1-to-clear mask. */
    uint64_t      fRw1cMask;
} IOMMUREGATTR;
/** Pointer to an IOMMU register attributes struct. */
typedef IOMMUREGATTR *PIOMMUREGATTR;
/** Pointer to a const IOMMU register attributes struct. */
typedef IOMMUREGATTR const *PCIOMMUREGATTR;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * Memory access bulk (one or more 4K pages) request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   idDevice        The device ID (bus, device, function).
 * @param   cIovas          The number of addresses being accessed.
 * @param   pauIovas        The I/O virtual addresses for each page being accessed.
 * @param   fFlags          The access flags, see PDMIOMMU_MEM_F_XXX.
 * @param   paGCPhysSpa     Where to store the translated physical addresses.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuIntelMemBulkAccess(PPDMDEVINS pDevIns, uint16_t idDevice, size_t cIovas, uint64_t const *pauIovas,
                                                 uint32_t fFlags, PRTGCPHYS paGCPhysSpa)
{
    RT_NOREF6(pDevIns, idDevice, cIovas, pauIovas, fFlags, paGCPhysSpa);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Memory access transaction from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns         The IOMMU device instance.
 * @param   idDevice        The device ID (bus, device, function).
 * @param   uIova           The I/O virtual address being accessed.
 * @param   cbIova          The size of the access.
 * @param   fFlags          The access flags, see PDMIOMMU_MEM_F_XXX.
 * @param   pGCPhysSpa      Where to store the translated system physical address.
 * @param   pcbContiguous   Where to store the number of contiguous bytes translated
 *                          and permission-checked.
 *
 * @thread  Any.
 */
static DECLCALLBACK(int) iommuIntelMemAccess(PPDMDEVINS pDevIns, uint16_t idDevice, uint64_t uIova, size_t cbIova,
                                             uint32_t fFlags, PRTGCPHYS pGCPhysSpa, size_t *pcbContiguous)
{
    RT_NOREF7(pDevIns, idDevice, uIova, cbIova, fFlags, pGCPhysSpa, pcbContiguous);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Interrupt remap request from a device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The IOMMU device instance.
 * @param   idDevice    The device ID (bus, device, function).
 * @param   pMsiIn      The source MSI.
 * @param   pMsiOut     Where to store the remapped MSI.
 */
static DECLCALLBACK(int) iommuIntelMsiRemap(PPDMDEVINS pDevIns, uint16_t idDevice, PCMSIMSG pMsiIn, PMSIMSG pMsiOut)
{
    RT_NOREF4(pDevIns, idDevice, pMsiIn, pMsiOut);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNIOMMMIONEWWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuIntelMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void const *pv, unsigned cb)
{
    RT_NOREF5(pDevIns, pvUser, off, pv, cb);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @callback_method_impl{FNIOMMMIONEWREAD}
 */
static DECLCALLBACK(VBOXSTRICTRC) iommuIntelMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS off, void *pv, unsigned cb)
{
    RT_NOREF5(pDevIns, pvUser, off, pv, cb);
    return VERR_NOT_IMPLEMENTED;
}


#ifdef IN_RING3
/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) iommuIntelR3Reset(PPDMDEVINS pDevIns)
{
    RT_NOREF1(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnDestruct}
 */
static DECLCALLBACK(int) iommuIntelR3Destruct(PPDMDEVINS pDevIns)
{
    RT_NOREF(pDevIns);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    RT_NOREF(pCfg);

    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUR3 pThisR3 = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUR3);
    pThisR3->pDevInsR3 = pDevIns;

    LogFlowFunc(("iInstance=%d\n", iInstance));
    NOREF(iInstance);

    /*
     * Register the IOMMU with PDM.
     */
    PDMIOMMUREGR3 IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version       = PDM_IOMMUREGCC_VERSION;
    IommuReg.pfnMemAccess     = iommuIntelMemAccess;
    IommuReg.pfnMemBulkAccess = iommuIntelMemBulkAccess;
    IommuReg.pfnMsiRemap      = iommuIntelMsiRemap;
    IommuReg.u32TheEnd        = PDM_IOMMUREGCC_VERSION;
    int rc = PDMDevHlpIommuRegister(pDevIns, &IommuReg, &pThisR3->CTX_SUFF(pIommuHlp), &pThis->idxIommu);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Failed to register ourselves as an IOMMU device"));
    if (pThisR3->CTX_SUFF(pIommuHlp)->u32Version != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper version mismatch; got %#x expected %#x"),
                                   pThisR3->CTX_SUFF(pIommuHlp)->u32Version, PDM_IOMMUHLPR3_VERSION);
    if (pThisR3->CTX_SUFF(pIommuHlp)->u32TheEnd != PDM_IOMMUHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("IOMMU helper end-version mismatch; got %#x expected %#x"),
                                   pThisR3->CTX_SUFF(pIommuHlp)->u32TheEnd, PDM_IOMMUHLPR3_VERSION);
    /*
     * Use PDM's critical section (via helpers) for the IOMMU device.
     */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Initialize read-only PCI configuration space.
     */
    PPDMPCIDEV pPciDev = pDevIns->apPciDevs[0];
    PDMPCIDEV_ASSERT_VALID(pDevIns, pPciDev);

    /** @todo Figure out the PCI vendor/product and other stuff. Can't seem to find
     *        this yet. Maybe dump info from real hw. */

    return VERR_NOT_IMPLEMENTED;
}

#else

/**
 * @callback_method_impl{PDMDEVREGR0,pfnConstruct}
 */
static DECLCALLBACK(int) iommuIntelRZConstruct(PPDMDEVINS pDevIns)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PIOMMU   pThis   = PDMDEVINS_2_DATA(pDevIns, PIOMMU);
    PIOMMUCC pThisCC = PDMDEVINS_2_DATA_CC(pDevIns, PIOMMUCC);
    pThisCC->CTX_SUFF(pDevIns) = pDevIns;

    /* We will use PDM's critical section (via helpers) for the IOMMU device. */
    int rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /* Set up the MMIO RZ handlers. */
    rc = PDMDevHlpMmioSetUpContext(pDevIns, pThis->hMmio, iommuIntelMmioWrite, iommuIntelMmioRead, NULL /* pvUser */);
    AssertRCReturn(rc, rc);

    /* Set up the IOMMU RZ callbacks. */
    PDMIOMMUREGCC IommuReg;
    RT_ZERO(IommuReg);
    IommuReg.u32Version       = PDM_IOMMUREGCC_VERSION;
    IommuReg.idxIommu         = pThis->idxIommu;
    IommuReg.pfnMemAccess     = iommuIntelMemAccess;
    IommuReg.pfnMemBulkAccess = iommuIntelMemBulkAccess;
    IommuReg.pfnMsiRemap      = iommuIntelMsiRemap;
    IommuReg.u32TheEnd        = PDM_IOMMUREGCC_VERSION;
    rc = PDMDevHlpIommuSetUpContext(pDevIns, &IommuReg, &pThisCC->CTX_SUFF(pIommuHlp));
    AssertRCReturn(rc, rc);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp), VERR_IOMMU_IPE_1);
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32Version == CTX_SUFF(PDM_IOMMUHLP)_VERSION, VERR_VERSION_MISMATCH);
    AssertReturn(pThisCC->CTX_SUFF(pIommuHlp)->u32TheEnd  == CTX_SUFF(PDM_IOMMUHLP)_VERSION, VERR_VERSION_MISMATCH);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp)->pfnLock,   VERR_INVALID_POINTER);
    AssertPtrReturn(pThisCC->CTX_SUFF(pIommuHlp)->pfnUnlock, VERR_INVALID_POINTER);
    return VINF_SUCCESS;
}

#endif


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceIommuIntel =
{
    /* .u32Version = */             PDM_DEVREG_VERSION,
    /* .uReserved0 = */             0,
    /* .szName = */                 "iommu-intel",
    /* .fFlags = */                 PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RZ | PDM_DEVREG_FLAGS_NEW_STYLE,
    /* .fClass = */                 PDM_DEVREG_CLASS_PCI_BUILTIN,
    /* .cMaxInstances = */          ~0U,
    /* .uSharedVersion = */         42,
    /* .cbInstanceShared = */       sizeof(IOMMU),
    /* .cbInstanceCC = */           sizeof(IOMMUCC),
    /* .cbInstanceRC = */           sizeof(IOMMURC),
    /* .cMaxPciDevices = */         1,
    /* .cMaxMsixVectors = */        0,
    /* .pszDescription = */         "IOMMU (Intel)",
#if defined(IN_RING3)
    /* .pszRCMod = */               "VBoxDDRC.rc",
    /* .pszR0Mod = */               "VBoxDDR0.r0",
    /* .pfnConstruct = */           iommuIntelR3Construct,
    /* .pfnDestruct = */            iommuIntelR3Destruct,
    /* .pfnRelocate = */            NULL,
    /* .pfnMemSetup = */            NULL,
    /* .pfnPowerOn = */             NULL,
    /* .pfnReset = */               iommuIntelR3Reset,
    /* .pfnSuspend = */             NULL,
    /* .pfnResume = */              NULL,
    /* .pfnAttach = */              NULL,
    /* .pfnDetach = */              NULL,
    /* .pfnQueryInterface = */      NULL,
    /* .pfnInitComplete = */        NULL,
    /* .pfnPowerOff = */            NULL,
    /* .pfnSoftReset = */           NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RING0)
    /* .pfnEarlyConstruct = */      NULL,
    /* .pfnConstruct = */           iommuIntelRZConstruct,
    /* .pfnDestruct = */            NULL,
    /* .pfnFinalDestruct = */       NULL,
    /* .pfnRequest = */             NULL,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#elif defined(IN_RC)
    /* .pfnConstruct = */           iommuIntelRZConstruct,
    /* .pfnReserved0 = */           NULL,
    /* .pfnReserved1 = */           NULL,
    /* .pfnReserved2 = */           NULL,
    /* .pfnReserved3 = */           NULL,
    /* .pfnReserved4 = */           NULL,
    /* .pfnReserved5 = */           NULL,
    /* .pfnReserved6 = */           NULL,
    /* .pfnReserved7 = */           NULL,
#else
# error "Not in IN_RING3, IN_RING0 or IN_RC!"
#endif
    /* .u32VersionEnd = */          PDM_DEVREG_VERSION
};

#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */

