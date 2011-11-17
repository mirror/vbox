/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
 *  ROM BIOS for use with Bochs/Plex86/QEMU emulation environment
 *
 *  Copyright (C) 2002  MandrakeSoft S.A.
 *
 *    MandrakeSoft S.A.
 *    43, rue d'Aboukir
 *    75002 Paris - France
 *    http://www.linux-mandrake.com/
 *    http://www.mandrakesoft.com/
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 *
 */


#include <stdint.h>

/* Must be defined here (EBDA structures depend on these). */
#define BX_MAX_ATA_INTERFACES   4
#define BX_MAX_ATA_DEVICES      (BX_MAX_ATA_INTERFACES*2)

#define BX_USE_ATADRV           1
#define BX_ELTORITO_BOOT        1
#define BX_APM                  1

#ifdef VBOX_WITH_SCSI
/* Enough for now */
#    define BX_MAX_SCSI_DEVICES 4
#    define BX_MAX_STORAGE_DEVICES (BX_MAX_ATA_DEVICES + BX_MAX_SCSI_DEVICES)

/* A SCSI device starts always at BX_MAX_ATA_DEVICES. */
#    define VBOX_IS_SCSI_DEVICE(device_id) (device_id >= BX_MAX_ATA_DEVICES)
#    define VBOX_GET_SCSI_DEVICE(device_id) (device_id - BX_MAX_ATA_DEVICES)
#else
#    define BX_MAX_STORAGE_DEVICES  BX_MAX_ATA_DEVICES
#endif

/* Generic storage device types. Bit of a misnomer! */
#define ATA_TYPE_NONE       0x00
#define ATA_TYPE_UNKNOWN    0x01
#define ATA_TYPE_ATA        0x02
#define ATA_TYPE_ATAPI      0x03
#define ATA_TYPE_SCSI       0x04 // SCSI disk

#define ATA_DEVICE_NONE     0x00
#define ATA_DEVICE_HD       0xFF
#define ATA_DEVICE_CDROM    0x05


#if 1 //BX_USE_ATADRV

/* Note: The DPTE and FDPT structures are industry standards and
 * may not be modified. The other disk-related structures are
 * internal to the BIOS.
 */

/* Translated DPT (Device Parameter Table). */
typedef struct {
    uint16_t    iobase1;
    uint16_t    iobase2;
    uint8_t     prefix;
    uint8_t     unused;
    uint8_t     irq;
    uint8_t     blkcount;
    uint8_t     dma;
    uint8_t     pio;
    uint16_t    options;
    uint16_t    reserved;
    uint8_t     revision;
    uint8_t     checksum;
} dpte_t;

ct_assert(sizeof(dpte_t) == 16);    /* Ensure correct size. */

#pragma pack(0)

/* FDPT - Fixed Disk Parameter Table. PC/AT compatible; note
 * that this structure is slightly misaligned.
 */
typedef struct {
    uint16_t    lcyl;
    uint8_t     lhead;
    uint8_t     sig;
    uint8_t     spt;
    uint8_t     resvd1[4];
    uint16_t    cyl;
    uint8_t     head;
    uint8_t     resvd2[2];
    uint8_t     lspt;
    uint8_t     csum;
} fdpt_t;

#pragma pack()

ct_assert(sizeof(fdpt_t) == 16);    /* Ensure correct size. */


/* C/H/S geometry information. */
typedef struct {
    uint16_t    heads;      /* Number of heads. */
    uint16_t    cylinders;  /* Number of cylinders. */
    uint16_t    spt;        /* Number of sectors per track. */
} chs_t;

/* IDE/ATA specific device information. */
typedef struct {
    uint8_t     iface;      /* ISA or PCI. */
    uint8_t     irq;        /* IRQ (on the PIC). */
    uint16_t    iobase1;    /* I/O base 1. */
    uint16_t    iobase2;    /* I/O base 2. */
} ata_chan_t;

#ifdef VBOX_WITH_SCSI

/* SCSI specific device information. */
typedef struct {
    uint16_t    io_base;        /* Port base for HBA communication. */
    uint8_t     target_id;      /* Target ID. */
} scsi_dev_t;

#endif

