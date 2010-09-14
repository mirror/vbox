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
typedef struct PCIBus
{
    /** Bus number. */
    int32_t             iBus;
    /** Number of bridges attached to the bus. */
    uint32_t            cBridges;

    /** Array of PCI devices. We assume 32 slots, each with 8 functions. */
    R3PTRTYPE(PPCIDEVICE)   apDevices[256];
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
    PCIDEVICE           aPciDev;

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

    /** I/O APIC irq levels */
    volatile uint32_t   uaPciApicIrqLevels[PCI_APIC_IRQ_PINS];

#if 1 /* Will be moved into the BIOS soon. */
    /** The next I/O port address which the PCI BIOS will use. */
    uint32_t            uPciBiosIo;
    /** The next MMIO address which the PCI BIOS will use. */
    uint32_t            uPciBiosMmio;
    /** Actual bus number. */
    uint8_t             uBus;
#endif


    /** Config register. */
    uint32_t            uConfigReg;

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
/** Converts a pointer to a PCI root bus instance to a PCIGLOBALS pointer.
 */
#define PCIROOTBUS_2_PCIGLOBALS(pPciBus)    ( (PPCIGLOBALS)((uintptr_t)(pPciBus) - RT_OFFSETOF(PCIGLOBALS, aPciBus)) )


/** @def PCI_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def PCI_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
#define PCI_LOCK(pDevIns, rc) \
    do { \
        int rc2 = DEVINS_2_PCIBUS(pDevIns)->CTX_SUFF(pPciHlp)->pfnLock((pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
#define PCI_UNLOCK(pDevIns) \
    DEVINS_2_PCIBUS(pDevIns)->CTX_SUFF(pPciHlp)->pfnUnlock(pDevIns)

#ifndef VBOX_DEVICE_STRUCT_TESTCASE

RT_C_DECLS_BEGIN

PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);

RT_C_DECLS_END

/* Prototypes */
static void ich9pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel);
#ifdef IN_RING3
static int ich9pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName);
static void ich9pciUpdateMappings(PCIDevice *d);
static DECLCALLBACK(uint32_t) ich9pciConfigRead(PCIDevice *aDev, uint32_t u32Address, unsigned len);
DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus);
static void ich9pciBiosInitDevice(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions);
#endif

PDMBOTHCBDECL(void) ich9pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    ich9pciSetIrqInternal(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), pPciDev->devfn, pPciDev, iIrq, iLevel);
}

PDMBOTHCBDECL(void) ich9pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    /*
     * The PCI-to-PCI bridge specification defines how the interrupt pins
     * are routed from the secondary to the primary bus (see chapter 9).
     * iIrq gives the interrupt pin the pci device asserted.
     * We change iIrq here according to the spec and call the SetIrq function
     * of our parent passing the device which asserted the interrupt instead of the device of the bridge.
     */
    PPCIBUS    pBus          = PDMINS_2_DATA(pDevIns, PPCIBUS);
    PPCIDEVICE pPciDevBus    = pPciDev;
    int        iIrqPinBridge = iIrq;
    uint8_t    uDevFnBridge  = 0;

    /* Walk the chain until we reach the host bus. */
    do
    {
        uDevFnBridge  = pBus->aPciDev.devfn;
        iIrqPinBridge = ((pPciDevBus->devfn >> 3) + iIrqPinBridge) & 3;

        /* Get the parent. */
        pBus = pBus->aPciDev.Int.s.CTX_SUFF(pBus);
        pPciDevBus = &pBus->aPciDev;
    } while (pBus->iBus != 0);

    AssertMsgReturnVoid(pBus->iBus == 0, ("This is not the host pci bus iBus=%d\n", pBus->iBus));
    ich9pciSetIrqInternal(PCIROOTBUS_2_PCIGLOBALS(pBus), uDevFnBridge, pPciDev, iIrqPinBridge, iLevel);
}

/**
 * Port I/O Handler for PCI address OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("ich9pciIOPortAddressWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        pThis->uConfigReg = u32 & ~3; /* Bits 0-1 are reserved and we silently clear them */
        PCI_UNLOCK(pDevIns);
    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for PCI address IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        *pu32 = pThis->uConfigReg;
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortAddressRead: Port=%#x cb=%d -> %#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
    }

    Log(("ich9pciIOPortAddressRead: Port=%#x cb=%d VERR_IOM_IOPORT_UNUSED\n", Port, cb));

    return VERR_IOM_IOPORT_UNUSED;
}

static int ich9pciDataWrite(PPCIGLOBALS pGlobals, uint32_t addr, uint32_t val, int len)
{
    uint8_t iBus, iDevice;
    uint32_t uConfigReg;

    Log(("ich9pciDataWrite: addr=%08x val=%08x len=%d\n", pGlobals->uConfigReg, val, len));

    if (!(pGlobals->uConfigReg & (1 << 31)))
        return VINF_SUCCESS;

    if ((pGlobals->uConfigReg & 0x3) != 0)
        return VINF_SUCCESS;

    /* Compute destination device */
    iBus = (pGlobals->uConfigReg >> 16) & 0xff;
    iDevice = (pGlobals->uConfigReg >> 8) & 0xff;
    /* And config register */
    uConfigReg = (pGlobals->uConfigReg & 0xfc) | (addr & 3);
    if (iBus != 0)
    {
        if (pGlobals->aPciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = ich9pciFindBridge(&pGlobals->aPciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
                pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, iBus, iDevice, uConfigReg, val, len);
            }
#else
            return VINF_IOM_HC_IOPORT_WRITE;
#endif
        }
    }
    else
    {
        if (pGlobals->aPciBus.apDevices[iDevice])
        {
#ifdef IN_RING3
            R3PTRTYPE(PCIDevice *) aDev = pGlobals->aPciBus.apDevices[iDevice];
            Log(("ich9pciConfigWrite: %s: addr=%02x val=%08x len=%d\n", aDev->name, uConfigReg, val, len));
            aDev->Int.s.pfnConfigWrite(aDev, uConfigReg, val, len);
#else
            return VINF_IOM_HC_IOPORT_WRITE;
#endif
        }
    }
    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for PCI data OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the OUT operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortDataWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    int rc = VINF_SUCCESS;
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        rc = ich9pciDataWrite(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, u32, cb);
        PCI_UNLOCK(pDevIns);
    }
    else
        AssertMsgFailed(("Unaligned write to port %#x u32=%#x cb=%d\n", Port, u32, cb));
    return rc;
}

