/* $Id$ */
/** @file
 * DevLPC - Minimal ICH9 LPC device emulation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEV_LPC
#include <VBox/vmm/pdmdev.h>
#include <VBox/vmm/stam.h>
#include <VBox/log.h>

#include <iprt/assert.h>
#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define LPC_REG_HPET_CONFIG_POINTER     0x3404
#define LPC_REG_GCS                     0x3410


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * The ICH9 LPC state.
 */
typedef struct LPCSTATE
{
    /** The PCI device.   */
    PDMPCIDEV       PciDev;
    /** Pointer to the ring-3 device instance. */
    PPDMDEVINSR3    pDevInsR3;

    /** The root complex base address. */
    RTGCPHYS32      GCPhys32Rcba;
    /** Set if R0/RC context is enabled.   */
    bool            fRZEnabled;
    /** The ICH version (7 or 9). */
    uint8_t         uIchVersion;
    /** Explicit padding. */
    uint8_t         abPadding[HC_ARCH_BITS == 32 ? 2 : 6];

    /** Pointer to generic PCI config reader. */
    R3PTRTYPE(PFNPCICONFIGREAD)  pfnPciConfigReadFallback;
    /** Pointer to generic PCI config write. */
    R3PTRTYPE(PFNPCICONFIGWRITE) pfnPciConfigWriteFallback;

    /** Number of MMIO reads. */
    STAMCOUNTER     StatMmioReads;
    /** Number of MMIO writes. */
    STAMCOUNTER     StatMmioWrites;
    /** Number of PCI config space reads. */
    STAMCOUNTER     StatPciCfgReads;
    /** Number of PCI config space writes. */
    STAMCOUNTER     StatPciCfgWrites;
} LPCSTATE;
/** Pointer to the LPC state. */
typedef LPCSTATE *PLPCSTATE;


#ifndef VBOX_DEVICE_STRUCT_TESTCASE

/**
 * @callback_method_impl{FNIOMMMIOREAD}
 */
PDMBOTHCBDECL(int) lpcMmioRead(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void *pv, unsigned cb)
{
    RT_NOREF(pvUser, cb);
    PLPCSTATE        pThis  = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    RTGCPHYS32 const offReg = (RTGCPHYS32)GCPhysAddr - pThis->GCPhys32Rcba;
    Assert(cb == 4); Assert(!(GCPhysAddr & 3)); /* IOMMMIO_FLAGS_READ_DWORD should make sure of this */

    uint32_t *puValue = (uint32_t *)pv;
    if (offReg == LPC_REG_HPET_CONFIG_POINTER)
    {
        *puValue = 0xf0;
        Log(("lpcMmioRead: HPET_CONFIG_POINTER: %#x\n", *puValue));
    }
    else if (offReg == LPC_REG_GCS)
    {
        *puValue = 0;
        Log(("lpcMmioRead: GCS: %#x\n", *puValue));
    }
    else
    {
        *puValue = 0;
        Log(("lpcMmioRead: WARNING! Unknown register %#x!\n", offReg));
    }

    STAM_REL_COUNTER_INC(&pThis->StatMmioReads);
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMMMIOWRITE}
 */
PDMBOTHCBDECL(int) lpcMmioWrite(PPDMDEVINS pDevIns, void *pvUser, RTGCPHYS GCPhysAddr, void const *pv, unsigned cb)
{
    RT_NOREF(pvUser, pv);
    PLPCSTATE        pThis  = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    RTGCPHYS32 const offReg = (RTGCPHYS32)GCPhysAddr - pThis->GCPhys32Rcba;

    if (cb == 4)
    {
        if (offReg == LPC_REG_GCS)
            Log(("lpcMmioWrite: Ignorning write to GCS: %.*Rhxs\n", cb, pv));
        else
            Log(("lpcMmioWrite: Ignorning write to unknown register %#x: %.*Rhxs\n", offReg, cb, pv));
    }
    else
        Log(("lpcMmioWrite: WARNING! Ignoring non-DWORD write to offReg=%#x: %.*Rhxs\n", offReg, cb, pv));

    STAM_REL_COUNTER_INC(&pThis->StatMmioWrites);
    return VINF_SUCCESS;
}

