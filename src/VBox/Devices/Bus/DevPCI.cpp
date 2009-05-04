/* $Id$ */
/** @file
 * DevPCI - PCI BUS Device.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEV_PCI
/* Hack to get PCIDEVICEINT declare at the right point - include "PCIInternal.h". */
#define PCI_INCLUDE_PRIVATE
#include <VBox/pci.h>
#include <VBox/pdmdev.h>
#include <iprt/assert.h>
#include <iprt/string.h>

#include "../Builtins.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/

/** Converts a bus instance pointer to a device instance pointer. */
#define PCIBUS_2_DEVINS(pPciBus)        ((pPciBus)->CTX_SUFF(pDevIns))
/** Converts a device instance pointer to a PCIGLOBALS pointer. */
#define DEVINS_2_PCIGLOBALS(pDevIns)    ((PPCIGLOBALS)(PDMINS_2_DATA(pDevIns, PPCIGLOBALS)))
/** Converts a device instance pointer to a PCIBUS pointer. */
#define DEVINS_2_PCIBUS(pDevIns)        ((PPCIBUS)(&PDMINS_2_DATA(pDevIns, PPCIGLOBALS)->PciBus))

/** Converts a pointer to a PCI bus instance to a PCIGLOBALS pointer.
 *  @note This works only if the bus number is 0!!!
 */
#define PCIBUS_2_PCIGLOBALS(pPciBus)    ( (PPCIGLOBALS)((uintptr_t)(pPciBus) - RT_OFFSETOF(PCIGLOBALS, PciBus)) )

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
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(void) pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);
PDMBOTHCBDECL(int)  pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb);
PDMBOTHCBDECL(int)  pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb);

#ifdef IN_RING3
DECLINLINE(PPCIDEVICE) pciFindBridge(PPCIBUS pBus, uint8_t iBus);
#endif

__END_DECLS

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
                    if (last_addr <= new_addr || new_addr == 0 ||
                        last_addr == ~0U) {
                        new_addr = ~0U;
                    }
                } else {
                no_mem_map:
                    new_addr = ~0U;
                }
            }
            /* now do the real mapping */
            if (new_addr != r->addr) {
                if (r->addr != ~0U) {
                    if (r->type & PCI_ADDRESS_SPACE_IO) {
                        int devclass;
                        /* NOTE: specific hack for IDE in PC case:
                           only one byte must be mapped. */
                        devclass = d->config[0x0a] | (d->config[0x0b] << 8);
                        if (devclass == 0x0101 && r->size == 4) {
                            int rc = d->pDevIns->pDevHlpR3->pfnIOPortDeregister(d->pDevIns, r->addr + 2, 1);
                            AssertRC(rc);
                        } else {
                            int rc = d->pDevIns->pDevHlpR3->pfnIOPortDeregister(d->pDevIns, r->addr, r->size);
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
                            rc = d->pDevIns->pDevHlpR3->pfnMMIODeregister(d->pDevIns, GCPhysBase, r->size);
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
            case 0x3d:
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
        if (addr == 0x06)
        {
            /* don't change read-only bits => actually all lower bits are read-only */
            val &= UINT32_C(~0xff);
            /* status register, low part: clear bits by writing a '1' to the corresponding bit */
            d->config[addr] &= ~val;
        }
        else if (addr == 0x07)
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
            PPCIDEVICE pBridgeDevice = pciFindBridge(&pGlobals->PciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigWrite);
                pBridgeDevice->Int.s.pfnBridgeConfigWrite(pBridgeDevice->pDevIns, iBus, iDevice, config_addr, val, len);
            }
#else
            return VINF_IOM_HC_IOPORT_WRITE;
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
            return VINF_IOM_HC_IOPORT_WRITE;
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
            PPCIDEVICE pBridgeDevice = pciFindBridge(&pGlobals->PciBus, iBus);
            if (pBridgeDevice)
            {
                AssertPtr(pBridgeDevice->Int.s.pfnBridgeConfigRead);
                *pu32 = pBridgeDevice->Int.s.pfnBridgeConfigRead(pBridgeDevice->pDevIns, iBus, iDevice, config_addr, len);
            }
#else
            return VINF_IOM_HC_IOPORT_READ;
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
            return VINF_IOM_HC_IOPORT_READ;
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

static void apic_set_irq(PPCIBUS pBus, uint8_t uDevFn, PCIDevice *pPciDev, int irq_num1, int iLevel, int acpi_irq)
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
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);

        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
            ASMAtomicDecU32(&pGlobals->pci_apic_irq_levels[irq_num]);
            pPciDev->Int.s.uIrqPinState = PDM_IRQ_LEVEL_LOW;
            apic_level = get_pci_irq_apic_level(pGlobals, irq_num);
            Log3(("apic_set_irq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d (flop)\n",
                  R3STRING(pPciDev->name), irq_num1, iLevel, apic_irq, apic_level, irq_num));
            pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), apic_irq, apic_level);
        }
    } else {
        Log3(("apic_set_irq: %s: irq_num1=%d level=%d acpi_irq=%d\n",
              R3STRING(pPciDev->name), irq_num1, iLevel, acpi_irq));
        pBus->CTX_SUFF(pPciHlp)->pfnIoApicSetIrq(pBus->CTX_SUFF(pDevIns), acpi_irq, iLevel);
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
 * @remark  uDevFn and pPciDev->devfn are not the same if the device is behind a bridge.
 *          In that case uDevFn will be the slot of the bridge which is needed to calculate the
 *          PIRQ value.
 */
static void pciSetIrqInternal(PPCIGLOBALS pGlobals, uint8_t uDevFn, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    PPCIBUS     pBus =     &pGlobals->PciBus;
    uint8_t    *pbCfg = pGlobals->PIIX3State.dev.config;
    const bool  fIsAcpiDevice = pPciDev->config[2] == 0x13 && pPciDev->config[3] == 0x71;
    const bool  fIsApicEnabled = pGlobals->fUseIoApic && pbCfg[0xde] == 0xbe && pbCfg[0xad] == 0xef;
    int pic_irq, pic_level;

    /* Check if the state changed. */
    if (pPciDev->Int.s.uIrqPinState != iLevel)
    {
        pPciDev->Int.s.uIrqPinState = (iLevel & PDM_IRQ_LEVEL_HIGH);

        /* apic only */
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
                apic_set_irq(pBus, uDevFn, pPciDev, -1, iLevel, pPciDev->config[PCI_INTERRUPT_LINE]);
            else
                apic_set_irq(pBus, uDevFn, pPciDev, iIrq, iLevel, -1);
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

        Log3(("pciSetIrq: %s: iLevel=%d iIrq=%d pic_irq=%d pic_level=%d\n",
              R3STRING(pPciDev->name), iLevel, iIrq, pic_irq, pic_level));
        pBus->CTX_SUFF(pPciHlp)->pfnIsaSetIrq(pBus->CTX_SUFF(pDevIns), pic_irq, pic_level);

        /** @todo optimize pci irq flip-flop some rainy day. */
        if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
            pciSetIrqInternal(pGlobals, uDevFn, pPciDev, iIrq, PDM_IRQ_LEVEL_LOW);
    }
}

/**
 * Set the IRQ for a PCI device on the host bus.
 *
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         The PCI device structure.
 * @param   iIrq            IRQ number to set.
 * @param   iLevel          IRQ level.
 */
PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    pciSetIrqInternal(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), pPciDev->devfn, pPciDev, iIrq, iLevel);
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
DECLINLINE(PPCIDEVICE) pciFindBridge(PPCIBUS pBus, uint8_t iBus)
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