static int ich9pciDataRead(PPCIGLOBALS pGlobals, uint32_t addr, int len, uint32_t *pu32)
{
    uint8_t iBus, iDevice;
    uint32_t uConfigReg;

    *pu32 = 0xffffffff;

    if (!(pGlobals->uConfigReg & (1 << 31)))
        return VINF_SUCCESS;

    if ((pGlobals->uConfigReg & 0x3) != 0)
        return VINF_SUCCESS;

    /* Compute destination device */
    iBus = (pGlobals->uConfigReg >> 16) & 0xff;
    iDevice = (pGlobals->uConfigReg >> 8) & 0xff;
    /* And config register */
    uConfigReg = (pGlobals->uConfigReg & 0xfc) | (addr & 3);
    if (iBus != 0)
    {
        if (pGlobals->aPciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = ich9pciFindBridge(&pGlobals->aPciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigRead);
                *pu32 = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, iBus, iDevice, uConfigReg, len);
            }
#else
            return VINF_IOM_HC_IOPORT_READ;
#endif
        }
    }
    else
    {
        if (pGlobals->aPciBus.apDevices[iDevice])
        {
#ifdef IN_RING3
            R3PTRTYPE(PCIDevice *) aDev = pGlobals->aPciBus.apDevices[iDevice];
            *pu32 = aDev->Int.s.pfnConfigRead(aDev, uConfigReg, len);
            Log(("ich9pciConfigRead: %s: addr=%02x val=%08x len=%d\n", aDev->name, uConfigReg, *pu32, len));
#else
            return VINF_IOM_HC_IOPORT_READ;
#endif
        }
    }

    return VINF_SUCCESS;
}

/**
 * Port I/O Handler for PCI data IN operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   pu32        Where to store the result.
 * @param   cb          Number of bytes read.
 */
