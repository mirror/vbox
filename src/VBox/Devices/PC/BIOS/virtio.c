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

/* The maximum CDB size. */
#define VIRTIO_SCSI_CDB_SZ      16
/** Maximum sense data to return. */
#define VIRTIO_SCSI_SENSE_SZ    32

/**
 * VirtIO queue descriptor.
 */
typedef struct
{
    /** 64bit guest physical address of the buffer, split into high and low part because we work in real mode. */
    uint32_t        GCPhysBufLow;
    uint32_t        GCPhysBufHigh;
    /** Length of the buffer in bytes. */
    uint32_t        cbBuf;
    /** Flags for the buffer. */
    uint16_t        fFlags;
    /** Next field where the buffer is continued if _NEXT flag is set. */
    uint16_t        idxNext;
} virtio_q_desc_t;

#define VIRTIO_Q_DESC_F_NEXT     0x1
#define VIRTIO_Q_DESC_F_WRITE    0x2
#define VIRTIO_Q_DESC_F_INDIRECT 0x4

/**
 * VirtIO available ring.
 */
typedef struct
{
    /** Flags. */
    uint16_t        fFlags;
    /** Next index to write an available buffer by the driver. */
    uint16_t        idxNextFree;
    /** The ring - we only provide one entry. */
    uint16_t        au16Ring[1];
    /** Used event index. */
    uint16_t        u16EvtUsed;
} virtio_q_avail_t;

/**
 * VirtIO queue used element.
 */
typedef struct
{
    /** Index of the start of the descriptor chain. */
    uint32_t        u32Id;
    /** Number of bytes used in the descriptor chain. */
    uint32_t        cbUsed;
} virtio_q_used_elem_t;

/**
 * VirtIo used ring.
 */
typedef struct
{
    /** Flags. */
    uint16_t             fFlags;
    /** Index where the next entry would be written by the device. */
    uint16_t             idxNextUsed;
    /** The used ring. */
    virtio_q_used_elem_t aRing[1];
} virtio_q_used_t;

/**
 * VirtIO queue structure we are using, needs to be aligned on a 16byte boundary.
 */
typedef struct
{
    /** The descriptor table, using 4 max. */
    virtio_q_desc_t      aDescTbl[4];
    /** Available ring. */
    virtio_q_avail_t     AvailRing;
    /** Used ring. */
    virtio_q_used_t      UsedRing;
} virtio_q_t;

/**
 * VirtIO SCSI request structure passed in the queue.
 */
typedef struct
{
    /** The LUN to address. */
    uint8_t                 au8Lun[8];
    /** Request ID - split into low and high part. */
    uint32_t                u32IdLow;
    uint32_t                u32IdHigh;
    /** Task attributes. */
    uint8_t                 u8TaskAttr;
    /** Priority. */
    uint8_t                 u8Prio;
    /** CRN value, usually 0. */
    uint8_t                 u8Crn;
    /** The CDB. */
    uint8_t                 abCdb[VIRTIO_SCSI_CDB_SZ];
} virtio_scsi_req_hdr_t;

/**
 * VirtIO SCSI status structure filled by the device.
 */
typedef struct
{
    /** Returned sense length. */
    uint32_t                cbSense;
    /** Residual amount of bytes left. */
    uint32_t                cbResidual;
    /** Status qualifier. */
    uint16_t                u16StatusQual;
    /** Status code. */
    uint8_t                 u8Status;
    /** Response code. */
    uint8_t                 u8Response;
    /** Sense data. */
    uint8_t                 abSense[VIRTIO_SCSI_SENSE_SZ];
} virtio_scsi_req_sts_t;

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
 * VirtIO-SCSI controller data.
 */