static void piix3_reset(PIIX3State *d)
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
    uint16_t cmd;
    uint32_t ofs;

    if ( region_num == PCI_ROM_SLOT )
        ofs = 0x30;
    else
        ofs = 0x10 + region_num * 4;

    /* Read memory type first. */
    uint8_t uRessourceType = pci_config_readb(pGlobals, uBus, uDevFn, ofs);

    /* Read command register. */
    cmd = pci_config_readw(pGlobals, uBus, uDevFn, PCI_COMMAND);
    if ( region_num == PCI_ROM_SLOT )
        cmd |= 2;
    else if ((uRessourceType & 0x01) == 1) /* Test if region is I/O space. */
        cmd |= 1; /* Enable I/O space access. */
    else /* The region is MMIO. */
        cmd |= 2; /* Enable MMIO access. */

    /* Write address of the device. */
    pci_config_writel(pGlobals, uBus, uDevFn, ofs, addr);

    /* enable memory mappings */
    pci_config_writew(pGlobals, uBus, uDevFn, PCI_COMMAND, cmd);
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
                }
                break;
            case 0x0300:
                if (vendor_id != 0x80ee)
                    goto default_map;
                /* VGA: map frame buffer to default Bochs VBE address */
                pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0xE0000000);
#ifdef VBOX_WITH_EFI
                /* The following is necessary for the VGA check in
                   PciVgaMiniPortDriverBindingSupported to succeed. */
                /** @todo Seems we're missing some I/O registers or something on the VGA device... Compare real/virt hw with lspci. */
                pci_config_writew(pGlobals, uBus, uDevFn, PCI_COMMAND,
                                  pci_config_readw(pGlobals, uBus, uDevFn, PCI_COMMAND)
                                  | 1 /* Enable I/O space access. */);
