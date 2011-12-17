/* $Id$ */
/** @file
 * SCSI host adapter driver to boot from SCSI disks
 */

/*
 * Copyright (C) 2004-2011 Oracle Corporation
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
#include "inlines.h"
#include "ebda.h"


//#define VBOX_SCSI_DEBUG 1 /* temporary */

#ifdef VBOX_SCSI_DEBUG
# define VBSCSI_DEBUG(...)    BX_INFO(__VA_ARGS__)
#else
# define VBSCSI_DEBUG(...)
#endif

#define VBSCSI_BUSY (1 << 0)

/* The I/O port of the BusLogic SCSI adapter. */
#define BUSLOGIC_ISA_IO_PORT 0x330
/* The I/O port of the LsiLogic SCSI adapter. */
#define LSILOGIC_ISA_IO_PORT 0x340
/* The I/O port of the LsiLogic SAS adapter. */
#define LSILOGIC_SAS_ISA_IO_PORT 0x350

#define VBSCSI_REGISTER_STATUS   0
#define VBSCSI_REGISTER_COMMAND  0
#define VBSCSI_REGISTER_DATA_IN  1
#define VBSCSI_REGISTER_IDENTIFY 2
#define VBSCSI_REGISTER_RESET    3

#define VBSCSI_MAX_DEVICES 16 /* Maximum number of devices a SCSI device can have. */

/* Command opcodes. */
#define SCSI_INQUIRY       0x12
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ_10       0x28
#define SCSI_WRITE_10      0x2a

/* Data transfer direction. */
#define SCSI_TXDIR_FROM_DEVICE 0
#define SCSI_TXDIR_TO_DEVICE   1

#pragma pack(1)

/* READ_10/WRITE_10 CDB layout. */
typedef struct {
    uint16_t    command;    /* Command. */
    uint32_t    lba;        /* LBA, MSB first! */
    uint8_t     pad1;       /* Unused. */
    uint16_t    nsect;      /* Sector count, MSB first! */
    uint8_t     pad2;       /* Unused. */
} cdb_rw10;

#pragma pack()

ct_assert(sizeof(cdb_rw10) == 10);

int scsi_cmd_data_in(uint16_t io_base, uint8_t device_id, uint8_t __far *aCDB,
                     uint8_t cbCDB, uint8_t __far *buffer, uint16_t cbBuffer)
{
    /* Check that the adapter is ready. */
    uint8_t     status;
    uint16_t    i;

    do
    {
        status = inb(io_base+VBSCSI_REGISTER_STATUS);
    } while (status & VBSCSI_BUSY);

    /* Write target ID. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, device_id);
    /* Write transfer direction. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, SCSI_TXDIR_FROM_DEVICE);
    /* Write the CDB size. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, cbCDB);
    /* Write buffer size. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, cbBuffer);
    outb(io_base+VBSCSI_REGISTER_COMMAND, (cbBuffer >> 8));
    /* Write the CDB. */
    for (i = 0; i < cbCDB; i++)
        outb(io_base+VBSCSI_REGISTER_COMMAND, aCDB[i]);

    /* Now wait for the command to complete. */
    do
    {
        status = inb(io_base+VBSCSI_REGISTER_STATUS);
    } while (status & VBSCSI_BUSY);

    /* Get the read data. */
    rep_insb(buffer, cbBuffer, io_base + VBSCSI_REGISTER_DATA_IN);

    return 0;
}

int scsi_cmd_data_out(uint16_t io_base, uint8_t device_id, uint8_t __far *aCDB,
                      uint8_t cbCDB, uint8_t __far *buffer, uint16_t cbBuffer)
{
    /* Check that the adapter is ready. */
    uint8_t     status;
    uint16_t    i;

    do
    {
        status = inb(io_base+VBSCSI_REGISTER_STATUS);
    } while (status & VBSCSI_BUSY);

    /* Write target ID. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, device_id);
    /* Write transfer direction. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, SCSI_TXDIR_TO_DEVICE);
    /* Write the CDB size. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, cbCDB);
    /* Write buffer size. */
    outb(io_base+VBSCSI_REGISTER_COMMAND, cbBuffer);
    outb(io_base+VBSCSI_REGISTER_COMMAND, (cbBuffer >> 8));
    /* Write the CDB. */
    for (i = 0; i < cbCDB; i++)
        outb(io_base+VBSCSI_REGISTER_COMMAND, aCDB[i]);

    /* Write data to I/O port. */
    rep_outsb(buffer, cbBuffer, io_base+VBSCSI_REGISTER_DATA_IN);

    /* Now wait for the command to complete. */
    do
    {
        status = inb(io_base+VBSCSI_REGISTER_STATUS);
    } while (status & VBSCSI_BUSY);

    return 0;
}

/**
 * Read sectors from an attached SCSI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the 
 *                      EBDA).
 */
