/* $Id$ */
/** @file
 * DevPCI - PCI BUS Device.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * QEMU PCI bus manager
 *
 * Copyright (c) 2004 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
/* Hack to get PCIDEVICEINT declared at the right point - include "PCIInternal.h". */
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/vmm/pdmdev.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "VBoxDD.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * PIIX3 ISA Bridge state.
 */
typedef struct PIIX3State
{
    /** The PCI device of the bridge. */
    PCIDEVICE dev;
} PIIX3State, PIIX3, *PPIIX3;

/**
 * PCI Bus instance.
 */
typedef struct PCIBus
{
    /** Bus number. */
    int32_t             iBus;
    /** Start device number. */
    int32_t             iDevSearch;
    /** Number of bridges attached to the bus. */
    uint32_t            cBridges;

    uint32_t            Alignment0;

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

} PCIBUS;
/** Pointer to a PCIBUS instance. */
typedef PCIBUS *PPCIBUS;
typedef PCIBUS PCIBus;

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
typedef struct PCIGLOBALS
{
    /** Irq levels for the four PCI Irqs.
     *  These count how many devices asserted
     *  the IRQ line. If greater 0 an IRQ is sent to the guest.
     *  If it drops to 0 the IRQ is deasserted.
     */
    volatile uint32_t   pci_irq_levels[PCI_IRQ_PINS];

#if 1 /* Will be moved into the BIOS soon. */
    /** The next I/O port address which the PCI BIOS will use. */
    uint32_t            pci_bios_io_addr;
    /** The next MMIO address which the PCI BIOS will use. */
    uint32_t            pci_bios_mem_addr;
    /** Actual bus number. */
    uint8_t             uBus;
#endif

    /** I/O APIC usage flag */
    bool                fUseIoApic;
    /** I/O APIC irq levels */
    volatile uint32_t   pci_apic_irq_levels[PCI_APIC_IRQ_PINS];
    /** ACPI IRQ level */
    uint32_t            acpi_irq_level;
    /** ACPI PIC IRQ */
    int                 acpi_irq;
    /** Config register. */
    uint32_t            uConfigReg;

    /** R3 pointer to the device instance. */
    PPDMDEVINSR3        pDevInsR3;
    /** R0 pointer to the device instance. */
    PPDMDEVINSR0        pDevInsR0;
    /** RC pointer to the device instance. */
    PPDMDEVINSRC        pDevInsRC;

#if HC_ARCH_BITS == 64
    uint32_t            Alignment0;
#endif

    /** ISA bridge state. */
    PIIX3               PIIX3State;
    /** PCI bus which is attached to the host-to-PCI bridge. */
    PCIBUS              PciBus;

} PCIGLOBALS;
/** Pointer to per VM data. */
typedef PCIGLOBALS *PPCIGLOBALS;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/** Converts a bus instance pointer to a device instance pointer. */
#define PCIBUS_2_DEVINS(pPciBus)        ((pPciBus)->CTX_SUFF(pDevIns))
/** Converts a PCI bus device instance pointer to a PCIGLOBALS pointer. */
#define DEVINS_2_PCIGLOBALS(pDevIns)    ((PPCIGLOBALS)(PDMINS_2_DATA(pDevIns, PPCIGLOBALS)))
/** Converts a PCI bus device instance pointer to a PCIBUS pointer. */
#define DEVINS_2_PCIBUS(pDevIns)        ((PPCIBUS)(&PDMINS_2_DATA(pDevIns, PPCIGLOBALS)->PciBus))

/** Converts a pointer to a PCI bus instance to a PCIGLOBALS pointer.
 *  @note This works only if the bus number is 0!!!
 */
#define PCIBUS_2_PCIGLOBALS(pPciBus)    RT_FROM_MEMBER(pPciBus, PCIGLOBALS, PciBus)

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

/** @def VBOX_PCI_SAVED_STATE_VERSION
 * Saved state version of the PCI bus device.
 */
#define VBOX_PCI_SAVED_STATE_VERSION 3


#ifndef VBOX_DEVICE_STRUCT_TESTCASE


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
RT_C_DECLS_BEGIN

PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel, uint32_t uTag);
PDMBOTHCBDECL(void) pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel, uint32_t uTag);
PDMBOTHCBDECL(int)  pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);

#ifdef IN_RING3
DECLINLINE(PPCIDEVICE) pciR3FindBridge(PPCIBUS pBus, uint8_t iBus);
#endif

RT_C_DECLS_END

#define DEBUG_PCI

#define PCI_VENDOR_ID       0x00    /* 16 bits */
#define PCI_DEVICE_ID       0x02    /* 16 bits */
#define PCI_COMMAND         0x04    /* 16 bits */
#define  PCI_COMMAND_IO     0x01    /* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY 0x02    /* Enable response in Memory space */
#define PCI_CLASS_DEVICE    0x0a    /* Device class */
#define PCI_INTERRUPT_LINE  0x3c    /* 8 bits */
#define PCI_INTERRUPT_PIN   0x3d    /* 8 bits */
#define PCI_MIN_GNT         0x3e    /* 8 bits */
#define PCI_MAX_LAT         0x3f    /* 8 bits */


#ifdef IN_RING3

static void pci_update_mappings(PCIDevice *d)
{
    PPCIBUS pBus = d->Int.s.CTX_SUFF(pBus);
    PCIIORegion *r;
    int cmd, i;
    uint32_t last_addr, new_addr, config_ofs;

    cmd = RT_LE2H_U16(*(uint16_t *)(d->config + PCI_COMMAND));
    for(i = 0; i < PCI_NUM_REGIONS; i++) {
        r = &d->Int.s.aIORegions[i];
        if (i == PCI_ROM_SLOT) {
            config_ofs = 0x30;
        } else {
            config_ofs = 0x10 + i * 4;
        }
        if (r->size != 0) {
            if (r->type & PCI_ADDRESS_SPACE_IO) {
                if (cmd & PCI_COMMAND_IO) {
                    new_addr = RT_LE2H_U32(*(uint32_t *)(d->config +
                                                         config_ofs));
                    new_addr = new_addr & ~(r->size - 1);
                    last_addr = new_addr + r->size - 1;
                    /* NOTE: we have only 64K ioports on PC */
                    if (last_addr <= new_addr || new_addr == 0 ||
                        last_addr >= 0x10000) {
                        new_addr = ~0U;
                    }
                } else {
                    new_addr = ~0U;
                }
            } else {
                if (cmd & PCI_COMMAND_MEMORY) {
                    new_addr = RT_LE2H_U32(*(uint32_t *)(d->config +
                                                         config_ofs));
                    /* the ROM slot has a specific enable bit */
                    if (i == PCI_ROM_SLOT && !(new_addr & 1))
                        goto no_mem_map;
                    new_addr = new_addr & ~(r->size - 1);
                    last_addr = new_addr + r->size - 1;
                    /* NOTE: we do not support wrapping */
                    /* XXX: as we cannot support really dynamic
                       mappings, we handle specific values as invalid
                       mappings. */
                    /* Unconditionally exclude I/O-APIC/HPET/ROM. Pessimistic, but better than causing a mess. */
                    if (last_addr <= new_addr || new_addr == 0 ||
                        (new_addr <= ~0U && last_addr >= 0xfec00000U)) {
                        new_addr = ~0U;
                    }
                } else {
                no_mem_map:
                    new_addr = ~0U;
                }
            }
            //LogRel(("PCI: config dev %u/%u BAR%i uOld=%#018llx uNew=%#018llx size=%llu\n", d->devfn >> 3, d->devfn & 7, i, r->addr, new_addr, r->size));
            /* now do the real mapping */
            if (new_addr != r->addr) {
                if (r->addr != ~0U) {
                    if (r->type & PCI_ADDRESS_SPACE_IO) {
                        int devclass;
                        /* NOTE: specific hack for IDE in PC case:
                           only one byte must be mapped. */
                        devclass = d->config[0x0a] | (d->config[0x0b] << 8);
                        if (devclass == 0x0101 && r->size == 4) {
                            int rc = PDMDevHlpIOPortDeregister(d->pDevIns, r->addr + 2, 1);
                            AssertRC(rc);
                        } else {
                            int rc = PDMDevHlpIOPortDeregister(d->pDevIns, r->addr, r->size);
                            AssertRC(rc);
                        }
                    } else {
                        RTGCPHYS GCPhysBase = r->addr;
                        int rc;
                        if (pBus->pPciHlpR3->pfnIsMMIO2Base(pBus->pDevInsR3, d->pDevIns, GCPhysBase))
                        {
                            /* unmap it. */
                            rc = r->map_func(d, i, NIL_RTGCPHYS, r->size, (PCIADDRESSSPACE)(r->type));
                            AssertRC(rc);
                            rc = PDMDevHlpMMIO2Unmap(d->pDevIns, i, GCPhysBase);
                        }
                        else
                            rc = PDMDevHlpMMIODeregister(d->pDevIns, GCPhysBase, r->size);
                        AssertMsgRC(rc, ("rc=%Rrc d=%s i=%d GCPhysBase=%RGp size=%#x\n", rc, d->name, i, GCPhysBase, r->size));
                    }
                }
                r->addr = new_addr;
                if (r->addr != ~0U) {
                    int rc = r->map_func(d, i,
                                         r->addr + (r->type & PCI_ADDRESS_SPACE_IO ? 0 : 0),
                                         r->size, (PCIADDRESSSPACE)(r->type));
                    AssertRC(rc);
                }
            }
        }
    }
}


static DECLCALLBACK(uint32_t) pci_default_read_config(PCIDevice *d, uint32_t address, unsigned len)
{
    uint32_t val;
    switch(len) {
    case 1:
        val = d->config[address];
        break;
    case 2:
        val = RT_LE2H_U16(*(uint16_t *)(d->config + address));
        break;
    default:
    case 4:
        val = RT_LE2H_U32(*(uint32_t *)(d->config + address));
        break;
    }
    return val;
}

