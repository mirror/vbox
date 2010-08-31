/* $Id$ */
/** @file
 * DevPCI - ICH9 southbridge PCI bus emulation Device.
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
 *   Header Files                                                              *
 *******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
/* Hack to get PCIDEVICEINT declare at the right point - include "PCIInternal.h". */
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/pdmdev.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "../Builtins.h"

/**
 * PCI Bus instance.
 */
typedef struct
{
    /** Bus number. */
    int32_t             iBus;
    /** Number of bridges attached to the bus. */
    uint32_t            cBridges;

    /** Array of PCI devices. */
    R3PTRTYPE(PPCIDEVICE) devices[256];
    /** Array of bridges attached to the bus. */
    R3PTRTYPE(PPCIDEVICE *) papBridgesR3;

    /** R3 pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** Pointer to the PCI R3  helpers. */
    PCPDMPCIHLPR3       pPciHlpR3;

    /** R0 pointer to the device instance. */
    PPDMDEVINSR0        pDevInsR0;
    /** Pointer to the PCI R0 helpers. */
    PCPDMPCIHLPR0       pPciHlpR0;

    /** RC pointer to the device instance. */
    PPDMDEVINSRC        pDevInsRC;
    /** Pointer to the PCI RC helpers. */
    PCPDMPCIHLPRC       pPciHlpRC;

    /** The PCI device for the PCI bridge. */
    PCIDEVICE           PciDev;

} PCIBUS, *PPCIBUS;


/** @def PCI_IRQ_PINS
 * Number of pins for interrupts (PIRQ#0...PIRQ#3)
 */
#define PCI_IRQ_PINS 4

/** @def PCI_APIC_IRQ_PINS
 * Number of pins for interrupts if the APIC is used.
 */
#define PCI_APIC_IRQ_PINS 8

/**
 * PCI Globals - This is the host-to-pci bridge and the root bus.
 */
typedef struct
{
    /** R3 pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** R0 pointer to the device instance. */
    PPDMDEVINSR0        pDevInsR0;
    /** RC pointer to the device instance. */
    PPDMDEVINSRC        pDevInsRC;

#if HC_ARCH_BITS == 64
    uint32_t            Alignment0;
#endif

    /** PIC irq levels */
    volatile uint32_t   uaPicPciIrqLevels[PCI_IRQ_PINS];
    /** I/O APIC irq levels */
    volatile uint32_t   uaPciApicIrqLevels[PCI_APIC_IRQ_PINS];

    /** ACPI IRQ level */
    uint32_t            uAcpiIrqLevel;
    /** .. and number */
    uint32_t            uAcpiIrq;

    /** Config register. */
    uint32_t            uConfigReg;

    /** I/O APIC usage flag */
    bool                fUseIoApic;


    /** PCI bus which is attached to the host-to-PCI bridge. */
    PCIBUS              aPciBus;

} PCIGLOBALS, *PPCIGLOBALS;


/*******************************************************************************
 *   Defined Constants And Macros                                              *
 *******************************************************************************/

/** @def VBOX_ICH9PCI_SAVED_STATE_VERSION
 * Saved state version of the ICH9 PCI bus device.
 */
#define VBOX_ICH9PCI_SAVED_STATE_VERSION 1

/** Converts a bus instance pointer to a device instance pointer. */
#define PCIBUS_2_DEVINS(pPciBus)        ((pPciBus)->CTX_SUFF(pDevIns))
/** Converts a device instance pointer to a PCIGLOBALS pointer. */
#define DEVINS_2_PCIGLOBALS(pDevIns)    ((PPCIGLOBALS)(PDMINS_2_DATA(pDevIns, PPCIGLOBALS)))
/** Converts a device instance pointer to a PCIBUS pointer. */
#define DEVINS_2_PCIBUS(pDevIns)        ((PPCIBUS)(&PDMINS_2_DATA(pDevIns, PPCIGLOBALS)->aPciBus))

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN

PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);

#ifdef IN_RING3
DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus);
#endif

RT_C_DECLS_END


PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    return;
}

PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    return;
}

PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    return 0;
}

PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    return 0;
}

PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    return 0;
}

PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    return 0;
}

#ifdef IN_RING3
DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus);
#endif

#ifdef IN_RING3

static DECLCALLBACK(int) ich9pciRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    return 0;
}

static DECLCALLBACK(int) ich9pcibridgeRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    return 0;
}

static DECLCALLBACK(int) ich9pciIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    return 0;
}

static DECLCALLBACK(void) ich9pciSetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                    PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
}

/**
 * Saves a state of the PCI device.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSM            The handle to save the state to.
 */
static DECLCALLBACK(int) pciGenericSaveExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSM)
{
    return SSMR3PutMem(pSSM, &pPciDev->config[0], sizeof(pPciDev->config));
}

static int pciR3CommonSaveExec(PPCIBUS pBus, PSSMHANDLE pSSM)
{
    /*
     * Iterate thru all the devices.
     */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->devices); i++)
    {
        PPCIDEVICE pDev = pBus->devices[i];
        if (pDev)
        {
            SSMR3PutU32(pSSM, i);
            SSMR3PutMem(pSSM, pDev->config, sizeof(pDev->config));

            int rc = SSMR3PutS32(pSSM, pDev->Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    return SSMR3PutU32(pSSM, UINT32_MAX); /* terminator */
}

static DECLCALLBACK(int) pciR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    return pciR3CommonSaveExec(pThis, pSSM);
}


static DECLCALLBACK(int) pcibridgeR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    return pciR3CommonSaveExec(pThis, pSSM);
}