PDMBOTHCBDECL(int)  ich9pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        int rc = ich9pciDataRead(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, cb, pu32);
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortDataRead: Port=%#x cb=%#x -> %#x (%Rrc)\n", Port, cb, *pu32, rc));
        return rc;
    }
    AssertMsgFailed(("Unaligned read from port %#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

/* Compute mapping of PCI slot and IRQ number to APIC interrupt line */
static inline int ich9pciSlot2ApicIrq(uint8_t uSlot, int irq_num)
{
    return (irq_num + uSlot) & 7;
}

/* Add one more level up request on APIC input line */
static inline void ich9pciApicLevelUp(PPCIGLOBALS pGlobals, int irq_num)
{
    ASMAtomicIncU32(&pGlobals->uaPciApicIrqLevels[irq_num]);
}

/* Remove one level up request on APIC input line */
static inline void ich9pciApicLevelDown(PPCIGLOBALS pGlobals, int irq_num)
{
    ASMAtomicDecU32(&pGlobals->uaPciApicIrqLevels[irq_num]);
}

static void ich9pciApicSetIrq(PPCIBUS pBus, uint8_t uDevFn, PCIDevice *pPciDev, int irq_num1, int iLevel, int iForcedIrq)
{
    /* This is only allowed to be called with a pointer to the root bus. */
    AssertMsg(pBus->iBus == 0, ("iBus=%u\n", pBus->iBus));

    if (iForcedIrq == -1)
    {
        int apic_irq, apic_level;
        PPCIGLOBALS pGlobals = PCIROOTBUS_2_PCIGLOBALS(pBus);
        int irq_num = ich9pciSlot2ApicIrq(uDevFn >> 3, irq_num1);

        if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_HIGH)
            ich9pciApicLevelUp(pGlobals, irq_num);
        else if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_LOW)
            ich9pciApicLevelDown(pGlobals, irq_num);

        apic_irq = irq_num + 0x10;
        apic_level = pGlobals->uaPciApicIrqLevels[irq_num] != 0;
        Log3(("ich9pciApicSetIrq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);

        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
        {
            /**
             *  we raised it few lines above, as PDM_IRQ_LEVEL_FLIP_FLOP has
             * PDM_IRQ_LEVEL_HIGH bit set
             */
            ich9pciApicLevelDown(pGlobals, irq_num);
            pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
            apic_level = pGlobals->uaPciApicIrqLevels[irq_num] != 0;
            Log3(("ich9pciApicSetIrq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d (flop)\n",
                  R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
            pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);
        }
    } else {
        Log3(("ich9pciApicSetIrq: %s: irq_num1=%d level=%d acpi_irq=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, iForcedIrq));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), iForcedIrq, iLevel);
    }
}

static void ich9pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    PPCIBUS     pBus =     &pGlobals->aPciBus;
    const bool  fIsAcpiDevice = PCIDevGetDeviceId(pPciDev) == 0x7113;

    /* Check if the state changed. */
    if (pPciDev->Int.s.uIrqPinState != iLevel)
    {
        pPciDev->Int.s.uIrqPinState = (iLevel & PDM_IRQ_LEVEL_HIGH);

        /* Send interrupt to I/O APIC only now. */
        if (fIsAcpiDevice)
            /*
             * ACPI needs special treatment since SCI is hardwired and
             * should not be affected by PCI IRQ routing tables at the
             * same time SCI IRQ is shared in PCI sense hence this
             * kludge (i.e. we fetch the hardwired value from ACPIs
             * PCI device configuration space).
             */
            ich9pciApicSetIrq(pBus, uDevFn, pPciDev, -1, iLevel, PCIDevGetInterruptLine(pPciDev));
        else
            ich9pciApicSetIrq(pBus, uDevFn, pPciDev, iIrq, iLevel, -1);
    }
}

#ifdef IN_RING3
DECLINLINE(PPCIDEVICE) ich9pciFindBridge(PPCIBUS pBus, uint8_t iBus)
{
    /* Search for a fitting bridge. */
    for (uint32_t iBridge = 0; iBridge < pBus->cBridges; iBridge++)
    {
        /*
         * Examine secondary and subordinate bus number.
         * If the target bus is in the range we pass the request on to the bridge.
         */
        PPCIDEVICE pBridgeTemp = pBus->papBridgesR3[iBridge];
        AssertMsg(pBridgeTemp && pBridgeTemp->Int.s.fPciToPciBridge,
                  ("Device is not a PCI bridge but on the list of PCI bridges\n"));

        if (   iBus >= pBridgeTemp->config[VBOX_PCI_SECONDARY_BUS]
            && iBus <= pBridgeTemp->config[VBOX_PCI_SUBORDINATE_BUS])
            return pBridgeTemp;
    }

    /* Nothing found. */
    return NULL;
}

static inline uint32_t ich9pciGetRegionReg(int iRegion)
{
    return (iRegion == PCI_ROM_SLOT) ?
            VBOX_PCI_ROM_ADDRESS : (VBOX_PCI_BASE_ADDRESS_0 + iRegion * 4);
}

#define INVALID_PCI_ADDRESS ~0U

static void ich9pciUpdateMappings(PCIDevice* pDev)
{
    PPCIBUS pBus = pDev->Int.s.CTX_SUFF(pBus);
    uint32_t uLast, uNew;

    int iCmd = PCIDevGetCommand(pDev);
    for (int iRegion = 0; iRegion < PCI_NUM_REGIONS; iRegion++)
    {
        PCIIORegion* pRegion = &pDev->Int.s.aIORegions[iRegion];
        uint32_t uConfigReg = ich9pciGetRegionReg(iRegion);
        int32_t  iRegionSize =  pRegion->size;
        int rc;

        if (iRegionSize == 0)
            continue;

        if (pRegion->type & PCI_ADDRESS_SPACE_IO)
        {
            /* port IO region */
            if (iCmd & PCI_COMMAND_IOACCESS)
            {
                /* IO access allowed */
                uNew = ich9pciConfigRead(pDev, uConfigReg, 4);
                uNew &= ~(iRegionSize - 1);
                uLast = uNew + iRegionSize - 1;
                /* only 64K ioports on PC */
                if (uLast <= uNew || uNew == 0 || uLast >= 0x10000)
                    uNew = INVALID_PCI_ADDRESS;
            } else
                uNew = INVALID_PCI_ADDRESS;
        }
        else
        {
            /* MMIO region */
            if (iCmd & PCI_COMMAND_MEMACCESS)
            {
                uNew = ich9pciConfigRead(pDev, uConfigReg, 4);
                /* the ROM slot has a specific enable bit */
                if (iRegion == PCI_ROM_SLOT && !(uNew & 1))
                    uNew = INVALID_PCI_ADDRESS;
                else
                {
                    uNew &= ~(iRegionSize - 1);
                    uLast = uNew + iRegionSize - 1;
                    /* NOTE: we do not support wrapping */
                    /* XXX: as we cannot support really dynamic
                       mappings, we handle specific values as invalid
                       mappings. */
                    if (uLast <= uNew || uNew == 0 || uLast == ~0U)
                        uNew = INVALID_PCI_ADDRESS;
                }
            } else
                uNew = INVALID_PCI_ADDRESS;
        }
        /* now do the real mapping */
        if (uNew != pRegion->addr)
        {
            if (pRegion->addr != INVALID_PCI_ADDRESS)
            {
                if (pRegion->type & PCI_ADDRESS_SPACE_IO)
                {
                    /* Port IO */
                    int devclass;
                    /* NOTE: specific hack for IDE in PC case:
                       only one byte must be mapped. */
                    /// @todo: do we need it?
                    devclass = pDev->config[0x0a] | (pDev->config[0x0b] << 8);
                    if (devclass == 0x0101 && iRegionSize == 4)
                    {
                        rc = PDMDevHlpIOPortDeregister(pDev->pDevIns, pRegion->addr + 2, 1);
                        AssertRC(rc);
                    }
                    else
                    {
                        rc = PDMDevHlpIOPortDeregister(pDev->pDevIns, pRegion->addr, pRegion->size);
                        AssertRC(rc);
                    }
                }
                else
                {
                    RTGCPHYS GCPhysBase = pRegion->addr;
                    if (pBus->pPciHlpR3->pfnIsMMIO2Base(pBus->pDevInsR3, pDev->pDevIns, GCPhysBase))
                    {
                        /* unmap it. */
                        rc = pRegion->map_func(pDev, iRegion, NIL_RTGCPHYS, pRegion->size, (PCIADDRESSSPACE)(pRegion->type));
                        AssertRC(rc);
                        rc = PDMDevHlpMMIO2Unmap(pDev->pDevIns, iRegion, GCPhysBase);
                    }
                    else
                        rc = PDMDevHlpMMIODeregister(pDev->pDevIns, GCPhysBase, pRegion->size);
                    AssertMsgRC(rc, ("rc=%Rrc d=%s i=%d GCPhysBase=%RGp size=%#x\n", rc, pDev->name, iRegion, GCPhysBase, pRegion->size));
                }
            }
            pRegion->addr = uNew;
            if (pRegion->addr != INVALID_PCI_ADDRESS)
            {
                /* finally, map the region */
                rc = pRegion->map_func(pDev, iRegion,
                                       pRegion->addr, pRegion->size,
                                       (PCIADDRESSSPACE)(pRegion->type));
                AssertRC(rc);
            }
        }
    }
}

static DECLCALLBACK(int) ich9pciRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    PPCIBUS     pBus = DEVINS_2_PCIBUS(pDevIns);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)RT_ELEMENTS(pBus->apDevices)
        )
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the device.
     */
    return ich9pciRegisterInternal(pBus, iDev, pPciDev, pszName);
}

static DECLCALLBACK(int) ich9pcibridgeRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    return 0;
}

static DECLCALLBACK(int) ich9pciIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    /*
     * Validate.
     */
    AssertMsgReturn(   enmType == PCI_ADDRESS_SPACE_MEM
                    || enmType == PCI_ADDRESS_SPACE_IO
                    || enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH,
                    ("Invalid enmType=%#x? Or was this a bitmask after all...\n", enmType),
                    VERR_INVALID_PARAMETER);
    AssertMsgReturn((unsigned)iRegion < PCI_NUM_REGIONS,
                    ("Invalid iRegion=%d PCI_NUM_REGIONS=%d\n", iRegion, PCI_NUM_REGIONS),
                    VERR_INVALID_PARAMETER);
    int iLastSet = ASMBitLastSetU32(cbRegion);
    AssertMsgReturn(    iLastSet != 0
                    &&  RT_BIT_32(iLastSet - 1) == cbRegion,
                    ("Invalid cbRegion=%#x iLastSet=%#x (not a power of 2 or 0)\n", cbRegion, iLastSet),
                    VERR_INVALID_PARAMETER);

    /*
     * Register the I/O region.
     */
    PPCIIOREGION pRegion = &pPciDev->Int.s.aIORegions[iRegion];
    pRegion->addr        = ~0U;
    pRegion->size        = cbRegion;
    pRegion->type        = enmType;
    pRegion->map_func    = pfnCallback;

    /* Set type in the config space. */
    uint32_t u32Address = ich9pciGetRegionReg(iRegion);
    uint32_t u32Value   =   (enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH ? (1 << 3) : 0)
                          | (enmType == PCI_ADDRESS_SPACE_IO ? 1 : 0);
    *(uint32_t *)(pPciDev->config + u32Address) = RT_H2LE_U32(u32Value);

    return VINF_SUCCESS;
}