static DECLCALLBACK(void) pci_default_write_config(PCIDevice *d, uint32_t address, uint32_t val, unsigned len)
{
    int can_write;
    unsigned i;
    uint32_t end, addr;

    if (len == 4 && ((address >= 0x10 && address < 0x10 + 4 * 6) ||
                     (address >= 0x30 && address < 0x34))) {
        PCIIORegion *r;
        int reg;

        if ( address >= 0x30 ) {
            reg = PCI_ROM_SLOT;
        }else{
            reg = (address - 0x10) >> 2;
        }
        r = &d->Int.s.aIORegions[reg];
        if (r->size == 0)
            goto default_config;
        /* compute the stored value */
        if (reg == PCI_ROM_SLOT) {
            /* keep ROM enable bit */
            val &= (~(r->size - 1)) | 1;
        } else {
            val &= ~(r->size - 1);
            val |= r->type;
        }
        *(uint32_t *)(d->config + address) = RT_H2LE_U32(val);
        pci_update_mappings(d);
        return;
    }
 default_config:
    /* not efficient, but simple */
    addr = address;
    for(i = 0; i < len; i++) {
        /* default read/write accesses */
        switch(d->config[0x0e]) {
        case 0x00: /* normal device */
        case 0x80: /* multi-function device */
            switch(addr) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x08:
            case 0x09:
            case 0x0a:
            case 0x0b:
            case 0x0e:
            case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x16: case 0x17: /* base */
            case 0x18: case 0x19: case 0x1a: case 0x1b: case 0x1c: case 0x1d: case 0x1e: case 0x1f:
            case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x26: case 0x27:
            case 0x2c: case 0x2d:                                                                   /* subsystem ID */
            case 0x2e: case 0x2f:                                                                   /* vendor ID */
            case 0x30: case 0x31: case 0x32: case 0x33:                                             /* rom */
            case 0x34:                                                                              /* Capabilities pointer. */
            case 0x3d:                                                                              /* Interrupt pin. */
                can_write = 0;
                break;
            default:
                can_write = 1;
                break;
            }
            break;
        default:
        case 0x01: /* bridge */
            switch(addr) {
            case 0x00:
            case 0x01:
            case 0x02:
            case 0x03:
            case 0x08:
            case 0x09:
            case 0x0a:
            case 0x0b:
            case 0x0e:
            case 0x38: case 0x39: case 0x3a: case 0x3b: /* rom */
            case 0x3d:
                can_write = 0;
                break;
            default:
                can_write = 1;
                break;
            }
            break;
        }
#ifdef VBOX
        if (addr == 0x05)       /* Command register, bits 8-15. */
        {
            /* don't change reserved bits (11-15) */
            val &= UINT32_C(~0xf8);
            d->config[addr] = val;
        }
        else if (addr == 0x06)  /* Status register, bits 0-7. */
        {
            /* don't change read-only bits => actually all lower bits are read-only */
            val &= UINT32_C(~0xff);
            /* status register, low part: clear bits by writing a '1' to the corresponding bit */
            d->config[addr] &= ~val;
        }
        else if (addr == 0x07)  /* Status register, bits 8-15. */
        {
            /* don't change read-only bits */
            val &= UINT32_C(~0x06);
            /* status register, high part: clear bits by writing a '1' to the corresponding bit */
            d->config[addr] &= ~val;
        }
        else
#endif
        if (can_write) {
            d->config[addr] = val;
        }
        addr++;
        val >>= 8;
    }

    end = address + len;
    if (end > PCI_COMMAND && address < (PCI_COMMAND + 2)) {
        /* if the command register is modified, we must modify the mappings */
        pci_update_mappings(d);
    }
}

#endif /* IN_RING3 */

static int pci_data_write(PPCIGLOBALS pGlobals, uint32_t addr, uint32_t val, int len)
{
    uint8_t iBus, iDevice;
    uint32_t config_addr;

    Log(("pci_data_write: addr=%08x val=%08x len=%d\n", pGlobals->uConfigReg, val, len));

    if (!(pGlobals->uConfigReg & (1 << 31))) {
        return VINF_SUCCESS;
    }
    if ((pGlobals->uConfigReg & 0x3) != 0) {
        return VINF_SUCCESS;
    }
    iBus = (pGlobals->uConfigReg >> 16) & 0xff;
    iDevice = (pGlobals->uConfigReg >> 8) & 0xff;
    config_addr = (pGlobals->uConfigReg & 0xfc) | (addr & 3);
    if (iBus != 0)
    {
        if (pGlobals->PciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = pciR3FindBridge(&pGlobals->PciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
                pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, iBus, iDevice, config_addr, val, len);
            }
#else
            return VINF_IOM_R3_IOPORT_WRITE;
#endif
        }
    }
    else
    {
        R3PTRTYPE(PCIDevice *) pci_dev = pGlobals->PciBus.devices[iDevice];
        if (pci_dev)
        {
#ifdef IN_RING3
            Log(("pci_config_write: %s: addr=%02x val=%08x len=%d\n", pci_dev->name, config_addr, val, len));
            pci_dev->Int.s.pfnConfigWrite(pci_dev, config_addr, val, len);
#else
            return VINF_IOM_R3_IOPORT_WRITE;
#endif
        }
    }
    return VINF_SUCCESS;
}

static int pci_data_read(PPCIGLOBALS pGlobals, uint32_t addr, int len, uint32_t *pu32)
{
    uint8_t iBus, iDevice;
    uint32_t config_addr;

    *pu32 = 0xffffffff;

    if (!(pGlobals->uConfigReg & (1 << 31)))
        return VINF_SUCCESS;
    if ((pGlobals->uConfigReg & 0x3) != 0)
        return VINF_SUCCESS;
    iBus = (pGlobals->uConfigReg >> 16) & 0xff;
    iDevice = (pGlobals->uConfigReg >> 8) & 0xff;
    config_addr = (pGlobals->uConfigReg & 0xfc) | (addr & 3);
    if (iBus != 0)
    {
        if (pGlobals->PciBus.cBridges)
        {
#ifdef IN_RING3 /** @todo do lookup in R0/RC too! */
            PPCIDEVICE pBridgeDevice = pciR3FindBridge(&pGlobals->PciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigRead);
                *pu32 = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, iBus, iDevice, config_addr, len);
            }
#else
            NOREF(len);
            return VINF_IOM_R3_IOPORT_READ;
#endif
        }
    }
    else
    {
        R3PTRTYPE(PCIDevice *) pci_dev = pGlobals->PciBus.devices[iDevice];
        if (pci_dev)
        {
#ifdef IN_RING3
            *pu32 = pci_dev->Int.s.pfnConfigRead(pci_dev, config_addr, len);
            Log(("pci_config_read: %s: addr=%02x val=%08x len=%d\n", pci_dev->name, config_addr, *pu32, len));
#else
            NOREF(len);
            return VINF_IOM_R3_IOPORT_READ;
#endif
        }
    }

    return VINF_SUCCESS;
}



/* return the global irq number corresponding to a given device irq
   pin. We could also use the bus number to have a more precise
   mapping.
   This is the implementation note described in the PCI spec chapter 2.2.6 */
static inline int pci_slot_get_pirq(uint8_t uDevFn, int irq_num)
{
    int slot_addend;
    slot_addend = (uDevFn >> 3) - 1;
    return (irq_num + slot_addend) & 3;
}

static inline int pci_slot_get_apic_pirq(uint8_t uDevFn, int irq_num)
{
    return (irq_num + (uDevFn >> 3)) & 7;
}

static inline int get_pci_irq_apic_level(PPCIGLOBALS pGlobals, int irq_num)
{
    return (pGlobals->pci_apic_irq_levels[irq_num] != 0);
}

static void apic_set_irq(PPCIBUS pBus, uint8_t uDevFn, PCIDevice *pPciDev, int irq_num1, int iLevel, int acpi_irq, uint32_t uTagSrc)
{
    /* This is only allowed to be called with a pointer to the host bus. */
    AssertMsg(pBus->iBus == 0, ("iBus=%u\n", pBus->iBus));

    if (acpi_irq == -1) {
        int apic_irq, apic_level;
        PPCIGLOBALS pGlobals = PCIBUS_2_PCIGLOBALS(pBus);
        int irq_num = pci_slot_get_apic_pirq(uDevFn, irq_num1);

        if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_HIGH)
            ASMAtomicIncU32(&pGlobals->pci_apic_irq_levels[irq_num]);
        else if ((iLevel & PDM_IRQ_LEVEL_HIGH) == PDM_IRQ_LEVEL_LOW)
            ASMAtomicDecU32(&pGlobals->pci_apic_irq_levels[irq_num]);

        apic_irq = irq_num + 0x10;
        apic_level = get_pci_irq_apic_level(pGlobals, irq_num);
        Log3(("apic_set_irq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level, uTagSrc);

        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
            ASMAtomicDecU32(&pGlobals->pci_apic_irq_levels[irq_num]);
            pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
            apic_level = get_pci_irq_apic_level(pGlobals, irq_num);
            Log3(("apic_set_irq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d (flop)\n",
                  R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
            pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level, uTagSrc);
        }
    } else {
        Log3(("apic_set_irq: %s: irq_num1=%d level=%d acpi_irq=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, acpi_irq));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), acpi_irq, iLevel, uTagSrc);
    }
}

DECLINLINE(int) get_pci_irq_level(PPCIGLOBALS pGlobals, int irq_num)
{
    return (pGlobals->pci_irq_levels[irq_num] != 0);
}

/**
 * Set the IRQ for a PCI device on the host bus - shared by host bus and bridge.
 *
 * @param   pDevIns         Device instance of the host PCI Bus.
 * @param   uDevFn          The device number on the host bus which will raise the IRQ
 * @param   pPciDev         The PCI device structure which raised the interrupt.
 * @param   iIrq            IRQ number to set.
 * @param   iLevel          IRQ level.
 * @param   uTagSrc         The IRQ tag and source ID (for tracing).
 * @remark  uDevFn and pPciDev->devfn are not the same if the device is behind a bridge.
 *          In that case uDevFn will be the slot of the bridge which is needed to calculate the
 *          PIRQ value.
 */
static void pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel, uint32_t uTagSrc)
{
    PPCIBUS     pBus =     &pGlobals->PciBus;
    uint8_t    *pbCfg = pGlobals->PIIX3State.dev.config;
    const bool  fIsAcpiDevice = pPciDev->config[2] == 0x13 && pPciDev->config[3] == 0x71;
    /* If the two configuration space bytes at 0xde, 0xad are set to 0xbe, 0xef, a back door
     * is opened to route PCI interrupts directly to the I/O APIC and bypass the PIC.
     * See the \_SB_.PCI0._PRT method in vbox.dsl.
     */
    const bool  fIsApicEnabled = pGlobals->fUseIoApic && pbCfg[0xde] == 0xbe && pbCfg[0xad] == 0xef;
    int pic_irq, pic_level;

    /* Check if the state changed. */
    if (pPciDev->Int.s.uIrqPinState != iLevel)
    {
        pPciDev->Int.s.uIrqPinState = (iLevel & PDM_IRQ_LEVEL_HIGH);

        /* Send interrupt to I/O APIC only. */
        if (fIsApicEnabled)
        {
            if (fIsAcpiDevice)
                /*
                 * ACPI needs special treatment since SCI is hardwired and
                 * should not be affected by PCI IRQ routing tables at the
                 * same time SCI IRQ is shared in PCI sense hence this
                 * kludge (i.e. we fetch the hardwired value from ACPIs
                 * PCI device configuration space).
                 */
                apic_set_irq(pBus, uDevFn, pPciDev, -1, iLevel, pPciDev->config[PCI_INTERRUPT_LINE], uTagSrc);
            else
                apic_set_irq(pBus, uDevFn, pPciDev, iIrq, iLevel, -1, uTagSrc);
            return;
        }

        if (fIsAcpiDevice)
        {
            /* As per above treat ACPI in a special way */
            pic_irq = pPciDev->config[PCI_INTERRUPT_LINE];
            pGlobals->acpi_irq = pic_irq;
            pGlobals->acpi_irq_level = iLevel & PDM_IRQ_LEVEL_HIGH;
        }
        else
        {
            int irq_num;
            irq_num = pci_slot_get_pirq(uDevFn, iIrq);

            if (pPciDev->Int.s.uIrqPinState == PDM_IRQ_LEVEL_HIGH)
                ASMAtomicIncU32(&pGlobals->pci_irq_levels[irq_num]);
            else if (pPciDev->Int.s.uIrqPinState == PDM_IRQ_LEVEL_LOW)
                ASMAtomicDecU32(&pGlobals->pci_irq_levels[irq_num]);

            /* now we change the pic irq level according to the piix irq mappings */
            pic_irq = pbCfg[0x60 + irq_num];
            if (pic_irq >= 16)
            {
                if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
                {
                    ASMAtomicDecU32(&pGlobals->pci_irq_levels[irq_num]);
                    pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
                }

                return;
            }
        }

        /* the pic level is the logical OR of all the PCI irqs mapped to it */
        pic_level = 0;
        if (pic_irq == pbCfg[0x60])
            pic_level |= get_pci_irq_level(pGlobals, 0);
        if (pic_irq == pbCfg[0x61])
            pic_level |= get_pci_irq_level(pGlobals, 1);
        if (pic_irq == pbCfg[0x62])
            pic_level |= get_pci_irq_level(pGlobals, 2);
        if (pic_irq == pbCfg[0x63])
            pic_level |= get_pci_irq_level(pGlobals, 3);
        if (pic_irq == pGlobals->acpi_irq)
            pic_level |= pGlobals->acpi_irq_level;

        Log3(("pciSetIrq: %s: iLevel=%d iIrq=%d pic_irq=%d pic_level=%d uTagSrc=%#x\n",
              R3STRING(pPciDev->name), iLevel, iIrq, pic_irq, pic_level, uTagSrc));
        pBus->CTX_SUFF(pPciHlp)->pfnIsaSetIrq(pBus->CTX_SUFF(pDevIns), pic_irq, pic_level, uTagSrc);

        /** @todo optimize pci irq flip-flop some rainy day. */
        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
            pciSetIrqInternal(pGlobals, uDevFn, pPciDev, iIrq, PDM_IRQ_LEVEL_LOW, uTagSrc);
    }
}


/**
 * @interface_method_impl{PDMPCIBUSREG,pfnSetIrqR3}
 */
PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel, uint32_t uTagSrc)
{
    pciSetIrqInternal(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), pPciDev->devfn, pPciDev, iIrq, iLevel, uTagSrc);
}