typedef struct
{
    /** The queue used - must be first for alignment reasons. */
    virtio_q_t       Queue;
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

/* The VirtIO specific data must fit into 1KB (statically allocated). */
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

#define VIRTIO_COMMON_REG_DEV_FEAT_SLCT 0x00
#define VIRTIO_COMMON_REG_DEV_FEAT      0x04
# define VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT 0x01
#define VIRTIO_COMMON_REG_DRV_FEAT_SLCT 0x08
#define VIRTIO_COMMON_REG_DRV_FEAT      0x0c
#define VIRTIO_COMMON_REG_MSIX_CFG      0x10
#define VIRTIO_COMMON_REG_NUM_QUEUES    0x12
#define VIRTIO_COMMON_REG_DEV_STS       0x14
# define VIRTIO_CMN_REG_DEV_STS_F_RST     0x00
# define VIRTIO_CMN_REG_DEV_STS_F_ACK     0x01
# define VIRTIO_CMN_REG_DEV_STS_F_DRV     0x02
# define VIRTIO_CMN_REG_DEV_STS_F_DRV_OK  0x04
# define VIRTIO_CMN_REG_DEV_STS_F_FEAT_OK 0x08
# define VIRTIO_CMN_REG_DEV_STS_F_DEV_RST 0x40
# define VIRTIO_CMN_REG_DEV_STS_F_FAILED  0x80
#define VIRTIO_COMMON_REG_CFG_GEN       0x15

#define VIRTIO_COMMON_REG_Q_SELECT      0x16
#define VIRTIO_COMMON_REG_Q_SIZE        0x18
#define VIRTIO_COMMON_REG_Q_MSIX_VEC    0x1a
#define VIRTIO_COMMON_REG_Q_ENABLE      0x1c
#define VIRTIO_COMMON_REG_Q_NOTIFY_OFF  0x1e
#define VIRTIO_COMMON_REG_Q_DESC        0x20
#define VIRTIO_COMMON_REG_Q_DRIVER      0x28
#define VIRTIO_COMMON_REG_Q_DEVICE      0x30

#define VIRTIO_DEV_CFG_REG_Q_NUM        0x00
#define VIRTIO_DEV_CFG_REG_SEG_MAX      0x04
#define VIRTIO_DEV_CFG_REG_SECT_MAX     0x08
#define VIRTIO_DEV_CFG_REG_CMD_PER_LUN  0x0c
#define VIRTIO_DEV_CFG_REG_EVT_INFO_SZ  0x10
#define VIRTIO_DEV_CFG_REG_SENSE_SZ     0x14
#define VIRTIO_DEV_CFG_REG_CDB_SZ       0x18
#define VIRTIO_DEV_CFG_REG_MAX_CHANNEL  0x1c
#define VIRTIO_DEV_CFG_REG_MAX_TGT      0x1e
#define VIRTIO_DEV_CFG_REG_MAX_LUN      0x20

#define VIRTIO_SCSI_Q_CONTROL           0x00
#define VIRTIO_SCSI_Q_EVENT             0x01
#define VIRTIO_SCSI_Q_REQUEST           0x02

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

static void virtio_reg_set_bar_offset_length(virtio_t __far *virtio, uint8_t u8Bar, uint32_t offReg, uint8_t cb)
{
    pci_write_config_byte(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff  +  4, u8Bar);
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff +  8, offReg);
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + 12, (uint32_t)cb);
}

static void virtio_reg_common_access_prepare(virtio_t __far *virtio, uint16_t offReg, uint16_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_COMMON_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_COMMON_CFG - 1].u32Offset + offReg,
                                     cbAcc);
}

static void virtio_reg_dev_access_prepare(virtio_t __far *virtio, uint16_t offReg, uint16_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_DEVICE_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_DEVICE_CFG - 1].u32Offset + offReg,
                                     cbAcc);
}

static uint8_t virtio_reg_common_read_u8(virtio_t __far *virtio, uint16_t offReg)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint8_t));
    return pci_read_config_byte(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
}

static void virtio_reg_common_write_u8(virtio_t __far *virtio, uint16_t offReg, uint8_t u8Val)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint8_t));
    pci_write_config_byte(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u8Val);
}

static uint16_t virtio_reg_common_read_u16(virtio_t __far *virtio, uint16_t offReg)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint16_t));
    return pci_read_config_word(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
}

static void virtio_reg_common_write_u16(virtio_t __far *virtio, uint16_t offReg, uint16_t u16Val)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint16_t));
    pci_write_config_word(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u16Val);
}

static uint32_t virtio_reg_common_read_u32(virtio_t __far *virtio, uint16_t offReg)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint32_t));
    return pci_read_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
}

static void virtio_reg_common_write_u32(virtio_t __far *virtio, uint16_t offReg, uint32_t u32Val)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint32_t));
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u32Val);
}

static uint16_t virtio_reg_dev_cfg_read_u16(virtio_t __far *virtio, uint16_t offReg)
{
    virtio_reg_dev_access_prepare(virtio, offReg, sizeof(uint16_t));
    return pci_read_config_word(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
}

static uint32_t virtio_reg_dev_cfg_read_u32(virtio_t __far *virtio, uint16_t offReg)
{
    virtio_reg_dev_access_prepare(virtio, offReg, sizeof(uint32_t));
    return pci_read_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
}

static void virtio_reg_dev_cfg_write_u32(virtio_t __far *virtio, uint16_t offReg, uint32_t u32Val)
{
    virtio_reg_dev_access_prepare(virtio, offReg, sizeof(uint32_t));
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u32Val);
}

/**
 * Allocates 1K of conventional memory.
 */
static uint16_t virtio_mem_alloc(void)
{
    uint16_t    base_mem_kb;
    uint16_t    virtio_seg;

    base_mem_kb = read_word(0x00, 0x0413);

    DBG_VIRTIO("VirtIO: %dK of base mem\n", base_mem_kb);

    if (base_mem_kb == 0)
        return 0;

    base_mem_kb--; /* Allocate one block. */
    virtio_seg = (((uint32_t)base_mem_kb * 1024) >> 4); /* Calculate start segment. */

    write_word(0x00, 0x0413, base_mem_kb);

    return virtio_seg;
}

/**
 * Converts a segment:offset pair into a 32bit physical address.
 */
static uint32_t virtio_addr_to_phys(void __far *ptr)
{
    return ((uint32_t)FP_SEG(ptr) << 4) + FP_OFF(ptr);
}

/**
 * Initializes the VirtIO SCSI HBA and detects attached devices.
 */