#endif
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
                    }
                }
                break;
            case 0xff00:
                if (   (vendor_id == 0x0106b)
                    && (device_id == 0x0017 || device_id == 0x0022))
                {
                    /* macio bridge */
                    pci_set_io_region_addr(pGlobals, uBus, uDevFn, 0, 0x80800000);
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
                for (int i = 0; i <= 255; i++)
                    pci_bios_init_device(pGlobals, uBus + 1, i, cBridgeDepth + 1, paBridgePositions);

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
                 * which may be behind a bridge. Thatswhy it is unconditionally disabled here atm by writing a higher value into
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
                    /* Clear ressource information depending on ressource type. */
                    if ((u8RessourceType & 0x01) == 1) /* I/O */
                        u32Size &= ~(0x01);
                    else                        /* MMIO */
                        u32Size &= ~(0x0f);

                    /*
                     * Invert all bits and add 1 to get size of the region.
                     * (From PCI implementation note)
                     */
                    if (((u8RessourceType & 0x01) == 1) && (u32Size & UINT32_C(0xffff0000)) == 0)
                        u32Size = (~(u32Size | UINT32_C(0xffff0000))) + 1;
                    else
                        u32Size = (~u32Size) + 1;

                    Log(("%s: Size of region %u for device %d on bus %d is %u\n", __FUNCTION__, i, uDevFn, uBus, u32Size));

                    if (u32Size)
                    {
                        if ((u8RessourceType & 0x01) == 1)
                            paddr = &pGlobals->pci_bios_io_addr;
                        else
                            paddr = &pGlobals->pci_bios_mem_addr;
                        *paddr = (*paddr + u32Size - 1) & ~(u32Size - 1);
                        Log(("%s: Start address of %s region %u is %#x\n", __FUNCTION__, ((u8RessourceType & 0x01) == 1 ? "I/O" : "MMIO"), i, *paddr));
                        pci_set_io_region_addr(pGlobals, uBus, uDevFn, i, *paddr);
                        *paddr += u32Size;
                        Log(("%s: New address is %#x\n", __FUNCTION__, *paddr));
                    }
                }
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

/* -=-=-=-=-=- wrappers -=-=-=-=-=- */