static DECLCALLBACK(int) pciR3CommonLoadExec(PPCIBUS pBus, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    return 0;
}

/**
 * Loads a saved PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSM            The handle to the saved state.
 */
static DECLCALLBACK(int) pciGenericLoadExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSM)
{
    return SSMR3GetMem(pSSM, &pPciDev->config[0], sizeof(pPciDev->config));
}

static DECLCALLBACK(int) pciR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    if (uVersion > VBOX_ICH9PCI_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    return pciR3CommonLoadExec(pThis, pSSM, uVersion, uPass);
}

static DECLCALLBACK(int) pcibridgeR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    if (uVersion > VBOX_ICH9PCI_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    return pciR3CommonLoadExec(pThis, pSSM, uVersion, uPass);
}

static DECLCALLBACK(int) ich9pciFakePCIBIOS(PPDMDEVINS pDevIns)
{
    return 0;
}

static int ich9pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
{
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) ich9pciConstruct(PPDMDEVINS pDevIns,
                                          int        iInstance,
                                          PCFGMNODE  pCfg)
{
    int rc;
    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "IOAPIC\0" "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* query whether we got an IOAPIC */
    bool fUseIoApic;
    rc = CFGMR3QueryBoolDef(pCfg, "IOAPIC", &fUseIoApic, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"IOAPIC\""));

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("PCI: fUseIoApic=%RTbool fGCEnabled=%RTbool fR0Enabled=%RTbool\n", fUseIoApic, fGCEnabled, fR0Enabled));

    /*
     * Init data.
     */
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->aPciBus;
    /* Zero out everything */
    memset(pGlobals, 0, sizeof(*pGlobals));
    /* And fill values */
    pGlobals->fUseIoApic          = fUseIoApic;
    pGlobals->pDevInsR3 = pDevIns;
    pGlobals->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pGlobals->aPciBus.pDevInsR3 = pDevIns;
    pGlobals->aPciBus.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->aPciBus.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pGlobals->aPciBus.papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pGlobals->aPciBus.devices));

    /*
     * Register bus
     */
    PDMPCIBUSREG PciBusReg;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = ich9pciRegister;
    PciBusReg.pfnIORegionRegisterR3   = ich9pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = ich9pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = ich9pciSetIrq;
    PciBusReg.pfnSaveExecR3           = pciGenericSaveExec;
    PciBusReg.pfnLoadExecR3           = pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSR3        = ich9pciFakePCIBIOS;
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "ich9pciSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "ich9pciSetIrq" : NULL;
    rc = PDMDevHlpPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Failed to register ourselves as a PCI Bus"));
    if (pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                   pBus->pPciHlpR3->u32Version, PDM_PCIHLPR3_VERSION);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pPciHlpR0 = pBus->pPciHlpR3->pfnGetR0Helpers(pDevIns);

    /*
     * Fill in PCI configs and add them to the bus.
     */

    /**
     * We emulate 82801IB ICH9 IO chip used in Q35,
     * see http://ark.intel.com/Product.aspx?id=31892
     *
     * Stepping   S-Spec   Top Marking
     *
     *   A2        SLA9M    NH82801IB
     */
    PCIDevSetVendorId(  &pBus->PciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->PciDev, 0x244e); /* Desktop */
    PCIDevSetRevisionId(&pBus->PciDev,   0x92); /* rev. A2 */
    PCIDevSetClassSub(  &pBus->PciDev,   0x00); /* Host/PCI bridge */
    PCIDevSetClassBase( &pBus->PciDev,   0x06); /* bridge */
    PCIDevSetHeaderType(&pBus->PciDev,   0x00);

    pBus->PciDev.pDevIns              = pDevIns;
    pBus->PciDev.Int.s.fRequestedDevFn= true;
    ich9pciRegisterInternal(pBus, 0, &pBus->PciDev, "i82801");

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
}

/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)   ich9pcibridgeConstruct(PPDMDEVINS pDevIns,
                                                  int        iInstance,
                                                  PCFGMNODE  pCfg)
{
    int rc;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfg, "R0Enabled", &fR0Enabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("PCI: fGCEnabled=%RTbool fR0Enabled=%RTbool\n", fGCEnabled, fR0Enabled));

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) ich9pcibridgeReset(PPDMDEVINS pDevIns)
{
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pcibridgeRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
}

/**
 * The PCI bus device registration structure.
 */
const PDMDEVREG g_DevicePciIch9 =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ich9pci",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "ICH9 PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI | PDM_DEVREG_CLASS_BUS_ISA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIGLOBALS),
    /* pfnConstruct */
    ich9pciConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    ich9pciRelocate,
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
 * The device registration structure
 * for the PCI-to-PCI bridge.
 */
const PDMDEVREG g_DevicePciIch9Bridge =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "ich9pcibridge",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "ICH9 PCI to PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(PCIBUS),
    /* pfnConstruct */
    ich9pcibridgeConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    ich9pcibridgeRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    ich9pcibridgeReset,
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

#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
