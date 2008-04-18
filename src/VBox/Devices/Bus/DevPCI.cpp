/* $Id: $ */
/** @file
 * PCI BUS Device.
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

#include "Builtins.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def PCI_LOCK
 * Acquires the PDM lock. This is a NOP if locking is disabled. */
/** @def PCI_UNLOCK
 * Releases the PDM lock. This is a NOP if locking is disabled. */
#ifdef VBOX_WITH_PDM_LOCK
# define PCI_LOCK(pDevIns, rc) \
    do { \
        int rc2 = PDMINS2DATA(pDevIns, PCIBus *)->CTXALLSUFF(pPciHlp)->pfnLock((pDevIns), rc); \
        if (rc2 != VINF_SUCCESS) \
            return rc2; \
    } while (0)
# define PCI_UNLOCK(pDevIns) \
    PDMINS2DATA(pDevIns, PCIBus *)->CTXALLSUFF(pPciHlp)->pfnUnlock(pDevIns)
#else /* !VBOX_WITH_PDM_LOCK */
# define PCI_LOCK(pThis, rc)   do { } while (0)
# define PCI_UNLOCK(pThis)     do { } while (0)
#endif /* !VBOX_WITH_PDM_LOCK */


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


/** Maximum number of PCI devices.
 * Defined like this to make interrupt handling simple. */
#define PCI_DEVICES_MAX     64
/** Number of uint32_t entries needed make a bitmask of the interrupts. */
#define PCI_IRQ_WORDS       ((PCI_DEVICES_MAX + 31) / 32)

/**
 * PCI Globals.
 *
 * @remark
 * These are currently put in the PCIBus structure since we've
 * only got one PCI bus in the current VM configurations. This
 * makes life somewhat simpler in GC.
 */
typedef struct PCIGLOBALS
{
    /** Irq levels for the four PCI Irqs. */
    uint32_t            pci_irq_levels[4][PCI_IRQ_WORDS];
    /** The base address for PCI assigned MMIO addresses. */
    RTGCPHYS32          pci_mem_base;
    /** The next I/O port address which the PCI BIOS will use. */
    uint32_t            pci_bios_io_addr;
    /** The next MMIO address which the PCI BIOS will use. */
    uint32_t            pci_bios_mem_addr;
    /** I/O APIC usage flag */
    bool                fUseIoApic;
    /** I/O APIC irq levels */
    uint32_t            pci_apic_irq_levels[8][PCI_IRQ_WORDS];
    /** ACPI IRQ level */
    uint32_t            acpi_irq_level;
    /** ACPI PIC IRQ */
    int                 acpi_irq;
} PCIGLOBALS;
/** Pointer to per VM data. */
typedef PCIGLOBALS *PPCIGLOBALS;


/**
 * PCI Bus instance.
 */
typedef struct PCIBus
{
    /** IRQ index */
    uint32_t            uIrqIndex;
    /** Bus number. */
    int32_t             iBus;
    /** Start device number. */
    int32_t             iDevSearch;
    /** Config register. */
    uint32_t            uConfigReg;
    /** Array of PCI devices. */
    R3PTRTYPE(PPCIDEVICE) devices[256];

    /** HC pointer to the device instance. */
    R3R0PTRTYPE(PPDMDEVINS) pDevInsHC;
    /** Pointer to the PCI R3  helpers. */
    PCPDMPCIHLPR3           pPciHlpR3;

    /** GC pointer to the device instance. */
    PPDMDEVINSGC        pDevInsGC;
    /** Pointer to the PCI GC helpers. */
    PCPDMPCIHLPGC       pPciHlpGC;
    /** Pointer to the PCI R0 helpers. */
    PCPDMPCIHLPR0       pPciHlpR0;

    /** The PCI device for the PCI bridge. */
    PCIDEVICE           PciDev;
    /** ISA bridge state. */
    PIIX3               PIIX3State;
    /** The global data.
     * Since we've only got one bus at present, we put it here to keep things simple. */
    PCIGLOBALS          Globals;
} PCIBUS;
/** Pointer to a PCIBUS instance. */
typedef PCIBUS *PPCIBUS;
typedef PCIBUS PCIBus;


/** Converts a bus instance pointer to a device instance pointer. */
#define PCIBUS2DEVINS(pPciBus)    ((pPciBus)->CTXSUFF(pDevIns))
/** Converts a device instance pointer to a PCIGLOBALS pointer. */
#define DEVINS2PCIGLOBALS(pDevIns) ((PPCIGLOBALS)(&PDMINS2DATA(pDevIns, PPCIBUS)->Globals))
/** Converts a bus instance pointer to a PCIGLOBALS pointer. */
#define PCIBUS2PCIGLOBALS(pPciBus) ((PPCIGLOBALS)(&pPciBus->Globals))


#ifndef VBOX_DEVICE_STRUCT_TESTCASE
/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS

PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel);

__END_DECLS


#define DEBUG_PCI

