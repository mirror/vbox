/* $Id$ */
/** @file
 * VirtIO-SCSI host adapter driver to boot from disks.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdint.h>
#include <string.h>
#include "biosint.h"
#include "ebda.h"
#include "inlines.h"
#include "pciutil.h"
#include "vds.h"

#define DEBUG_VIRTIO 1
#if DEBUG_VIRTIO
# define DBG_VIRTIO(...)        BX_INFO(__VA_ARGS__)
#else
# define DBG_VIRTIO(...)
#endif

/**
 * VirtIO PCI capability structure.
 */
typedef struct
{
    /** Capability typem should always be PCI_CAP_ID_VNDR*/
    uint8_t         u8PciCapId;
    /** Offset where to find the next capability or 0 if last capability. */
    uint8_t         u8PciCapNext;
    /** Size of the capability in bytes. */
    uint8_t         u8PciCapLen;
    /** VirtIO capability type. */
    uint8_t         u8VirtIoCfgType;
    /** BAR where to find it. */
    uint8_t         u8Bar;
    /** Padding. */
    uint8_t         abPad[3];
    /** Offset within the bar. */
    uint32_t        u32Offset;
    /** Length of the structure in bytes. */
    uint32_t        u32Length;
} virtio_pci_cap_t;

/**
 * VirtIO config for the different data structures.
 */
typedef struct
{
    /** BAR where to find it. */
    uint8_t         u8Bar;
    /** Padding. */
    uint8_t         abPad[3];
    /** Offset within the bar. */
    uint32_t        u32Offset;
    /** Length of the structure in bytes. */
    uint32_t        u32Length;
} virtio_bar_cfg_t;

/**
 * VirtIO-SCSI controller data.
 */
typedef struct
{
    /** The BAR configs read from the PCI configuration space, see VIRTIO_PCI_CAP_*_CFG,
     * only use 4 because VIRTIO_PCI_CAP_PCI_CFG is not part of this. */
    virtio_bar_cfg_t aBarCfgs[4];
    /** The start offset in the PCI configuration space where to find the VIRTIO_PCI_CAP_PCI_CFG
     * capability for the alternate access method to the registers. */
    uint8_t          u8PciCfgOff;
    /** PCI bus where the device is located. */
    uint8_t          u8Bus;
    /** Device/Function number. */
    uint8_t          u8DevFn;
    /** Saved high bits of EAX. */
    uint16_t         saved_eax_hi;
} virtio_t;

/* The AHCI specific data must fit into 1KB (statically allocated). */
ct_assert(sizeof(virtio_t) <= 1024);

/** PCI configuration fields. */
#define PCI_CONFIG_CAP                  0x34

#define PCI_CAP_ID_VNDR                 0x09
#define VBOX_VIRTIO_NO_DEVICE 0xffff

#define VBOX_VIRTIO_NIL_CFG             0xff

#define VIRTIO_PCI_CAP_COMMON_CFG       0x01
#define VIRTIO_PCI_CAP_NOTIFY_CFG       0x02
#define VIRTIO_PCI_CAP_ISR_CFG          0x03
#define VIRTIO_PCI_CAP_DEVICE_CFG       0x04
#define VIRTIO_PCI_CAP_PCI_CFG          0x05

#define RT_BIT_32(bit) ((uint32_t)(1L << (bit)))

/* Warning: Destroys high bits of EAX. */
uint32_t inpd(uint16_t port);
#pragma aux inpd =      \
    ".386"              \
    "in     eax, dx"    \
    "mov    dx, ax"     \
    "shr    eax, 16"    \
    "xchg   ax, dx"     \
    parm [dx] value [dx ax] modify nomemory;

/* Warning: Destroys high bits of EAX. */
void outpd(uint16_t port, uint32_t val);
#pragma aux outpd =     \
    ".386"              \
    "xchg   ax, cx"     \
    "shl    eax, 16"    \
    "mov    ax, cx"     \
    "out    dx, eax"    \
    parm [dx] [cx ax] modify nomemory;


