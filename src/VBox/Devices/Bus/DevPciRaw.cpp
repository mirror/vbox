/* $Id$ */
/** @file
 * PCI passthrough device emulation.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_PCI
#include <VBox/pdmdev.h>
#include <VBox/log.h>
#include <VBox/stam.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "../Builtins.h"

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The version of the saved state. */
#define PCIRAW_SAVED_STATE_VERSION       1

/*******************************************************************************
 *   Structures and Typedefs                                                    *
 *******************************************************************************/

/* Temporary PDM stubs */
typedef struct PDMPCIRAWREG
{
    /** Struct version+magic number (PDM_PCIRAWREG_VERSION). */
    uint32_t            u32Version;

} PDMPCIRAWREG;
/** Pointer to a raw PCI registration structure. */
typedef PDMPCIRAWREG *PPDMPCIRAWREG;

/** Current PDMPCIRAWREG version number. */
#define PDM_PCIRAWREG_VERSION                     PDM_VERSION_MAKE(0xffe3, 1, 0)

struct PDMPCIRAWHLPRC
{
    uint32_t u32Version;
};
typedef RCPTRTYPE(PDMPCIRAWHLPRC *) PPDMPCIRAWHLPRC;
typedef RCPTRTYPE(const PDMPCIRAWHLPRC *) PCPDMPCIRAWHLPRC;

struct PDMPCIRAWHLPR0
{
    uint32_t u32Version;
};
typedef R0PTRTYPE(PDMPCIRAWHLPR0 *) PPDMPCIRAWHLPR0;
typedef R0PTRTYPE(const PDMPCIRAWHLPR0 *) PCPDMPCIRAWHLPR0;

struct PDMPCIRAWHLPR3
{
    uint32_t u32Version;
    /**
     * Gets the address of the RC PCI raw helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the RC helpers.
     *
     * @returns RC pointer to the PCI raw helpers.
     * @param   pDevIns         Device instance of the raw PCI device.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIRAWHLPRC, pfnGetRCHelpers,(PPDMDEVINS pDevIns));

    /**
     * Gets the address of the R0 PCI raw helpers.
     *
     * This should be called at both construction and relocation time
     * to obtain the correct address of the R0 helpers.
     *
     * @returns R0 pointer to the PCI raw helpers.
     * @param   pDevIns         Device instance of the raw PCI device.
     */
    DECLR3CALLBACKMEMBER(PCPDMPCIRAWHLPR0, pfnGetR0Helpers,(PPDMDEVINS pDevIns));

    /** Just a safety precaution. */
    uint32_t                u32TheEnd;
};
/** Pointer to raw PCI R3 helpers. */
typedef R3PTRTYPE(PDMPCIRAWHLPR3 *) PPDMPCIRAWHLPR3;
/** Pointer to const raw PCI R3 helpers. */
typedef R3PTRTYPE(const PDMPCIRAWHLPR3 *) PCPDMPCIRAWHLPR3;

/**
 * @copydoc PDMDEVHLPR3::pfnPciRawRegister
 */
DECLINLINE(int) PDMDevHlpPciRawRegister(PPDMDEVINS pDevIns, PPDMPCIRAWREG pPciRawReg, PCPDMPCIRAWHLPR3 *ppPciRawHlpR3)
{
    //return pDevIns->pHlpR3->pfnPciRawRegister(pDevIns, pPciRawReg, ppPciRawHlpR3);
    return VINF_SUCCESS;
}

/* End of PDM stubs */

typedef struct PciRawState
{
    /** Pointer to the device instance. - R3 ptr. */
    PPDMDEVINSR3         pDevInsR3;
    /** The PCI raw helpers - R3 Ptr. */
    PCPDMPCIRAWHLPR3     pPciRawHlpR3;

    /** Pointer to the device instance. - R0 ptr. */
    PPDMDEVINSR0         pDevInsR0;
    /** The PCI raw helpers - R0 Ptr. */
    PCPDMPCIRAWHLPR0     pPciRawHlpR0;

    /** Pointer to the device instance. - RC ptr. */
    PPDMDEVINSRC         pDevInsRC;
    /** The PCI raw helpers - RC Ptr. */
    PCPDMPCIRAWHLPRC     pPciRawHlpRC;

    /* Virtual PCI device */
    PCIDEVICE            aPciDevice;

    /* Address of device on the host */
    PciBusAddress        aHostDeviceAddress;
    /* Address of device in the guest */
    PciBusAddress        aGuestDeviceAddress;

    /* Global device lock */
    PDMCRITSECT          csLock;
} PciRawState;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN
PDMBOTHCBDECL(int) pcirawMMIOWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) pcirawMMIORead (PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb);
PDMBOTHCBDECL(int) pcirawIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int) pcirawIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                     RTIOPORT Port, uint32_t *pu32, unsigned cb);
RT_C_DECLS_END