/* Generic disk information. */
typedef struct {
    uint8_t     type;         /* Device type (ATA/ATAPI/SCSI/none/unknown). */
    uint8_t     device;       /* Detected type of attached device (HD/CD/none). */
    uint8_t     removable;    /* Removable device flag. */
    uint8_t     lock;         /* Lock count for removable devices. */
    uint8_t     mode;         /* Transfer mode: PIO 16/32 bits - IRQ - ISADMA - PCIDMA. */
    uint8_t     translation;  /* Type of geometry translation. */
    uint16_t    blksize;      /* Disk block size. */
    chs_t       lchs;         /* Logical CHS geometry. */
    chs_t       pchs;         /* Physical CHS geometry. */
    uint32_t    sectors;      /* Total sector count. */
} disk_dev_t;

typedef struct {
    /* ATA bus-specific device information. */
    ata_chan_t  channels[BX_MAX_ATA_INTERFACES];

    /* Bus-independent disk device information. */
    disk_dev_t  devices[BX_MAX_ATA_DEVICES + BX_MAX_SCSI_DEVICES];

    uint8_t     hdcount;            /* Number of ATA disks. */
    /* Map between (BIOS disk ID - 0x80) and ATA/SCSI disks. */
    uint8_t     hdidmap[BX_MAX_STORAGE_DEVICES];

    uint8_t     cdcount;            /* Number of CD-ROMs. */
    /* Map between (BIOS CD-ROM ID - 0xE0) and ATA channels. */
    uint8_t     cdidmap[BX_MAX_STORAGE_DEVICES];

#ifdef VBOX_WITH_SCSI
    /* SCSI bus-specific device information. */
    scsi_dev_t  scsidev[BX_MAX_SCSI_DEVICES];
    uint8_t     scsi_hdcount;       /* Number of SCSI disks. */
#endif

    dpte_t      dpte;               /* Buffer for building a DPTE. */
    uint16_t    trsfsectors;        /* Count of sectors transferred. */
    uint32_t    trsfbytes;          /* Count of bytes transferred. */
} bio_dsk_t;

#if BX_ELTORITO_BOOT
/* El Torito device emulation state. */
typedef struct {
    uint8_t     active;
    uint8_t     media;
    uint8_t     emulated_drive;
    uint8_t     controller_index;
    uint16_t    device_spec;
    uint16_t    buffer_segment;
    uint32_t    ilba;
    uint16_t    load_segment;
    uint16_t    sector_count;
    chs_t       vdevice;        /* Virtual device geometry. */
} cdemu_t;
#endif

// for access to EBDA area
//     The EBDA structure should conform to
//     http://www.frontiernet.net/~fys/rombios.htm document
//     I made the ata and cdemu structs begin at 0x121 in the EBDA seg
/* MS-DOS KEYB.COM may overwrite the word at offset 0x117 in the EBDA
 * which contains the keyboard ID for PS/2 BIOSes.
 */
typedef struct {
    uint8_t     filler1[0x3D];

    fdpt_t      fdpt0;      /* FDPTs for the first two ATA disks. */
    fdpt_t      fdpt1;

#if 0
    uint8_t     filler2[0xC4];
#else
    uint8_t     filler2[0xC2];
    uint16_t    ahci_seg;
#endif

    bio_dsk_t   bdisk;      /* Disk driver data (ATA/SCSI/AHCI). */

#if BX_ELTORITO_BOOT
    cdemu_t     cdemu;      /* El Torito floppy/HD emulation data. */
#endif

#ifdef VBOX_WITH_BIOS_AHCI
//    ahci_t      ahci;
//    uint16_t    ahci_seg;    //@todo: Someone is trashing the data here!?!
#endif

    unsigned char   uForceBootDrive;
    unsigned char   uForceBootDevice;
} ebda_data_t;

ct_assert(sizeof(ebda_data_t) < 0x400);     /* Must be under 1K in size. */

// the last 16 bytes of the EBDA segment are used for the MPS floating
// pointer structure (though only if an I/O APIC is present)

#define EbdaData ((ebda_data_t *) 0)

// @todo: put this elsewhere (and change/eliminate?)
#define SET_DISK_RET_STATUS(status) write_byte(0x0040, 0x0074, status)

#endif