int scsi_read_sectors(bio_dsk_t __far *bios_dsk)
{
    uint8_t             rc;
    cdb_rw10            cdb;
    uint16_t            count;
    uint16_t            io_base;
    uint8_t             target_id;
    uint8_t             device_id;

    device_id = bios_dsk->drqp.dev_id - BX_MAX_ATA_DEVICES;
    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("scsi_read_sectors: device_id out of range %d\n", device_id);

    count    = bios_dsk->drqp.nsect;

    /* Prepare a CDB. */
    cdb.command = SCSI_READ_10;
    cdb.lba     = swap_32(bios_dsk->drqp.lba);
    cdb.pad1    = 0;
    cdb.nsect   = swap_16(count);
    cdb.pad2    = 0;


    io_base   = bios_dsk->scsidev[device_id].io_base;
    target_id = bios_dsk->scsidev[device_id].target_id;

    rc = scsi_cmd_data_in(io_base, target_id, (void __far *)&cdb, 10, 
                          bios_dsk->drqp.buffer, (count * 512));

    if (!rc)
    {
        bios_dsk->drqp.trsfsectors = count;
        bios_dsk->drqp.trsfbytes   = count * 512;
    }

    return rc;
}

/**
 * Write sectors to an attached SCSI device.
 *
 * @returns status code.
 * @param   bios_dsk    Pointer to disk request packet (in the 
 *                      EBDA).
 */
int scsi_write_sectors(bio_dsk_t __far *bios_dsk)
{
    uint8_t             rc;
    cdb_rw10            cdb;
    uint16_t            count;
    uint16_t            io_base;
    uint8_t             target_id;
    uint8_t             device_id;

    device_id = bios_dsk->drqp.dev_id - BX_MAX_ATA_DEVICES;
    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("scsi_write_sectors: device_id out of range %d\n", device_id);

    count    = bios_dsk->drqp.nsect;

    /* Prepare a CDB. */
    cdb.command = SCSI_WRITE_10;
    cdb.lba     = swap_32(bios_dsk->drqp.lba);
    cdb.pad1    = 0;
    cdb.nsect   = swap_16(count);
    cdb.pad2    = 0;

    io_base   = bios_dsk->scsidev[device_id].io_base;
    target_id = bios_dsk->scsidev[device_id].target_id;

    rc = scsi_cmd_data_out(io_base, target_id, (void __far *)&cdb, 10,
                           bios_dsk->drqp.buffer, (count * 512));

    if (!rc)
    {
        bios_dsk->drqp.trsfsectors = count;
        bios_dsk->drqp.trsfbytes   = (count * 512);
    }

    return rc;
}

/**
 * Enumerate attached devices.
 *
 * @returns nothing.
 * @param   io_base    The I/O base port of the controller.
 */