/*
 * Temporary control to disable locking if problems found
 */
DECLINLINE(int) pcirawLock(PciRawState* pThis, int rcBusy)
{
    return PDMCritSectEnter(&pThis->csLock, rcBusy);
}

DECLINLINE(void) pcirawUnlock(PciRawState* pThis)
{
    PDMCritSectLeave(&pThis->csLock);
}


PDMBOTHCBDECL(int)  pcirawMMIORead(PPDMDEVINS pDevIns,
                                   void *     pvUser,
                                   RTGCPHYS   GCPhysAddr,
                                   void *     pv,
                                   unsigned   cb)
{
    PciRawState * pThis  = PDMINS_2_DATA(pDevIns, PciRawState*);
    int         rc     = VINF_SUCCESS;

    LogFlow(("pcirawMMIORead: %llx (%x)\n", (uint64_t)GCPhysAddr, cb));

    rc = pcirawLock(pThis, VINF_IOM_HC_MMIO_READ);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VINF_SUCCESS;
    }

    pcirawUnlock(pThis);

    return rc;
}

PDMBOTHCBDECL(int) pcirawMMIOWrite(PPDMDEVINS pDevIns,
                                   void  *    pvUser,
                                   RTGCPHYS   GCPhysAddr,
                                   void *     pv,
                                   unsigned   cb)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState*);
    int rc = VINF_SUCCESS;

    LogFlow(("pcirawMMIOWrite: %llx (%d) <- %x\n",
             (uint64_t)GCPhysAddr, cb, *(uint32_t*)pv));

    rc = pcirawLock(pThis, VINF_IOM_HC_MMIO_WRITE);
    if (RT_UNLIKELY(rc != VINF_SUCCESS))
        return rc;

    switch (cb)
    {
        case 1:
        case 2:
        case 4:
        case 8:
        {
            break;
        }

        default:
            AssertReleaseMsgFailed(("cb=%d\n", cb)); /* for now we assume simple accesses. */
            rc = VERR_INTERNAL_ERROR;
    }

    pcirawUnlock(pThis);

    return rc;
}

PDMBOTHCBDECL(int) pcirawIOPortWrite (PPDMDEVINS pDevIns, void *pvUser,
                                      RTIOPORT Port, uint32_t u32, unsigned cb)
{
    return VINF_SUCCESS;
}

PDMBOTHCBDECL(int) pcirawIOPortRead (PPDMDEVINS pDevIns, void *pvUser,
                                     RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    return VINF_SUCCESS;
}

#ifdef IN_RING3
/**
 * @copydoc FNSSMDEVLIVEEXEC
 */
static DECLCALLBACK(int) pcirawLiveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uPass)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);


    return VINF_SSM_DONT_CALL_AGAIN;
}

/**
 * Saves a state of the raw PCI device. Do nothing yet.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) pcirawSaveExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSM)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    /* The config. */
    pcirawLiveExec(pDevIns, pSSM, SSM_PASS_FINAL);

    return VINF_SUCCESS;
}

/**
 * Loads a state of the raw PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   uVersion    The data unit version number.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) pcirawLoadExec(PPDMDEVINS pDevIns,
                                        PSSMHANDLE pSSM,
                                        uint32_t   uVersion,
                                        uint32_t   uPass)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    int        rc;

    if (uVersion != PCIRAW_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;

    return VINF_SUCCESS;
}

/**
 * Relocation notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 * @param   offDelta    The delta relative to the old address.
 */
static DECLCALLBACK(void) pcirawRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    unsigned i;
    LogFlow(("pcirawRelocate:\n"));

    pThis->pDevInsRC    = PDMDEVINS_2_RCPTR(pDevIns);
    pThis->pPciRawHlpRC   = pThis->pPciRawHlpR3->pfnGetRCHelpers(pDevIns);
}

