/* $Id$ */
/** @file
 * VirtIO-SCSI host adapter driver to boot from disks.
 */

/*
 * Copyright (C) 2019-2020 Oracle Corporation
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
#include "scsi.h"

//#define DEBUG_VIRTIO 1
#if DEBUG_VIRTIO
# define DBG_VIRTIO(...)        BX_INFO(__VA_ARGS__)
#else
# define DBG_VIRTIO(...)
#endif

#define VBSCSI_MAX_DEVICES 16 /* Maximum number of devices a SCSI device currently supported. */

/* The maximum CDB size. */
#define VIRTIO_SCSI_CDB_SZ      16
/** Maximum sense data to return. */
#define VIRTIO_SCSI_SENSE_SZ    32

#define VIRTIO_SCSI_RING_ELEM    3

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
    volatile uint16_t        fFlags;
    /** Next index to write an available buffer by the driver. */
    volatile uint16_t        idxNextFree;
    /** The ring - we only provide one entry. */
    volatile uint16_t        au16Ring[VIRTIO_SCSI_RING_ELEM];
    /** Used event index. */
    volatile uint16_t        u16EvtUsed;
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
    volatile uint16_t    fFlags;
    /** Index where the next entry would be written by the device. */
    volatile uint16_t    idxNextUsed;
    /** The used ring. */
    virtio_q_used_elem_t aRing[VIRTIO_SCSI_RING_ELEM];
} virtio_q_used_t;

/**
 * VirtIO queue structure we are using, needs to be aligned on a 16byte boundary.
 */
typedef struct
{
    /** The descriptor table, using 5 max. */
    virtio_q_desc_t      aDescTbl[5];
    /** Available ring. */
    virtio_q_avail_t     AvailRing;
    /** Used ring. */
    virtio_q_used_t      UsedRing;
    /** The notification offset for the queue. */
    uint32_t             offNotify;
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
    /** The notification offset multiplier. */
    uint32_t         u32NotifyOffMult;
    /** PCI bus where the device is located. */
    uint8_t          u8Bus;
    /** Device/Function number. */
    uint8_t          u8DevFn;
    /** Saved high bits of EAX. */
    uint16_t         saved_eax_hi;
    /** The current executed command structure. */
    virtio_scsi_req_hdr_t ScsiReqHdr;
    virtio_scsi_req_sts_t ScsiReqSts;
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

#define VIRTIO_SCSI_STS_RESPONSE_OK     0x00

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

static void virtio_reg_set_bar_offset_length(virtio_t __far *virtio, uint8_t u8Bar, uint32_t offReg, uint32_t cb)
{
    pci_write_config_byte(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff  +  4, u8Bar);
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff +  8, offReg);
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + 12, cb);
}

static void virtio_reg_common_access_prepare(virtio_t __far *virtio, uint16_t offReg, uint32_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_COMMON_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_COMMON_CFG - 1].u32Offset + offReg,
                                     cbAcc);
}

static void virtio_reg_dev_access_prepare(virtio_t __far *virtio, uint16_t offReg, uint32_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_DEVICE_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_DEVICE_CFG - 1].u32Offset + offReg,
                                     cbAcc);
}

static void virtio_reg_notify_access_prepare(virtio_t __far *virtio, uint16_t offReg, uint32_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_NOTIFY_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_NOTIFY_CFG - 1].u32Offset + offReg,
                                     cbAcc);
}