#ifdef IN_RING3

/**
 * Finds a bridge on the bus which contains the destination bus.
 *
 * @return Pointer to the device instance data of the bus or
 *         NULL if no bridge was found.
 * @param  pBus    Pointer to the bus to search on.
 * @param  iBus    Destination bus number.
 */
DECLINLINE(PPCIDEVICE) pciR3FindBridge(PPCIBUS pBus, uint8_t iBus)
{
    /* Search for a fitting bridge. */
    for (uint32_t iBridge = 0; iBridge < pBus->cBridges; iBridge++)
    {
        /*
         * Examine secondary and subordinate bus number.
         * If the target bus is in the range we pass the request on to the bridge.
         */
        PPCIDEVICE pBridgeTemp = pBus->papBridgesR3[iBridge];
        AssertMsg(pBridgeTemp && pciDevIsPci2PciBridge(pBridgeTemp),
                  ("Device is not a PCI bridge but on the list of PCI bridges\n"));

        if (   iBus >= pBridgeTemp->config[VBOX_PCI_SECONDARY_BUS]
            && iBus <= pBridgeTemp->config[VBOX_PCI_SUBORDINATE_BUS])
            return pBridgeTemp;
    }

    /* Nothing found. */
    return NULL;
}

static void pciR3Piix3Reset(PIIX3State *d)
{
    uint8_t *pci_conf = d->dev.config;

    pci_conf[0x04] = 0x07; /* master, memory and I/O */
    pci_conf[0x05] = 0x00;
    pci_conf[0x06] = 0x00;
    pci_conf[0x07] = 0x02; /* PCI_status_devsel_medium */
    pci_conf[0x4c] = 0x4d;
    pci_conf[0x4e] = 0x03;
    pci_conf[0x4f] = 0x00;
    pci_conf[0x60] = 0x80;
    pci_conf[0x69] = 0x02;
    pci_conf[0x70] = 0x80;
    pci_conf[0x76] = 0x0c;
    pci_conf[0x77] = 0x0c;
    pci_conf[0x78] = 0x02;
    pci_conf[0x79] = 0x00;
    pci_conf[0x80] = 0x00;
    pci_conf[0x82] = 0x02; /* Get rid of the Linux guest "Enabling Passive Release" PCI quirk warning. */
    pci_conf[0xa0] = 0x08;
    pci_conf[0xa2] = 0x00;
    pci_conf[0xa3] = 0x00;
    pci_conf[0xa4] = 0x00;
    pci_conf[0xa5] = 0x00;
    pci_conf[0xa6] = 0x00;
    pci_conf[0xa7] = 0x00;
    pci_conf[0xa8] = 0x0f;
    pci_conf[0xaa] = 0x00;
    pci_conf[0xab] = 0x00;
    pci_conf[0xac] = 0x00;
    pci_conf[0xae] = 0x00;
}

static void pci_config_writel(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t val)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | addr;
    pci_data_write(pGlobals, 0, val, 4);
}

static void pci_config_writew(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t val)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | (addr & ~3);
    pci_data_write(pGlobals, addr & 3, val, 2);
}

static void pci_config_writeb(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr, uint32_t val)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | (addr & ~3);
    pci_data_write(pGlobals, addr & 3, val, 1);
}

static uint32_t pci_config_readl(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | addr;
    uint32_t u32Val;
    int rc = pci_data_read(pGlobals, 0, 4, &u32Val);
    AssertRC(rc);
    return u32Val;
}

static uint32_t pci_config_readw(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | (addr & ~3);
    uint32_t u32Val;
    int rc = pci_data_read(pGlobals, addr & 3, 2, &u32Val);
    AssertRC(rc);
    return u32Val;
}

static uint32_t pci_config_readb(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint32_t addr)
{
    pGlobals->uConfigReg = 0x80000000 | (uBus << 16) |
        (uDevFn << 8) | (addr & ~3);
    uint32_t u32Val;
    int rc = pci_data_read(pGlobals, addr & 3, 1, &u32Val);
    AssertRC(rc);
    return u32Val;
}

/* host irqs corresponding to PCI irqs A-D */
static const uint8_t pci_irqs[4] = { 11, 9, 11, 9 }; /* bird: added const */

static void pci_set_io_region_addr(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, int region_num, uint32_t addr)
{
    uint32_t ofs;

    if ( region_num == PCI_ROM_SLOT )
        ofs = 0x30;
    else
        ofs = 0x10 + region_num * 4;

    Log(("Set region address: %02x:%02x.%d region %d address=%lld\n",
         uBus, uDevFn >> 3, uDevFn & 7, region_num, addr));

    /* Write address of the device. */
    pci_config_writel(pGlobals, uBus, uDevFn, ofs, addr);
}