/**
 * Port I/O Handler for PCI address OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortAddressWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (cb == 4)
    {
        PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        pThis->uConfigReg = u32;
        PCI_UNLOCK(pDevIns);
    }
    /* else: 440FX does "pass through to the bus" for other writes, what ever that means.
     * Linux probes for cmd640 using byte writes/reads during ide init. We'll just ignore it. */
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
PDMBOTHCBDECL(int) pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
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
    /* else: 440FX does "pass through to the bus" for other writes, what ever that means.
     * Linux probes for cmd640 using byte writes/reads during ide init. We'll just ignore it. */
    Log(("pciIOPortAddressRead: Port=%#x cb=%d VERR_IOM_IOPORT_UNUSED\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}


/**
 * Port I/O Handler for PCI data OUT operations.
 *
 * @returns VBox status code.
 *
 * @param   pDevIns     The device instance.
 * @param   pvUser      User argument - ignored.
 * @param   uPort       Port number used for the IN operation.
 * @param   u32         The value to output.
 * @param   cb          The value size in bytes.
 */
PDMBOTHCBDECL(int) pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortDataWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    int rc = VINF_SUCCESS;
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        rc = pci_data_write(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, u32, cb);
        PCI_UNLOCK(pDevIns);
    }
    else
        AssertMsgFailed(("Write to port %#x u32=%#x cb=%d\n", Port, u32, cb));
    return rc;
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
PDMBOTHCBDECL(int) pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        int rc = pci_data_read(PDMINS_2_DATA(pDevIns, PPCIGLOBALS), Port, cb, pu32);
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortDataRead: Port=%#x cb=%#x -> %#x (%Rrc)\n", Port, cb, *pu32, rc));
        return rc;
    }
    AssertMsgFailed(("Read from port %#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}

#ifdef IN_RING3

/**
 * Saves a state of the PCI device.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSMHandle      The handle to save the state to.
 */
static DECLCALLBACK(int) pciGenericSaveExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle)
{
    return SSMR3PutMem(pSSMHandle, &pPciDev->config[0], sizeof(pPciDev->config));
}


/**
 * Loads a saved PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         Pointer to PCI device.
 * @param   pSSMHandle      The handle to the saved state.
 */
static DECLCALLBACK(int) pciGenericLoadExec(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PSSMHANDLE pSSMHandle)
{
    return SSMR3GetMem(pSSMHandle, &pPciDev->config[0], sizeof(pPciDev->config));
}


/**
 * Saves a state of the PCI device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pPciDev     Pointer to PCI device.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) pciSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    uint32_t    i;
    PPCIGLOBALS pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS     pBus =  &pThis->PciBus;

    /*
     * Bus state data.
     */
    SSMR3PutU32(pSSMHandle, pThis->uConfigReg);
    SSMR3PutBool(pSSMHandle, pThis->fUseIoApic);
    /*
     * Save IRQ states.
     */
    for (i = 0; i < PCI_IRQ_PINS; i++)
        SSMR3PutU32(pSSMHandle, pThis->pci_irq_levels[i]);
    for (i = 0; i < PCI_APIC_IRQ_PINS; i++)
        SSMR3PutU32(pSSMHandle, pThis->pci_apic_irq_levels[i]);

    SSMR3PutU32(pSSMHandle, pThis->acpi_irq_level);
    SSMR3PutS32(pSSMHandle, pThis->acpi_irq);

    SSMR3PutU32(pSSMHandle, ~0);        /* separator */

    /*
     * Iterate all the devices.
     */
    for (i = 0; i < RT_ELEMENTS(pBus->devices); i++)
    {
        PPCIDEVICE pDev = pBus->devices[i];
        if (pDev)
        {
            int rc;
            SSMR3PutU32(pSSMHandle, i);
            SSMR3PutMem(pSSMHandle, pDev->config, sizeof(pDev->config));

            rc = SSMR3PutS32(pSSMHandle, pDev->Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    return SSMR3PutU32(pSSMHandle, ~0); /* terminator */
}

/**
 * Loads a saved PCI device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) pciLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    PPCIGLOBALS  pThis = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PPCIBUS      pBus  = &pThis->PciBus;
    uint32_t    u32;
    uint32_t    i;
    int         rc;

    /*
     * Check the version.
     */
    if (u32Version > VBOX_PCI_SAVED_STATE_VERSION)
    {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Bus state data.
     */
    SSMR3GetU32(pSSMHandle, &pThis->uConfigReg);
    if (u32Version > 1)
        SSMR3GetBool(pSSMHandle, &pThis->fUseIoApic);

    /* Load IRQ states. */
    if (u32Version > 2)
    {
        for (uint8_t i = 0; i < PCI_IRQ_PINS; i++)
            SSMR3GetU32(pSSMHandle, (uint32_t *)&pThis->pci_irq_levels[i]);
        for (uint8_t i = 0; i < PCI_APIC_IRQ_PINS; i++)
            SSMR3GetU32(pSSMHandle, (uint32_t *)&pThis->pci_apic_irq_levels[i]);

        SSMR3GetU32(pSSMHandle, &pThis->acpi_irq_level);
        SSMR3GetS32(pSSMHandle, &pThis->acpi_irq);
    }

    /* separator */
    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (RT_FAILURE(rc))
        return rc;
    if (u32 != (uint32_t)~0)
        AssertMsgFailedReturn(("u32=%#x\n", u32), rc);

    /*
     * Iterate all the devices.
     */
    for (i = 0;; i++)
    {
        PCIDEVICE   DevTmp;
        PPCIDEVICE  pDev;

        /* index / terminator */
        rc = SSMR3GetU32(pSSMHandle, &u32);
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
                LogRel(("New device in slot %#x, %s (vendor=%#06x device=%#06x)\n", i, pBus->devices[i]->name,
                        PCIDevGetVendorId(pBus->devices[i]), PCIDevGetDeviceId(pBus->devices[i])));
                if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                    AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            }
        }

        /* Get the data */
        DevTmp.Int.s.uIrqPinState = ~0; /* Invalid value in case we have an older saved state to force a state change in pciSetIrq. */
        SSMR3GetMem(pSSMHandle, DevTmp.config, sizeof(DevTmp.config));
        if (u32Version < 3)
        {
            int32_t i32Temp;
            /* Irq value not needed anymore. */
            rc = SSMR3GetS32(pSSMHandle, &i32Temp);
            if (RT_FAILURE(rc))
                return rc;
        }
        else
        {
            rc = SSMR3GetS32(pSSMHandle, &DevTmp.Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;
        }

        /* check that it's still around. */
        pDev = pBus->devices[i];
        if (!pDev)
        {
            LogRel(("Device in slot %#x has been removed! vendor=%#06x device=%#06x\n", i,
                    PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp)));
            if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            continue;
        }

        /* match the vendor id assuming that this will never be changed. */
        if (    DevTmp.config[0] != pDev->config[0]
            ||  DevTmp.config[1] != pDev->config[1])
        {
            LogRel(("Device in slot %#x (%s) vendor id mismatch! saved=%.4Rhxs current=%.4Rhxs\n",
                    i, pDev->name, DevTmp.config, pDev->config));
            AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
        }

        /* commit the loaded device config. */
        memcpy(pDev->config, DevTmp.config, sizeof(pDev->config));

        pDev->Int.s.uIrqPinState = DevTmp.Int.s.uIrqPinState;
    }

    return VINF_SUCCESS;
}


/* -=-=-=-=-=- real code -=-=-=-=-=- */

/**
 * Registers the device with the specified PCI bus.
 *
 * @returns VBox status code.
 * @param   pBus            The bus to register with.
 * @param   iDev            The PCI device ordinal.
 * @param   pPciDev         The PCI device structure.
 * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
 */
static int pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
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
#ifdef VBOX_WITH_LPC
        /* LPC bus expected to be there by some guests, better make an additional argument to PDM
           device helpers, but requires significant rewrite */
        else if (!strcmp(pszName, "lpc")
             &&  !pBus->devices[0xf8])
            iDev = 0xf8;
#endif
        else
        {
            Assert(!(pBus->iDevSearch % 8));
            for (iDev = pBus->iDevSearch; iDev < (int)RT_ELEMENTS(pBus->devices); iDev += 8)
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
        pPciDev->Int.s.fRequestedDevFn = false;
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
            if (    pBus->devices[iDev]->Int.s.fRequestedDevFn
                ||  (pBus->devices[iDev + 1] && pBus->devices[iDev + 1]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 2] && pBus->devices[iDev + 2]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 3] && pBus->devices[iDev + 3]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 4] && pBus->devices[iDev + 4]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 5] && pBus->devices[iDev + 5]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 6] && pBus->devices[iDev + 6]->Int.s.fRequestedDevFn)
                ||  (pBus->devices[iDev + 7] && pBus->devices[iDev + 7]->Int.s.fRequestedDevFn))
            {
                AssertReleaseMsgFailed(("Configuration error:'%s' and '%s' are both configured as device %d\n",
                                        pszName, pBus->devices[iDev]->name, iDev));
                return VERR_INTERNAL_ERROR;
            }

            /* Find free slot for the device(s) we're moving and move them. */
            for (iDevRel = pBus->iDevSearch; iDevRel < (int)RT_ELEMENTS(pBus->devices); iDevRel += 8)
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
                        pBus->devices[iDevRel + i]->devfn = i;
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
        pPciDev->Int.s.fRequestedDevFn = true;
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
    if (pPciDev->Int.s.fPciToPciBridge)
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
 * Registers the device with the default PCI bus.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         The PCI device structure.
 *                          Any PCI enabled device must keep this in it's instance data!
 *                          Fill in the PCI data config before registration, please.
 * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
 * @param   iDev            The PCI device number. Use a negative value for auto assigning one.
 */