static void virtio_reg_isr_prepare(virtio_t __far *virtio, uint32_t cbAcc)
{
    virtio_reg_set_bar_offset_length(virtio,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_ISR_CFG - 1].u8Bar,
                                     virtio->aBarCfgs[VIRTIO_PCI_CAP_ISR_CFG - 1].u32Offset,
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

static void virtio_reg_common_write_u32(virtio_t __far *virtio, uint16_t offReg, uint32_t u32Val)
{
    virtio_reg_common_access_prepare(virtio, offReg, sizeof(uint32_t));
    pci_write_config_dword(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u32Val);
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

static void virtio_reg_notify_write_u16(virtio_t __far *virtio, uint16_t offReg, uint16_t u16Val)
{
    virtio_reg_notify_access_prepare(virtio, offReg, sizeof(uint16_t));
    pci_write_config_word(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t), u16Val);
}

static uint8_t virtio_reg_isr_read_u8(virtio_t __far *virtio)
{
    virtio_reg_isr_prepare(virtio, sizeof(uint8_t));
    return pci_read_config_byte(virtio->u8Bus, virtio->u8DevFn, virtio->u8PciCfgOff + sizeof(virtio_pci_cap_t));
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

int virtio_scsi_cmd_data_out(virtio_t __far *virtio, uint8_t idTgt, uint8_t __far *aCDB,
                            uint8_t cbCDB, uint8_t __far *buffer, uint32_t length)
{
    uint16_t idxUsedOld = virtio->Queue.UsedRing.idxNextUsed;

    _fmemset(&virtio->ScsiReqHdr, 0, sizeof(virtio->ScsiReqHdr));
    _fmemset(&virtio->ScsiReqSts, 0, sizeof(virtio->ScsiReqSts));

    virtio->ScsiReqHdr.au8Lun[0] = 0x1;
    virtio->ScsiReqHdr.au8Lun[1] = idTgt;
    virtio->ScsiReqHdr.au8Lun[2] = 0;
    virtio->ScsiReqHdr.au8Lun[3] = 0;
    _fmemcpy(&virtio->ScsiReqHdr.abCdb[0], aCDB, cbCDB);

    /* Fill in the descriptors. */
    virtio->Queue.aDescTbl[0].GCPhysBufLow  = virtio_addr_to_phys(&virtio->ScsiReqHdr);
    virtio->Queue.aDescTbl[0].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[0].cbBuf         = sizeof(virtio->ScsiReqHdr);
    virtio->Queue.aDescTbl[0].fFlags        = VIRTIO_Q_DESC_F_NEXT;
    virtio->Queue.aDescTbl[0].idxNext       = 1;

    virtio->Queue.aDescTbl[1].GCPhysBufLow  = virtio_addr_to_phys(buffer);
    virtio->Queue.aDescTbl[1].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[1].cbBuf         = length;
    virtio->Queue.aDescTbl[1].fFlags        = VIRTIO_Q_DESC_F_NEXT;
    virtio->Queue.aDescTbl[1].idxNext       = 2;

    virtio->Queue.aDescTbl[2].GCPhysBufLow  = virtio_addr_to_phys(&virtio->ScsiReqSts);
    virtio->Queue.aDescTbl[2].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[2].cbBuf         = sizeof(virtio->ScsiReqSts);
    virtio->Queue.aDescTbl[2].fFlags        = VIRTIO_Q_DESC_F_WRITE; /* End of chain. */
    virtio->Queue.aDescTbl[2].idxNext       = 0;

    /* Put it into the queue. */
    virtio->Queue.AvailRing.au16Ring[virtio->Queue.AvailRing.idxNextFree % VIRTIO_SCSI_RING_ELEM] = 0;
    virtio->Queue.AvailRing.idxNextFree++;

    /* Notify the device about the new command. */
    DBG_VIRTIO("VirtIO: Submitting new request, Queue.offNotify=0x%x\n", virtio->Queue.offNotify);
    virtio_reg_notify_write_u16(virtio, virtio->Queue.offNotify, VIRTIO_SCSI_Q_REQUEST);

    /* Wait for it to complete. */
    while (idxUsedOld == virtio->Queue.UsedRing.idxNextUsed);

    DBG_VIRTIO("VirtIO: Request complete u8Response=%u\n", virtio->ScsiReqSts.u8Response);

    /* Read ISR register to de-assert the interrupt, don't need to do anything with it. */
    virtio_reg_isr_read_u8(virtio);

    if (virtio->ScsiReqSts.u8Response != VIRTIO_SCSI_STS_RESPONSE_OK)
        return 4;

    return 0;
}

int virtio_scsi_cmd_data_in(virtio_t __far *virtio, uint8_t idTgt, uint8_t __far *aCDB,
                            uint8_t cbCDB, uint8_t __far *buffer, uint32_t length, uint16_t skip_a,
                            uint16_t skip_b)
{
    uint16_t idxUsedOld = virtio->Queue.UsedRing.idxNextUsed;
    uint8_t idxDesc = 0;

    _fmemset(&virtio->ScsiReqHdr, 0, sizeof(virtio->ScsiReqHdr));
    _fmemset(&virtio->ScsiReqSts, 0, sizeof(virtio->ScsiReqSts));

    virtio->ScsiReqHdr.au8Lun[0] = 0x1;
    virtio->ScsiReqHdr.au8Lun[1] = idTgt;
    virtio->ScsiReqHdr.au8Lun[2] = 0;
    virtio->ScsiReqHdr.au8Lun[3] = 0;
    _fmemcpy(&virtio->ScsiReqHdr.abCdb[0], aCDB, cbCDB);

    /* Fill in the descriptors. */
    virtio->Queue.aDescTbl[idxDesc].GCPhysBufLow  = virtio_addr_to_phys(&virtio->ScsiReqHdr);
    virtio->Queue.aDescTbl[idxDesc].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[idxDesc].cbBuf         = sizeof(virtio->ScsiReqHdr);
    virtio->Queue.aDescTbl[idxDesc].fFlags        = VIRTIO_Q_DESC_F_NEXT;
    virtio->Queue.aDescTbl[idxDesc].idxNext       = 1;
    idxDesc++;

    /* No data out buffer, the status comes right after this in the next descriptor. */
    virtio->Queue.aDescTbl[idxDesc].GCPhysBufLow  = virtio_addr_to_phys(&virtio->ScsiReqSts);
    virtio->Queue.aDescTbl[idxDesc].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[idxDesc].cbBuf         = sizeof(virtio->ScsiReqSts);
    virtio->Queue.aDescTbl[idxDesc].fFlags        = VIRTIO_Q_DESC_F_WRITE | VIRTIO_Q_DESC_F_NEXT;
    virtio->Queue.aDescTbl[idxDesc].idxNext       = 2;
    idxDesc++;

    /* Prepend a sinkhole if data is skipped upfront. */
    if (skip_b)
    {
        virtio->Queue.aDescTbl[idxDesc].GCPhysBufLow  = 0; /* See ahci.c:sink_buf_phys */
        virtio->Queue.aDescTbl[idxDesc].GCPhysBufHigh = 0;
        virtio->Queue.aDescTbl[idxDesc].cbBuf         = skip_b;
        virtio->Queue.aDescTbl[idxDesc].fFlags        = VIRTIO_Q_DESC_F_WRITE | VIRTIO_Q_DESC_F_NEXT;
        virtio->Queue.aDescTbl[idxDesc].idxNext       = idxDesc + 1;
        idxDesc++;
    }

    virtio->Queue.aDescTbl[idxDesc].GCPhysBufLow  = virtio_addr_to_phys(buffer);
    virtio->Queue.aDescTbl[idxDesc].GCPhysBufHigh = 0;
    virtio->Queue.aDescTbl[idxDesc].cbBuf         = length;
    virtio->Queue.aDescTbl[idxDesc].fFlags        = VIRTIO_Q_DESC_F_WRITE; /* End of chain. */
    virtio->Queue.aDescTbl[idxDesc].idxNext       = skip_a ? idxDesc + 1 : 0;
    idxDesc++;

    /* Append a sinkhole if data is skipped at the end. */
    if (skip_a)
    {
        virtio->Queue.aDescTbl[idxDesc - 1].fFlags  |= VIRTIO_Q_DESC_F_NEXT;
        virtio->Queue.aDescTbl[idxDesc - 1].idxNext = idxDesc;

        virtio->Queue.aDescTbl[idxDesc].GCPhysBufLow  = 0; /* See ahci.c:sink_buf_phys */
        virtio->Queue.aDescTbl[idxDesc].GCPhysBufHigh = 0;
        virtio->Queue.aDescTbl[idxDesc].cbBuf         = skip_a;
        virtio->Queue.aDescTbl[idxDesc].fFlags        = VIRTIO_Q_DESC_F_WRITE; /* End of chain. */
        virtio->Queue.aDescTbl[idxDesc].idxNext       = 0;
    }

    /* Put it into the queue, the index is supposed to be free-running and clipped to the ring size
     * internally. The free running index is what the driver sees. */
    virtio->Queue.AvailRing.au16Ring[virtio->Queue.AvailRing.idxNextFree % VIRTIO_SCSI_RING_ELEM] = 0;
    virtio->Queue.AvailRing.idxNextFree++;

    /* Notify the device about the new command. */
    DBG_VIRTIO("VirtIO: Submitting new request, Queue.offNotify=0x%x\n", virtio->Queue.offNotify);
    virtio_reg_notify_write_u16(virtio, virtio->Queue.offNotify, VIRTIO_SCSI_Q_REQUEST);

    /* Wait for it to complete. */
    while (idxUsedOld == virtio->Queue.UsedRing.idxNextUsed);

    DBG_VIRTIO("VirtIO: Request complete u8Response=%u\n", virtio->ScsiReqSts.u8Response);

    /* Read ISR register to de-assert the interrupt, don't need to do anything with it. */
    virtio_reg_isr_read_u8(virtio);

    if (virtio->ScsiReqSts.u8Response != VIRTIO_SCSI_STS_RESPONSE_OK)
        return 4;

    return 0;
}

/**
 * Read sectors from an attached VirtIO SCSI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the
 *                      EBDA).
 */
int virtio_scsi_read_sectors(bio_dsk_t __far *bios_dsk)
{
    uint8_t             rc;
    cdb_rw16            cdb;
    uint32_t            count;
    uint8_t             target_id;
    uint8_t             device_id;

    device_id = VBOX_GET_SCSI_DEVICE(bios_dsk->drqp.dev_id);
    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("%s: device_id out of range %d\n", __func__, device_id);

    count    = bios_dsk->drqp.nsect;

    high_bits_save(bios_dsk->virtio_seg :> 0);
    /* Prepare a CDB. */
    cdb.command = SCSI_READ_16;
    cdb.lba     = swap_64(bios_dsk->drqp.lba);
    cdb.pad1    = 0;
    cdb.nsect32 = swap_32(count);
    cdb.pad2    = 0;


    target_id = bios_dsk->scsidev[device_id].target_id;

    DBG_VIRTIO("%s: reading %u sectors, device %d, target %d\n", __func__,
             count, device_id, bios_dsk->scsidev[device_id].target_id);

    rc = virtio_scsi_cmd_data_in(bios_dsk->virtio_seg :> 0, target_id, (void __far *)&cdb, 16,
                                 bios_dsk->drqp.buffer, (count * 512L), 0, 0);

    if (!rc)
    {
        bios_dsk->drqp.trsfsectors = count;
        bios_dsk->drqp.trsfbytes   = count * 512L;
    }
    DBG_VIRTIO("%s: transferred %u sectors\n", __func__, bios_dsk->drqp.nsect);
    high_bits_restore(bios_dsk->virtio_seg :> 0);

    return rc;
}

/**
 * Write sectors to an attached VirtIO SCSI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the
 *                      EBDA).
 */
int virtio_scsi_write_sectors(bio_dsk_t __far *bios_dsk)
{
    uint8_t             rc;
    cdb_rw16            cdb;
    uint32_t            count;
    uint8_t             target_id;
    uint8_t             device_id;

    device_id = VBOX_GET_SCSI_DEVICE(bios_dsk->drqp.dev_id);
    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("%s: device_id out of range %d\n", __func__, device_id);

    count    = bios_dsk->drqp.nsect;

    high_bits_save(bios_dsk->virtio_seg :> 0);

    /* Prepare a CDB. */
    cdb.command = SCSI_WRITE_16;
    cdb.lba     = swap_64(bios_dsk->drqp.lba);
    cdb.pad1    = 0;
    cdb.nsect32 = swap_32(count);
    cdb.pad2    = 0;

    target_id = bios_dsk->scsidev[device_id].target_id;

    DBG_VIRTIO("%s: writing %u sectors, device %d, target %d\n", __func__,
             count, device_id, bios_dsk->scsidev[device_id].target_id);

    rc = virtio_scsi_cmd_data_out(bios_dsk->virtio_seg :> 0, target_id, (void __far *)&cdb, 16,
                                  bios_dsk->drqp.buffer, (count * 512L));

    if (!rc)
    {
        bios_dsk->drqp.trsfsectors = count;
        bios_dsk->drqp.trsfbytes   = (count * 512L);
    }
    DBG_VIRTIO("%s: transferred %u sectors\n", __func__, bios_dsk->drqp.nsect);
    high_bits_restore(bios_dsk->virtio_seg :> 0);

    return rc;
}

#define ATA_DATA_OUT     0x02

/**
 * Perform a "packet style" read with supplied CDB.
 *
 * @returns status code.
 * @param   device_id   ID of the device to access.
 * @param   cmdlen      Length of the CDB.
 * @param   cmdbuf      The CDB buffer.
 * @param   skip_b      How much to skip before reading into the provided data buffer.
 * @param   length      How much to transfer.
 * @param   inout       Read/Write direction indicator.
 * @param   buffer      Data buffer to store the data from the device in.
 */
uint16_t virtio_scsi_cmd_packet(uint16_t device_id, uint8_t cmdlen, char __far *cmdbuf,
                                uint16_t skip_b, uint32_t length, uint8_t inout, char __far *buffer)
{
    bio_dsk_t __far *bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;
    uint8_t         rc;
    uint8_t         target_id;

    /* Data out is currently not supported. */
    if (inout == ATA_DATA_OUT) {
        BX_INFO("%s: DATA_OUT not supported yet\n", __func__);
        return 1;
    }

    /* The skip length must be even. */
    if (skip_b & 1) {
        DBG_VIRTIO("%s: skip must be even (%04x)\n", __func__, skip_b);
        return 1;
    }

    /* Convert to SCSI specific device number. */
    device_id = VBOX_GET_SCSI_DEVICE(device_id);

    DBG_VIRTIO("%s: reading %lu bytes, skip %u/%u, device %d, target %d\n", __func__,
               length, bios_dsk->drqp.skip_b, bios_dsk->drqp.skip_a,
               device_id, bios_dsk->scsidev[device_id].target_id);
    DBG_VIRTIO("%s: reading %u %u-byte sectors\n", __func__,
               bios_dsk->drqp.nsect, bios_dsk->drqp.sect_sz);

    high_bits_save(bios_dsk->virtio_seg :> 0);
    target_id = bios_dsk->scsidev[device_id].target_id;

    bios_dsk->drqp.lba     = length << 8;     /// @todo xfer length limit
    bios_dsk->drqp.buffer  = buffer;
    bios_dsk->drqp.nsect   = length / bios_dsk->drqp.sect_sz;

    DBG_VIRTIO("%s: reading %u bytes, device %d, target %d\n", __func__,
               length, device_id, bios_dsk->scsidev[device_id].target_id);

    rc = virtio_scsi_cmd_data_in(bios_dsk->virtio_seg :> 0, target_id, (void __far *)cmdbuf, cmdlen,
                                 bios_dsk->drqp.buffer, length, skip_b, bios_dsk->drqp.skip_a);
    if (!rc)
        bios_dsk->drqp.trsfbytes = length;

    DBG_VIRTIO("%s: transferred %u bytes\n", __func__, length);
    high_bits_restore(bios_dsk->virtio_seg :> 0);

    return rc;
}

static int virtio_scsi_detect_devices(virtio_t __far *virtio)
{
    int                 i;
    uint8_t             buffer[0x0200];
    bio_dsk_t __far     *bios_dsk;

    bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;

    /* Go through target devices. */
    for (i = 0; i < VBSCSI_MAX_DEVICES; i++)
    {
        uint8_t     rc;
        uint8_t     aCDB[16];
        uint8_t     hd_index, devcount_scsi;

        aCDB[0] = SCSI_INQUIRY;
        aCDB[1] = 0;
        aCDB[2] = 0;
        aCDB[3] = 0;
        aCDB[4] = 5; /* Allocation length. */
        aCDB[5] = 0;

        _fmemset(buffer, 0, sizeof(buffer));

        rc = virtio_scsi_cmd_data_in(virtio, i, aCDB, 6, buffer, 5, 0, 0);
        if (!rc)
        {
            devcount_scsi = bios_dsk->scsi_devcount;

            /* Check the attached device. */
            if (   ((buffer[0] & 0xe0) == 0)
                && ((buffer[0] & 0x1f) == 0x00))
            {
                DBG_VIRTIO("%s: Disk detected at %d\n", __func__, i);

                /* We add the disk only if the maximum is not reached yet. */
                if (devcount_scsi < BX_MAX_SCSI_DEVICES)
                {
                    uint64_t    sectors, t;
                    uint32_t    sector_size, cylinders;
                    uint16_t    heads, sectors_per_track;
                    uint8_t     hdcount;
                    uint8_t     cmos_base;

                    /* Issue a read capacity command now. */
                    _fmemset(aCDB, 0, sizeof(aCDB));
                    aCDB[0] = SCSI_SERVICE_ACT;
                    aCDB[1] = SCSI_READ_CAP_16;
                    aCDB[13] = 32; /* Allocation length. */

                    rc = virtio_scsi_cmd_data_in(virtio, i, aCDB, 16, buffer, 32, 0, 0);
                    if (rc != 0)
                        BX_PANIC("%s: SCSI_READ_CAPACITY failed\n", __func__);

                    /* The value returned is the last addressable LBA, not
                     * the size, which what "+ 1" is for.
                     */
                    sectors = swap_64(*(uint64_t *)buffer) + 1;

                    sector_size =   ((uint32_t)buffer[8] << 24)
                                  | ((uint32_t)buffer[9] << 16)
                                  | ((uint32_t)buffer[10] << 8)
                                  | ((uint32_t)buffer[11]);

                    /* We only support the disk if sector size is 512 bytes. */
                    if (sector_size != 512)
                    {
                        /* Leave a log entry. */
                        BX_INFO("Disk %d has an unsupported sector size of %u\n", i, sector_size);
                        continue;
                    }

                    /* Get logical CHS geometry. */
                    switch (devcount_scsi)
                    {
                        case 0:
                            cmos_base = 0x90;
                            break;
                        case 1:
                            cmos_base = 0x98;
                            break;
                        case 2:
                            cmos_base = 0xA0;
                            break;
                        case 3:
                            cmos_base = 0xA8;
                            break;
                        default:
                            cmos_base = 0;
                    }

                    if (cmos_base && inb_cmos(cmos_base + 7))
                    {
                        /* If provided, grab the logical geometry from CMOS. */
                        cylinders         = inb_cmos(cmos_base + 0) + (inb_cmos(cmos_base + 1) << 8);
                        heads             = inb_cmos(cmos_base + 2);
                        sectors_per_track = inb_cmos(cmos_base + 7);
                    }
                    else
                    {
                        /* Calculate default logical geometry. NB: Very different
                         * from default ATA/SATA logical geometry!
                         */
                        if (sectors >= (uint32_t)4 * 1024 * 1024)
                        {
                            heads = 255;
                            sectors_per_track = 63;
                            /* Approximate x / (255 * 63) using shifts */
                            t = (sectors >> 6) + (sectors >> 12);
                            cylinders = (t >> 8) + (t >> 16);
                        }
                        else if (sectors >= (uint32_t)2 * 1024 * 1024)
                        {
                            heads = 128;
                            sectors_per_track = 32;
                            cylinders = sectors >> 12;
                        }
                        else
                        {
                            heads = 64;
                            sectors_per_track = 32;
                            cylinders = sectors >> 11;
                        }
                    }

                    /* Calculate index into the generic disk table. */
                    hd_index = devcount_scsi + BX_MAX_ATA_DEVICES;

                    bios_dsk->scsidev[devcount_scsi].target_id = i;
                    bios_dsk->devices[hd_index].type        = DSK_TYPE_VIRTIO_SCSI;
                    bios_dsk->devices[hd_index].device      = DSK_DEVICE_HD;
                    bios_dsk->devices[hd_index].removable   = 0;
                    bios_dsk->devices[hd_index].lock        = 0;
                    bios_dsk->devices[hd_index].blksize     = sector_size;
                    bios_dsk->devices[hd_index].translation = GEO_TRANSLATION_LBA;

                    /* Write LCHS/PCHS values. */
                    bios_dsk->devices[hd_index].lchs.heads = heads;
                    bios_dsk->devices[hd_index].lchs.spt   = sectors_per_track;
                    bios_dsk->devices[hd_index].pchs.heads = heads;
                    bios_dsk->devices[hd_index].pchs.spt   = sectors_per_track;

                    if (cylinders > 1024) {
                        bios_dsk->devices[hd_index].lchs.cylinders = 1024;
                        bios_dsk->devices[hd_index].pchs.cylinders = 1024;
                    } else {
                        bios_dsk->devices[hd_index].lchs.cylinders = (uint16_t)cylinders;
                        bios_dsk->devices[hd_index].pchs.cylinders = (uint16_t)cylinders;
                    }

                    BX_INFO("SCSI %d-ID#%d: LCHS=%lu/%u/%u 0x%llx sectors\n", devcount_scsi,
                            i, (uint32_t)cylinders, heads, sectors_per_track, sectors);

                    bios_dsk->devices[hd_index].sectors = sectors;

                    /* Store the id of the disk in the ata hdidmap. */
                    hdcount = bios_dsk->hdcount;
                    bios_dsk->hdidmap[hdcount] = devcount_scsi + BX_MAX_ATA_DEVICES;
                    hdcount++;
                    bios_dsk->hdcount = hdcount;

                    /* Update hdcount in the BDA. */
                    hdcount = read_byte(0x40, 0x75);
                    hdcount++;
                    write_byte(0x40, 0x75, hdcount);

                    devcount_scsi++;
                }
                else
                {
                    /* We reached the maximum of SCSI disks we can boot from. We can quit detecting. */
                    break;
                }
            }
            else if (   ((buffer[0] & 0xe0) == 0)
                     && ((buffer[0] & 0x1f) == 0x05))
            {
                uint8_t     cdcount;
                uint8_t     removable;

                BX_INFO("SCSI %d-ID#%d: CD/DVD-ROM\n", devcount_scsi, i);

                /* Calculate index into the generic device table. */
                hd_index = devcount_scsi + BX_MAX_ATA_DEVICES;

                removable = buffer[1] & 0x80 ? 1 : 0;

                bios_dsk->scsidev[devcount_scsi].target_id = i;
                bios_dsk->devices[hd_index].type        = DSK_TYPE_VIRTIO_SCSI;
                bios_dsk->devices[hd_index].device      = DSK_DEVICE_CDROM;
                bios_dsk->devices[hd_index].removable   = removable;
                bios_dsk->devices[hd_index].blksize     = 2048;
                bios_dsk->devices[hd_index].translation = GEO_TRANSLATION_NONE;

                /* Store the ID of the device in the BIOS cdidmap. */
                cdcount = bios_dsk->cdcount;
                bios_dsk->cdidmap[cdcount] = devcount_scsi + BX_MAX_ATA_DEVICES;
                cdcount++;
                bios_dsk->cdcount = cdcount;

                devcount_scsi++;
            }
            else
                DBG_VIRTIO("%s: No supported device detected at %d\n", __func__, i);
        }
        else
            DBG_VIRTIO("%s: INQUIRY failed with %u\n", __func__, rc);

        bios_dsk->scsi_devcount = devcount_scsi;
    }

    return 0;
}

/**
 * Initializes the VirtIO SCSI HBA and detects attached devices.
 */
static int virtio_scsi_hba_init(uint8_t u8Bus, uint8_t u8DevFn, uint8_t u8PciCapOffVirtIo)
{
    uint8_t             u8PciCapOff;
    uint16_t            ebda_seg;
    uint16_t            virtio_seg;
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
                    if (u8PciVirtioCfg == VIRTIO_PCI_CAP_NOTIFY_CFG)
                    {
                        virtio->u32NotifyOffMult = pci_read_config_dword(u8Bus, u8DevFn, u8PciCapOff + 16);
                        DBG_VIRTIO("VirtIO: u32NotifyOffMult 0x%x\n", virtio->u32NotifyOffMult);
                    }
                    break;
                }
                case VIRTIO_PCI_CAP_PCI_CFG:
                    virtio->u8PciCfgOff = u8PciCapOff;
                    DBG_VIRTIO("VirtIO PCI CAP window offset: %x\n", u8PciCapOff);
                    break;
                default:
                    DBG_VIRTIO("VirtIO SCSI HBA with unknown PCI capability type 0x%x\n", u8PciVirtioCfg);
                    break;
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
    /* Our driver knows how to operate the device. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_DRV;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);

#if 0
    /* Read the feature bits and only program the VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT bit if available. */
    fFeatures = virtio_reg_common_read_u32(virtio, VIRTIO_COMMON_REG_DEV_FEAT);
    fFeatures &= VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT;
#endif

    /* Check that the device is sane. */
    if (   virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_Q_NUM) < 1
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_CDB_SZ) < 16
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_SENSE_SZ) < 32
        || virtio_reg_dev_cfg_read_u32(virtio, VIRTIO_DEV_CFG_REG_SECT_MAX) < 1)
    {
        DBG_VIRTIO("VirtIO-SCSI: Invalid SCSI device configuration, ignoring device\n");
        return 0;
    }

    virtio_reg_common_write_u32(virtio, VIRTIO_COMMON_REG_DRV_FEAT, VIRTIO_CMN_REG_DEV_FEAT_SCSI_INOUT);

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
    virtio_reg_common_write_u16(virtio, VIRTIO_COMMON_REG_Q_SIZE, VIRTIO_SCSI_RING_ELEM);
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

    DBG_VIRTIO("VirtIO: Q notify offset 0x%x\n", virtio_reg_common_read_u16(virtio, VIRTIO_COMMON_REG_Q_NOTIFY_OFF));
    virtio->Queue.offNotify = virtio_reg_common_read_u16(virtio, VIRTIO_COMMON_REG_Q_NOTIFY_OFF) * virtio->u32NotifyOffMult;

    /* Bring the device into operational mode. */
    u8DevStat |= VIRTIO_CMN_REG_DEV_STS_F_DRV_OK;
    virtio_reg_common_write_u8(virtio, VIRTIO_COMMON_REG_DEV_STS, u8DevStat);

    return virtio_scsi_detect_devices(virtio);
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

            /* Enable PCI memory, I/O, bus mastering access in command register. */
            pci_write_config_word(u8Bus, u8DevFn, 4, 0x7);

            rc = virtio_scsi_hba_init(u8Bus, u8DevFn, u8PciCapOffVirtIo);
        }
        else
            DBG_VIRTIO("VirtIO SCSI HBA with no usable PCI config access!\n");
    }
    else
        DBG_VIRTIO("No VirtIO SCSI HBA!\n");
}
