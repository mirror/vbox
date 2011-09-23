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
#define ATA_TYPE_NONE     0x00
#define ATA_TYPE_UNKNOWN  0x01
#define ATA_TYPE_ATA      0x02
#define ATA_TYPE_ATAPI    0x03
#define ATA_TYPE_SCSI     0x04 // SCSI disk

#define ATA_DEVICE_NONE  0x00
#define ATA_DEVICE_HD    0xFF
#define ATA_DEVICE_CDROM 0x05


#if 1 //BX_USE_ATADRV

//@todo: does the struct really have to be misaligned?
#pragma pack(0)

typedef struct {
    uint16_t    heads;      // # heads
    uint16_t    cylinders;  // # cylinders
    uint16_t    spt;        // # sectors / track
} chs_t;

  // DPTE definition
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

typedef struct {
    uint8_t     iface;        // ISA or PCI
    uint16_t    iobase1;      // IO Base 1
    uint16_t    iobase2;      // IO Base 2
    uint8_t     irq;          // IRQ
} ata_channel_t;

typedef struct {
    uint8_t     type;         // Detected type of ata (ata/atapi/none/unknown/scsi)
    uint8_t     device;       // Detected type of attached devices (hd/cd/none)
    uint8_t     removable;    // Removable device flag
    uint8_t     lock;         // Locks for removable devices
    uint8_t     mode;         // transfer mode : PIO 16/32 bits - IRQ - ISADMA - PCIDMA
    uint16_t    blksize;      // block size

    uint8_t     translation;  // type of translation
    chs_t       lchs;         // Logical CHS
    chs_t       pchs;         // Physical CHS

    uint32_t    sectors;      // Total sectors count
} ata_device_t;

typedef struct {
    // ATA channels info
    ata_channel_t channels[BX_MAX_ATA_INTERFACES];

    // ATA devices info
    ata_device_t  devices[BX_MAX_ATA_DEVICES];
    //
    // map between (bios hd id - 0x80) and ata channels and scsi disks.
    uint8_t     hdcount, hdidmap[BX_MAX_STORAGE_DEVICES];

    // map between (bios cd id - 0xE0) and ata channels
    uint8_t     cdcount, cdidmap[BX_MAX_STORAGE_DEVICES];

    // Buffer for DPTE table
    dpte_t      dpte;

    // Count of transferred sectors and bytes
    uint16_t    trsfsectors;
    uint32_t    trsfbytes;
} ata_t;

#if BX_ELTORITO_BOOT
  // ElTorito Device Emulation data
typedef struct {
    uint8_t     active;
    uint8_t     media;
    uint8_t     emulated_drive;
    uint8_t     controller_index;
    uint16_t    device_spec;
    uint32_t    ilba;
    uint16_t    buffer_segment;
    uint16_t    load_segment;
    uint16_t    sector_count;

    // Virtual device
    chs_t       vdevice;
} cdemu_t;
#endif // BX_ELTORITO_BOOT

#ifdef VBOX_WITH_SCSI
typedef struct {
    // I/O port this device is attached to.
    uint16_t        io_base;
    // Target Id.
    uint8_t         target_id;
    // SCSI devices info
    ata_device_t    device_info;
} scsi_device_t;

typedef struct {
    // SCSI device info
    scsi_device_t   devices[BX_MAX_SCSI_DEVICES];
    // Number of scsi disks.
    uint8_t         hdcount;
} scsi_t;
#endif

#ifdef VBOX_WITH_BIOS_AHCI
//typedef struct {
//    uint16_t    iobase;
//} ahci_t;
#endif

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

// for access to EBDA area
//     The EBDA structure should conform to
//     http://www.frontiernet.net/~fys/rombios.htm document
//     I made the ata and cdemu structs begin at 0x121 in the EBDA seg
/* MS-DOS KEYB.COM may overwrite the word at offset 0x117 in the EBDA. */
typedef struct {
    unsigned char filler1[0x3D];

    // FDPT - Can be split into data members if needed
    fdpt_t      fdpt0;
    fdpt_t      fdpt1;

#if 0
    unsigned char filler2[0xC4];
#else
    unsigned char filler2[0xC2];
    uint16_t    ahci_seg;
#endif

    // ATA Driver data
    ata_t       ata;

#if BX_ELTORITO_BOOT
    // El Torito Emulation data
    cdemu_t     cdemu;
#endif // BX_ELTORITO_BOOT

#ifdef VBOX_WITH_SCSI
    // SCSI Driver data
    scsi_t      scsi;
# endif

#ifdef VBOX_WITH_BIOS_AHCI
//    ahci_t      ahci;
//    uint16_t    ahci_seg;    //@todo: Someone is trashing the data here!?!
#endif

    unsigned char   uForceBootDrive;
    unsigned char   uForceBootDevice;
} ebda_data_t;

#pragma pack()

// the last 16 bytes of the EBDA segment are used for the MPS floating
// pointer structure (though only if an I/O APIC is present)

#define EbdaData ((ebda_data_t *) 0)

// @todo: put this elsewhere (and change/eliminate?)
#define SET_DISK_RET_STATUS(status) write_byte(0x0040, 0x0074, status)

#endif