static DECLCALLBACK(void) ich9pciSetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                                                    PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    if (ppfnReadOld)
        *ppfnReadOld = pPciDev->Int.s.pfnConfigRead;
    pPciDev->Int.s.pfnConfigRead  = pfnRead;

    if (ppfnWriteOld)
        *ppfnWriteOld = pPciDev->Int.s.pfnConfigWrite;
    pPciDev->Int.s.pfnConfigWrite = pfnWrite;
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
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        PPCIDEVICE pDev = pBus->apDevices[i];
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

static uint32_t ich9pciConfigRead(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t len)
{
    /* Set destination address */
    /// @todo: device locking?
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
                           (uDevFn << 8) | (addr & ~3);
    uint32_t u32Val;
    int rc = ich9pciDataRead(pGlobals, addr & 3, len, &u32Val);
    AssertRC(rc);
    switch (len)
    {
        case 1:
            u32Val &= 0xff;
            break;
        case 2:
            u32Val &= 0xffff;
            break;
    }
    return u32Val;
}

static void ich9pciConfigWrite(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t val, uint32_t len)
{
    /* Set destination address */
    /// @todo: device locking?
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
                           (uDevFn << 8) | addr;
    ich9pciDataWrite(pGlobals, 0, val, len);
}

static void ich9pciSetRegionAddress(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, int iRegion, uint32_t addr)
{
    uint32_t uReg = ich9pciGetRegionReg(iRegion);

    /* Read memory type first. */
    uint8_t uResourceType = ich9pciConfigRead(pGlobals, uBus, uDevFn, uReg, 1);
    /* Read command register. */
    uint16_t uCmd = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, 2);

    if ( iRegion == PCI_ROM_SLOT )
        uCmd |= PCI_COMMAND_MEMACCESS;
    else if ((uResourceType & PCI_ADDRESS_SPACE_IO) == PCI_ADDRESS_SPACE_IO)
        uCmd |= PCI_COMMAND_IOACCESS; /* Enable I/O space access. */
    else /* The region is MMIO. */
        uCmd |= PCI_COMMAND_MEMACCESS; /* Enable MMIO access. */

    /* Write address of the device. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, uReg, addr, 4);

    /* enable memory mappings */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, uCmd, 2);
}


static void ich9pciBiosInitBridge(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions)
{
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SECONDARY_BUS, pGlobals->uBus, 1);
    /* Temporary until we know how many other bridges are behind this one. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, 0xff, 1);

    /* Add position of this bridge into the array. */
    paBridgePositions[cBridgeDepth+1] = (uDevFn >> 3);

    /*
     * The I/O range for the bridge must be aligned to a 4KB boundary.
     * This does not change anything really as the access to the device is not going
     * through the bridge but we want to be compliant to the spec.
     */
    if ((pGlobals->uPciBiosIo % 4096) != 0)
    {
        pGlobals->uPciBiosIo = RT_ALIGN_32(pGlobals->uPciBiosIo, 4*1024);
        Log(("%s: Aligned I/O start address. New address %#x\n", __FUNCTION__, pGlobals->uPciBiosIo));
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_IO_BASE, (pGlobals->uPciBiosIo >> 8) & 0xf0, 1);

    /* The MMIO range for the bridge must be aligned to a 1MB boundary. */
    if ((pGlobals->uPciBiosMmio % (1024 * 1024)) != 0)
    {
        pGlobals->uPciBiosMmio = RT_ALIGN_32(pGlobals->uPciBiosMmio, 1024*1024);
        Log(("%s: Aligned MMIO start address. New address %#x\n", __FUNCTION__, pGlobals->uPciBiosMmio));
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_BASE, (pGlobals->uPciBiosMmio >> 16) & UINT32_C(0xffff0), 2);

    /* Save values to compare later to. */
    uint32_t u32IoAddressBase = pGlobals->uPciBiosIo;
    uint32_t u32MMIOAddressBase = pGlobals->uPciBiosMmio;

    /* Init devices behind the bridge and possibly other bridges as well. */
    for (int iDev = 0; iDev <= 255; iDev++)
        ich9pciBiosInitDevice(pGlobals, uBus + 1, iDev, cBridgeDepth + 1, paBridgePositions);

    /* The number of bridges behind the this one is now available. */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, pGlobals->uBus, 1);

    /*
     * Set I/O limit register. If there is no device with I/O space behind the bridge
     * we set a lower value than in the base register.
     * The result with a real bridge is that no I/O transactions are passed to the secondary
     * interface. Again this doesn't really matter here but we want to be compliant to the spec.
     */
    if ((u32IoAddressBase != pGlobals->uPciBiosIo) && ((pGlobals->uPciBiosIo % 4096) != 0))
    {
        /* The upper boundary must be one byte less than a 4KB boundary. */
        pGlobals->uPciBiosIo = RT_ALIGN_32(pGlobals->uPciBiosIo, 4*1024);
    }

    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_IO_LIMIT, ((pGlobals->uPciBiosIo >> 8) & 0xf0) - 1, 1);

    /* Same with the MMIO limit register but with 1MB boundary here. */
    if ((u32MMIOAddressBase != pGlobals->uPciBiosMmio) && ((pGlobals->uPciBiosMmio % (1024 * 1024)) != 0))
    {
        /* The upper boundary must be one byte less than a 1MB boundary. */
        pGlobals->uPciBiosMmio = RT_ALIGN_32(pGlobals->uPciBiosMmio, 1024*1024);
    }
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_LIMIT, ((pGlobals->uPciBiosMmio >> 16) & UINT32_C(0xfff0)) - 1, 2);

    /*
     * Set the prefetch base and limit registers. We currently have no device with a prefetchable region
     * which may be behind a bridge. Thatswhy it is unconditionally disabled here atm by writing a higher value into
     * the base register than in the limit register.
     */
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_BASE, 0xfff0, 2);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_LIMIT, 0x0, 2);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_BASE_UPPER32, 0x00, 4);
    ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_LIMIT_UPPER32, 0x00, 4);
}