static void pci_bios_init_device(PPCIGLOBALS pGlobals, uint8_t uBus, uint8_t uDevFn, uint8_t cBridgeDepth, uint8_t *paBridgePositions)
{
    uint32_t *paddr;
    int i, pin, pic_irq;
    uint16_t devclass, vendor_id, device_id;

    devclass  = pci_config_readw(pGlobals, uBus, uDevFn, PCI_CLASS_DEVICE);
    vendor_id = pci_config_readw(pGlobals, uBus, uDevFn, PCI_VENDOR_ID);
    device_id = pci_config_readw(pGlobals, uBus, uDevFn, PCI_DEVICE_ID);

    /* Check if device is present. */
    if (vendor_id != 0xffff)
    {
        switch(devclass)
        {
            case 0x0101:
                if (   (vendor_id == 0x8086)
                    && (device_id == 0x7010 || device_id == 0x7111 || device_id == 0x269e))
                {
                    /* PIIX3, PIIX4 or ICH6 IDE */
                    pci_config_writew(pGlobals, uBus, uDevFn, 0x40, 0x8000); /* enable IDE0 */
                    pci_config_writew(pGlobals, uBus, uDevFn, 0x42, 0x8000); /* enable IDE1 */
                    goto default_map;
                }
                else
                {
                    /* IDE: we map it as in ISA mode */
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0x1f0);
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 1, 0x3f4);
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 2, 0x170);
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 3, 0x374);
                    pci_config_writeb(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                      pci_config_readb(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                      | PCI_COMMAND_IOACCESS);
                }
                break;
            case 0x0300:
                if (vendor_id != 0x80ee)
                    goto default_map;
                /* VGA: map frame buffer to default Bochs VBE address */
                pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0xE0000000);
                /*
                 * Legacy VGA I/O ports are implicitly decoded by a VGA class device. But
                 * only the framebuffer (i.e., a memory region) is explicitly registered via
                 * pci_set_io_region_addr, so don't forget to enable I/O decoding.
                 */
                pci_config_writeb(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                  pci_config_readb(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                  | PCI_COMMAND_IOACCESS | PCI_COMMAND_MEMACCESS);
                break;
            case 0x0800:
                /* PIC */
                vendor_id = pci_config_readw(pGlobals, uBus, uDevFn, PCI_VENDOR_ID);
                device_id = pci_config_readw(pGlobals, uBus, uDevFn, PCI_DEVICE_ID);
                if (vendor_id == 0x1014)
                {
                    /* IBM */
                    if (device_id == 0x0046 || device_id == 0xFFFF)
                    {
                        /* MPIC & MPIC2 */
                        pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0x80800000 + 0x00040000);
                        pci_config_writeb(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                          pci_config_readb(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                          | PCI_COMMAND_MEMACCESS);
                    }
                }
                break;
            case 0xff00:
                if (   (vendor_id == 0x0106b)
                    && (device_id == 0x0017 || device_id == 0x0022))
                {
                    /* macio bridge */
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0x80800000);
                    pci_config_writeb(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                      pci_config_readb(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                      | PCI_COMMAND_MEMACCESS);
                }
                break;
            case 0x0604:
            {
                /* Init PCI-to-PCI bridge. */
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_PRIMARY_BUS, uBus);

                AssertMsg(pGlobals->uBus < 255, ("Too many bridges on the bus\n"));
                pGlobals->uBus++;
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_SECONDARY_BUS, pGlobals->uBus);
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, 0xff); /* Temporary until we know how many other bridges are behind this one. */

                /* Add position of this bridge into the array. */
                paBridgePositions[cBridgeDepth+1] = (uDevFn >> 3);

                /*
                 * The I/O range for the bridge must be aligned to a 4KB boundary.
                 * This does not change anything really as the access to the device is not going
                 * through the bridge but we want to be compliant to the spec.
                 */
                if ((pGlobals->pci_bios_io_addr % 4096) != 0)
                    pGlobals->pci_bios_io_addr = RT_ALIGN_32(pGlobals->pci_bios_io_addr, 4*1024);
                Log(("%s: Aligned I/O start address. New address %#x\n", __FUNCTION__, pGlobals->pci_bios_io_addr));
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_IO_BASE, (pGlobals->pci_bios_io_addr >> 8) & 0xf0);

                /* The MMIO range for the bridge must be aligned to a 1MB boundary. */
                if ((pGlobals->pci_bios_mem_addr % (1024 * 1024)) != 0)
                    pGlobals->pci_bios_mem_addr = RT_ALIGN_32(pGlobals->pci_bios_mem_addr, 1024*1024);
                Log(("%s: Aligned MMIO start address. New address %#x\n", __FUNCTION__, pGlobals->pci_bios_mem_addr));
                pci_config_writew(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_BASE, (pGlobals->pci_bios_mem_addr >> 16) & UINT32_C(0xffff0));

                /* Save values to compare later to. */
                uint32_t u32IoAddressBase = pGlobals->pci_bios_io_addr;
                uint32_t u32MMIOAddressBase = pGlobals->pci_bios_mem_addr;

                /* Init devices behind the bridge and possibly other bridges as well. */
                for (int iDev = 0; iDev <= 255; iDev++)
                    pci_bios_init_device(pGlobals, uBus + 1, iDev, cBridgeDepth + 1, paBridgePositions);

                /* The number of bridges behind the this one is now available. */
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_SUBORDINATE_BUS, pGlobals->uBus);

                /*
                 * Set I/O limit register. If there is no device with I/O space behind the bridge
                 * we set a lower value than in the base register.
                 * The result with a real bridge is that no I/O transactions are passed to the secondary
                 * interface. Again this doesn't really matter here but we want to be compliant to the spec.
                 */
                if ((u32IoAddressBase != pGlobals->pci_bios_io_addr) && ((pGlobals->pci_bios_io_addr % 4096) != 0))
                {
                    /* The upper boundary must be one byte less than a 4KB boundary. */
                    pGlobals->pci_bios_io_addr = RT_ALIGN_32(pGlobals->pci_bios_io_addr, 4*1024);
                }
                pci_config_writeb(pGlobals, uBus, uDevFn, VBOX_PCI_IO_LIMIT, ((pGlobals->pci_bios_io_addr >> 8) & 0xf0) - 1);

                /* Same with the MMIO limit register but with 1MB boundary here. */
                if ((u32MMIOAddressBase != pGlobals->pci_bios_mem_addr) && ((pGlobals->pci_bios_mem_addr % (1024 * 1024)) != 0))
                {
                    /* The upper boundary must be one byte less than a 1MB boundary. */
                    pGlobals->pci_bios_mem_addr = RT_ALIGN_32(pGlobals->pci_bios_mem_addr, 1024*1024);
                }
                pci_config_writew(pGlobals, uBus, uDevFn, VBOX_PCI_MEMORY_LIMIT, ((pGlobals->pci_bios_mem_addr >> 16) & UINT32_C(0xfff0)) - 1);

                /*
                 * Set the prefetch base and limit registers. We currently have no device with a prefetchable region
                 * which may be behind a bridge. That's why it is unconditionally disabled here atm by writing a higher value into
                 * the base register than in the limit register.
                 */
                pci_config_writew(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_BASE, 0xfff0);
                pci_config_writew(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_MEMORY_LIMIT, 0x0);
                pci_config_writel(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_BASE_UPPER32, 0x00);
                pci_config_writel(pGlobals, uBus, uDevFn, VBOX_PCI_PREF_LIMIT_UPPER32, 0x00);
                break;
            }
            default:
            default_map:
            {
                /* default memory mappings */
                bool fActiveMemRegion = false;
                bool fActiveIORegion = false;
                /*
                 * PCI_NUM_REGIONS is 7 because of the rom region but there are only 6 base address register defined by the PCI spec.
                 * Leaving only PCI_NUM_REGIONS would cause reading another and enabling a memory region which does not exist.
                 */
                for(i = 0; i < (PCI_NUM_REGIONS-1); i++)
                {
                    uint32_t u32Size;
                    uint8_t  u8RessourceType;
                    uint32_t u32Address = 0x10 + i * 4;

                    /* Calculate size. */
                    u8RessourceType = pci_config_readb(pGlobals, uBus, uDevFn, u32Address);
                    pci_config_writel(pGlobals, uBus, uDevFn, u32Address, UINT32_C(0xffffffff));
                    u32Size = pci_config_readl(pGlobals, uBus, uDevFn, u32Address);
                    bool fIsPio = ((u8RessourceType & PCI_COMMAND_IOACCESS) == PCI_COMMAND_IOACCESS);
                    /* Clear resource information depending on resource type. */
                    if (fIsPio) /* I/O */
                        u32Size &= ~(0x01);
                    else                        /* MMIO */
                        u32Size &= ~(0x0f);

                    /*
                     * Invert all bits and add 1 to get size of the region.
                     * (From PCI implementation note)
                     */
                    if (fIsPio && (u32Size & UINT32_C(0xffff0000)) == 0)
                        u32Size = (~(u32Size | UINT32_C(0xffff0000))) + 1;
                    else
                        u32Size = (~u32Size) + 1;

                    Log2(("%s: Size of region %u for device %d on bus %d is %u\n", __FUNCTION__, i, uDevFn, uBus, u32Size));

                    if (u32Size)
                    {
                        if (fIsPio)
                            paddr = &pGlobals->pci_bios_io_addr;
                        else
                            paddr = &pGlobals->pci_bios_mem_addr;
                        uint32_t uNew = *paddr;
                        uNew = (uNew + u32Size - 1) & ~(u32Size - 1);
                        if (fIsPio)
                            uNew &= UINT32_C(0xffff);
                        /* Unconditionally exclude I/O-APIC/HPET/ROM. Pessimistic, but better than causing a mess. */
                        if (!uNew || (uNew <= UINT32_C(0xffffffff) && uNew + u32Size - 1 >= UINT32_C(0xfec00000)))
                        {
                            LogRel(("PCI: no space left for BAR%u of device %u/%u/%u (vendor=%#06x device=%#06x)\n",
                                    i, uBus, uDevFn >> 3, uDevFn & 7, vendor_id, device_id)); /** @todo make this a VM start failure later. */
                            /* Undo the mapping mess caused by the size probing. */
                            pci_config_writel(pGlobals, uBus, uDevFn, u32Address, UINT32_C(0));
                        }
                        else
                        {
                            Log(("%s: Start address of %s region %u is %#x\n", __FUNCTION__, (fIsPio ? "I/O" : "MMIO"), i, uNew));
                            pci_set_io_region_addr(pGlobals, uBus, uDevFn, i, uNew);
                            if (fIsPio)
                                fActiveIORegion = true;
                            else
                                fActiveMemRegion = true;
                            *paddr = uNew + u32Size;
                            Log2(("%s: New address is %#x\n", __FUNCTION__, *paddr));
                        }
                    }
                }

                /* Update the command word appropriately. */
                pci_config_writeb(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                  pci_config_readb(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                  | (fActiveMemRegion ? PCI_COMMAND_MEMACCESS : 0)
                                  | (fActiveIORegion ? PCI_COMMAND_IOACCESS : 0));

                break;
            }
        }

        /* map the interrupt */
        pin = pci_config_readb(pGlobals, uBus, uDevFn, PCI_INTERRUPT_PIN);
        if (pin != 0)
        {
            uint8_t uBridgeDevFn = uDevFn;
            pin--;

            /* We need to go up to the host bus to see which irq this device will assert there. */
            while (cBridgeDepth != 0)
            {
                /* Get the pin the device would assert on the bridge. */
                pin = ((uBridgeDevFn >> 3) + pin) & 3;
                uBridgeDevFn = paBridgePositions[cBridgeDepth];
                cBridgeDepth--;
            }

            pin = pci_slot_get_pirq(uDevFn, pin);
            pic_irq = pci_irqs[pin];
            pci_config_writeb(pGlobals, uBus, uDevFn, PCI_INTERRUPT_LINE, pic_irq);
        }
    }
}

#endif /* IN_RING3 */


/* -=-=-=-=-=- I/O ports -=-=-=-=-=- */

/**
 * @callback_method_impl{FNIOMIOPORTOUT, PCI address}
 */
PDMBOTHCBDECL(int) pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortAddressWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_R3_IOPORT_WRITE);
        pThis->uConfigReg = u32 & ~3; /* Bits 0-1 are reserved and we silently clear them */
        PCI_UNLOCK(pDevIns);
    }
    /* else: 440FX does "pass through to the bus" for other writes, what ever that means.
     * Linux probes for cmd640 using byte writes/reads during ide init. We'll just ignore it. */
    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNIOMIOPORTIN, PCI address}
 */
PDMBOTHCBDECL(int) pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_R3_IOPORT_READ);
        *pu32 = pThis->uConfigReg;
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortAddressRead: Port=%#x cb=%d -> %#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
    }
    /* else: 440FX does "pass through to the bus" for other writes, what ever that means.
     * Linux probes for cmd640 using byte writes/reads during ide init. We'll just ignore it. */
    Log(("pciIOPortAddressRead: Port=%#x cb=%d VERR_IOM_IOPORT_UNUSED\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * @callback_method_impl{FNIOMIOPORTOUT, PCI data}
 */
PDMBOTHCBDECL(int) pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortDataWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    int rc = VINF_SUCCESS;
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_R3_IOPORT_WRITE);
        rc = pci_data_write(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, u32, cb);
        PCI_UNLOCK(pDevIns);
    }
    else
        AssertMsgFailed(("Write to port %#x u32=%#x cb=%d\n", Port, u32, cb));
    return rc;
}


/**
 * @callback_method_impl{FNIOMIOPORTIN, PCI data}
 */
PDMBOTHCBDECL(int) pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_R3_IOPORT_READ);
        int rc = pci_data_read(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, cb, pu32);
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortDataRead: Port=%#x cb=%#x -> %#x (%Rrc)\n", Port, cb, *pu32, rc));
        return rc;
    }
    AssertMsgFailed(("Read from port %#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

#ifdef IN_RING3

/* -=-=-=-=-=- Saved state -=-=-=-=-=- */

/**
 * Common worker for pciR3SaveExec and pcibridgeR3SaveExec.
 *
 * @returns VBox status code.
 * @param   pBus            The bus to save.
 * @param   pSSM            The saved state handle.
 */
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


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) pciR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    uint32_t    i;
    PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);

    /*
     * Bus state data.
     */
    SSMR3PutU32(pSSM, pThis->uConfigReg);
    SSMR3PutBool(pSSM, pThis->fUseIoApic);

    /*
     * Save IRQ states.
     */
    for (i = 0; i < PCI_IRQ_PINS; i++)
        SSMR3PutU32(pSSM, pThis->pci_irq_levels[i]);
    for (i = 0; i < PCI_APIC_IRQ_PINS; i++)
        SSMR3PutU32(pSSM, pThis->pci_apic_irq_levels[i]);

    SSMR3PutU32(pSSM, pThis->acpi_irq_level);
    SSMR3PutS32(pSSM, pThis->acpi_irq);

    SSMR3PutU32(pSSM, ~0);        /* separator */

    /*
     * Join paths with pcibridgeR3SaveExec.
     */
    return pciR3CommonSaveExec(&pThis->PciBus, pSSM);
}


/**
 * Common routine for restoring the config registers of a PCI device.
 *
 * @param   pDev                The PCI device.
 * @param   pbSrcConfig         The configuration register values to be loaded.
 * @param   fIsBridge           Whether this is a bridge device or not.
 */