/* Machinery to save/restore high bits of EAX. 32-bit port I/O needs to use
 * EAX, but saving/restoring EAX around each port access would be inefficient.
 * Instead, each externally callable routine must save the high bits before
 * modifying them and restore the high bits before exiting.
 */

/* Note: Reading high EAX bits destroys them - *must* be restored later. */
uint16_t eax_hi_rd(void);
#pragma aux eax_hi_rd = \
    ".386"              \
    "shr    eax, 16"    \
    value [ax] modify nomemory;

void eax_hi_wr(uint16_t);
#pragma aux eax_hi_wr = \
    ".386"              \
    "shl    eax, 16"    \
    parm [ax] modify nomemory;

void inline high_bits_save(virtio_t __far *virtio)
{
    virtio->saved_eax_hi = eax_hi_rd();
}

void inline high_bits_restore(virtio_t __far *virtio)
{
    eax_hi_wr(virtio->saved_eax_hi);
}

/**
 * Allocates 1K of conventional memory.
 */
static uint16_t virtio_mem_alloc(void)
{
    uint16_t    base_mem_kb;
    uint16_t    virtio_seg;

    base_mem_kb = read_word(0x00, 0x0413);

    DBG_VIRTIO("AHCI: %dK of base mem\n", base_mem_kb);

    if (base_mem_kb == 0)
        return 0;

    base_mem_kb--; /* Allocate one block. */
    virtio_seg = (((uint32_t)base_mem_kb * 1024) >> 4); /* Calculate start segment. */

    write_word(0x00, 0x0413, base_mem_kb);

    return virtio_seg;
}

/**
 * Initializes the VirtIO SCSI HBA and detects attached devices.
 */
static int virtio_scsi_hba_init(uint8_t u8Bus, uint8_t u8DevFn, uint8_t u8PciCapOffVirtIo)
{
    uint8_t             u8PciCapOff;
    uint16_t            ebda_seg;
    uint16_t            virtio_seg;
    bio_dsk_t __far     *bios_dsk;
    virtio_t __far      *virtio;

    ebda_seg = read_word(0x0040, 0x000E);
    bios_dsk = ebda_seg :> &EbdaData->bdisk;

    /* Allocate 1K of base memory. */
    virtio_seg = virtio_mem_alloc();
    if (virtio_seg == 0)
    {
        DBG_VIRTIO("VirtIO: Could not allocate 1K of memory, can't boot from controller\n");
        return 0;
    }
    DBG_VIRTIO("VirtIO: virtio_seg=%04x, size=%04x, pointer at EBDA:%04x (EBDA size=%04x)\n",
               virtio_seg, sizeof(virtio_t), (uint16_t)&EbdaData->bdisk.virtio_seg, sizeof(ebda_data_t));

    bios_dsk->virtio_seg    = virtio_seg;
    bios_dsk->virtio_devcnt = 0;

    virtio = virtio_seg :> 0;
    virtio->u8Bus   = u8Bus;
    virtio->u8DevFn = u8DevFn;

    /*
     * Go through the config space again, read the complete config capabilities
     * this time and fill in the data.
     */
    u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOffVirtIo);

    while (u8PciCapOff != 0)
    {
        uint8_t u8PciCapId = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);
        uint8_t cbPciCap   = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 2); /* Capability length. */

        if (   u8PciCapId == PCI_CAP_ID_VNDR
            && cbPciCap == sizeof(virtio_pci_cap_t))
        {
            /* Read in the config type and see what we got. */
            uint8_t i;
            virtio_pci_cap_t VirtIoPciCap;
            uint8_t *pbTmp = (uint8_t *)&VirtIoPciCap;

            for (i = 0; i < sizeof(VirtIoPciCap); i++)
                *pbTmp++ = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);

            switch (VirtIoPciCap.u8VirtIoCfgType)
            {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                case VIRTIO_PCI_CAP_ISR_CFG:
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                {
                    virtio_bar_cfg_t *pBarCfg = &virtio->aBarCfgs[VirtIoPciCap.u8VirtIoCfgType - 1];

                    pBarCfg->u8Bar     = VirtIoPciCap.u8Bar;
                    pBarCfg->u32Offset = VirtIoPciCap.u32Offset;
                    pBarCfg->u32Length = VirtIoPciCap.u32Length;
                    break;
                }
                case VIRTIO_PCI_CAP_PCI_CFG:
                    virtio->u8PciCfgOff = u8PciCapOff;
                    break;
                default:
                    DBG_VIRTIO("VirtIO SCSI HBA with unknown PCI capability type 0x%x\n", VirtIoPciCap.u8VirtIoCfgType);
            }

            u8PciCapOff = VirtIoPciCap.u8PciCapNext; /* Saves one VM exit. */
        }
        else
            u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);
    }

    /** @todo Start reading actual registers, initializing the controller, detecting disks, etc. */

    return 0;
}