static void ich9pciBiosInitDevice(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions)
{
    uint32_t *paddr;
    uint16_t uDevClass, uVendor, uDevice;
    uint8_t uCmd;

    uDevClass  = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_CLASS_DEVICE, 2);
    uVendor    = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_VENDOR_ID, 2);
    uDevice    = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_DEVICE_ID, 2);

    /* If device is present */
    if (uVendor == 0xffff)
        return;

    switch (uDevClass)
    {
        case 0x0101:
            /* IDE controller */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, 0x40, 0x8000, 2); /* enable IDE0 */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, 0x42, 0x8000, 2); /* enable IDE1 */
            goto default_map;
            break;
        case 0x0300:
            /* VGA controller */
            if (uVendor != 0x80ee)
                goto default_map;
            /* VGA: map frame buffer to default Bochs VBE address */
            ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, 0, 0xE0000000);
            /*
             * Legacy VGA I/O ports are implicitly decoded by a VGA class device. But
             * only the framebuffer (i.e., a memory region) is explicitly registered via
             * ich9pciSetRegionAddress, so I/O decoding must be enabled manually.
             */
            uCmd = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND, 1);
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_COMMAND,
                               /* Enable I/O space access. */
                               uCmd | PCI_COMMAND_IOACCESS,
                               1);
            break;
        case 0x0800:
            /* PIC */
            if (uVendor == 0x1014)
            {
                /* IBM */
                if (uDevice == 0x0046 || uDevice == 0xFFFF)
                    /* MPIC & MPIC2 */
                    ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, 0, 0x80800000 + 0x00040000);
            }
            break;
        case 0xff00:
            if ((uVendor == 0x0106b)
                && (uDevice == 0x0017 || uDevice == 0x0022))
            {
                /* macio bridge */
                ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, 0, 0x80800000);
            }
            break;
        case 0x0604:
            /* PCI-to-PCI bridge. */
            ich9pciConfigWrite(pGlobals, uBus, uDevFn, VBOX_PCI_PRIMARY_BUS, uBus, 1);

            AssertMsg(pGlobals->uBus < 255, ("Too many bridges on the bus\n"));
            pGlobals->uBus++;
            ich9pciBiosInitBridge(pGlobals, uBus, uDevFn, cBridgeDepth, paBridgePositions);
            break;
        default:
        default_map:
        {
            /* default memory mappings */
            /*
             * We ignore ROM region here.
             */
            for (int iRegion = 0; iRegion < (PCI_NUM_REGIONS-1); iRegion++)
            {
                uint32_t u32Address = ich9pciGetRegionReg(iRegion);

                /* Calculate size. */
                uint8_t u8ResourceType = ich9pciConfigRead(pGlobals, uBus, uDevFn, u32Address, 1);
                ich9pciConfigWrite(pGlobals, uBus, uDevFn, u32Address, UINT32_C(0xffffffff), 4);
                uint32_t u32Size = ich9pciConfigRead(pGlobals, uBus, uDevFn, u32Address, 4);
                /* Clear resource information depending on resource type. */
                if ((u8ResourceType & PCI_COMMAND_IOACCESS) == PCI_COMMAND_IOACCESS) /* I/O */
                    u32Size &= ~(0x01);
                else                        /* MMIO */
                    u32Size &= ~(0x0f);

                bool fIsPio = ((u8ResourceType & PCI_COMMAND_IOACCESS) == PCI_COMMAND_IOACCESS);
                /*
                 * Invert all bits and add 1 to get size of the region.
                 * (From PCI implementation note)
                 */
                if (fIsPio && (u32Size & UINT32_C(0xffff0000)) == 0)
                    u32Size = (~(u32Size | UINT32_C(0xffff0000))) + 1;
                else
                    u32Size = (~u32Size) + 1;

                Log(("%s: Size of region %u for device %d on bus %d is %u\n", __FUNCTION__, iRegion, uDevFn, uBus, u32Size));

                if (u32Size)
                {
                    paddr = fIsPio ? &pGlobals->uPciBiosIo : &pGlobals->uPciBiosMmio;
                    *paddr = (*paddr + u32Size - 1) & ~(u32Size - 1);
                    Log(("%s: Start address of %s region %u is %#x\n", __FUNCTION__, (fIsPio ? "I/O" : "MMIO"), iRegion, *paddr));
                    ich9pciSetRegionAddress(pGlobals, uBus, uDevFn, iRegion, *paddr);
                    *paddr += u32Size;
                    Log(("%s: New address is %#x\n", __FUNCTION__, *paddr));
                }
            }
            break;
        }
    }

    /* map the interrupt */
    uint32_t uPin = ich9pciConfigRead(pGlobals, uBus, uDevFn, VBOX_PCI_INTERRUPT_PIN, 1);
    if (uPin != 0)
    {
        uint8_t uBridgeDevFn = uDevFn;
        uPin--;

        /* We need to go up to the host bus to see which irq this device will assert there. */
        while (cBridgeDepth != 0)
        {
            /* Get the pin the device would assert on the bridge. */
            uPin = ((uBridgeDevFn >> 3) + uPin) & 3;
            uBridgeDevFn = paBridgePositions[cBridgeDepth];
            cBridgeDepth--;
        }
#if 0
        uPin = pci_slot_get_pirq(uDevFn, pin);
        pic_irq = pci_irqs[pin];
        ich9pciConfigWrite(pGlobals, uBus, uDevFn, PCI_INTERRUPT_LINE, pic_irq);
#endif
    }
}

static const uint8_t auPciIrqs[4] = { 11, 9, 11, 9 };