static DECLCALLBACK(int) pciRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
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
    return pciRegisterInternal(pBus, iDev, pPciDev, pszName);
}


static DECLCALLBACK(int) pciIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
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
    uint32_t u32Address = 0x10 + iRegion * 4;
    uint32_t u32Value   =   (enmType == PCI_ADDRESS_SPACE_MEM_PREFETCH ? (1 << 3) : 0)
                          | (enmType == PCI_ADDRESS_SPACE_IO ? 1 : 0);
    *(uint32_t *)(pPciDev->config + u32Address) = RT_H2LE_U32(u32Value);

    return VINF_SUCCESS;
}


/**
 * @copydoc PDMPCIBUSREG::pfnSetConfigCallbacksR3
 */
static DECLCALLBACK(void) pciSetConfigCallbacks(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, PFNPCICONFIGREAD pfnRead, PPFNPCICONFIGREAD ppfnReadOld,
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
 * Called to perform the job of the bios.
 *
 * @returns VBox status.
 * @param   pDevIns     Device instance of the first bus.
 */
static DECLCALLBACK(int) pciFakePCIBIOS(PPDMDEVINS pDevIns)
{
    int         rc;
    unsigned    i;
    uint8_t     elcr[2] = {0, 0};
    PPCIGLOBALS pGlobals = PDMINS_2_DATA(pDevIns, PPCIGLOBALS);
    PVM         pVM = PDMDevHlpGetVM(pDevIns);
    Assert(pVM);

    /*
     * Set the start addresses.
     */
    pGlobals->pci_bios_io_addr  = 0xc000;
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
    rc = IOMIOPortWrite(pVM, 0x4d0, elcr[0], sizeof(uint8_t));
    if (rc == VINF_SUCCESS)
        rc = IOMIOPortWrite(pVM, 0x4d1, elcr[1], sizeof(uint8_t));
    if (rc != VINF_SUCCESS)
    {
        AssertMsgFailed(("Writing to PIC failed!\n"));
        return RT_SUCCESS(rc) ? VERR_INTERNAL_ERROR : rc;
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

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) pciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
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
 * Construct a host to PCI Bus device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int)   pciConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int rc;
    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IOAPIC\0" "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* query whether we got an IOAPIC */
    bool fUseIoApic;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "IOAPIC", &fUseIoApic, false);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"IOAPIC\""));

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &fR0Enabled, true);
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
    pGlobals->PciBus.papBridgesR3 = (PPCIDEVICE *)PDMDevHlpMMHeapAllocZ(pDevIns, sizeof(PPCIDEVICE) * RT_ELEMENTS(pGlobals->PciBus.devices));

    PDMPCIBUSREG PciBusReg;
    PPCIBUS      pBus = &pGlobals->PciBus;
    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterR3           = pciRegister;
    PciBusReg.pfnIORegionRegisterR3   = pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = pciSetIrq;
    PciBusReg.pfnSaveExecR3           = pciGenericSaveExec;
    PciBusReg.pfnLoadExecR3           = pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSR3        = pciFakePCIBIOS;
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "pciSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "pciSetIrq" : NULL;
    rc = pDevIns->pDevHlpR3->pfnPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Failed to register ourselves as a PCI Bus"));
    if (pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                  pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION);

    pBus->pPciHlpRC = pBus->pPciHlpR3->pfnGetRCHelpers(pDevIns);
    pBus->pPciHlpR0 = pBus->pPciHlpR3->pfnGetR0Helpers(pDevIns);

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
    pBus->PciDev.Int.s.fRequestedDevFn= true;
    pciRegisterInternal(pBus, 0, &pBus->PciDev, "i440FX");

    /* PIIX3 */
    PCIDevSetVendorId(  &pGlobals->PIIX3State.dev, 0x8086); /* Intel */
    PCIDevSetDeviceId(  &pGlobals->PIIX3State.dev, 0x7000); /* 82371SB PIIX3 PCI-to-ISA bridge (Step A1) */
    PCIDevSetClassSub(  &pGlobals->PIIX3State.dev,   0x01); /* PCI_ISA */
    PCIDevSetClassBase( &pGlobals->PIIX3State.dev,   0x06); /* PCI_bridge */
    PCIDevSetHeaderType(&pGlobals->PIIX3State.dev,   0x80); /* PCI_multifunction, generic */

    pGlobals->PIIX3State.dev.pDevIns      = pDevIns;
    pGlobals->PIIX3State.dev.Int.s.fRequestedDevFn= true;
    pciRegisterInternal(pBus, 8, &pGlobals->PIIX3State.dev, "PIIX3");
    piix3_reset(&pGlobals->PIIX3State);

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
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x0cf8, 1, NIL_RTGCPTR, "pciIOPortAddressWrite", "pciIOPortAddressRead", NULL, NULL, "i440FX (PCI)");
        if (RT_FAILURE(rc))
            return rc;
        rc = PDMDevHlpIOPortRegisterGC(pDevIns, 0x0cfc, 4, NIL_RTGCPTR, "pciIOPortDataWrite", "pciIOPortDataRead", NULL, NULL, "i440FX (PCI)");
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

    rc = PDMDevHlpSSMRegister(pDevIns, "pci", iInstance, VBOX_PCI_SAVED_STATE_VERSION, sizeof(*pBus),
                              NULL, pciSaveExec, NULL, NULL, pciLoadExec, NULL);
    if (RT_FAILURE(rc))
        return rc;

    return VINF_SUCCESS;
}