static void pciR3CommonRestoreConfig(PPCIDEVICE pDev, uint8_t const *pbSrcConfig, bool fIsBridge)
{
    /*
     * This table defines the fields for normal devices and bridge devices, and
     * the order in which they need to be restored.
     */
    static const struct PciField
    {
        uint8_t     off;
        uint8_t     cb;
        uint8_t     fWritable;
        uint8_t     fBridge;
        const char *pszName;
    } s_aFields[] =
    {
        /* off,cb,fW,fB, pszName */
        { 0x00, 2, 0, 3, "VENDOR_ID" },
        { 0x02, 2, 0, 3, "DEVICE_ID" },
        { 0x06, 2, 1, 3, "STATUS" },
        { 0x08, 1, 0, 3, "REVISION_ID" },
        { 0x09, 1, 0, 3, "CLASS_PROG" },
        { 0x0a, 1, 0, 3, "CLASS_SUB" },
        { 0x0b, 1, 0, 3, "CLASS_BASE" },
        { 0x0c, 1, 1, 3, "CACHE_LINE_SIZE" },
        { 0x0d, 1, 1, 3, "LATENCY_TIMER" },
        { 0x0e, 1, 0, 3, "HEADER_TYPE" },
        { 0x0f, 1, 1, 3, "BIST" },
        { 0x10, 4, 1, 3, "BASE_ADDRESS_0" },
        { 0x14, 4, 1, 3, "BASE_ADDRESS_1" },
        { 0x18, 4, 1, 1, "BASE_ADDRESS_2" },
        { 0x18, 1, 1, 2, "PRIMARY_BUS" },       // fWritable = ??
        { 0x19, 1, 1, 2, "SECONDARY_BUS" },     // fWritable = ??
        { 0x1a, 1, 1, 2, "SUBORDINATE_BUS" },   // fWritable = ??
        { 0x1b, 1, 1, 2, "SEC_LATENCY_TIMER" }, // fWritable = ??
        { 0x1c, 4, 1, 1, "BASE_ADDRESS_3" },
        { 0x1c, 1, 1, 2, "IO_BASE" },           // fWritable = ??
        { 0x1d, 1, 1, 2, "IO_LIMIT" },          // fWritable = ??
        { 0x1e, 2, 1, 2, "SEC_STATUS" },        // fWritable = ??
        { 0x20, 4, 1, 1, "BASE_ADDRESS_4" },
        { 0x20, 2, 1, 2, "MEMORY_BASE" },       // fWritable = ??
        { 0x22, 2, 1, 2, "MEMORY_LIMIT" },      // fWritable = ??
        { 0x24, 4, 1, 1, "BASE_ADDRESS_5" },
        { 0x24, 2, 1, 2, "PREF_MEMORY_BASE" },  // fWritable = ??
        { 0x26, 2, 1, 2, "PREF_MEMORY_LIMIT" }, // fWritable = ??
        { 0x28, 4, 1, 1, "CARDBUS_CIS" },       // fWritable = ??
        { 0x28, 4, 1, 2, "PREF_BASE_UPPER32" }, // fWritable = ??
        { 0x2c, 2, 0, 1, "SUBSYSTEM_VENDOR_ID" },// fWritable = !?
        { 0x2c, 4, 1, 2, "PREF_LIMIT_UPPER32" },// fWritable = ??
        { 0x2e, 2, 0, 1, "SUBSYSTEM_ID" },      // fWritable = !?
        { 0x30, 4, 1, 1, "ROM_ADDRESS" },       // fWritable = ?!
        { 0x30, 2, 1, 2, "IO_BASE_UPPER16" },   // fWritable = ?!
        { 0x32, 2, 1, 2, "IO_LIMIT_UPPER16" },  // fWritable = ?!
        { 0x34, 4, 0, 3, "CAPABILITY_LIST" },   // fWritable = !? cb=!?
        { 0x38, 4, 1, 1, "RESERVED_38" },       // ???
        { 0x38, 4, 1, 2, "ROM_ADDRESS_BR" },    // fWritable = !? cb=!? fBridge=!?
        { 0x3c, 1, 1, 3, "INTERRUPT_LINE" },    // fBridge=??
        { 0x3d, 1, 0, 3, "INTERRUPT_PIN" },     // fBridge=??
        { 0x3e, 1, 0, 1, "MIN_GNT" },
        { 0x3e, 2, 1, 2, "BRIDGE_CONTROL" },    // fWritable = !?
        { 0x3f, 1, 0, 1, "MAX_LAT" },
        /* The COMMAND register must come last as it requires the *ADDRESS*
           registers to be restored before we pretent to change it from 0 to
           whatever value the guest assigned it. */
        { 0x04, 2, 1, 3, "COMMAND" },
    };

#ifdef RT_STRICT
    /* Check that we've got full register coverage. */
    uint32_t bmDevice[0x40 / 32];
    uint32_t bmBridge[0x40 / 32];
    RT_ZERO(bmDevice);
    RT_ZERO(bmBridge);
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFields); i++)
    {
        uint8_t off = s_aFields[i].off;
        uint8_t cb  = s_aFields[i].cb;
        uint8_t f   = s_aFields[i].fBridge;
        while (cb-- > 0)
        {
            if (f & 1) AssertMsg(!ASMBitTest(bmDevice, off), ("%#x\n", off));
            if (f & 2) AssertMsg(!ASMBitTest(bmBridge, off), ("%#x\n", off));
            if (f & 1) ASMBitSet(bmDevice, off);
            if (f & 2) ASMBitSet(bmBridge, off);
            off++;
        }
    }
    for (uint32_t off = 0; off < 0x40; off++)
    {
        AssertMsg(ASMBitTest(bmDevice, off), ("%#x\n", off));
        AssertMsg(ASMBitTest(bmBridge, off), ("%#x\n", off));
    }
#endif

    /*
     * Loop thru the fields covering the 64 bytes of standard registers.
     */
    uint8_t const fBridge = fIsBridge ? 2 : 1;
    uint8_t *pbDstConfig = &pDev->config[0];
    for (uint32_t i = 0; i < RT_ELEMENTS(s_aFields); i++)
        if (s_aFields[i].fBridge & fBridge)
        {
            uint8_t const   off = s_aFields[i].off;
            uint8_t const   cb  = s_aFields[i].cb;
            uint32_t        u32Src;
            uint32_t        u32Dst;
            switch (cb)
            {
                case 1:
                    u32Src = pbSrcConfig[off];
                    u32Dst = pbDstConfig[off];
                    break;
                case 2:
                    u32Src = *(uint16_t const *)&pbSrcConfig[off];
                    u32Dst = *(uint16_t const *)&pbDstConfig[off];
                    break;
                case 4:
                    u32Src = *(uint32_t const *)&pbSrcConfig[off];
                    u32Dst = *(uint32_t const *)&pbDstConfig[off];
                    break;
                default:
                    AssertFailed();
                    continue;
            }

            if (    u32Src != u32Dst
                ||  off == VBOX_PCI_COMMAND)
            {
                if (u32Src != u32Dst)
                {
                    if (!s_aFields[i].fWritable)
                        LogRel(("PCI: %8s/%u: %2u-bit field %s: %x -> %x - !READ ONLY!\n",
                                pDev->name, pDev->pDevIns->iInstance, cb*8, s_aFields[i].pszName, u32Dst, u32Src));
                    else
                        LogRel(("PCI: %8s/%u: %2u-bit field %s: %x -> %x\n",
                                pDev->name, pDev->pDevIns->iInstance, cb*8, s_aFields[i].pszName, u32Dst, u32Src));
                }
                if (off == VBOX_PCI_COMMAND)
                    PCIDevSetCommand(pDev, 0); /* For remapping, see pciR3CommonLoadExec. */
                pDev->Int.s.pfnConfigWrite(pDev, off, u32Src, cb);
            }
        }

    /*
     * The device dependent registers.
     *
     * We will not use ConfigWrite here as we have no clue about the size
     * of the registers, so the device is responsible for correctly
     * restoring functionality governed by these registers.
     */
    for (uint32_t off = 0x40; off < sizeof(pDev->config); off++)
        if (pbDstConfig[off] != pbSrcConfig[off])
        {
            LogRel(("PCI: %8s/%u: register %02x: %02x -> %02x\n",
                    pDev->name, pDev->pDevIns->iInstance, off, pbDstConfig[off], pbSrcConfig[off])); /** @todo make this Log() later. */
            pbDstConfig[off] = pbSrcConfig[off];
        }
}


/**
 * Common worker for pciR3LoadExec and pcibridgeR3LoadExec.
 *
 * @returns VBox status code.
 * @param   pBus                The bus which data is being loaded.
 * @param   pSSM                The saved state handle.
 * @param   uVersion            The data version.
 * @param   uPass               The pass.
 */
static DECLCALLBACK(int) pciR3CommonLoadExec(PPCIBUS pBus, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    uint32_t    u32;
    uint32_t    i;
    int         rc;

    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Iterate thru all the devices and write 0 to the COMMAND register so
     * that all the memory is unmapped before we start restoring the saved
     * mapping locations.
     *
     * The register value is restored afterwards so we can do proper
     * LogRels in pciR3CommonRestoreConfig.
     */
    for (i = 0; i < RT_ELEMENTS(pBus->devices); i++)
    {
        PPCIDEVICE pDev = pBus->devices[i];
        if (pDev)
        {
            uint16_t u16 = PCIDevGetCommand(pDev);
            pDev->Int.s.pfnConfigWrite(pDev, VBOX_PCI_COMMAND, 0, 2);
            PCIDevSetCommand(pDev, u16);
            Assert(PCIDevGetCommand(pDev) == u16);
        }
    }

    /*
     * Iterate all the devices.
     */
    for (i = 0;; i++)
    {
        PCIDEVICE   DevTmp;
        PPCIDEVICE  pDev;

        /* index / terminator */
        rc = SSMR3GetU32(pSSM, &u32);
        if (RT_FAILURE(rc))
            return rc;
        if (u32 == (uint32_t)~0)
            break;
        if (    u32 >= RT_ELEMENTS(pBus->devices)
            ||  u32 < i)
        {
            AssertMsgFailed(("u32=%#x i=%#x\n", u32, i));
            return rc;
        }

        /* skip forward to the device checking that no new devices are present. */
        for (; i < u32; i++)
        {
            if (pBus->devices[i])
            {
                LogRel(("PCI: New device in slot %#x, %s (vendor=%#06x device=%#06x)\n", i, pBus->devices[i]->name,
                        PCIDevGetVendorId(pBus->devices[i]), PCIDevGetDeviceId(pBus->devices[i])));
                if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                    return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("New device in slot %#x, %s (vendor=%#06x device=%#06x)"),
                                            i, pBus->devices[i]->name, PCIDevGetVendorId(pBus->devices[i]), PCIDevGetDeviceId(pBus->devices[i]));
            }
        }

        /* get the data */
        DevTmp.Int.s.uIrqPinState = ~0; /* Invalid value in case we have an older saved state to force a state change in pciSetIrq. */
        SSMR3GetMem(pSSM, DevTmp.config, sizeof(DevTmp.config));
        if (uVersion < 3)
        {
            int32_t i32Temp;
            /* Irq value not needed anymore. */
            rc = SSMR3GetS32(pSSM, &i32Temp);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
        {
            rc = SSMR3GetS32(pSSM, &DevTmp.Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;
        }

        /* check that it's still around. */
        pDev = pBus->devices[i];
        if (!pDev)
        {
            LogRel(("PCI: Device in slot %#x has been removed! vendor=%#06x device=%#06x\n", i,
                    PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp)));
            if (SSMR3HandleGetAfter(pSSM) != SSMAFTER_DEBUG_IT)
                return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device in slot %#x has been removed! vendor=%#06x device=%#06x"),
                                        i, PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp));
            continue;
        }

        /* match the vendor id assuming that this will never be changed. */
        if (    DevTmp.config[0] != pDev->config[0]
            ||  DevTmp.config[1] != pDev->config[1])
            return SSMR3SetCfgError(pSSM, RT_SRC_POS, N_("Device in slot %#x (%s) vendor id mismatch! saved=%.4Rhxs current=%.4Rhxs"),
                                     i, pDev->name, DevTmp.config, pDev->config);

        /* commit the loaded device config. */
        pciR3CommonRestoreConfig(pDev, &DevTmp.config[0], false ); /** @todo fix bridge fun! */

        pDev->Int.s.uIrqPinState = DevTmp.Int.s.uIrqPinState;
    }

    return VINF_SUCCESS;
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) pciR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus  = &pThis->PciBus;
    uint32_t    u32;
    int         rc;

    /*
     * Check the version.
     */
    if (uVersion > VBOX_PCI_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Bus state data.
     */
    SSMR3GetU32(pSSM, &pThis->uConfigReg);
    if (uVersion > 1)
        SSMR3GetBool(pSSM, &pThis->fUseIoApic);

    /* Load IRQ states. */
    if (uVersion > 2)
    {
        for (uint8_t i = 0; i < PCI_IRQ_PINS; i++)
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->pci_irq_levels[i]);
        for (uint8_t i = 0; i < PCI_APIC_IRQ_PINS; i++)
            SSMR3GetU32(pSSM, (uint32_t *)&pThis->pci_apic_irq_levels[i]);

        SSMR3GetU32(pSSM, &pThis->acpi_irq_level);
        SSMR3GetS32(pSSM, &pThis->acpi_irq);
    }

    /* separator */
    rc = SSMR3GetU32(pSSM, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != (uint32_t)~0)
        AssertMsgFailedReturn(("u32=%#x\n", u32), rc);

    /*
     * The devices.
     */
    return pciR3CommonLoadExec(pBus, pSSM, uVersion, uPass);
}