#ifdef IN_RING3

/**
 * @callback_method_impl{FNPCICONFIGREAD}
 */
static DECLCALLBACK(uint32_t) lpcPciConfigRead(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t uAddress, unsigned cb)
{
    PLPCSTATE pThis = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    Assert(pPciDev == &pThis->PciDev);

    STAM_REL_COUNTER_INC(&pThis->StatPciCfgReads);
    uint32_t uValue = pThis->pfnPciConfigReadFallback(pDevIns, pPciDev, uAddress, cb);
    switch (cb)
    {
        case 1: Log(("lpcPciConfigRead: %#04x -> %#04x\n",  uAddress, uValue)); break;
        case 2: Log(("lpcPciConfigRead: %#04x -> %#06x\n",  uAddress, uValue)); break;
        case 4: Log(("lpcPciConfigRead: %#04x -> %#010x\n", uAddress, uValue)); break;
    }
    return uValue;
}


/**
 * @callback_method_impl{FNPCICONFIGWRITE}
 */
static DECLCALLBACK(VBOXSTRICTRC)
lpcPciConfigWrite(PPDMDEVINS pDevIns, PPDMPCIDEV pPciDev, uint32_t uAddress, uint32_t u32Value, unsigned cb)
{
    PLPCSTATE pThis = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    Assert(pPciDev == &pThis->PciDev);

    STAM_REL_COUNTER_INC(&pThis->StatPciCfgWrites);
    switch (cb)
    {
        case 1: Log(("lpcPciConfigWrite: %#04x <- %#04x\n",  uAddress, u32Value)); break;
        case 2: Log(("lpcPciConfigWrite: %#04x <- %#06x\n",  uAddress, u32Value)); break;
        case 4: Log(("lpcPciConfigWrite: %#04x <- %#010x\n", uAddress, u32Value)); break;
    }

    return pThis->pfnPciConfigWriteFallback(pDevIns, pPciDev, uAddress, u32Value, cb);
}