/**
 * The device registration structure.
 */
const PDMDEVREG g_DevicePCI =
{
    /* u32Version */
    PDM_DEVREG_VERSION,
    /* szDeviceName */
    "pci",
    /* szRCMod */
    "VBoxDDGC.gc",
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
    pciConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pciRelocate,
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
#endif /* IN_RING3 */

/**
 * Set the IRQ for a PCI device on a secondary bus.
 *
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         The PCI device structure.
 * @param   iIrq            IRQ number to set.
 * @param   iLevel          IRQ level.
 */
PDMBOTHCBDECL(void) pcibridgeSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    /*
     * The PCI-to-PCI bridge specification defines how the interrupt pins
     * are routed from the secondary to the primary bus (see chapter 9).
     * iIrq gives the interrupt pin the pci device asserted.
     * We change iIrq here according to the spec and call the SetIrq function
     * of our parent passing the device which asserted the interrupt instead of the device of the bridge.
     */
    PPCIBUS     pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    int iIrqPinBridge = 0;
    uint8_t uDevFnBridge = pPciDev->devfn;

    /* Walk the chain until we reach the host bus. */
    while (pBus->iBus != 0)
    {
        uDevFnBridge = pBus->PciDev.devfn;
        iIrqPinBridge = ((uDevFnBridge >> 3) + iIrqPinBridge) & 3;
        /* Get the parent. */
        pBus = pBus->PciDev.Int.s.CTX_SUFF(pBus);
    }

    AssertMsg(pBus->iBus == 0, ("This is not the host pci bus iBus=%d\n", pBus->iBus));
    pciSetIrqInternal(PCIBUS_2_PCIGLOBALS(pBus), uDevFnBridge, pPciDev, iIrqPinBridge, iLevel);
}