/**
 * Init the VirtIO SCSI driver and detect attached disks.
 */
void BIOSCALL virtio_scsi_init(void)
{
    uint16_t    busdevfn;

    busdevfn = pci_find_device(0x1af4, 0x1048);
    if (busdevfn != VBOX_VIRTIO_NO_DEVICE)
    {
        uint8_t     u8Bus, u8DevFn;
        uint8_t     u8PciCapOff;
        uint8_t     u8PciCapOffVirtIo = VBOX_VIRTIO_NIL_CFG;
        uint8_t     u8PciCapVirtioSeen = 0;

        u8Bus = (busdevfn & 0xff00) >> 8;
        u8DevFn = busdevfn & 0x00ff;

        DBG_VIRTIO("VirtIO SCSI HBA at Bus %u DevFn 0x%x (raw 0x%x)\n", u8Bus, u8DevFn, busdevfn);

        /* Examine the capability list and search for the VirtIO specific capabilities. */
        u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, PCI_CONFIG_CAP);

        while (u8PciCapOff != 0)
        {
            uint8_t u8PciCapId = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);
            uint8_t cbPciCap   = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 2); /* Capability length. */

            DBG_VIRTIO("Capability ID 0x%x at 0x%x\n", u8PciCapId, u8PciCapOff);

            if (   u8PciCapId == PCI_CAP_ID_VNDR
                && cbPciCap == sizeof(virtio_pci_cap_t))
            {
                /* Read in the config type and see what we got. */
                uint8_t u8PciVirtioCfg = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 3);

                if (u8PciCapOffVirtIo == VBOX_VIRTIO_NIL_CFG)
                    u8PciCapOffVirtIo = u8PciCapOff;

                DBG_VIRTIO("VirtIO: CFG ID 0x%x\n", u8PciVirtioCfg);
                switch (u8PciVirtioCfg)
                {
                    case VIRTIO_PCI_CAP_COMMON_CFG:
                    case VIRTIO_PCI_CAP_NOTIFY_CFG:
                    case VIRTIO_PCI_CAP_ISR_CFG:
                    case VIRTIO_PCI_CAP_DEVICE_CFG:
                    case VIRTIO_PCI_CAP_PCI_CFG:
                        u8PciCapVirtioSeen |= 1 << (u8PciVirtioCfg - 1);
                        break;
                    default:
                        DBG_VIRTIO("VirtIO SCSI HBA with unknown PCI capability type 0x%x\n", u8PciVirtioCfg);
                }
            }

            u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);
        }

        /* Initialize the controller if all required PCI capabilities where found. */
        if (   u8PciCapOffVirtIo != VBOX_VIRTIO_NIL_CFG
            && u8PciCapVirtioSeen == 0x1f)
        {
            int rc;

            DBG_VIRTIO("VirtIO SCSI HBA with all required capabilities at 0x%x\n", u8PciCapOffVirtIo);

            rc = virtio_scsi_hba_init(u8Bus, u8DevFn, u8PciCapOffVirtIo);
        }
        else
            DBG_VIRTIO("VirtIO SCSI HBA with no usable PCI config access!\n");
    }
    else
        DBG_VIRTIO("No VirtIO SCSI HBA!\n");
}