/* -=-=-=-=-=- PCI Bus Interface Methods (PDMPCIBUSREG) -=-=-=-=-=- */

/**
 * Registers the device with the specified PCI bus.
 *
 * @returns VBox status code.
 * @param   pBus            The bus to register with.
 * @param   iDev            The PCI device ordinal.
 * @param   pPciDev         The PCI device structure.
 * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
 */
static int pciR3RegisterDeviceInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
{
    /*
     * Find device slot.
     */
    if (iDev < 0)
    {
        /*
         * Special check for the IDE controller which is our function 1 device
         * before searching.
         */
        if (    !strcmp(pszName, "piix3ide")
            &&  !pBus->devices[9])
            iDev = 9;
        /* LPC bus expected to be there by some guests, better make an additional argument to PDM
           device helpers, but requires significant rewrite */
        else if (!strcmp(pszName, "lpc")
             &&  !pBus->devices[0xf8])
            iDev = 0xf8;
        else
        {
            Assert(!(pBus->iDevSearch % 8));
            for (iDev = pBus->iDevSearch; iDev < (int)RT_ELEMENTS(pBus->devices)-7; iDev += 8)
                if (    !pBus->devices[iDev]
                    &&  !pBus->devices[iDev + 1]
                    &&  !pBus->devices[iDev + 2]
                    &&  !pBus->devices[iDev + 3]
                    &&  !pBus->devices[iDev + 4]
                    &&  !pBus->devices[iDev + 5]
                    &&  !pBus->devices[iDev + 6]
                    &&  !pBus->devices[iDev + 7])
                    break;
            if (iDev >= (int)RT_ELEMENTS(pBus->devices))
            {
                AssertMsgFailed(("Couldn't find free spot!\n"));
                return VERR_PDM_TOO_PCI_MANY_DEVICES;
            }
        }
        pciDevClearRequestedDevfunc(pPciDev);
    }
    else
    {
        /*
         * An explicit request.
         *
         * If the slot is occupied we'll have to relocate the device
         * currently occupying it first. This can only be done if the
         * existing device wasn't explicitly assigned. Also we limit
         * ourselves to function 0 devices.
         *
         * If you start setting devices + function in the
         * config, do it for all pci devices!
         */
        //AssertReleaseMsg(iDev > 8 || pBus->iBus != 0, ("iDev=%d pszName=%s\n", iDev, pszName));
        if (pBus->devices[iDev])
        {
            int iDevRel;
            AssertReleaseMsg(!(iDev % 8), ("PCI Configuration Conflict! iDev=%d pszName=%s clashes with %s\n",
                                           iDev, pszName, pBus->devices[iDev]->name));
            if (    pciDevIsRequestedDevfunc(pBus->devices[iDev])
                ||  (pBus->devices[iDev + 1] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 1]))
                ||  (pBus->devices[iDev + 2] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 2]))
                ||  (pBus->devices[iDev + 3] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 3]))
                ||  (pBus->devices[iDev + 4] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 4]))
                ||  (pBus->devices[iDev + 5] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 5]))
                ||  (pBus->devices[iDev + 6] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 6]))
                ||  (pBus->devices[iDev + 7] && pciDevIsRequestedDevfunc(pBus->devices[iDev + 7])))
            {
                AssertReleaseMsgFailed(("Configuration error:'%s' and '%s' are both configured as device %d\n",
                                        pszName, pBus->devices[iDev]->name, iDev));
                return VERR_INTERNAL_ERROR;
            }

            /* Find free slot for the device(s) we're moving and move them. */
            for (iDevRel = pBus->iDevSearch; iDevRel < (int)RT_ELEMENTS(pBus->devices)-7; iDevRel += 8)
            {
                if (    !pBus->devices[iDevRel]
                    &&  !pBus->devices[iDevRel + 1]
                    &&  !pBus->devices[iDevRel + 2]
                    &&  !pBus->devices[iDevRel + 3]
                    &&  !pBus->devices[iDevRel + 4]
                    &&  !pBus->devices[iDevRel + 5]
                    &&  !pBus->devices[iDevRel + 6]
                    &&  !pBus->devices[iDevRel + 7])
                {
                    int i = 0;
                    for (i = 0; i < 8; i++)
                    {
                        if (!pBus->devices[iDev + i])
                            continue;
                        Log(("PCI: relocating '%s' from slot %#x to %#x\n", pBus->devices[iDev + i]->name, iDev + i, iDevRel + i));
                        pBus->devices[iDevRel + i] = pBus->devices[iDev + i];
                        pBus->devices[iDevRel + i]->devfn = iDevRel + i;
                        pBus->devices[iDev + i] = NULL;
                    }
                }
            }
            if (pBus->devices[iDev])
            {
                AssertMsgFailed(("Couldn't find free spot!\n"));
                return VERR_PDM_TOO_PCI_MANY_DEVICES;
            }
        } /* if conflict */
        pciDevSetRequestedDevfunc(pPciDev);
    }

    Assert(!pBus->devices[iDev]);
    pPciDev->devfn                  = iDev;
    pPciDev->name                   = pszName;
    pPciDev->Int.s.pBusR3           = pBus;
    pPciDev->Int.s.pBusR0           = MMHyperR3ToR0(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pBusRC           = MMHyperR3ToRC(PDMDevHlpGetVM(pBus->CTX_SUFF(pDevIns)), pBus);
    pPciDev->Int.s.pfnConfigRead    = pci_default_read_config;
    pPciDev->Int.s.pfnConfigWrite   = pci_default_write_config;
    pBus->devices[iDev]             = pPciDev;
    if (pciDevIsPci2PciBridge(pPciDev))
    {
        AssertMsg(pBus->cBridges < RT_ELEMENTS(pBus->devices), ("Number of bridges exceeds the number of possible devices on the bus\n"));
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
 * @interface_method_impl{PDMPCIBUSREG,pfnRegisterR3}
 */
static DECLCALLBACK(int) pciR3Register(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    PPCIBUS     pBus = DEVINS_2_PCIBUS(pDevIns);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)RT_ELEMENTS(pBus->devices)
        ||  (iDev >= 0 && iDev <= 8))
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the device.
     */
    return pciR3RegisterDeviceInternal(pBus, iDev, pPciDev, pszName);
}


/**
 * @interface_method_impl{PDMPCIBUSREG,pfnIORegionRegisterR3}
 */
static DECLCALLBACK(int) pciR3CommonIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion,
                                                     PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    NOREF(pDevIns);

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
    uint32_t u32Address = 0x10 + iRegion * 4;
    uint32_t u32Value   =   (enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH ? (1 << 3) : 0)
                          | (enmType == PCI_ADDRESS_SPACE_IO ? 1 : 0);
    *(uint32_t *)(pPciDev->config + u32Address) = RT_H2LE_U32(u32Value);

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{PDMPCIBUSREG,pfnSetConfigCallbacksR3}
 */
static DECLCALLBACK(void)
pciR3CommonSetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
                              PFNPCICONFIGWRITE pfnWrite, PPFNPCICONFIGWRITE ppfnWriteOld)
{
    NOREF(pDevIns);

    if (ppfnReadOld)
        *ppfnReadOld = pPciDev->Int.s.pfnConfigRead;
    pPciDev->Int.s.pfnConfigRead  = pfnRead;

    if (ppfnWriteOld)
        *ppfnWriteOld = pPciDev->Int.s.pfnConfigWrite;
    pPciDev->Int.s.pfnConfigWrite = pfnWrite;
}


/**
 * @interface_method_impl{PDMPCIBUSREG,pfnFakePCIBIOSR3}
 */
static DECLCALLBACK(int) pciR3FakePCIBIOS(PPDMDEVINS pDevIns)
{
    unsigned    i;
    uint8_t     elcr[2]  = {0, 0};
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PVM         pVM      = PDMDevHlpGetVM(pDevIns); Assert(pVM);
    PVMCPU      pVCpu    = PDMDevHlpGetVMCPU(pDevIns); Assert(pVM);

    /*
     * Set the start addresses.
     */
    pGlobals->pci_bios_io_addr  = 0xd000;
    pGlobals->pci_bios_mem_addr = UINT32_C(0xf0000000);
    pGlobals->uBus = 0;

    /*
     * Activate IRQ mappings.
     */
    for (i = 0; i < 4; i++)
    {
        uint8_t irq = pci_irqs[i];
        /* Set to trigger level. */
        elcr[irq >> 3] |= (1 << (irq & 7));
        /* Activate irq remapping in PIIX3. */
        pci_config_writeb(pGlobals, 0, pGlobals->PIIX3State.dev.devfn, 0x60 + i, irq);
    }

    /* Tell to the PIC. */
    VBOXSTRICTRC rcStrict = IOMIOPortWrite(pVM, pVCpu, 0x4d0, elcr[0], sizeof(uint8_t));
    if (rcStrict == VINF_SUCCESS)
        rcStrict = IOMIOPortWrite(pVM, pVCpu, 0x4d1, elcr[1], sizeof(uint8_t));
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
        pci_bios_init_device(pGlobals, 0, i, 0, aBridgePositions);
    }

    return VINF_SUCCESS;
}