/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) lpcInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PLPCSTATE pThis = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    RT_NOREF(pszArgs);

    if (pThis->uIchVersion == 7)
    {
        uint8_t b1 = PDMPciDevGetByte(&pThis->PciDev, 0xde);
        uint8_t b2 = PDMPciDevGetByte(&pThis->PciDev, 0xad);
        if (   b1 == 0xbe
            && b2 == 0xef)
            pHlp->pfnPrintf(pHlp, "APIC backdoor activated\n");
        else
            pHlp->pfnPrintf(pHlp, "APIC backdoor closed: %02x %02x\n", b1, b2);
    }

    for (unsigned iLine = 0; iLine < 8; iLine++)
    {
        unsigned offBase = iLine < 4 ? 0x60 : 0x68 - 4;
        uint8_t  bMap    = PDMPciDevGetByte(&pThis->PciDev, offBase + iLine);
        if (bMap & 0x80)
            pHlp->pfnPrintf(pHlp, "PIRQ%c_ROUT disabled\n", 'A' + iLine);
        else
            pHlp->pfnPrintf(pHlp, "PIRQ%c_ROUT -> IRQ%d\n", 'A' + iLine, bMap & 0xf);
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int) lpcConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);
    PLPCSTATE pThis = PDMINS_2_DATA(pDevIns, PLPCSTATE);
    Assert(iInstance == 0); RT_NOREF(iInstance);

    /*
     * Initialize state.
     */
    pThis->pDevInsR3 = pDevIns;

    /*
     * Read configuration.
     */
    PDMDEV_VALIDATE_CONFIG_RETURN(pDevIns, "RZEnabled|RCBA|ICHVersion", "");

    int rc = CFGMR3QueryBoolDef(pCfg, "RZEnabled", &pThis->fRZEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to query boolean value \"RZEnabled\""));

    rc = CFGMR3QueryU8Def(pCfg, "ICHVersion", &pThis->uIchVersion, 7 /** @todo 9 */);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to query boolean value \"ICHVersion\""));
    if (   pThis->uIchVersion != 7
        && pThis->uIchVersion != 9)
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Invalid \"ICHVersion\" value (must be 7 or 9)"));

    rc = CFGMR3QueryU32Def(pCfg, "RCBA", &pThis->GCPhys32Rcba, UINT32_C(0xfed1c000));
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc, N_("Configuration error: Failed to query boolean value \"RCBA\""));


    /*
     * Register the PCI device.
     *
     * See sections 13.1 (page 371) and section 13.8.1 (page 429) in the ICH9
     * specification.
     *
     * We set these up so they don't need much/any configuration from the
     * guest.  This is quite possibly wrong, but at the moment we just need to
     * have this device working w/o lots of firmware fun.
     */
    PDMPciDevSetVendorId(     &pThis->PciDev,     0x8086);  /* Intel */
    if (pThis->uIchVersion == 7)
        PDMPciDevSetDeviceId( &pThis->PciDev,     0x27b9);
    else if (pThis->uIchVersion == 9)
        PDMPciDevSetDeviceId( &pThis->PciDev,     0x2918); /** @todo unsure if 0x2918 is the right PCI ID... */
    else
        AssertFailedReturn(VERR_INTERNAL_ERROR_3);
    PDMPciDevSetCommand(      &pThis->PciDev, PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS | PCI_COMMAND_BUSMASTER);
    PDMPciDevSetStatus(       &pThis->PciDev,     0x0210);  /* Note! Used to be 0x0200 for ICH7. */
    PDMPciDevSetRevisionId(   &pThis->PciDev,       0x02);
    PDMPciDevSetClassSub(     &pThis->PciDev,       0x01);  /* PCI-to-ISA bridge */
    PDMPciDevSetClassBase(    &pThis->PciDev,       0x06);  /* bridge */
    PDMPciDevSetHeaderType(   &pThis->PciDev,       0x80);  /* Normal, multifunction device (so that other devices can be its functions) */
    if (pThis->uIchVersion == 7)
    {
        PDMPciDevSetSubSystemVendorId(&pThis->PciDev, 0x8086);
        PDMPciDevSetSubSystemId(      &pThis->PciDev, 0x7270);
    }
    else if (pThis->uIchVersion == 9)
    {
        PDMPciDevSetSubSystemVendorId(&pThis->PciDev, 0x0000); /** @todo  docs stays subsystem IDs are zero, check real HW */
        PDMPciDevSetSubSystemId(      &pThis->PciDev, 0x0000);
    }
    PDMPciDevSetInterruptPin(     &pThis->PciDev,   0x00);  /* The LPC device itself generates no interrupts */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x40, 0x00008001);  /* PMBASE: ACPI base address; (PM_PORT_BASE (?) * 2 | PCI_ADDRESS_SPACE_IO) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x44,       0x80);  /* ACPI_CNTL: SCI is IRQ9, ACPI enabled  */ /** @todo documented as defaulting to 0x00. */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x48, 0x00000001);  /* GPIOBASE (note: used to be zero) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x4c,       0x4d);  /* GC - GPIO control: ??? */ /** @todo documented as defaulting to 0x00. */
    if (pThis->uIchVersion == 7)
        PDMPciDevSetByte(&pThis->PciDev, 0x4e,      0x03);  /* ??? */
    PDMPciDevSetByte(   &pThis->PciDev, 0x60,       0x0b);  /* PIRQA_ROUT: PCI A -> IRQ 11 (documented default is 0x80) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x61,       0x09);  /* PIRQB_ROUT: PCI B -> IRQ 9  (documented default is 0x80) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x62,       0x0b);  /* PIRQC_ROUT: PCI C -> IRQ 11 (documented default is 0x80) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x63,       0x09);  /* PIRQD_ROUT: PCI D -> IRQ 9  (documented default is 0x80) */
    PDMPciDevSetByte(   &pThis->PciDev, 0x64,       0x10);  /* SIRQ_CNTL: Serial IRQ Control 10h R/W, RO */
    PDMPciDevSetByte(   &pThis->PciDev, 0x68,       0x80);  /* PIRQE_ROUT */
    PDMPciDevSetByte(   &pThis->PciDev, 0x69,       0x80);  /* PIRQF_ROUT */
    PDMPciDevSetByte(   &pThis->PciDev, 0x6a,       0x80);  /* PIRQG_ROUT */
    PDMPciDevSetByte(   &pThis->PciDev, 0x6b,       0x80);  /* PIRQH_ROUT */
    PDMPciDevSetWord(   &pThis->PciDev, 0x6c,     0x00f8);  /* IPC_IBDF: IOxAPIC bus:device:function.  (Note! Used to be zero.) */
    if (pThis->uIchVersion == 7)
    {
        /* No idea what this is/was yet: */
        PDMPciDevSetByte(   &pThis->PciDev, 0x70,   0x80);
        PDMPciDevSetByte(   &pThis->PciDev, 0x76,   0x0c);
        PDMPciDevSetByte(   &pThis->PciDev, 0x77,   0x0c);
        PDMPciDevSetByte(   &pThis->PciDev, 0x78,   0x02);
        PDMPciDevSetByte(   &pThis->PciDev, 0x79,   0x00);
    }
    PDMPciDevSetWord(   &pThis->PciDev, 0x80,     0x0000);  /* LPC_I/O_DEC: I/O decode ranges. */
    PDMPciDevSetWord(   &pThis->PciDev, 0x82,     0x0000);  /* LPC_EN: LPC I/F enables. */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x84, 0x00000000);  /* GEN1_DEC: LPC I/F generic decode range 1. */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x88, 0x00000000);  /* GEN2_DEC: LPC I/F generic decode range 2. */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x8c, 0x00000000);  /* GEN3_DEC: LPC I/F generic decode range 3. */
    PDMPciDevSetDWord(  &pThis->PciDev, 0x90, 0x00000000);  /* GEN4_DEC: LPC I/F generic decode range 4. */

    PDMPciDevSetWord(   &pThis->PciDev, 0xa0,     0x0008);  /* GEN_PMCON_1: Documented default is 0x0000 */
    PDMPciDevSetByte(   &pThis->PciDev, 0xa2,       0x00);  /* GEN_PMON_2: */
    PDMPciDevSetByte(   &pThis->PciDev, 0xa4,       0x00);  /* GEN_PMON_3: */
    PDMPciDevSetByte(   &pThis->PciDev, 0xa6,       0x00);  /* GEN_PMON_LOCK: Configuration lock. */
    if (pThis->uIchVersion == 7)
        PDMPciDevSetByte(&pThis->PciDev, 0xa8,      0x0f);  /* Is this part of GEN_PMON_LOCK? */
    PDMPciDevSetByte(   &pThis->PciDev, 0xab,       0x00);  /* BM_BREAK_EN */
    PDMPciDevSetDWord(  &pThis->PciDev, 0xac, 0x00000000);  /* PMIR: Power */
    PDMPciDevSetDWord(  &pThis->PciDev, 0xb8, 0x00000000);  /* GPI_ROUT: GPI Route Control */
    if (pThis->uIchVersion == 9)
    {
        /** @todo the next two values looks bogus.   */
        PDMPciDevSetDWord(&pThis->PciDev, 0xd0, 0x00112233); /* FWH_SEL1: Firmware Hub Select 1  */
        PDMPciDevSetWord( &pThis->PciDev, 0xd4,     0x4567); /* FWH_SEL2: Firmware Hub Select 2 */
        PDMPciDevSetWord( &pThis->PciDev, 0xd8,     0xffcf); /* FWH_DEC_EN1: Firmware Hub Decode Enable 1 */
        PDMPciDevSetByte( &pThis->PciDev, 0xdc,       0x00); /* BIOS_CNTL: BIOS control */
        PDMPciDevSetWord( &pThis->PciDev, 0xe0,     0x0009); /* FDCAP: Feature Detection Capability ID */
        PDMPciDevSetByte( &pThis->PciDev, 0xe2,       0x0c); /* FDLEN: Feature Detection Capability Length */
        PDMPciDevSetByte( &pThis->PciDev, 0xe3,       0x10); /* FDVER: Feature Detection Version */
        PDMPciDevSetByte( &pThis->PciDev, 0xe4,       0x20); /* FDVCT[0]: 5=SATA RAID 0/1/5/10 capability (1=disabled) */
        PDMPciDevSetByte( &pThis->PciDev, 0xe5,       0x00); /* FDVCT[1]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xe6,       0x00); /* FDVCT[2]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xe7,       0x00); /* FDVCT[3]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xe8,       0xc0); /* FDVCT[4]: 6-7=Intel active magament technology capability (11=disabled). */
        PDMPciDevSetByte( &pThis->PciDev, 0xe9,       0x00); /* FDVCT[5]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xea,       0x00); /* FDVCT[6]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xeb,       0x00); /* FDVCT[7]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xec,       0x00); /* FDVCT[8]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xed,       0x00); /* FDVCT[9]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xee,       0x00); /* FDVCT[a]: */
        PDMPciDevSetByte( &pThis->PciDev, 0xef,       0x00); /* FDVCT[b]: */
    }

    /* RCBA: Root complex base address (documented default is 0x00000000). Bit 0 is enable bit. */
    Assert(!(pThis->GCPhys32Rcba & 0x3fff)); /* 16KB aligned */
    PDMPciDevSetDWord(&pThis->PciDev, 0xf0, pThis->GCPhys32Rcba | 1);

    rc = PDMDevHlpPCIRegisterEx(pDevIns, &pThis->PciDev, PDMPCIDEVREG_CFG_PRIMARY, PDMPCIDEVREG_F_NOT_MANDATORY_NO,
                                31 /*uPciDevNo*/, 0 /*uPciFunNo*/, "lpc");
    AssertRCReturn(rc, rc);
    PDMDevHlpPCISetConfigCallbacks(pDevIns, &pThis->PciDev,
                                   lpcPciConfigRead, &pThis->pfnPciConfigReadFallback,
                                   lpcPciConfigWrite, &pThis->pfnPciConfigWriteFallback);

    /*
     * Register the MMIO regions.
     */
    /** @todo This should actually be done when RCBA is enabled, but was
     *        mentioned above we just want this working. */
    rc = PDMDevHlpMMIORegister(pDevIns, pThis->GCPhys32Rcba, 0x4000, pThis,
                               IOMMMIO_FLAGS_READ_DWORD | IOMMMIO_FLAGS_WRITE_PASSTHRU,
                               lpcMmioWrite, lpcMmioRead, "LPC Memory");
    AssertRCReturn(rc, rc);


    /*
     * Debug info and stats.
     */
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioReads,    STAMTYPE_COUNTER, "/Devices/LPC/MMIOReads", STAMUNIT_OCCURENCES, "MMIO reads");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatMmioWrites,   STAMTYPE_COUNTER, "/Devices/LPC/MMIOWrites", STAMUNIT_OCCURENCES, "MMIO writes");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPciCfgReads,  STAMTYPE_COUNTER, "/Devices/LPC/ConfigReads", STAMUNIT_OCCURENCES, "PCI config reads");
    PDMDevHlpSTAMRegister(pDevIns, &pThis->StatPciCfgWrites, STAMTYPE_COUNTER, "/Devices/LPC/ConfigWrites", STAMUNIT_OCCURENCES, "PCI config writes");

    PDMDevHlpDBGFInfoRegister(pDevIns, "lpc", "Display LPC status. (no arguments)", lpcInfo);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DeviceLPC =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "lpc",
    /* szRCMod */
    "",
    /* szR0Mod */
    "",
    /* pszDescription */
    "Low Pin Count (LPC) Bus",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32_64 | PDM_DEVREG_FLAGS_PAE36,
    /* fClass */
    PDM_DEVREG_CLASS_MISC,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(LPCSTATE),
    /* pfnConstruct */
    lpcConstruct,
    /* pfnDestruct */
    NULL,
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