#define PCI_VENDOR_ID		0x00	/* 16 bits */
#define PCI_DEVICE_ID		0x02	/* 16 bits */
#define PCI_COMMAND		0x04	/* 16 bits */
#define  PCI_COMMAND_IO		0x1	/* Enable response in I/O space */
#define  PCI_COMMAND_MEMORY	0x2	/* Enable response in Memory space */
#define PCI_CLASS_DEVICE        0x0a    /* Device class */
#define PCI_INTERRUPT_LINE	0x3c	/* 8 bits */
#define PCI_INTERRUPT_PIN	0x3d	/* 8 bits */
#define PCI_MIN_GNT		0x3e	/* 8 bits */
#define PCI_MAX_LAT		0x3f	/* 8 bits */

#ifdef IN_RING3

static void pci_addr_writel(PCIBus *s, uint32_t addr, uint32_t val)
{
    s->uConfigReg = val;
}

static uint32_t pci_addr_readl(PCIBus *s, uint32_t addr)
{
    return s->uConfigReg;
}

static void pci_update_mappings(PCIDevice *d)
{
    PPCIBUS pBus = d->Int.s.pBus;
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
                            int rc = d->pDevIns->pDevHlp->pfnIOPortDeregister(d->pDevIns, r->addr + 2, 1);
                            AssertRC(rc);
                        } else {
                            int rc = d->pDevIns->pDevHlp->pfnIOPortDeregister(d->pDevIns, r->addr, r->size);
                            AssertRC(rc);
                        }
                    } else {
                        RTGCPHYS GCPhysBase = r->addr + PCIBUS2PCIGLOBALS(pBus)->pci_mem_base;
                        int rc;
                        if (pBus->pPciHlpR3->pfnIsMMIO2Base(pBus->pDevInsHC, d->pDevIns, GCPhysBase))
                        {
                            /* unmap it. */
                            rc = r->map_func(d, i, NIL_RTGCPHYS, r->size, (PCIADDRESSSPACE)(r->type));
                            AssertRC(rc);
                            rc = PDMDevHlpMMIO2Unmap(d->pDevIns, i, GCPhysBase);
                        }
                        else
                            rc = d->pDevIns->pDevHlp->pfnMMIODeregister(d->pDevIns, GCPhysBase, r->size);
                        AssertMsgRC(rc, ("rc=%Rrc d=%s i=%d GCPhysBase=%RGp size=%#x\n", rc, d->name, i, GCPhysBase, r->size));
                    }
                }
                r->addr = new_addr;
                if (r->addr != ~0U) {
                    int rc = r->map_func(d, i,
                                         r->addr + (r->type & PCI_ADDRESS_SPACE_IO ? 0 : PCIBUS2PCIGLOBALS(pBus)->pci_mem_base),
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
        case 0x00:
        case 0x80:
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
        case 0x01:
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
	/* status register: only clear bits by writing a '1' at the corresponding bit */
        if (addr == 0x06)
        {
            d->config[addr] &= ~val;
            d->config[addr] |= 0x08; /* interrupt status */
        }
        else if (addr == 0x07)
        {
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

static void pci_data_write(PCIBus *s, uint32_t addr, uint32_t val, int len)
{
    PCIDevice *pci_dev;
    int config_addr, iBus;

    Log(("pci_data_write: addr=%08x val=%08x len=%d\n", s->uConfigReg, val, len));

    if (!(s->uConfigReg & (1 << 31))) {
        return;
    }
    if ((s->uConfigReg & 0x3) != 0) {
        return;
    }
    iBus = (s->uConfigReg >> 16) & 0xff;
    if (iBus != 0)
        return;
    pci_dev = s->devices[(s->uConfigReg >> 8) & 0xff];
    if (!pci_dev)
        return;
    config_addr = (s->uConfigReg & 0xfc) | (addr & 3);
    Log(("pci_config_write: %s: addr=%02x val=%08x len=%d\n", pci_dev->name, config_addr, val, len));
    pci_dev->Int.s.pfnConfigWrite(pci_dev, config_addr, val, len);
}

static uint32_t pci_data_read(PCIBus *s, uint32_t addr, int len)
{
    PCIDevice *pci_dev;
    int config_addr, iBus;
    uint32_t val;

    if (!(s->uConfigReg & (1 << 31)))
        goto fail;
    if ((s->uConfigReg & 0x3) != 0)
        goto fail;
    iBus = (s->uConfigReg >> 16) & 0xff;
    if (iBus != 0)
        goto fail;
    pci_dev = s->devices[(s->uConfigReg >> 8) & 0xff];
    if (!pci_dev) {
    fail:
        switch(len) {
        case 1:
            val = 0xff;
            break;
        case 2:
            val = 0xffff;
            break;
        default:
        case 4:
            val = 0xffffffff;
            break;
        }
        goto the_end;
    }
    config_addr = (s->uConfigReg & 0xfc) | (addr & 3);
    val = pci_dev->Int.s.pfnConfigRead(pci_dev, config_addr, len);
    Log(("pci_config_read: %s: addr=%02x val=%08x len=%d\n", pci_dev->name, config_addr, val, len));
 the_end:
    return val;
}

#endif /* IN_RING3 */


/* return the global irq number corresponding to a given device irq
   pin. We could also use the bus number to have a more precise
   mapping. */
static inline int pci_slot_get_pirq(PCIDevice *pci_dev, int irq_num)
{
    int slot_addend;
    slot_addend = (pci_dev->devfn >> 3) - 1;
    return (irq_num + slot_addend) & 3;
}

static inline int pci_slot_get_apic_pirq(PCIDevice *pci_dev, int irq_num)
{
    return (irq_num + (pci_dev->devfn >> 3)) & 7;
}

static inline int get_pci_irq_apic_level(PPCIGLOBALS pGlobals, int irq_num)
{
    int apic_level;
    apic_level = ((pGlobals->pci_apic_irq_levels[irq_num][0] |
                   pGlobals->pci_apic_irq_levels[irq_num][1]) != 0);
    return apic_level;
}

static void apic_set_irq(PPCIBUS pBus, PCIDevice *pci_dev, int irq_num1, int level, int acpi_irq)
{
    if (acpi_irq == -1) {
        int shift, apic_irq, apic_level;
        uint32_t *p;
        PPCIGLOBALS pGlobals = PCIBUS2PCIGLOBALS(pBus);
        int uIrqIndex = pci_dev->Int.s.iIrq;
        int irq_num = pci_slot_get_apic_pirq(pci_dev, irq_num1);

        p = &pGlobals->pci_apic_irq_levels[irq_num][uIrqIndex >> 5];
        shift = (uIrqIndex & 0x1f);
        *p = (*p & ~(1 << shift)) | ((level & PDM_IRQ_LEVEL_HIGH) << shift);
        apic_irq = irq_num + 0x10;
        apic_level = get_pci_irq_apic_level(pGlobals, irq_num);
        Log3(("apic_set_irq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d\n",
              HCSTRING(pci_dev->name), irq_num1, level, apic_irq, apic_level, irq_num));
        pBus->CTXALLSUFF(pPciHlp)->pfnIoApicSetIrq(CTXSUFF(pBus->pDevIns), apic_irq, apic_level);

        if ((level & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP) {
            *p = (*p & ~(1 << shift));
            apic_level = get_pci_irq_apic_level(pGlobals, irq_num);
            Log3(("apic_set_irq: %s: irq_num1=%d level=%d apic_irq=%d apic_level=%d irq_num1=%d (flop)\n",
                  HCSTRING(pci_dev->name), irq_num1, level, apic_irq, apic_level, irq_num));
            pBus->CTXALLSUFF(pPciHlp)->pfnIoApicSetIrq(CTXSUFF(pBus->pDevIns), apic_irq, apic_level);
        }
    } else {
        Log3(("apic_set_irq: %s: irq_num1=%d level=%d acpi_irq=%d\n",
              HCSTRING(pci_dev->name), irq_num1, level, acpi_irq));
        pBus->CTXALLSUFF(pPciHlp)->pfnIoApicSetIrq(CTXSUFF(pBus->pDevIns), acpi_irq, level);
    }
}

static inline int get_pci_irq_level(PPCIGLOBALS pGlobals, int irq_num)
{
    int pic_level;
#if (PCI_IRQ_WORDS == 2)
    pic_level = ((pGlobals->pci_irq_levels[irq_num][0] |
                  pGlobals->pci_irq_levels[irq_num][1]) != 0);
#else
    {
        int i;
        pic_level = 0;
        for(i = 0; i < PCI_IRQ_WORDS; i++) {
            if (pGlobals->pci_irq_levels[irq_num][i]) {
                pic_level = 1;
                break;
            }
        }
    }
#endif
    return pic_level;
}

/**
 * Set the IRQ for a PCI device.
 *
 * @param   pDevIns         Device instance of the PCI Bus.
 * @param   pPciDev         The PCI device structure.
 * @param   iIrq            IRQ number to set.
 * @param   iLevel          IRQ level.
 */
PDMBOTHCBDECL(void) pciSetIrq(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iIrq, int iLevel)
{
    PPCIBUS     pBus = PDMINS2DATA(pDevIns, PPCIBUS);
    PPCIGLOBALS pGlobals = PCIBUS2PCIGLOBALS(pBus);
    uint8_t    *pbCfg = pBus->PIIX3State.dev.config;
    const bool  fIsAcpiDevice = pPciDev->config[2] == 0x13 && pPciDev->config[3] == 0x71;
    const bool  fIsApicEnabled = pGlobals->fUseIoApic && pbCfg[0xde] == 0xbe && pbCfg[0xad] == 0xef;
    int pic_irq, pic_level;
    uint32_t *p;

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
            apic_set_irq(pBus, pPciDev, -1, iLevel, pPciDev->config[0x3c]);
        else
            apic_set_irq(pBus, pPciDev, iIrq, iLevel, -1);
        return;
    }

    if (fIsAcpiDevice)
    {
        /* As per above treat ACPI in a special way */
        pic_irq = pPciDev->config[0x3c];
        pGlobals->acpi_irq = pic_irq;
        pGlobals->acpi_irq_level = iLevel & PDM_IRQ_LEVEL_HIGH;
    }
    else
    {
        int shift, irq_num, uIrqIndex;
        irq_num = pci_slot_get_pirq(pPciDev, iIrq);
        uIrqIndex = pPciDev->Int.s.iIrq;
        p = &pGlobals->pci_irq_levels[irq_num][uIrqIndex >> 5];
        shift = (uIrqIndex & 0x1f);
        *p = (*p & ~(1 << shift)) | ((iLevel & PDM_IRQ_LEVEL_HIGH) << shift);

        /* now we change the pic irq level according to the piix irq mappings */
        pic_irq = pbCfg[0x60 + irq_num];
        if (pic_irq >= 16)
        {
            if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
                *p = (*p & ~(1 << shift));
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

    Log3(("piix3_set_irq: %s: iLevel=%d iIrq=%d pic_irq=%d pic_level=%d\n",
          HCSTRING(pPciDev->name), iLevel, iIrq, pic_irq, pic_level));
    pBus->CTXALLSUFF(pPciHlp)->pfnIsaSetIrq(CTXSUFF(pBus->pDevIns), pic_irq, pic_level);

    /** @todo optimize pci irq flip-flop some rainy day. */
    if ((iLevel & PDM_IRQ_LEVEL_FLIP_FLOP) == PDM_IRQ_LEVEL_FLIP_FLOP)
        pciSetIrq(pDevIns, pPciDev, iIrq, PDM_IRQ_LEVEL_LOW);
}

#ifdef IN_RING3

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

static void pci_config_writel(PCIDevice *d, uint32_t addr, uint32_t val)
{
    PCIBus *s = d->Int.s.pBus;
    s->uConfigReg = 0x80000000 | (s->iBus << 16) |
        (d->devfn << 8) | addr;
    pci_data_write(s, 0, val, 4);
}

static void pci_config_writew(PCIDevice *d, uint32_t addr, uint32_t val)
{
    PCIBus *s = d->Int.s.pBus;
    s->uConfigReg = 0x80000000 | (s->iBus << 16) |
        (d->devfn << 8) | (addr & ~3);
    pci_data_write(s, addr & 3, val, 2);
}

static void pci_config_writeb(PCIDevice *d, uint32_t addr, uint32_t val)
{
    PCIBus *s = d->Int.s.pBus;
    s->uConfigReg = 0x80000000 | (s->iBus << 16) |
        (d->devfn << 8) | (addr & ~3);
    pci_data_write(s, addr & 3, val, 1);
}

static uint32_t pci_config_readw(PCIDevice *d, uint32_t addr)
{
    PCIBus *s = d->Int.s.pBus;
    s->uConfigReg = 0x80000000 | (s->iBus << 16) |
        (d->devfn << 8) | (addr & ~3);
    return pci_data_read(s, addr & 3, 2);
}

static uint32_t pci_config_readb(PCIDevice *d, uint32_t addr)
{
    PCIBus *s = d->Int.s.pBus;
    s->uConfigReg = 0x80000000 | (s->iBus << 16) |
        (d->devfn << 8) | (addr & ~3);
    return pci_data_read(s, addr & 3, 1);
}

/* host irqs corresponding to PCI irqs A-D */
static const uint8_t pci_irqs[4] = { 11, 9, 11, 9 }; /* bird: added const */

static void pci_set_io_region_addr(PCIDevice *d, int region_num, uint32_t addr)
{
    PCIIORegion *r;
    uint16_t cmd;
    uint32_t ofs;

    if ( region_num == PCI_ROM_SLOT ) {
        ofs = 0x30;
    }else{
        ofs = 0x10 + region_num * 4;
    }

    pci_config_writel(d, ofs, addr);
    r = &d->Int.s.aIORegions[region_num];

    /* enable memory mappings */
    cmd = pci_config_readw(d, PCI_COMMAND);
    if ( region_num == PCI_ROM_SLOT )
        cmd |= 2;
    else if (r->type & PCI_ADDRESS_SPACE_IO)
        cmd |= 1;
    else
        cmd |= 2;
    pci_config_writew(d, PCI_COMMAND, cmd);
}

static void pci_bios_init_device(PCIDevice *d)
{
    int devclass;
    PCIIORegion *r;
    uint32_t *paddr;
    int i, pin, pic_irq, vendor_id, device_id;

    devclass = pci_config_readw(d, PCI_CLASS_DEVICE);
    vendor_id = pci_config_readw(d, PCI_VENDOR_ID);
    device_id = pci_config_readw(d, PCI_DEVICE_ID);
    switch(devclass)
    {
    case 0x0101:
        if (vendor_id == 0x8086 &&
            (device_id == 0x7010 || device_id == 0x7111)) {
            /* PIIX3 or PIIX4 IDE */
            pci_config_writew(d, 0x40, 0x8000); /* enable IDE0 */
            pci_config_writew(d, 0x42, 0x8000); /* enable IDE1 */
            goto default_map;
        } else {
            /* IDE: we map it as in ISA mode */
            pci_set_io_region_addr(d, 0, 0x1f0);
            pci_set_io_region_addr(d, 1, 0x3f4);
            pci_set_io_region_addr(d, 2, 0x170);
            pci_set_io_region_addr(d, 3, 0x374);
        }
        break;
    case 0x0300:
        if (vendor_id != 0x80ee)
            goto default_map;
        /* VGA: map frame buffer to default Bochs VBE address */
        pci_set_io_region_addr(d, 0, 0xE0000000);
        break;
    case 0x0800:
        /* PIC */
        vendor_id = pci_config_readw(d, PCI_VENDOR_ID);
        device_id = pci_config_readw(d, PCI_DEVICE_ID);
        if (vendor_id == 0x1014) {
            /* IBM */
            if (device_id == 0x0046 || device_id == 0xFFFF) {
                /* MPIC & MPIC2 */
                pci_set_io_region_addr(d, 0, 0x80800000 + 0x00040000);
            }
        }
        break;
    case 0xff00:
        if (vendor_id == 0x0106b &&
            (device_id == 0x0017 || device_id == 0x0022)) {
            /* macio bridge */
            pci_set_io_region_addr(d, 0, 0x80800000);
        }
        break;
    default:
    default_map:
        /* default memory mappings */
        for(i = 0; i < PCI_NUM_REGIONS; i++) {
            r = &d->Int.s.aIORegions[i];

            if (r->size) {
                if (r->type & PCI_ADDRESS_SPACE_IO)
                    paddr = &PCIBUS2PCIGLOBALS(d->Int.s.pBus)->pci_bios_io_addr;
                else
                    paddr = &PCIBUS2PCIGLOBALS(d->Int.s.pBus)->pci_bios_mem_addr;
                *paddr = (*paddr + r->size - 1) & ~(r->size - 1);
                pci_set_io_region_addr(d, i, *paddr);
                *paddr += r->size;
            }
        }
        break;
    }

    /* map the interrupt */
    pin = pci_config_readb(d, PCI_INTERRUPT_PIN);
    if (pin != 0) {
        pin = pci_slot_get_pirq(d, pin - 1);
        pic_irq = pci_irqs[pin];
        pci_config_writeb(d, PCI_INTERRUPT_LINE, pic_irq);
    }
}

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
static DECLCALLBACK(int) pciIOPortAddressWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortAddressWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (cb == 4)
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        pci_addr_writel(PDMINS2DATA(pDevIns, PCIBus *), Port, u32);
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
static DECLCALLBACK(int) pciIOPortAddressRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (cb == 4)
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        *pu32 = pci_addr_readl(PDMINS2DATA(pDevIns, PCIBus *), Port);
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
static DECLCALLBACK(int) pciIOPortDataWrite(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t u32, unsigned cb)
{
    Log(("pciIOPortDataWrite: Port=%#x u32=%#x cb=%d\n", Port, u32, cb));
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_WRITE);
        pci_data_write(PDMINS2DATA(pDevIns, PCIBus *), Port, u32, cb);
        PCI_UNLOCK(pDevIns);
    }
    else
        AssertMsgFailed(("Write to port %#x u32=%#x cb=%d\n", Port, u32, cb));
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
static DECLCALLBACK(int) pciIOPortDataRead(PPDMDEVINS pDevIns, void *pvUser, RTIOPORT Port, uint32_t *pu32, unsigned cb)
{
    NOREF(pvUser);
    if (!(Port % cb))
    {
        PCI_LOCK(pDevIns, VINF_IOM_HC_IOPORT_READ);
        *pu32 = pci_data_read(PDMINS2DATA(pDevIns, PCIBus *), Port, cb);
        PCI_UNLOCK(pDevIns);
        Log(("pciIOPortDataRead: Port=%#x cb=%#x -> %#x\n", Port, cb, *pu32));
        return VINF_SUCCESS;
    }
    AssertMsgFailed(("Read from port %#x cb=%d\n", Port, cb));
    return VERR_IOM_IOPORT_UNUSED;
}


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
    PPCIBUS     pData = PDMINS2DATA(pDevIns, PPCIBUS);
    PPCIGLOBALS pGlobals = PCIBUS2PCIGLOBALS(pData);

    /*
     * Bus state data.
     */
    SSMR3PutU32(pSSMHandle, pData->uConfigReg);
    SSMR3PutBool(pSSMHandle, pGlobals->fUseIoApic);
    SSMR3PutU32(pSSMHandle, ~0);        /* separator */

    /*
     * Iterate all the devices.
     */
    for (i = 0; i < ELEMENTS(pData->devices); i++)
    {
        PPCIDEVICE pDev = pData->devices[i];
        if (pDev)
        {
            int rc;
            SSMR3PutU32(pSSMHandle, i);
            SSMR3PutMem(pSSMHandle, pDev->config, sizeof(pDev->config));
            rc = SSMR3PutS32(pSSMHandle, pDev->Int.s.iIrq);
            if (VBOX_FAILURE(rc))
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
    PPCIBUS     pData = PDMINS2DATA(pDevIns, PPCIBUS);
    PPCIGLOBALS  pGlobals = PCIBUS2PCIGLOBALS(pData);
    uint32_t    u32;
    uint32_t    i;
    int         rc;

    /*
     * Check the version.
     */
    if (u32Version > 2)
    {
        AssertFailed();
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    /*
     * Bus state data.
     */
    SSMR3GetU32(pSSMHandle, &pData->uConfigReg);
    if (u32Version > 1)
        SSMR3GetBool(pSSMHandle, &pGlobals->fUseIoApic);

    /* separator */
    rc = SSMR3GetU32(pSSMHandle, &u32);
    if (VBOX_FAILURE(rc))
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
        if (VBOX_FAILURE(rc))
            return rc;
        if (u32 == (uint32_t)~0)
            break;
        if (    u32 >= ELEMENTS(pData->devices)
            ||  u32 < i)
        {
            AssertMsgFailed(("u32=%#x i=%#x\n", u32, i));
            return rc;
        }

        /* skip forward to the device checking that no new devices are present. */
        for (; i < u32; i++)
        {
            if (pData->devices[i])
            {
                LogRel(("New device in slot %#x, %s (vendor=%#06x device=%#06x)\n", i, pData->devices[i]->name,
                        PCIDevGetVendorId(pData->devices[i]), PCIDevGetDeviceId(pData->devices[i])));
                if (SSMR3HandleGetAfter(pSSMHandle) != SSMAFTER_DEBUG_IT)
                    AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
            }
        }

        /* get the data */
        SSMR3GetMem(pSSMHandle, DevTmp.config, sizeof(DevTmp.config));
        rc = SSMR3GetS32(pSSMHandle, &DevTmp.Int.s.iIrq);
        if (VBOX_FAILURE(rc))
            return rc;

        /* check that it's still around. */
        pDev = pData->devices[i];
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
            LogRel(("Device in slot %#x (%s) vendor id mismatch! saved=%.4Vhxs current=%.4Vhxs\n",
                    i, pDev->name, DevTmp.config, pDev->config));
            AssertFailedReturn(VERR_SSM_LOAD_CONFIG_MISMATCH);
        }

        /* commit the loaded device config. */
        memcpy(pDev->config, DevTmp.config, sizeof(pDev->config));
        if (DevTmp.Int.s.iIrq >= PCI_DEVICES_MAX)
        {
            LogRel(("Device %s: Too many devices %d (max=%d)\n", pDev->name, DevTmp.Int.s.iIrq, PCI_DEVICES_MAX));
            AssertFailedReturn(VERR_TOO_MUCH_DATA);
        }

        pDev->Int.s.iIrq = DevTmp.Int.s.iIrq;
    }
    return VINF_SUCCESS;
}


/* -=-=-=-=-=- real code -=-=-=-=-=- */


/**
 * Registers the device with the default PCI bus.
 *
 * @returns VBox status code.
 * @param   pBus            The bus to register with.
 * @param   iDev            The PCI device ordinal.
 * @param   pPciDev         The PCI device structure.
 * @param   pszName         Pointer to device name (permanent, readonly). For debugging, not unique.
 */
static void pciRegisterInternal(PPCIBUS pBus, int iDev, PPCIDEVICE pPciDev, const char *pszName)
{
    Assert(!pBus->devices[iDev]);
    pPciDev->devfn                  = iDev;
    pPciDev->name                   = pszName;
    pPciDev->Int.s.pBus             = pBus;
    pPciDev->Int.s.pfnConfigRead    = pci_default_read_config;
    pPciDev->Int.s.pfnConfigWrite   = pci_default_write_config;
    AssertMsg(pBus->uIrqIndex < PCI_DEVICES_MAX,
              ("Device %s: Too many devices %d (max=%d)\n",
               pszName, pBus->uIrqIndex, PCI_DEVICES_MAX));
    pPciDev->Int.s.iIrq             = pBus->uIrqIndex++;
    pBus->devices[iDev]             = pPciDev;
    Log(("PCI: Registered device %d function %d (%#x) '%s'.\n",
         iDev >> 3, iDev & 7, 0x80000000 | (iDev << 8), pszName));
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
    PPCIBUS     pBus = PDMINS2DATA(pDevIns, PPCIBUS);

    /*
     * Check input.
     */
    if (    !pszName
        ||  !pPciDev
        ||  iDev >= (int)ELEMENTS(pBus->devices)
        ||  (iDev >= 0 && iDev <= 8))
    {
        AssertMsgFailed(("Invalid argument! pszName=%s pPciDev=%p iDev=%d\n", pszName, pPciDev, iDev));
        return VERR_INVALID_PARAMETER;
    }

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
        else
        {
            Assert(!(pBus->iDevSearch % 8));
            for (iDev = pBus->iDevSearch; iDev < (int)ELEMENTS(pBus->devices); iDev += 8)
                if (    !pBus->devices[iDev]
                    &&  !pBus->devices[iDev + 1]
                    &&  !pBus->devices[iDev + 2]
                    &&  !pBus->devices[iDev + 3]
                    &&  !pBus->devices[iDev + 4]
                    &&  !pBus->devices[iDev + 5]
                    &&  !pBus->devices[iDev + 6]
                    &&  !pBus->devices[iDev + 7])
                    break;
            if (iDev >= (int)ELEMENTS(pBus->devices))
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
        AssertReleaseMsg(iDev > 8, ("iDev=%d pszName=%s\n", iDev, pszName));
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
            for (iDevRel = pBus->iDevSearch; iDevRel < (int)ELEMENTS(pBus->devices); iDevRel += 8)
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

    /*
     * Register the device.
     */
    pciRegisterInternal(pBus, iDev, pPciDev, pszName);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) pciIORegionRegister(PPDMDEVINS pDevIns, PPCIDEVICE pPciDev, int iRegion, uint32_t cbRegion, PCIADDRESSSPACE enmType, PFNPCIIOREGIONMAP pfnCallback)
{
    PPCIIOREGION pRegion;

    /*
     * Validate.
     */
    if (    enmType != PCI_ADDRESS_SPACE_MEM
        &&  enmType != PCI_ADDRESS_SPACE_IO
        &&  enmType != PCI_ADDRESS_SPACE_MEM_PREFETCH)
    {
        AssertMsgFailed(("Invalid enmType=%#x? Or was this a bitmask after all...\n", enmType));
        return VERR_INVALID_PARAMETER;
    }
    if ((unsigned)iRegion >= PCI_NUM_REGIONS)
    {
        AssertMsgFailed(("Invalid iRegion=%d PCI_NUM_REGIONS=%d\n", iRegion, PCI_NUM_REGIONS));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Register the I/O region.
     */
    pRegion = &pPciDev->Int.s.aIORegions[iRegion];
    pRegion->addr       = ~0U;
    pRegion->size       = cbRegion;
    pRegion->type       = enmType;
    pRegion->map_func   = pfnCallback;
    return VINF_SUCCESS;
}


/**
 * @copydoc PDMPCIBUSREG::pfnSetConfigCallbacksHC
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
    PPCIGLOBALS pGlobals = DEVINS2PCIGLOBALS(pDevIns);
    PPCIBUS     pBus = PDMINS2DATA(pDevIns, PPCIBUS);
    PVM         pVM = PDMDevHlpGetVM(pDevIns);
    Assert(pVM);

    /*
     * Set the start addresses.
     */
    pGlobals->pci_bios_io_addr  = 0xc000;
    pGlobals->pci_bios_mem_addr = 0xf0000000;

    /*
     * Activate IRQ mappings.
     */
    for (i = 0; i < 4; i++)
    {
        uint8_t irq = pci_irqs[i];
        /* Set to trigger level. */
        elcr[irq >> 3] |= (1 << (irq & 7));
        /* Activate irq remapping in PIIX3. */
        pci_config_writeb(&pBus->PIIX3State.dev, 0x60 + i, irq);
    }

    /* Tell to the PIC. */
    rc = IOMIOPortWrite(pVM, 0x4d0, elcr[0], sizeof(uint8_t));
    if (rc == VINF_SUCCESS)
        rc = IOMIOPortWrite(pVM, 0x4d1, elcr[1], sizeof(uint8_t));
    if (rc != VINF_SUCCESS)
    {
        AssertMsgFailed(("Writing to PIC failed!\n"));
        return VBOX_SUCCESS(rc) ? VERR_INTERNAL_ERROR : rc;
    }

    /*
     * Init the devices.
     */
    for (i = 0; i < ELEMENTS(pBus->devices); i++)
    {
        if (pBus->devices[i])
        {
            Log2(("PCI: Initializing device %d (%#x) '%s'\n",
                  i, 0x80000000 | (i << 8), pBus->devices[i]->name));
            pci_bios_init_device(pBus->devices[i]);
        }
    }
    return VINF_SUCCESS;
}

/**
 * @copydoc FNPDMDEVRELOCATE
 */
static DECLCALLBACK(void) pciRelocate(PPDMDEVINS pDevIns, RTGCINTPTR offDelta)
{
    PPCIBUS pBus = PDMINS2DATA(pDevIns, PPCIBUS);
    pBus->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);
    pBus->pPciHlpGC = pBus->pPciHlpR3->pfnGetGCHelpers(pDevIns);
}


/**
 * Construct a PCI Bus device instance for a VM.
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
    PPCIGLOBALS     pGlobals = DEVINS2PCIGLOBALS(pDevIns);
    PPCIBUS         pBus = PDMINS2DATA(pDevIns, PPCIBUS);
    PDMPCIBUSREG    PciBusReg;
    int             rc;
    bool            fGCEnabled;
    bool            fR0Enabled;
    bool            fUseIoApic;
    Assert(iInstance == 0);

    /*
     * Validate and read configuration.
     */
    if (!CFGMR3AreValuesValid(pCfgHandle, "IOAPIC\0" "GCEnabled\0R0Enabled\0"))
        return VERR_PDM_DEVINS_UNKNOWN_CFG_VALUES;

    /* query whether we got an IOAPIC */
    rc = CFGMR3QueryBool(pCfgHandle, "IOAPIC", &fUseIoApic);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fUseIoApic = false;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"IOAPIC\""));

    /* check if GC code is enabled. */
    rc = CFGMR3QueryBool(pCfgHandle, "GCEnabled", &fGCEnabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fGCEnabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"GCEnabled\""));
    Log(("PCI: fGCEnabled=%d\n", fGCEnabled));

    /* check if R0 code is enabled. */
    rc = CFGMR3QueryBool(pCfgHandle, "R0Enabled", &fR0Enabled);
    if (rc == VERR_CFGM_VALUE_NOT_FOUND)
        fR0Enabled = true;
    else if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Configuration error: Failed to query boolean value \"R0Enabled\""));
    Log(("PCI: fR0Enabled=%d\n", fR0Enabled));

    /*
     * Init data and register the PCI bus.
     */
    pGlobals->pci_mem_base        = 0;
    pGlobals->pci_bios_io_addr    = 0xc000;
    pGlobals->pci_bios_mem_addr   = 0xf0000000;
    memset(&pGlobals->pci_irq_levels, 0, sizeof(pGlobals->pci_irq_levels));
    pGlobals->fUseIoApic          = fUseIoApic;
    memset(&pGlobals->pci_apic_irq_levels, 0, sizeof(pGlobals->pci_apic_irq_levels));

    pBus->pDevInsHC = pDevIns;
    pBus->pDevInsGC = PDMDEVINS_2_GCPTR(pDevIns);

    PciBusReg.u32Version              = PDM_PCIBUSREG_VERSION;
    PciBusReg.pfnRegisterHC           = pciRegister;
    PciBusReg.pfnIORegionRegisterHC   = pciIORegionRegister;
    PciBusReg.pfnSetConfigCallbacksHC = pciSetConfigCallbacks;
    PciBusReg.pfnSetIrqHC             = pciSetIrq;
    PciBusReg.pfnSaveExecHC           = pciGenericSaveExec;
    PciBusReg.pfnLoadExecHC           = pciGenericLoadExec;
    PciBusReg.pfnFakePCIBIOSHC        = pciFakePCIBIOS;
    PciBusReg.pszSetIrqGC             = fGCEnabled ? "pciSetIrq" : NULL;
    PciBusReg.pszSetIrqR0             = fR0Enabled ? "pciSetIrq" : NULL;
    rc = pDevIns->pDevHlp->pfnPCIBusRegister(pDevIns, &PciBusReg, &pBus->pPciHlpR3);
    if (VBOX_FAILURE(rc))
        return PDMDEV_SET_ERROR(pDevIns, rc,
                                N_("Failed to register ourselves as a PCI Bus"));
    if (pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION)
        return PDMDevHlpVMSetError(pDevIns, VERR_VERSION_MISMATCH, RT_SRC_POS,
                                   N_("PCI helper version mismatch; got %#x expected %#x"),
                                   pBus->pPciHlpR3->u32Version != PDM_PCIHLPR3_VERSION);

    pBus->pPciHlpGC = pBus->pPciHlpR3->pfnGetGCHelpers(pDevIns);
    pBus->pPciHlpR0 = pBus->pPciHlpR3->pfnGetR0Helpers(pDevIns);

    /*
     * Fill in PCI configs and add them to the bus.
     */
    /* i440FX */
    pBus->PciDev.config[0x00]         = 0x86; /* vendor_id: Intel */
    pBus->PciDev.config[0x01]         = 0x80;
    pBus->PciDev.config[0x02]         = 0x37; /* device_id: */
    pBus->PciDev.config[0x03]         = 0x12;
    pBus->PciDev.config[0x08]         = 0x02; /* revision */
    pBus->PciDev.config[0x0a]         = 0x00; /* class_sub = host2pci */
    pBus->PciDev.config[0x0b]         = 0x06; /* class_base = PCI_bridge */
    pBus->PciDev.config[0x0e]         = 0x00; /* header_type */
    pBus->PciDev.pDevIns              = pDevIns;
    pBus->PciDev.Int.s.fRequestedDevFn= true;
    pciRegisterInternal(pBus, 0, &pBus->PciDev, "i440FX");

    /* PIIX3 */
    pBus->PIIX3State.dev.config[0x00] = 0x86; /* vendor:    Intel */
    pBus->PIIX3State.dev.config[0x01] = 0x80;
    pBus->PIIX3State.dev.config[0x02] = 0x00; /* device_id: 82371SB PIIX3 PCI-to-ISA bridge (Step A1) */
    pBus->PIIX3State.dev.config[0x03] = 0x70;
    pBus->PIIX3State.dev.config[0x0a] = 0x01; /* class_sub = PCI_ISA */
    pBus->PIIX3State.dev.config[0x0b] = 0x06; /* class_base = PCI_bridge */
    pBus->PIIX3State.dev.config[0x0e] = 0x80; /* header_type = PCI_multifunction, generic */
    pBus->PIIX3State.dev.pDevIns      = pDevIns;
    pBus->PciDev.Int.s.fRequestedDevFn= true;
    pciRegisterInternal(pBus, 8, &pBus->PIIX3State.dev, "PIIX3");
    piix3_reset(&pBus->PIIX3State);

    pBus->iDevSearch = 16;

    /*
     * Register I/O ports and save state.
     */
    rc = PDMDevHlpIOPortRegister(pDevIns, 0xcf8, 1, NULL, pciIOPortAddressWrite, pciIOPortAddressRead, NULL, NULL, "i440FX (PCI)");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpIOPortRegister(pDevIns, 0xcfc, 4, NULL, pciIOPortDataWrite, pciIOPortDataRead, NULL, NULL, "i440FX (PCI)");
    if (VBOX_FAILURE(rc))
        return rc;
    rc = PDMDevHlpSSMRegister(pDevIns, "pci", iInstance, 2, sizeof(*pBus),
                              NULL, pciSaveExec, NULL, NULL, pciLoadExec, NULL);
    if (VBOX_FAILURE(rc))
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
    /* szGCMod */
    "VBoxDDGC.gc",
    /* szR0Mod */
    "VBoxDDR0.r0",
    /* pszDescription */
    "i440FX PCI bridge and PIIX3 ISA bridge.",
    /* fFlags */
    PDM_DEVREG_FLAGS_HOST_BITS_DEFAULT | PDM_DEVREG_FLAGS_GUEST_BITS_32 | PDM_DEVREG_FLAGS_GC | PDM_DEVREG_FLAGS_R0,
    /* fClass */
    PDM_DEVREG_CLASS_BUS_PCI | PDM_DEVREG_CLASS_BUS_ISA,
    /* cMaxInstances */
    1,
    /* cbInstance */
    sizeof(PCIBUS),
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
    NULL
};
#endif /* IN_RING3 */
#endif /* !VBOX_DEVICE_STRUCT_TESTCASE */