static int virtio_scsi_hba_init(uint8_t u8Bus, uint8_t u8DevFn, uint8_t u8PciCapOffVirtIo)
{
    uint8_t             u8PciCapOff;
    uint16_t            ebda_seg;
    uint16_t            virtio_seg;
    uint32_t            fFeatures;
    uint8_t             u8DevStat;
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
    u8PciCapOff = u8PciCapOffVirtIo;
    while (u8PciCapOff != 0)
    {
        uint8_t u8PciCapId = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff);
        uint8_t cbPciCap   = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 2); /* Capability length. */

        DBG_VIRTIO("Capability ID 0x%x at 0x%x\n", u8PciCapId, u8PciCapOff);

        if (   u8PciCapId == PCI_CAP_ID_VNDR
            && cbPciCap >= sizeof(virtio_pci_cap_t))
        {
            /* Read in the config type and see what we got. */
            uint8_t u8PciVirtioCfg = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 3);

            DBG_VIRTIO("VirtIO: CFG ID 0x%x\n", u8PciVirtioCfg);
            switch (u8PciVirtioCfg)
            {
                case VIRTIO_PCI_CAP_COMMON_CFG:
                case VIRTIO_PCI_CAP_NOTIFY_CFG:
                case VIRTIO_PCI_CAP_ISR_CFG:
                case VIRTIO_PCI_CAP_DEVICE_CFG:
                {
                    virtio_bar_cfg_t __far *pBarCfg = &virtio->aBarCfgs[u8PciVirtioCfg - 1];

                    pBarCfg->u8Bar     = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 4);
                    pBarCfg->u32Offset = pci_read_config_dword(u8Bus, u8DevFn, u8PciCapOff + 8);
                    pBarCfg->u32Length = pci_read_config_dword(u8Bus, u8DevFn, u8PciCapOff + 12);
                    break;
                }
                case VIRTIO_PCI_CAP_PCI_CFG:
                    virtio->u8PciCfgOff = u8PciCapOff;
                    DBG_VIRTIO("VirtIO PCI CAP window offset: %x\n", u8PciCapOff);
                    break;
                default:
                    DBG_VIRTIO("VirtIO SCSI HBA with unknown PCI capability type 0x%x\n", u8PciVirtioCfg);
            }
        }

        u8PciCapOff = pci_read_config_byte(u8Bus, u8DevFn, u8PciCapOff + 1);
    }

    /* Reset the device. */
    u8DevStat = VIRTIO_CMN_REG_DEV_STS_F_RST;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);
    /* Acknowledge presence. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_ACK;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);
    /* Our driver knows how to operatet the device. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_DRV;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);

    /* Read the feature bits and only program the VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT bit if available. */
    fFeatures = virtio_reg_common_read_u32(virtio, VIRTIO_COMMON_REG_DEV_FEAT);
    fFeatures &= VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT;

    /* Check that the device is sane. */
    if (   virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_Q_NUM) < 1
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_CDB_SZ) < 16
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_SENSE_SZ) < 32
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_SECT_MAX) < 1)
    {
        DBG_VIRTIO("VirtIO-SCSI: Invalid SCSI device configuration, ignoring device\n");
        return 0;
    }

    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_DEV_FEAT, fFeatures);

    /* Set the features OK bit. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_FEAT_OK;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);

    /* Read again and check the the okay bit is still set. */
    if (!(virtio_reg_common_read_u8(virtio, VIRTIO_COMMON_REG_DEV_STS) & VIRTIO_CMN_REG_DEV_STS_F_FEAT_OK))
    {
        DBG_VIRTIO("VirtIO-SCSI: Device doesn't accept our feature set, ignoring device\n");
        return 0;
    }

    /* Disable event and control queue. */
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SELECT, VIRTIO_SCSI_Q_CONTROL);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SIZE, 0);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_ENABLE, 0);

    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SELECT, VIRTIO_SCSI_Q_EVENT);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SIZE, 0);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_ENABLE, 0);

    /* Setup the request queue. */
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SELECT, VIRTIO_SCSI_Q_REQUEST);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SIZE, 1);
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_ENABLE, 1);

    /* Set queue area addresses (only low part, leave high part 0). */
    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DESC,     virtio_addr_to_phys(&virtio->Queue.aDescTbl[0]));
    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DESC + 4, 0);

    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DRIVER,     virtio_addr_to_phys(&virtio->Queue.AvailRing));
    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DRIVER + 4, 0);

    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DEVICE,     virtio_addr_to_phys(&virtio->Queue.UsedRing));
    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_Q_DEVICE + 4, 0);

    virtio_reg_dev_cfg_write_u32(virtio, VIRTIO_DEV_CFG_REG_CDB_SZ, VIRTIO_SCSI_CDB_SZ);
    virtio_reg_dev_cfg_write_u32(virtio, VIRTIO_DEV_CFG_REG_SENSE_SZ, VIRTIO_SCSI_SENSE_SZ);

    /* Bring the device into operational mode. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_DRV_OK;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);

    /** @todo Detect attached devices. */

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
                && cbPciCap >= sizeof(virtio_pci_cap_t))
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