static DECLCALLBACK(int) ich9pciFakePCIBIOS(PPDMDEVINS pDevIns)
{
    unsigned    i;
    uint8_t     elcr[2] = {0, 0};
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PVM         pVM = PDMDevHlpGetVM(pDevIns);
    Assert(pVM);

    /*
     * Set the start addresses.
     */
    pGlobals->uPciBiosIo  = 0xd000;
    pGlobals->uPciBiosMmio = UINT32_C(0xf0000000);
    pGlobals->uBus = 0;

    /*
     * Activate IRQ mappings.
     */
    for (i = 0; i < 4; i++)
    {
        uint8_t irq = auPciIrqs[i];
        /* Set to trigger level. */
        elcr[irq >> 3] |= (1 << (irq & 7));
    }

    /* Tell to the PIC. */
    VBOXSTRICTRC rcStrict = IOMIOPortWrite(pVM, 0x4d0, elcr[0], sizeof(uint8_t));
    if (rcStrict == VINF_SUCCESS)
        rcStrict = IOMIOPortWrite(pVM, 0x4d1, elcr[1], sizeof(uint8_t));
    if (rcStrict != VINF_SUCCESS)
    {
        AssertMsgFailed(("Writing to PIC failed! rcStrict=%Rrc\n", VBOXSTRICTRC_VAL(rcStrict)));
        return RT_SUCCESS(rcStrict) ? VERR_INTERNAL_ERROR : VBOXSTRICTRC_VAL(rcStrict);
    }

    /*
     * Init the devices.
     */
    for (i = 0; i < 256; i++)
    {
        uint8_t aBridgePositions[256];

        memset(aBridgePositions, 0, sizeof(aBridgePositions));
        Log2(("PCI: Initializing device %d (%#x)\n",
              i, 0x80000000 | (i << 8)));
        ich9pciBiosInitDevice(pGlobals, 0, i, 0, aBridgePositions);
    }

    return VINF_SUCCESS;
}

static DECLCALLBACK(uint32_t) ich9pciConfigRead(PCIDevice *aDev, uint32_t u32Address, unsigned len)
{
    AssertMsgReturn(u32Address + len <= 256, ("Read after end of PCI config space\n"),
                    0);
    switch (len)
    {
        case 1:
            return aDev->config[u32Address];
        case 2:
            return RT_LE2H_U16(*(uint16_t *)(aDev->config + u32Address));
        default:
        case 4:
            return RT_LE2H_U32(*(uint32_t *)(aDev->config + u32Address));
    }
}


/**
 * See paragraph 7.5 of PCI Express specification (p. 349) for definition of
 * registers and their writability policy.
 */
static DECLCALLBACK(void) ich9pciConfigWrite(PCIDevice *aDev, uint32_t u32Address,
                                             uint32_t val, unsigned len)
{
    /* Fast case - update one of BARs or ROM address, 'while' only for 'break' */
    while (   len == 4
           && (   (   u32Address >= VBOX_PCI_BASE_ADDRESS_0
                   && u32Address <  VBOX_PCI_BASE_ADDRESS_0 + 6 * 4)
               || (   u32Address >= VBOX_PCI_ROM_ADDRESS
                   && u32Address <  VBOX_PCI_ROM_ADDRESS+4)
              )
           )
    {
        PCIIORegion *pRegion;
        int reg, regionSize;

        reg = (u32Address >= VBOX_PCI_ROM_ADDRESS) ? PCI_ROM_SLOT : (u32Address - VBOX_PCI_BASE_ADDRESS_0) >> 2;
        pRegion = &aDev->Int.s.aIORegions[reg];
        regionSize = pRegion->size;
        if (regionSize == 0)
            break;
        /* compute the stored value */
        if (reg == PCI_ROM_SLOT) {
            /* keep ROM enable bit */
            val &= (~(regionSize - 1)) | 1;
        } else {
            val &= ~(regionSize - 1);
            val |= pRegion->type;
        }
        *(uint32_t *)(aDev->config + u32Address) = RT_H2LE_U32(val);
        ich9pciUpdateMappings(aDev);
        return;
    }

    uint32_t addr = u32Address;
    bool fUpdateMappings = false;
    for (uint32_t i = 0; i < len; i++)
    {
        bool fWritable = false;
        switch (PCIDevGetHeaderType(aDev))
        {
            case 0x00: /* normal device */
            case 0x80: /* multi-function device */
                switch (addr)
                {
                    /* Read-only registers, see  */
                    case VBOX_PCI_VENDOR_ID: case VBOX_PCI_VENDOR_ID+1:
                    case VBOX_PCI_DEVICE_ID: case VBOX_PCI_DEVICE_ID+1:
                    case VBOX_PCI_REVISION_ID:
                    case VBOX_PCI_CLASS_PROG:
                    case VBOX_PCI_CLASS_SUB:
                    case VBOX_PCI_CLASS_BASE:
                    case VBOX_PCI_HEADER_TYPE:
                    case VBOX_PCI_BASE_ADDRESS_0: case VBOX_PCI_BASE_ADDRESS_0+1: case VBOX_PCI_BASE_ADDRESS_0+2: case VBOX_PCI_BASE_ADDRESS_0+3:
                    case VBOX_PCI_BASE_ADDRESS_1: case VBOX_PCI_BASE_ADDRESS_1+1: case VBOX_PCI_BASE_ADDRESS_1+2: case VBOX_PCI_BASE_ADDRESS_1+3:
                    case VBOX_PCI_BASE_ADDRESS_2: case VBOX_PCI_BASE_ADDRESS_2+1: case VBOX_PCI_BASE_ADDRESS_2+2: case VBOX_PCI_BASE_ADDRESS_2+3:
                    case VBOX_PCI_BASE_ADDRESS_3: case VBOX_PCI_BASE_ADDRESS_3+1: case VBOX_PCI_BASE_ADDRESS_3+2: case VBOX_PCI_BASE_ADDRESS_3+3:
                    case VBOX_PCI_BASE_ADDRESS_4: case VBOX_PCI_BASE_ADDRESS_4+1: case VBOX_PCI_BASE_ADDRESS_4+2: case VBOX_PCI_BASE_ADDRESS_4+3:
                    case VBOX_PCI_BASE_ADDRESS_5: case VBOX_PCI_BASE_ADDRESS_5+1: case VBOX_PCI_BASE_ADDRESS_5+2: case VBOX_PCI_BASE_ADDRESS_5+3:
                    case VBOX_PCI_SUBSYSTEM_VENDOR_ID: case VBOX_PCI_SUBSYSTEM_VENDOR_ID+1:
                    case VBOX_PCI_SUBSYSTEM_ID: case VBOX_PCI_SUBSYSTEM_ID+1:
                    case VBOX_PCI_ROM_ADDRESS: case VBOX_PCI_ROM_ADDRESS+1: case VBOX_PCI_ROM_ADDRESS+2: case VBOX_PCI_ROM_ADDRESS+3:
                    case VBOX_PCI_CAPABILITY_LIST:
                    case VBOX_PCI_INTERRUPT_PIN:
                        fWritable = false;
                        break;
                    /* Others can be written */
                    default:
                        fWritable = true;
                        break;
                }
                break;
            default:
            case 0x01: /* bridge */
                switch (addr)
                {
                    /* Read-only registers */
                    case VBOX_PCI_VENDOR_ID: case VBOX_PCI_VENDOR_ID+1:
                    case VBOX_PCI_DEVICE_ID: case VBOX_PCI_DEVICE_ID+1:
                    case VBOX_PCI_REVISION_ID:
                    case VBOX_PCI_CLASS_PROG:
                    case VBOX_PCI_CLASS_SUB:
                    case VBOX_PCI_CLASS_BASE:
                    case VBOX_PCI_HEADER_TYPE:
                    case VBOX_PCI_ROM_ADDRESS_BR: case VBOX_PCI_ROM_ADDRESS_BR+1: case VBOX_PCI_ROM_ADDRESS_BR+2: case VBOX_PCI_ROM_ADDRESS_BR+3:
                    case VBOX_PCI_INTERRUPT_PIN:
                        fWritable = false;
                        break;
                    default:
                        fWritable = true;
                        break;
            }
            break;
        }

        switch (addr)
        {
            case VBOX_PCI_COMMAND: /* Command register, bits 0-7. */
                fUpdateMappings = true;
                aDev->config[addr] = val;
                break;
            case VBOX_PCI_COMMAND+1: /* Command register, bits 8-15. */
                /* don't change reserved bits (11-15) */
                val &= UINT32_C(~0xf8);
                fUpdateMappings = true;
                aDev->config[addr] = val;
                break;
            case VBOX_PCI_STATUS:  /* Status register, bits 0-7. */
                /* don't change read-only bits => actually all lower bits are read-only */
                val &= UINT32_C(~0xff);
                /* status register, low part: clear bits by writing a '1' to the corresponding bit */
                aDev->config[addr] &= ~val;
                break;
            case VBOX_PCI_STATUS+1:  /* Status register, bits 8-15. */
                /* don't change read-only bits */
                val &= UINT32_C(~0x06);
                /* status register, high part: clear bits by writing a '1' to the corresponding bit */
                aDev->config[addr] &= ~val;
                break;
            default:
                if (fWritable)
                    aDev->config[addr] = val;
        }
        addr++;
        val >>= 8;
    }

    if (fUpdateMappings)
        /* if the command register is modified, we must modify the mappings */
        ich9pciUpdateMappings(aDev);
}