/* -=-=-=-=-=- Debug Info Handlers -=-=-=-=-=- */

/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) pciR3IrqInfo(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    NOREF(pszArgs);

    uint16_t router = pGlobals->PIIX3State.dev.devfn;
    pHlp->pfnPrintf(pHlp, "PCI interrupt router at: %02X:%02X:%X\n",
                    router >> 8, (router >> 3) & 0x1f, router & 0x7);

    for (int i = 0; i < 4; ++i)
    {
        uint8_t irq_map = pci_config_readb(pGlobals, 0, router, 0x60 + i);
        if (irq_map & 0x80)
            pHlp->pfnPrintf(pHlp, "PIRQ%c disabled\n", 'A' + i);
        else
            pHlp->pfnPrintf(pHlp, "PIRQ%c -> IRQ%d\n", 'A' + i, irq_map & 0xf);
    }
}

/**
 * Outputs indent.
 *
 * @param   pHlp        Output helpers.
 * @param   iIndent     Indentation level.
 */
static void pciR3PrintIndent(PCDBGFINFOHLP pHlp, int iIndent)
{
    while (iIndent-- > 0)
        pHlp->pfnPrintf(pHlp, "    ");
}

/**
 * Recursive worker for pciR3Info.
 *
 * @param   pBus        The bus to display.
 * @param   pHlp        Output helpers.
 * @param   iIndent     Indentation level.
 * @param   fRegisters  Whether to also display the PCI configuration registers
 *                      of each device on the bus.
 */
static void pciR3BusInfo(PPCIBUS pBus, PCDBGFINFOHLP pHlp, int iIndent, bool fRegisters)
{
    for (uint32_t iDev = 0; iDev < RT_ELEMENTS(pBus->devices); iDev++)
    {
        PPCIDEVICE pPciDev = pBus->devices[iDev];
        if (pPciDev != NULL)
        {
            pciR3PrintIndent(pHlp, iIndent);

            /*
             * For passthrough devices MSI/MSI-X mostly reflects the way interrupts delivered to the guest,
             * as host driver handles real devices interrupts.
             */
            pHlp->pfnPrintf(pHlp, "%02x:%02x:%02x %s%s: %04x-%04x%s%s",
                            pBus->iBus, (iDev >> 3) & 0xff, iDev & 0x7,
                            pPciDev->name,
                            pciDevIsPassthrough(pPciDev) ? " (PASSTHROUGH)" : "",
                            PCIDevGetWord(pPciDev, VBOX_PCI_VENDOR_ID), PCIDevGetWord(pPciDev, VBOX_PCI_DEVICE_ID),
                            pciDevIsMsiCapable(pPciDev)  ? " MSI" : "",
                            pciDevIsMsixCapable(pPciDev) ? " MSI-X" : ""
                            );
            if (PCIDevGetByte(pPciDev, VBOX_PCI_INTERRUPT_PIN) != 0)
                pHlp->pfnPrintf(pHlp, " IRQ%d", PCIDevGetByte(pPciDev, VBOX_PCI_INTERRUPT_LINE));

            pHlp->pfnPrintf(pHlp, "\n");

            uint16_t iCmd = PCIDevGetWord(pPciDev, VBOX_PCI_COMMAND);
            if ((iCmd & (VBOX_PCI_COMMAND_IO | VBOX_PCI_COMMAND_MEMORY)) != 0)
            {
                for (int iRegion = 0; iRegion < PCI_NUM_REGIONS; iRegion++)
                {
                    PCIIORegion* pRegion = &pPciDev->Int.s.aIORegions[iRegion];
                    uint64_t  iRegionSize = pRegion->size;

                    if (iRegionSize == 0)
                        continue;

                    uint32_t u32Addr = PCIDevGetDWord(pPciDev, PCIDevGetRegionReg(iRegion));
                    const char * pszDesc;
                    char szDescBuf[128];

                    bool f64Bit = !!(pRegion->type & PCI_ADDRESS_SPACE_BAR64);
                    if (pRegion->type & PCI_ADDRESS_SPACE_IO)
                    {
                        pszDesc = "IO";
                        u32Addr &= ~0x3;
                    }
                    else
                    {
                        RTStrPrintf(szDescBuf, sizeof(szDescBuf), "MMIO%s%s",
                                    f64Bit ? "64" : "32",
                                    (pRegion->type & PCI_ADDRESS_SPACE_MEM_PREFETCH) ? " PREFETCH" : "");
                        pszDesc = szDescBuf;
                        u32Addr &= ~0xf;
                    }

                    pciR3PrintIndent(pHlp, iIndent + 2);
                    pHlp->pfnPrintf(pHlp, "%s region #%d: %x..%x\n",
                                    pszDesc, iRegion, u32Addr, u32Addr+iRegionSize);
                    if (f64Bit)
                        iRegion++;
                }
            }

            pciR3PrintIndent(pHlp, iIndent + 2);
            uint16_t iStatus = PCIDevGetWord(pPciDev, VBOX_PCI_STATUS);
            pHlp->pfnPrintf(pHlp, "Command: %.*Rhxs, Status: %.*Rhxs\n",
                            sizeof(uint16_t), &iCmd, sizeof(uint16_t), &iStatus);
            pciR3PrintIndent(pHlp, iIndent + 2);
            pHlp->pfnPrintf(pHlp, "Bus master: %s\n",
                            iCmd & VBOX_PCI_COMMAND_MASTER ? "Yes" : "No");

            if (fRegisters)
            {
                pciR3PrintIndent(pHlp, iIndent + 2);
                pHlp->pfnPrintf(pHlp, "PCI registers:\n");
                for (int iReg = 0; iReg < 0x100; )
                {
                    int iPerLine = 0x10;
                    Assert (0x100 % iPerLine == 0);
                    pciR3PrintIndent(pHlp, iIndent + 3);

                    while (iPerLine-- > 0)
                    {
                        pHlp->pfnPrintf(pHlp, "%02x ", PCIDevGetByte(pPciDev, iReg++));
                    }
                    pHlp->pfnPrintf(pHlp, "\n");
                }
            }
        }
    }

    if (pBus->cBridges > 0)
    {
        pciR3PrintIndent(pHlp, iIndent);
        pHlp->pfnPrintf(pHlp, "Registered %d bridges, subordinate buses info follows\n", pBus->cBridges);
        for (uint32_t iBridge = 0; iBridge < pBus->cBridges; iBridge++)
        {
            PPCIBUS pBusSub = PDMINS_2_DATA(pBus->papBridgesR3[iBridge]->pDevIns, PPCIBUS);
            pciR3BusInfo(pBusSub, pHlp, iIndent + 1, fRegisters);
        }
    }
}


/**
 * @callback_method_impl{FNDBGFHANDLERDEV}
 */
static DECLCALLBACK(void) pciR3Info(PPDMDEVINS pDevIns, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    PPCIBUS pBus = DEVINS_2_PCIBUS(pDevIns);

    if (pszArgs == NULL || !*pszArgs || !strcmp(pszArgs, "basic"))
        pciR3BusInfo(pBus, pHlp, 0, false);
    else if (!strcmp(pszArgs, "verbose"))
        pciR3BusInfo(pBus, pHlp, 0, true);
    else
        pHlp->pfnPrintf(pHlp, "Invalid argument. Recognized arguments are 'basic', 'verbose'.\n");
}


/* -=-=-=-=-=- PDMDEVREG  -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) pciR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus     = &pGlobals->PciBus;
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->devices); i++)
    {
        if (pBus->devices[i])
            pBus->devices[i]->Int.s.pBusRC += offDelta;
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) pciR3Reset(PPDMDEVINS pDevIns)
{
    pciR3FakePCIBIOS(pDevIns);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)   pciR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    Assert(iInstance == 0);
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "IOAPIC\0" "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* query whether we got an IOAPIC */
    bool fUseIoApic;
    int rc = CFGMR3QueryBoolDef(pCfg, "IOAPIC", &fUseIoApic, false);
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
     * Init data and register the PCI bus.
     */
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    pGlobals->pci_bios_io_addr    = 0xc000;
    pGlobals->pci_bios_mem_addr   = 0xf0000000;
    memset((void *)&pGlobals->pci_irq_levels, 0, sizeof(pGlobals->pci_irq_levels));
    pGlobals->fUseIoApic          = fUseIoApic;
    memset((void *)&pGlobals->pci_apic_irq_levels, 0, sizeof(pGlobals->pci_apic_irq_levels));

    pGlobals->pDevInsR3 = pDevIns;
    pGlobals->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    pGlobals->PciBus.pDevInsR3 = pDevIns;
    pGlobals->PciBus.pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pGlobals->PciBus.pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pGlobals->PciBus.papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns,   sizeof(PPCIDEVICE)
                                                                                 * RT_ELEMENTS(pGlobals->PciBus.devices));

    PDMPCIBUSREG PciBusReg;
    PPCIBUS      pBus = &pGlobals->PciBus;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = pciR3Register;
    PciBusReg.pfnRegisterMsiR3        = NULL;
    PciBusReg.pfnIORegionRegisterR3   = pciR3CommonIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = pciR3CommonSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = pciSetIrq;
    PciBusReg.pfnFakePCIBIOSR3        = pciR3FakePCIBIOS;
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "pciSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "pciSetIrq" : NULL;
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

    /* Disable default device locking. */
    rc = PDMDevHlpSetDeviceCritSect(pDevIns, PDMDevHlpCritSectGetNop(pDevIns));
    AssertRCReturn(rc, rc);

    /*
     * Fill in PCI configs and add them to the bus.
     */
    /* i440FX */
    PCIDevSetVendorId(  &pBus->PciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->PciDev, 0x1237);
    PCIDevSetRevisionId(&pBus->PciDev,   0x02);
    PCIDevSetClassSub(  &pBus->PciDev,   0x00); /* host2pci */
    PCIDevSetClassBase( &pBus->PciDev,   0x06); /* PCI_bridge */
    PCIDevSetHeaderType(&pBus->PciDev,   0x00);

    pBus->PciDev.pDevIns              = pDevIns;
    pciDevSetRequestedDevfunc(&pBus->PciDev);
    pciR3RegisterDeviceInternal(pBus, 0, &pBus->PciDev, "i440FX");

    /* PIIX3 */
    PCIDevSetVendorId(  &pGlobals->PIIX3State.dev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pGlobals->PIIX3State.dev, 0x7000); /* 82371SB PIIX3 PCI-to-ISA bridge (Step A1) */
    PCIDevSetClassSub(  &pGlobals->PIIX3State.dev,   0x01); /* PCI_ISA */
    PCIDevSetClassBase( &pGlobals->PIIX3State.dev,   0x06); /* PCI_bridge */
    PCIDevSetHeaderType(&pGlobals->PIIX3State.dev,   0x80); /* PCI_multifunction, generic */

    pGlobals->PIIX3State.dev.pDevIns      = pDevIns;
    pciDevSetRequestedDevfunc(&pGlobals->PIIX3State.dev);
    pciR3RegisterDeviceInternal(pBus, 8, &pGlobals->PIIX3State.dev, "PIIX3");
    pciR3Piix3Reset(&pGlobals->PIIX3State);

    pBus->iDevSearch = 16;

    /*
     * Register I/O ports and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cf8, 1, NULL, pciIOPortAddressWrite, pciIOPortAddressRead, NULL, NULL, "i440FX (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0x0cfc, 4, NULL, pciIOPortDataWrite, pciIOPortDataRead, NULL, NULL, "i440FX (PCI)");
    if (RT_FAILURE(rc))
        return rc;
    if (fGCEnabled)
    {
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cf8, 1, NIL_RTGCPTR, "pciIOPortAddressWrite", "pciIOPortAddressRead", NULL, NULL, "i440FX (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterRC(pDevIns, 0x0cfc, 4, NIL_RTGCPTR, "pciIOPortDataWrite", "pciIOPortDataRead", NULL, NULL, "i440FX (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }
    if (fR0Enabled)
    {
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cf8, 1, NIL_RTR0PTR, "pciIOPortAddressWrite", "pciIOPortAddressRead", NULL, NULL, "i440FX (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterR0(pDevIns, 0x0cfc, 4, NIL_RTR0PTR, "pciIOPortDataWrite", "pciIOPortDataRead", NULL, NULL, "i440FX (PCI)");
        if (RT_FAILURE(rc))
            return rc;
    }

    rc = PDMDevHlpSSMRegisterEx(pDevIns, VBOX_PCI_SAVED_STATE_VERSION, sizeof(*pBus) + 16*128, "pgm",
                                NULL, NULL, NULL,
                                NULL, pciR3SaveExec, NULL,
                                NULL, pciR3LoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    PDMDevHlpDBGFInfoRegister(pDevIns, "pci",
                              "Display PCI bus status. Recognizes 'basic' or 'verbose' as arguments, defaults to 'basic'.",
                              pciR3Info);
    PDMDevHlpDBGFInfoRegister(pDevIns, "pciirq", "Display PCI IRQ routing state. (no arguments)", pciR3IrqInfo);

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePCI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "pci",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "i440FX PCI bridge and PIIX3 ISA bridge.",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI | PDM_DEVREG_CLASS_BUS_ISA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIGLOBALS),
    /* pfnConstruct */
    pciR3Construct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pciR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pciR3Reset,
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