void scsi_enumerate_attached_devices(uint16_t io_base)
{
    int                 i;
    uint8_t             buffer[0x0200];
    bio_dsk_t __far     *bios_dsk;

    bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;

    /* Go through target devices. */
    for (i = 0; i < VBSCSI_MAX_DEVICES; i++)
    {
        uint8_t     rc;
        uint8_t     aCDB[10];

        aCDB[0] = SCSI_INQUIRY;
        aCDB[1] = 0;
        aCDB[2] = 0;
        aCDB[3] = 0;
        aCDB[4] = 5; /* Allocation length. */
        aCDB[5] = 0;

        rc = scsi_cmd_data_in(io_base, i, aCDB, 6, buffer, 5);
        if (rc != 0)
            BX_PANIC("scsi_enumerate_attached_devices: SCSI_INQUIRY failed\n");

        /* Check if there is a disk attached. */
        if (   ((buffer[0] & 0xe0) == 0)
            && ((buffer[0] & 0x1f) == 0x00))
        {
            VBSCSI_DEBUG("scsi_enumerate_attached_devices: Disk detected at %d\n", i);

            /* We add the disk only if the maximum is not reached yet. */
            if (bios_dsk->scsi_hdcount < BX_MAX_SCSI_DEVICES)
            {
                uint32_t    sectors, sector_size, cylinders;
                uint16_t    heads, sectors_per_track;
                uint8_t     hdcount, hdcount_scsi, hd_index;

                /* Issue a read capacity command now. */
                _fmemset(aCDB, 0, sizeof(aCDB));
                aCDB[0] = SCSI_READ_CAPACITY;

                rc = scsi_cmd_data_in(io_base, i, aCDB, 10, buffer, 8);
                if (rc != 0)
                    BX_PANIC("scsi_enumerate_attached_devices: SCSI_READ_CAPACITY failed\n");

                /* Build sector number and size from the buffer. */
                //@todo: byte swapping for dword sized items should be farmed out...
                sectors =   ((uint32_t)buffer[0] << 24)
                          | ((uint32_t)buffer[1] << 16)
                          | ((uint32_t)buffer[2] << 8)
                          | ((uint32_t)buffer[3]);

                sector_size =   ((uint32_t)buffer[4] << 24)
                              | ((uint32_t)buffer[5] << 16)
                              | ((uint32_t)buffer[6] << 8)
                              | ((uint32_t)buffer[7]);

                /* We only support the disk if sector size is 512 bytes. */
                if (sector_size != 512)
                {
                    /* Leave a log entry. */
                    BX_INFO("Disk %d has an unsupported sector size of %u\n", i, sector_size);
                    continue;
                }

                /* We need to calculate the geometry for the disk. From 
                 * the BusLogic driver in the Linux kernel. 
                 */
                if (sectors >= 4 * 1024 * 1024)
                {
                    heads = 255;
                    sectors_per_track = 63;
                }
                else if (sectors >= 2 * 1024 * 1024)
                {
                    heads = 128;
                    sectors_per_track = 32;
                }
                else
                {
                    heads = 64;
                    sectors_per_track = 32;
                }
                cylinders = (uint32_t)(sectors / (heads * sectors_per_track));
                hdcount_scsi = bios_dsk->scsi_hdcount;

                /* Calculate index into the generic disk table. */
                hd_index = hdcount_scsi + BX_MAX_ATA_DEVICES;

                bios_dsk->scsidev[hdcount_scsi].io_base   = io_base;
                bios_dsk->scsidev[hdcount_scsi].target_id = i;
                bios_dsk->devices[hd_index].type        = DSK_TYPE_SCSI;
                bios_dsk->devices[hd_index].device      = DSK_DEVICE_HD;
                bios_dsk->devices[hd_index].removable   = 0;
                bios_dsk->devices[hd_index].lock        = 0;
                bios_dsk->devices[hd_index].blksize     = sector_size;
                bios_dsk->devices[hd_index].translation = GEO_TRANSLATION_LBA;

                /* Write LCHS values. */
                bios_dsk->devices[hd_index].lchs.heads = heads;
                bios_dsk->devices[hd_index].lchs.spt   = sectors_per_track;
                if (cylinders > 1024)
                    bios_dsk->devices[hd_index].lchs.cylinders = 1024;
                else
                    bios_dsk->devices[hd_index].lchs.cylinders = (uint16_t)cylinders;

                /* Write PCHS values. */
                bios_dsk->devices[hd_index].pchs.heads = heads;
                bios_dsk->devices[hd_index].pchs.spt   = sectors_per_track;
                if (cylinders > 1024)
                    bios_dsk->devices[hd_index].pchs.cylinders = 1024;
                else
                    bios_dsk->devices[hd_index].pchs.cylinders = (uint16_t)cylinders;

                bios_dsk->devices[hd_index].sectors = sectors;

                /* Store the id of the disk in the ata hdidmap. */
                hdcount = bios_dsk->hdcount;
                bios_dsk->hdidmap[hdcount] = hdcount_scsi + BX_MAX_ATA_DEVICES;
                hdcount++;
                bios_dsk->hdcount = hdcount;

                /* Update hdcount in the BDA. */
                hdcount = read_byte(0x40, 0x75);
                hdcount++;
                write_byte(0x40, 0x75, hdcount);

                hdcount_scsi++;
                bios_dsk->scsi_hdcount = hdcount_scsi;
            }
            else
            {
                /* We reached the maximum of SCSI disks we can boot from. We can quit detecting. */
                break;
            }
        }
        else
            VBSCSI_DEBUG("scsi_enumerate_attached_devices: No disk detected at %d\n", i);
    }
}

/**
 * Init the SCSI driver and detect attached disks.
 */
void BIOSCALL scsi_init(void)
{
    uint8_t             identifier;
    bio_dsk_t __far     *bios_dsk;

    bios_dsk = read_word(0x0040, 0x000E) :> &EbdaData->bdisk;

    bios_dsk->scsi_hdcount = 0;

    identifier = 0;

    /* Detect BusLogic adapter. */
    outb(BUSLOGIC_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(BUSLOGIC_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBSCSI_DEBUG("scsi_init: BusLogic SCSI adapter detected\n");
        outb(BUSLOGIC_ISA_IO_PORT+VBSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(BUSLOGIC_ISA_IO_PORT);
    }
    else
    {
        VBSCSI_DEBUG("scsi_init: BusLogic SCSI adapter not detected\n");
    }

    /* Detect LsiLogic adapter. */
    outb(LSILOGIC_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(LSILOGIC_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBSCSI_DEBUG("scsi_init: LSI Logic SCSI adapter detected\n");
        outb(LSILOGIC_ISA_IO_PORT+VBSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(LSILOGIC_ISA_IO_PORT);
    }
    else
    {
        VBSCSI_DEBUG("scsi_init: LSI Logic SCSI adapter not detected\n");
    }

    /* Detect LsiLogic SAS adapter. */
    outb(LSILOGIC_SAS_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(LSILOGIC_SAS_ISA_IO_PORT+VBSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBSCSI_DEBUG("scsi_init: LSI Logic SAS adapter detected\n");
        outb(LSILOGIC_SAS_ISA_IO_PORT+VBSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(LSILOGIC_SAS_ISA_IO_PORT);
    }
    else
    {
        VBSCSI_DEBUG("scsi_init: LSI Logic SAS adapter not detected\n");
    }
}