/* Slot/functions assignment per table at p. 12 of ICH9 family spec update */
static const struct {
    const char* pszName;
    int32_t     iSlot;
    int32_t     iFunction;
} PciSlotAssignments[] = {
    {
        "piix3ide", 1, 1 // do we really need it?
    },
    {
        "lan",      25, 0 /* LAN controller */
    },
    {
        "hda",      27, 0 /* High Definition Audio */
    },
    {
        "i82801",   30, 0 /* Host Controller */
    },
    {
        "lpc",      31, 0 /* Low Pin Count bus */
    },
    {
        "ahci",     31, 2 /* SATA controller */
    },
    {
        "smbus",    31, 3 /* System Management Bus */
    },
    {
        "thermal",  31, 6 /* Thermal controller */
    },
};

static int assignPosition(PPCIBUS pBus, PPCIDEVICE pPciDev, const char *pszName)
{
    /* Hardcoded slots/functions, per chipset spec */
    for (size_t i = 0; i < RT_ELEMENTS(PciSlotAssignments); i++)
    {
        if (!strcmp(pszName, PciSlotAssignments[i].pszName))
        {
            pPciDev->Int.s.fRequestedDevFn = true;
            return (PciSlotAssignments[i].iSlot << 3) + PciSlotAssignments[i].iFunction;
        }
    }

    /* Otherwise when assigning a slot, we need to make sure all its functions are available */
    for (int iPos = 0; iPos < (int)RT_ELEMENTS(pBus->apDevices); iPos += 8)
        if (        !pBus->apDevices[iPos]
                &&  !pBus->apDevices[iPos + 1]
                &&  !pBus->apDevices[iPos + 2]
                &&  !pBus->apDevices[iPos + 3]
                &&  !pBus->apDevices[iPos + 4]
                &&  !pBus->apDevices[iPos + 5]
                &&  !pBus->apDevices[iPos + 6]
                &&  !pBus->apDevices[iPos + 7])
        {
            pPciDev->Int.s.fRequestedDevFn = false;
            return iPos;
        }

    return -1;
}

static bool hasHardAssignedDevsInSlot(PPCIBUS pBus, int iSlot)
{
    PCIDevice** aSlot = &pBus->apDevices[iSlot << 3];

    return    (aSlot[0] && aSlot[0]->Int.s.fRequestedDevFn)
           || (aSlot[1] && aSlot[1]->Int.s.fRequestedDevFn)
           || (aSlot[2] && aSlot[2]->Int.s.fRequestedDevFn)
           || (aSlot[3] && aSlot[3]->Int.s.fRequestedDevFn)
           || (aSlot[4] && aSlot[4]->Int.s.fRequestedDevFn)
           || (aSlot[5] && aSlot[5]->Int.s.fRequestedDevFn)
           || (aSlot[6] && aSlot[6]->Int.s.fRequestedDevFn)
           || (aSlot[7] && aSlot[7]->Int.s.fRequestedDevFn)
           ;
}