/* -=-=-=-=-=- The PCI bridge specific bits -=-=-=-=-=- */

/**
 * @interface_method_impl{PDMPCIBUSREG,pfnSetIrqR3}
 */
PDMBOTHCBDECL(void) pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel, uint32_t uTagSrc)
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
        uDevFnBridge  = pBus->PciDev.devfn;
        iIrqPinBridge = ((pPciDevBus->devfn >> 3) + iIrqPinBridge) & 3;

        /* Get the parent. */
        pBus = pBus->PciDev.Int.s.CTX_SUFF(pBus);
        pPciDevBus = &pBus->PciDev;
    } while (pBus->iBus != 0);

    AssertMsg(pBus->iBus == 0, ("This is not the host pci bus iBus=%d\n", pBus->iBus));
    pciSetIrqInternal(PCIBUS_2_PCIGLOBALS(pBus), uDevFnBridge, pPciDev, iIrqPinBridge, iLevel, uTagSrc);
}

#ifdef IN_RING3

/**
 * @callback_method_impl{FNPCIBRIDGECONFIGWRITE}
 */
static DECLCALLBACK(void) pcibridgeR3ConfigWrite(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, uint32_t u32Value, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    LogFlowFunc(("pDevIns=%p iBus=%d iDevice=%d u32Address=%u u32Value=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, u32Value, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS])
    {
        PPCIDEVICE pBridgeDevice = pciR3FindBridge(pBus, iBus);
        if (pBridgeDevice)
        {
            AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
            pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, iBus, iDevice, u32Address, u32Value, cb);
        }
    }
    else
    {
        /* This is the target bus, pass the write to the device. */
        PPCIDEVICE pPciDev = pBus->devices[iDevice];
        if (pPciDev)
        {
            Log(("%s: %s: addr=%02x val=%08x len=%d\n", __FUNCTION__, pPciDev->name, u32Address, u32Value, cb));
            pPciDev->Int.s.pfnConfigWrite(pPciDev, u32Address, u32Value, cb);
        }
    }
}


/**
 * @callback_method_impl{FNPCIBRIDGECONFIGREAD}
 */
static DECLCALLBACK(uint32_t) pcibridgeR3ConfigRead(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    uint32_t u32Value = 0xffffffff; /* Return value in case there is no device. */

    LogFlowFunc(("pDevIns=%p iBus=%d iDevice=%d u32Address=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS])
    {
        PPCIDEVICE pBridgeDevice = pciR3FindBridge(pBus, iBus);
        if (pBridgeDevice)
        {
            AssertPtr( pBridgeDevice->Int.s.pfnBridgeConfigRead);
            u32Value = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, iBus, iDevice, u32Address, cb);
        }
    }
    else
    {
        /* This is the target bus, pass the read to the device. */
        PPCIDEVICE pPciDev = pBus->devices[iDevice];
        if (pPciDev)
        {
            u32Value = pPciDev->Int.s.pfnConfigRead(pPciDev, u32Address, cb);
            Log(("%s: %s: u32Address=%02x u32Value=%08x cb=%d\n", __FUNCTION__, pPciDev->name, u32Address, u32Value, cb));
        }
    }

    return u32Value;
}


/**
 * @callback_method_impl{FNSSMDEVSAVEEXEC}
 */
static DECLCALLBACK(int) pcibridgeR3SaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    return pciR3CommonSaveExec(pThis, pSSM);
}


/**
 * @callback_method_impl{FNSSMDEVLOADEXEC}
 */
static DECLCALLBACK(int) pcibridgeR3LoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);
    if (uVersion > VBOX_PCI_SAVED_STATE_VERSION)
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    return pciR3CommonLoadExec(pThis, pSSM, uVersion, uPass);
}


/**
 * @interface_method_impl{PDMPCIBUSREG,pfnRegisterR3}
 */
static DECLCALLBACK(int) pcibridgeR3RegisterDevice(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)RT_ELEMENTS(pBus->devices))
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the device.
     */
    return pciR3RegisterDeviceInternal(pBus, iDev, pPciDev, pszName);
}


/**
 * @interface_method_impl{PDMDEVREG,pfnReset}
 */
static DECLCALLBACK(void) pcibridgeR3Reset(PPDMDEVINS pDevIns)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /* Reset config space to default values. */
    pBus->PciDev.config[VBOX_PCI_PRIMARY_BUS] = 0;
    pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS] = 0;
    pBus->PciDev.config[VBOX_PCI_SUBORDINATE_BUS] = 0;
}


/**
 * @interface_method_impl{PDMDEVREG,pfnRelocate}
 */
static DECLCALLBACK(void) pcibridgeR3Relocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);

    /* Relocate RC pointers for the attached pci devices. */
    for (uint32_t i = 0; i < RT_ELEMENTS(pBus->devices); i++)
    {
        if (pBus->devices[i])
            pBus->devices[i]->Int.s.pBusRC += offDelta;
    }
}


/**
 * @interface_method_impl{PDMDEVREG,pfnConstruct}
 */
static DECLCALLBACK(int)   pcibridgeR3Construct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfg)
{
    PDMDEV_CHECK_VERSIONS_RETURN(pDevIns);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfg, "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* check if RC code is enabled. */
    bool fGCEnabled;
    int rc = CFGMR3QueryBoolDef(pCfg, "GCEnabled", &fGCEnabled, true);
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

    /*
     * Init data and register the PCI bus.
     */
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    pBus->pDevInsR3 = pDevIns;
    pBus->pDevInsR0 = PDMDEVINS_2_R0PTR(pDevIns);
    pBus->pDevInsRC = PDMDEVINS_2_RCPTR(pDevIns);
    pBus->papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pBus->devices));

    PDMPCIBUSREG PciBusReg;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = pcibridgeR3RegisterDevice;
    PciBusReg.pfnRegisterMsiR3        = NULL;
    PciBusReg.pfnIORegionRegisterR3   = pciR3CommonIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = pciR3CommonSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = pcibridgeSetIrq;
    PciBusReg.pfnFakePCIBIOSR3        = NULL; /* Only needed for the first bus. */
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "pcibridgeSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "pcibridgeSetIrq" : NULL;
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
    PCIDevSetVendorId(  &pBus->PciDev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pBus->PciDev, 0x2448); /* 82801 Mobile PCI bridge. */
    PCIDevSetRevisionId(&pBus->PciDev,   0xf2);
    PCIDevSetClassSub(  &pBus->PciDev,   0x04); /* pci2pci */
    PCIDevSetClassBase( &pBus->PciDev,   0x06); /* PCI_bridge */
    PCIDevSetClassProg( &pBus->PciDev,   0x01); /* Supports subtractive decoding. */
    PCIDevSetHeaderType(&pBus->PciDev,   0x01); /* Single function device which adheres to the PCI-to-PCI bridge spec. */
    PCIDevSetCommand(   &pBus->PciDev,   0x00);
    PCIDevSetStatus(    &pBus->PciDev,   0x20); /* 66MHz Capable. */
    PCIDevSetInterruptLine(&pBus->PciDev, 0x00); /* This device does not assert interrupts. */

    /*
     * This device does not generate interrupts. Interrupt delivery from
     * devices attached to the bus is unaffected.
     */
    PCIDevSetInterruptPin(&pBus->PciDev, 0x00);

    pBus->PciDev.pDevIns                    = pDevIns;

    /* Bridge-specific data */
    pciDevSetPci2PciBridge(&pBus->PciDev);
    pBus->PciDev.Int.s.pfnBridgeConfigRead  = pcibridgeR3ConfigRead;
    pBus->PciDev.Int.s.pfnBridgeConfigWrite = pcibridgeR3ConfigWrite;

    /*
     * Register this PCI bridge. The called function will take care on which bus we will get registered.
     */
    rc = PDMDevHlpPCIRegister(pDevIns, &pBus->PciDev);
    if (RT_FAILURE(rc))
        return rc;

    pBus->iDevSearch = 0;
    /*
     * The iBus property doesn't really represent the bus number
     * because the guest and the BIOS can choose different bus numbers
     * for them.
     * The bus number is mainly for the setIrq function to indicate
     * when the host bus is reached which will have iBus = 0.
     * That's why the + 1.
     */
    pBus->iBus = iInstance + 1;

    /*
     * Register SSM handlers. We use the same saved state version as for the host bridge
     * to make changes easier.
     */
    rc = PDMDevHlpSSMRegisterEx(pDevIns, VBOX_PCI_SAVED_STATE_VERSION, sizeof(*pBus) + 16*128, "pgm",
                                NULL, NULL, NULL,
                                NULL, pcibridgeR3SaveExec, NULL,
                                NULL, pcibridgeR3LoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The device registration structure
 * for the PCI-to-PCI bridge.
 */
const PDMDEVREG g_DevicePCIBridge =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szName */
    "pcibridge",
    /* szRCMod */
    "VBoxDDRC.rc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "82801 Mobile PCI to PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI,
    /* cMaxInstances */
    ~0U,
    /* cbInstance */
    sizeof(PCIBUS),
    /* pfnConstruct */
    pcibridgeR3Construct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pcibridgeR3Relocate,
    /* pfnMemSetup */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pcibridgeR3Reset,
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