#ifdef IN_RING3

static void pcibridgeConfigWrite(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, uint32_t u32Value, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    LogFlowFunc((": pDevIns=%p iBus=%d iDevice=%d u32Address=%u u32Value=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, u32Value, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS])
    {
        PPCIDEVICE pBridgeDevice = pciFindBridge(pBus, iBus);
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

static uint32_t pcibridgeConfigRead(PPDMDEVINSR3 pDevIns, uint8_t iBus, uint8_t iDevice, uint32_t u32Address, unsigned cb)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);
    uint32_t u32Value = 0xffffffff; /* Return value in case there is no device. */

    LogFlowFunc((": pDevIns=%p iBus=%d iDevice=%d u32Address=%u cb=%d\n", pDevIns, iBus, iDevice, u32Address, cb));

    /* If the current bus is not the target bus search for the bus which contains the device. */
    if (iBus != pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS])
    {
        PPCIDEVICE pBridgeDevice = pciFindBridge(pBus, iBus);
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
 * Saves a state of a PCI bridge device.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pPciDev     Pointer to PCI device.
 * @param   pSSMHandle  The handle to save the state to.
 */
static DECLCALLBACK(int) pcibridgeSaveExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle)
{
    uint32_t    i;
    PPCIBUS pThis = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /*
     * Iterate all the devices.
     */
    for (i = 0; i < RT_ELEMENTS(pThis->devices); i++)
    {
        PPCIDEVICE pDev = pThis->devices[i];
        if (pDev)
        {
            int rc;
            SSMR3PutU32(pSSMHandle, i);
            SSMR3PutMem(pSSMHandle, pDev->config, sizeof(pDev->config));

            rc = SSMR3PutS32(pSSMHandle, pDev->Int.s.uIrqPinState);
            if (RT_FAILURE(rc))
                return rc;
        }
    }
    return SSMR3PutU32(pSSMHandle, ~0); /* terminator */
}

/**
 * Loads a saved PCI bridge device state.
 *
 * @returns VBox status code.
 * @param   pDevIns     The device instance.
 * @param   pSSMHandle  The handle to the saved state.
 * @param   u32Version  The data unit version number.
 */
static DECLCALLBACK(int) pcibridgeLoadExec(PPDMDEVINS pDevIns, PSSMHANDLE pSSMHandle, uint32_t u32Version)
{
    PPCIBUS     pBus  = PDMINS_2_DATA(pDevIns, PPCIBUS);
    uint32_t    u32;
    uint32_t    i;
    int         rc;

    /*
     * Check the version.
     */
    if (u32Version > VBOX_PCI_SAVED_STATE_VERSION)
    {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Iterate all the devices.
     */
    for (i = 0;; i++)
    {
        PCIDEVICE   DevTmp;
        PPCIDEVICE  pDev;

        /* index / terminator */
        rc = SSMR3GetU32(pSSMHandle, &u32);
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
                LogRel(("New device in slot %#x, %s (vendor=%#06x device=%#06x)\n", i, pBus->devices[i]->name,
                        PCIDevGetVendorId(pBus->devices[i]), PCIDevGetDeviceId(pBus->devices[i])));
                if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                    AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            }
        }

        /* get the data */
        DevTmp.Int.s.uIrqPinState = 0;
        SSMR3GetMem(pSSMHandle, DevTmp.config, sizeof(DevTmp.config));
        rc = SSMR3GetS32(pSSMHandle, &DevTmp.Int.s.uIrqPinState);
        if (RT_FAILURE(rc))
            return rc;

        /* check that it's still around. */
        pDev = pBus->devices[i];
        if (!pDev)
        {
            LogRel(("Device in slot %#x has been removed! vendor=%#06x device=%#06x\n", i,
                    PCIDevGetVendorId(&DevTmp), PCIDevGetDeviceId(&DevTmp)));
            if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            continue;
        }

        /* match the vendor id assuming that this will never be changed. */
        if (    DevTmp.config[0] != pDev->config[0]
            ||  DevTmp.config[1] != pDev->config[1])
        {
            LogRel(("Device in slot %#x (%s) vendor id mismatch! saved=%.4Rhxs current=%.4Rhxs\n",
                    i, pDev->name, DevTmp.config, pDev->config));
            AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
        }

        /* commit the loaded device config. */
        memcpy(pDev->config, DevTmp.config, sizeof(pDev->config));

        pDev->Int.s.uIrqPinState = DevTmp.Int.s.uIrqPinState;
    }

    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRESET
 */
static DECLCALLBACK(void) pcibridgeReset(PPDMDEVINS pDevIns)
{
    PPCIBUS pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

    /* Reset config space to default values. */
    pBus->PciDev.config[VBOX_PCI_PRIMARY_BUS] = 0;
    pBus->PciDev.config[VBOX_PCI_SECONDARY_BUS] = 0;
    pBus->PciDev.config[VBOX_PCI_SUBORDINATE_BUS] = 0;
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) pcibridgeRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
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
 * Registers the device with the default PCI bus.
 *
 * @returns VBox status code.
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         The PCI device structure.
 *                          Any PCI enabled device must keep this in it's instance data!
 *                          Fill in the PCI data config before registration, please.
 * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
 * @param   iDev            The PCI device number. Use a negative value for auto assigning one.
 */
static DECLCALLBACK(int) pcibridgeRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, const char *pszName, int iDev)
{
    PPCIBUS     pBus = PDMINS_2_DATA(pDevIns, PPCIBUS);

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
    return pciRegisterInternal(pBus, iDev, pPciDev, pszName);
}

/**
 * Construct a PCI bridge device instance for a VM.
 *
 * @returns VBox status.
 * @param   pDevIns     The device instance data.
 *                      If the registration structure is needed, pDevIns->pDevReg points to it.
 * @param   iInstance   Instance number. Use this to figure out which registers and such to use.
 *                      The device number is also found in pDevIns->iInstance, but since it's
 *                      likely to be freqently used PDM passes it as parameter.
 * @param   pCfgHandle  Configuration node handle for the device. Use this to obtain the configuration
 *                      of the device instance. It's also found in pDevIns->pCfgHandle, but like
 *                      iInstance it's expected to be used a bit in this function.
 */
static DECLCALLBACK(int)   pcibridgeConstruct(PPDMDEVINS pDevIns, int iInstance, PCFGMNODE pCfgHandle)
{
    int rc;

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "GCEnabled\0" "R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* check if RC code is enabled. */
    bool fGCEnabled;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "GCEnabled", &fGCEnabled, true);
    if (RT_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));

    /* check if R0 code is enabled. */
    bool fR0Enabled;
    rc = CFGMR3QueryBoolDef(pCfgHandle, "R0Enabled", &fR0Enabled, true);
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
    PciBusReg.pfnRegisterR3           = pcibridgeRegister;
    PciBusReg.pfnIORegionRegisterR3   = pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksR3 = pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqR3             = pcibridgeSetIrq;
    PciBusReg.pfnSaveExecR3           = pciGenericSaveExec;
    PciBusReg.pfnLoadExecR3           = pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSR3        = NULL; /* Only needed for the first bus. */
    PciBusReg.pszSetIrqRC             = fGCEnabled ? "pcibridgeSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "pcibridgeSetIrq" : NULL;
    rc = pDevIns->pDevHlpR3->pfnPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
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
    PCIDevSetInterruptPin (&pBus->PciDev, 0x00);

    pBus->PciDev.pDevIns                    = pDevIns;
    pBus->PciDev.Int.s.fPciToPciBridge      = true;
    pBus->PciDev.Int.s.pfnBridgeConfigRead  = pcibridgeConfigRead;
    pBus->PciDev.Int.s.pfnBridgeConfigWrite = pcibridgeConfigWrite;

    /*
     * Register this PCI bridge. The called function will take care on which bus we will get registered.
     */
    rc = PDMDevHlpPCIRegister (pDevIns, &pBus->PciDev);
    if (RT_FAILURE(rc))
        return rc;

    pBus->iDevSearch = 0;
    /*
     * The iBus property doesn't really represent the bus number
     * because the guest and the BIOS can choose different bus numbers
     * for them.
     * The bus number is mainly for the setIrq function to indicate
     * when the host bus is reached which will have iBus = 0.
     * Thathswhy the + 1.
     */
    pBus->iBus = iInstance + 1;

    /*
     * Register SSM handlers. We use the same saved state version as for the host bridge
     * to make changes easier.
     */
    rc = PDMDevHlpSSMRegister(pDevIns, "pcibridge", iInstance, VBOX_PCI_SAVED_STATE_VERSION, sizeof(*pBus),
                              NULL, pcibridgeSaveExec, NULL, NULL, pcibridgeLoadExec, NULL);
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
    /* szDeviceName */
    "pcibridge",
    /* szRCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "82801 Mobile PCI to PCI bridge",
    /* fFlags */
    PDM_DEVREG_FLAGS_DEFAULT_BITS | PDM_DEVREG_FLAGS_RC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI,
    /* cMaxInstances */
    ~0,
    /* cbInstance */
    sizeof(PCIBUS),
    /* pfnConstruct */
    pcibridgeConstruct,
    /* pfnDestruct */
    NULL,
    /* pfnRelocate */
    pcibridgeRelocate,
    /* pfnIOCtl */
    NULL,
    /* pfnPowerOn */
    NULL,
    /* pfnReset */
    pcibridgeReset,
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