static int ich9pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
{
    /*
     * Find device position
     */
    if (iDev < 0)
    {
        iDev = assignPosition(pBus, pPciDev, pszName);
        if (iDev < 0)
        {
             AssertMsgFailed(("Couldn't find free spot!\n"));
             return VERR_PDM_TOO_PCI_MANY_DEVICES;
        }
    }

    /*
     * Check if we can really take this slot, possibly by relocating
     * its current habitant, if it wasn't hard assigned too.
     */
    if (pPciDev->Int.s.fRequestedDevFn &&
        pBus->apDevices[iDev]            &&
        pBus->apDevices[iDev]->Int.s.fRequestedDevFn)
    {
        /*
         * Smth like hasHardAssignedDevsInSlot(pBus, iDev >> 3) shall be use to make
         * it compatible with DevPCI.cpp version, but this way we cannot assign
         * in accordance with the chipset spec.
         */
        AssertReleaseMsgFailed(("Configuration error:'%s' and '%s' are both configured as device %d\n",
                                 pszName, pBus->apDevices[iDev]->name, iDev));
        return VERR_INTERNAL_ERROR;
    }

    if (pBus->apDevices[iDev])
    {
        /* if we got here, we shall (and usually can) relocate the device */
        int iRelDev = assignPosition(pBus, pBus->apDevices[iDev], pBus->apDevices[iDev]->name);
        if (iRelDev < 0 || iRelDev == iDev)
        {
            AssertMsgFailed(("Couldn't find free spot!\n"));
            return VERR_PDM_TOO_PCI_MANY_DEVICES;
        }
        /* Copy device function by function to its new position */
        for (int i = 0; i < 8; i++)
        {
            if (!pBus->apDevices[iDev + i])
                continue;
            Log(("PCI: relocating '%s' from slot %#x to %#x\n", pBus->apDevices[iDev + i]->name, iDev + i, iRelDev + i));
            pBus->apDevices[iRelDev + i] = pBus->apDevices[iDev + i];
            pBus->apDevices[iRelDev + i]->devfn = iRelDev + i;
            pBus->apDevices[iDev + i] = NULL;
        }
    }

    /*
     * Fill in device information.
     */
    pPciDev->devfn                  = iDev;
    pPciDev->name                   = pszName;
    pPciDev->Int.s.pBusR3           = pBus;
    pPciDev->Int.s.pBusR0           = MMHyperR3ToR0(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pBusRC           = MMHyperR3ToRC(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pfnConfigRead    = ich9pciConfigRead;
    pPciDev->Int.s.pfnConfigWrite   = ich9pciConfigWrite;
    pBus->apDevices[iDev]           = pPciDev;
    if (pPciDev->Int.s.fPciToPciBridge)
    {
        AssertMsg(pBus->cBridges < RT_ELEMENTS(pBus->apDevices), ("Number of bridges exceeds the number of possible devices on the bus\n"));
        AssertMsg(pPciDev->Int.s.pfnBridgeConfigRead && pPciDev->Int.s.pfnBridgeConfigWrite,
                  ("device is a bridge but does not implement read/write functions\n"));
        pBus->papBridgesR3[pBus->cBridges] = pPciDev;
        pBus->cBridges++;
    }

    Log(("PCI: Registered device %d function %d (%#x) '%s'.\n",
         iDev >> 3, iDev & 7, 0x80000000 | (iDev << 8), pszName));

    return VINF_SUCCESS;
}


/**
 * Info handler, device version.
 *
 * @param   pDevIns     Device instance which registered the info.
 * @param   pHlp        Callback functions for doing output.
 * @param   pszArgs     Argument string. Optional and specific to the handler.
 */
static DECLCALLBACK(void) ich9pciInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PPCIBUS pBus = DEVINS_2_PCIBUS(pDevIns);
    uint32_t iBus = 0, iDev;


    for (iDev = 0; iDev < RT_ELEMENTS(pBus->apDevices); iDev++)
    {
        PPCIDEVICE pPciDev = pBus->apDevices[iDev];
        if (pPciDev != NULL)
            pHlp->pfnPrintf(pHlp, "%02x:%02x:%02x %s: %x-%x\n",
                            iBus, (iDev >> 3) & 0xff, iDev & 0x7,
                            pPciDev->name,
                            PCIDevGetVendorId(pPciDev), PCIDevGetDeviceId(pPciDev)
                            );
    }
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
    if (!fUseIoApic)
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Must use IO-APIC with ICH9 chipset"));
    pGlobals->pDevInsR3 = pDevIns;
    pGlobals->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pGlobals->aPciBus.pDevInsR3 = pDevIns;
    pGlobals->aPciBus.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->aPciBus.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pGlobals->aPciBus.papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pGlobals->aPciBus.apDevices));

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
    PCIDevSetVendorId(  &pBus->aPciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->aPciDev, 0x244e); /* Desktop */
    PCIDevSetRevisionId(&pBus->aPciDev,   0x92); /* rev. A2 */
    PCIDevSetClassSub(  &pBus->aPciDev,   0x00); /* Host/PCI bridge */
    PCIDevSetClassBase( &pBus->aPciDev,   0x06); /* bridge */
    PCIDevSetHeaderType(&pBus->aPciDev,   0x00); /* normal device */

    pBus->aPciDev.pDevIns               = pDevIns;
    /* We register Host<->PCI controller on the bus */
    ich9pciRegisterInternal(pBus, -1, &pBus->aPciDev, "i82801");

    /*
     * Register I/O ports and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cf8, 1, NULL, ich9pciIOPortAddressWrite, ich9pciIOPortAddressRead, NULL, NULL, "ICH9 (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cfc, 4, NULL, ich9pciIOPortDataWrite, ich9pciIOPortDataRead, NULL, NULL, "ICH9 (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cf8, 1, NIL_RTGCPTR, "ich9pciIOPortAddressWrite", "ich9pciIOPortAddressRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cfc, 4, NIL_RTGCPTR, "ich9pciIOPortDataWrite", "ich9pciIOPortDataRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cf8, 1, NIL_RTR0PTR, "ich9pciIOPortAddressWrite", "ich9pciIOPortAddressRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cfc, 4, NIL_RTR0PTR, "ich9pciIOPortDataWrite", "ich9pciIOPortDataRead", NULL, NULL, "ICH9 (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }


    /** @todo: other chipset devices shall be registered too */
    /** @todo: what to with bridges? */

    PDMDevHlpDBGFInfoRegister(pDevIns, "pci", "Display PCI bus status. (no arguments)", ich9pciInfo);

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->aPciBus;
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        if (pBus->apDevices[i])
            pBus->apDevices[i]->Int.s.pBusRC += offDelta;
    }

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
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /* Reset config space to default values. */
    pBus->aPciDev.config[VBOX_PCI_PRIMARY_BUS] = 0;
    pBus->aPciDev.config[VBOX_PCI_SECONDARY_BUS] = 0;
    pBus->aPciDev.config[VBOX_PCI_SUBORDINATE_BUS] = 0;
}


/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) ich9pcibridgeRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->apDevices); i++)
    {
        if (pBus->apDevices[i])
            pBus->apDevices[i]->Int.s.pBusRC += offDelta;
    }

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