/**
 * Reset notification.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static DECLCALLBACK(void) pcirawReset(PPDMDEVINS pDevIns)
{
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    unsigned i;

    LogFlow(("pcirawReset:\n"));
}

/**
 * Initialization routine.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 */
static int pcirawInit(PPDMDEVINS pDevIns, PciBusAddress hostAddress)
{
    unsigned   i;
    int        rc;
    PciRawState *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);

    memset(pThis, 0, sizeof(*pThis));

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0  = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC  = PDMDEVINS_2_RCPTR(pDevIns);

    pThis->aHostDeviceAddress.init(hostAddress);

    pcirawReset(pDevIns);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) pcirawConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PciRawState  *pThis = PDMINS_2_DATA(pDevIns, PciRawState *);
    int          rc;
    bool         fRCEnabled = false;
    bool         fR0Enabled = false;
    PDMPCIRAWREG PciRawReg;

    /*
     * Validate configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg,
                              "GCEnabled\0"
                              "R0Enabled\0"
                              "HostPCIBusNo\0"
                              "HostPCIDeviceNo\0"
                              "HostPCIFunctionNo\0"
                              ))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* Query configuration. */
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fRCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"GCEnabled\" as a bool failed"));

    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: failed to read R0Enabled as boolean"));

    /* Obtain host device address */
    uint32_t u32Bus, u32Device, u32Fn;
    rc = CFGMR3QueryU32(pCfg, "HostPCIBusNo", &u32Bus);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HostPCIBusNo\" as a int failed"));
    rc = CFGMR3QueryU32(pCfg, "HostPCIDeviceNo", &u32Device);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HostPCIDeviceNo\" as a int failed"));
    rc = CFGMR3QueryU32(pCfg, "HostPCIFunctionNo", &u32Fn);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Querying \"HostPCIFunctionNo\" as a int failed"));


    /* Initialize the device state */
    rc = pcirawInit(pDevIns, PciBusAddress(u32Bus, u32Device, u32Fn));
    if (RT_FAILURE(rc))
        return rc;

    pThis->pDevInsR3 = pDevIns;
    pThis->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pThis->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /*
     * Register the raw device and get helpers.
     */
    PciRawReg.u32Version  = PDM_PCIRAWREG_VERSION;
    rc = PDMDevHlpPciRawRegister(pDevIns, &PciRawReg, &pThis->pPciRawHlpR3);
    if (RT_FAILURE(rc))
    {
        AssertMsgRC(rc, ("Cannot PciRawRegister: %Rrc\n", rc));
        return rc;
    }

    /*
     * Initialize critical section.
     */
    rc = PDMDevHlpCritSectInit(pDevIns, &pThis->csLock, RT_SRC_POS, "PCIRAW");
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Raw PCI device cannot initialize critical section"));

#if 0
    /*
     * Register IO/MMIO ranges for guest, basing on real device ranges.
     */
    for (int iRegion = 0; iRegion < VBOX_PCI_NUM_REGIONS; iRegion++)
    {

    }
#endif

    if (fRCEnabled)
    {
        pThis->pPciRawHlpRC = pThis->pPciRawHlpR3->pfnGetRCHelpers(pDevIns);
        if (!pThis->pPciRawHlpRC)
        {
            AssertReleaseMsgFailed(("cannot get RC helper\n"));
            return VERR_INTERNAL_ERROR;
        }
    }
    if (fR0Enabled)
    {
        pThis->pPciRawHlpR0 = pThis->pPciRawHlpR3->pfnGetR0Helpers(pDevIns);
        if (!pThis->pPciRawHlpR0)
        {
            AssertReleaseMsgFailed(("cannot get R0 helper\n"));
            return VERR_INTERNAL_ERROR;
        }
    }

    /* Register SSM callbacks */
    rc = PDMDevHlpSSMRegister3(pDevIns, PCIRAW_SAVED_STATE_VERSION, sizeof(*pThis), pcirawLiveExec, pcirawSaveExec, pcirawLoadExec);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePciRaw =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "pciraw",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "Raw PCI wrapper Device",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36 | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_HOST_DEV,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(PciRawState),
    /* pfnConstruct */
    pcirawConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pcirawRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pcirawReset,
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

#endif /* VBOX_DEVICE_STRUCT_TESTCASE */
