/* $Id$ */
/** @file
 * SCSI host adapter driver to boot from SCSI disks
 */

/*
 * Copyright (C) 2004-2009 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/* The I/O port of the BusLogic SCSI adapter. */
#define BUSLOGIC_BIOS_IO_PORT       0x330
/* The I/O port of the LsiLogic SCSI adapter. */
#define LSILOGIC_BIOS_IO_PORT       0x340
/* The I/O port of the LsiLogic SAS adapter. */
#define LSILOGIC_SAS_BIOS_IO_PORT   0x350

#define VBOXSCSI_REGISTER_STATUS   0
#define VBOXSCSI_REGISTER_COMMAND  0
#define VBOXSCSI_REGISTER_DATA_IN  1
#define VBOXSCSI_REGISTER_IDENTIFY 2
#define VBOXSCSI_REGISTER_RESET    3

#define VBOXSCSI_MAX_DEVICES 16 /* Maximum number of devices a SCSI device can have. */

/* Command opcodes. */
#define SCSI_INQUIRY       0x12
#define SCSI_READ_CAPACITY 0x25
#define SCSI_READ_10       0x28
#define SCSI_WRITE_10      0x2a

/* Data transfer direction. */
#define SCSI_TXDIR_FROM_DEVICE 0
#define SCSI_TXDIR_TO_DEVICE   1

#define VBOXSCSI_BUSY (1 << 0)

//#define VBOX_SCSI_DEBUG 1 /* temporary */

#ifdef VBOX_SCSI_DEBUG
# define VBOXSCSI_DEBUG(a...) BX_INFO(a)
#else
# define VBOXSCSI_DEBUG(a...)
#endif

int scsi_cmd_data_in(io_base, device_id, cdb_segment, aCDB, cbCDB, segment, offset, cbBuffer)
    Bit16u io_base, aCDB, offset, cbBuffer, segment, cdb_segment;
    Bit8u  device_id, cbCDB;
{
    /* Check that the adapter is ready. */
    Bit8u status;
    Bit16u i;

    do
    {
        status = inb(io_base+VBOXSCSI_REGISTER_STATUS);
    } while (status & VBOXSCSI_BUSY);

    /* Write target ID. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, device_id);
    /* Write transfer direction. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, SCSI_TXDIR_FROM_DEVICE);
    /* Write the CDB size. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, cbCDB);
    /* Write buffer size. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, cbBuffer);
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, (cbBuffer >> 8));
    /* Write the CDB. */
    for (i = 0; i < cbCDB; i++)
        outb(io_base+VBOXSCSI_REGISTER_COMMAND, read_byte(cdb_segment, aCDB + i));

    /* Now wait for the command to complete. */
    do
    {
        status = inb(io_base+VBOXSCSI_REGISTER_STATUS);
    } while (status & VBOXSCSI_BUSY);

#if 0
    /* Get the read data. */
    for (i = 0; i < cbBuffer; i++)
    {
        Bit8u data;

        data = inb(io_base+VBOXSCSI_REGISTER_DATA_IN);
        write_byte(segment, offset+i, data);

        VBOXSCSI_DEBUG("buffer[%d]=%x\n", i, data);
    }
#else
    io_base = io_base + VBOXSCSI_REGISTER_DATA_IN;

ASM_START
        push bp
        mov  bp, sp
        mov  di, _scsi_cmd_data_in.offset + 2[bp]
        mov  ax, _scsi_cmd_data_in.segment + 2[bp]
        mov  cx, _scsi_cmd_data_in.cbBuffer + 2[bp]

        mov   es, ax      ;; segment in es
        mov   dx, _scsi_cmd_data_in.io_base + 2[bp] ;; SCSI data read port

        rep
          insb ;; CX dwords transferred from port(DX) to ES:[DI]

        pop  bp
ASM_END
#endif

    return 0;
}

int scsi_cmd_data_out(io_base, device_id, cdb_segment, aCDB, cbCDB, segment, offset, cbBuffer)
    Bit16u io_base, aCDB, offset, cbBuffer, segment, cdb_segment;
    Bit8u  device_id, cbCDB;
{
    /* Check that the adapter is ready. */
    Bit8u status;
    Bit16u i;

    do
    {
        status = inb(io_base+VBOXSCSI_REGISTER_STATUS);
    } while (status & VBOXSCSI_BUSY);

    /* Write target ID. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, device_id);
    /* Write transfer direction. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, SCSI_TXDIR_TO_DEVICE);
    /* Write the CDB size. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, cbCDB);
    /* Write buffer size. */
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, cbBuffer);
    outb(io_base+VBOXSCSI_REGISTER_COMMAND, (cbBuffer >> 8));
    /* Write the CDB. */
    for (i = 0; i < cbCDB; i++)
        outb(io_base+VBOXSCSI_REGISTER_COMMAND, read_byte(cdb_segment, aCDB + i));

#if 0
    /* Write data to I/O port. */
    for (i = 0; i < cbBuffer; i++)
    {
        Bit8u data;

        data = read_byte(segment, offset+i);
        outb(io_base+VBOXSCSI_REGISTER_DATA_IN, data);

        VBOXSCSI_DEBUG("buffer[%d]=%x\n", i, data);
    }
#else
ASM_START
        push bp
        mov  bp, sp
        mov  si, _scsi_cmd_data_out.offset + 2[bp]
        mov  ax, _scsi_cmd_data_out.segment + 2[bp]
        mov  cx, _scsi_cmd_data_out.cbBuffer + 2[bp]

        mov   dx, _scsi_cmd_data_out.io_base + 2[bp] ;; SCSI data write port
        add   dx, #VBOXSCSI_REGISTER_DATA_IN
        push  ds
        mov   ds, ax      ;; segment in ds

        rep
          outsb ;; CX bytes transferred from DS:[SI] to port(DX)

        pop  ds
        pop  bp
ASM_END
#endif

    /* Now wait for the command to complete. */
    do
    {
        status = inb(io_base+VBOXSCSI_REGISTER_STATUS);
    } while (status & VBOXSCSI_BUSY);

    return 0;
}


/**
 * Read sectors from an attached scsi device.
 *
 * @returns status code.
 * @param   device_id   Id of the SCSI device to read from.
 * @param   count       The number of sectors to read.
 * @param   lba         The start sector to read from.
 * @param   segment     The segment the buffer is in.
 * @param   offset      The offset of the buffer.
 */
int scsi_read_sectors(device_id, count, lba, segment, offset)
    Bit8u device_id;
    Bit16u count, segment, offset;
    Bit32u lba;
{
    Bit8u rc;
    Bit8u aCDB[10];
    Bit16u io_base, ebda_seg;
    Bit8u  target_id;

    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("scsi_read_sectors: device_id out of range %d\n", device_id);

    ebda_seg = read_word(0x0040, 0x000E);
    // Reset count of transferred data
    write_word(ebda_seg, &EbdaData->ata.trsfsectors,0);
    write_dword(ebda_seg, &EbdaData->ata.trsfbytes,0L);

    /* Prepare CDB */
    write_byte(get_SS(), aCDB + 0, SCSI_READ_10);
    write_byte(get_SS(), aCDB + 1, 0);
    write_byte(get_SS(), aCDB + 2, (uint8_t)(lba >> 24));
    write_byte(get_SS(), aCDB + 3, (uint8_t)(lba >> 16));
    write_byte(get_SS(), aCDB + 4, (uint8_t)(lba >>  8));
    write_byte(get_SS(), aCDB + 5, (uint8_t)(lba));
    write_byte(get_SS(), aCDB + 6, 0);
    write_byte(get_SS(), aCDB + 7, (uint8_t)(count >> 8));
    write_byte(get_SS(), aCDB + 8, (uint8_t)(count));
    write_byte(get_SS(), aCDB + 9, 0);

    io_base = read_word(ebda_seg, &EbdaData->scsi.devices[device_id].io_base);
    target_id = read_byte(ebda_seg, &EbdaData->scsi.devices[device_id].target_id);

    rc = scsi_cmd_data_in(io_base, target_id, get_SS(), aCDB, 10, segment, offset, (count * 512));

    if (!rc)
    {
        write_word(ebda_seg, &EbdaData->ata.trsfsectors, count);
        write_dword(ebda_seg, &EbdaData->ata.trsfbytes, (count * 512));
    }

    return rc;
}

/**
 * Write sectors to an attached scsi device.
 *
 * @returns status code.
 * @param   device_id   Id of the SCSI device to write to.
 * @param   count       The number of sectors to write.
 * @param   lba         The start sector to write to.
 * @param   segment     The segment the buffer is in.
 * @param   offset      The offset of the buffer.
 */
int scsi_write_sectors(device_id, count, lba, segment, offset)
    Bit8u device_id;
    Bit16u count, segment, offset;
    Bit32u lba;
{
    Bit8u rc;
    Bit8u aCDB[10];
    Bit16u io_base, ebda_seg;
    Bit8u  target_id;

    if (device_id > BX_MAX_SCSI_DEVICES)
        BX_PANIC("scsi_write_sectors: device_id out of range %d\n", device_id);

    ebda_seg = read_word(0x0040, 0x000E);
    // Reset count of transferred data
    write_word(ebda_seg, &EbdaData->ata.trsfsectors,0);
    write_dword(ebda_seg, &EbdaData->ata.trsfbytes,0L);

    /* Prepare CDB */
    write_byte(get_SS(), aCDB + 0, SCSI_WRITE_10);
    write_byte(get_SS(), aCDB + 1, 0);
    write_byte(get_SS(), aCDB + 2, (uint8_t)(lba >> 24));
    write_byte(get_SS(), aCDB + 3, (uint8_t)(lba >> 16));
    write_byte(get_SS(), aCDB + 4, (uint8_t)(lba >>  8));
    write_byte(get_SS(), aCDB + 5, (uint8_t)(lba));
    write_byte(get_SS(), aCDB + 6, 0);
    write_byte(get_SS(), aCDB + 7, (uint8_t)(count >> 8));
    write_byte(get_SS(), aCDB + 8, (uint8_t)(count));
    write_byte(get_SS(), aCDB + 9, 0);

    io_base = read_word(ebda_seg, &EbdaData->scsi.devices[device_id].io_base);
    target_id = read_byte(ebda_seg, &EbdaData->scsi.devices[device_id].target_id);

    rc = scsi_cmd_data_out(io_base, target_id, get_SS(), aCDB, 10, segment, offset, (count * 512));

    if (!rc)
    {
        write_word(ebda_seg, &EbdaData->ata.trsfsectors, count);
        write_dword(ebda_seg, &EbdaData->ata.trsfbytes, (count * 512));
    }

    return rc;
}

/**
 * Enumerate attached devices.
 *
 * @returns nothing.
 * @param   io_base    The I/O base port of the controller.
 */
void scsi_enumerate_attached_devices(io_base)
    Bit16u io_base;
{
    Bit16u ebda_seg;
    Bit8u i;
    Bit8u buffer[0x0200];

    ebda_seg = read_word(0x0040, 0x000E);

    /* Go through target devices. */
    for (i = 0; i < VBOXSCSI_MAX_DEVICES; i++)
    {
        Bit8u rc;
        Bit8u z;
        Bit8u aCDB[10];

        write_byte(get_SS(), aCDB + 0, SCSI_INQUIRY);
        write_byte(get_SS(), aCDB + 1, 0);
        write_byte(get_SS(), aCDB + 2, 0);
        write_byte(get_SS(), aCDB + 3, 0);
        write_byte(get_SS(), aCDB + 4, 5); /* Allocation length. */
        write_byte(get_SS(), aCDB + 5, 0);

        rc = scsi_cmd_data_in(io_base, i, get_SS(), aCDB, 6, get_SS(), buffer, 5);
        if (rc != 0)
            BX_PANIC("scsi_enumerate_attached_devices: SCSI_INQUIRY failed\n");

        /* Check if there is a disk attached. */
        if (   ((buffer[0] & 0xe0) == 0)
            && ((buffer[0] & 0x1f) == 0x00))
        {
            VBOXSCSI_DEBUG("scsi_enumerate_attached_devices: Disk detected at %d\n", i);

            /* We add the disk only if the maximum is not reached yet. */
            if (read_byte(ebda_seg, &EbdaData->scsi.hdcount) < BX_MAX_SCSI_DEVICES)
            {
                Bit32u sectors, sector_size, cylinders;
                Bit16u heads, sectors_per_track;
                Bit8u  hdcount, hdcount_scsi;

                /* Issue a read capacity command now. */
                write_byte(get_SS(), aCDB + 0, SCSI_READ_CAPACITY);
                memsetb(get_SS(), aCDB + 1, 0, 9);

                rc = scsi_cmd_data_in(io_base, i, get_SS(), aCDB, 10, get_SS(), buffer, 8);
                if (rc != 0)
                    BX_PANIC("scsi_enumerate_attached_devices: SCSI_READ_CAPACITY failed\n");

                /* Build sector number and size from the buffer. */
                sectors =   ((uint32_t)read_byte(get_SS(), buffer + 0) << 24)
                          | ((uint32_t)read_byte(get_SS(), buffer + 1) << 16)
                          | ((uint32_t)read_byte(get_SS(), buffer + 2) << 8)
                          | ((uint32_t)read_byte(get_SS(), buffer + 3));

                sector_size =   ((uint32_t)read_byte(get_SS(), buffer + 4) << 24)
                              | ((uint32_t)read_byte(get_SS(), buffer + 5) << 16)
                              | ((uint32_t)read_byte(get_SS(), buffer + 6) << 8)
                              | ((uint32_t)read_byte(get_SS(), buffer + 7));

                /* We only support the disk if sector size is 512 bytes. */
                if (sector_size != 512)
                {
                    /* Leave a log entry. */
                    BX_INFO("Disk %d has an unsupported sector size of %u\n", i, sector_size);
                    continue;
                }

                /* We need to calculate the geometry for the disk. */
                if (io_base == BUSLOGIC_BIOS_IO_PORT)
                {
                    /* This is from the BusLogic driver in the Linux kernel. */
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
                }
                else if (io_base == LSILOGIC_BIOS_IO_PORT || io_base == LSILOGIC_SAS_BIOS_IO_PORT)
                {
                    /* This is from the BusLogic driver in the Linux kernel. */
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
                }
                hdcount_scsi = read_byte(ebda_seg, &EbdaData->scsi.hdcount);

                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].io_base, io_base);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].target_id, i);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.type, ATA_TYPE_SCSI);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.device, ATA_DEVICE_HD);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.removable, 0);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.lock, 0);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.mode, ATA_MODE_PIO16);
                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.blksize, sector_size);
                write_byte(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.translation, ATA_TRANSLATION_LBA);

                /* Write lchs values. */
                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.lchs.heads, heads);
                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.lchs.spt, sectors_per_track);
                if (cylinders > 1024)
                    write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.lchs.cylinders, 1024);
                else
                    write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.lchs.cylinders, (Bit16u)cylinders);

                /* Write pchs values. */
                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.pchs.heads, heads);
                write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.pchs.spt, sectors_per_track);
                if (cylinders > 1024)
                    write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.pchs.cylinders, 1024);
                else
                    write_word(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.pchs.cylinders, (Bit16u)cylinders);

                write_dword(ebda_seg, &EbdaData->scsi.devices[hdcount_scsi].device_info.sectors, sectors);

                /* Store the id of the disk in the ata hdidmap. */
                hdcount = read_byte(ebda_seg, &EbdaData->ata.hdcount);
                write_byte(ebda_seg, &EbdaData->ata.hdidmap[hdcount], hdcount_scsi + BX_MAX_ATA_DEVICES);
                hdcount++;
                write_byte(ebda_seg, &EbdaData->ata.hdcount, hdcount);

                /* Update hdcount in the BDA. */
                hdcount = read_byte(0x40, 0x75);
                hdcount++;
                write_byte(0x40, 0x75, hdcount);

                hdcount_scsi++;
                write_byte(ebda_seg, &EbdaData->scsi.hdcount, hdcount_scsi);
            }
            else
            {
                /* We reached the maximum of SCSI disks we can boot from. We can quit detecting. */
                break;
            }
        }
        else
            VBOXSCSI_DEBUG("scsi_enumerate_attached_devices: No disk detected at %d\n", i);
    }
}

/**
 * Init the SCSI driver and detect attached disks.
 */
void scsi_init( )
{
    Bit8u identifier;
    Bit16u ebda_seg;


    ebda_seg = read_word(0x0040, 0x000E);
    write_byte(ebda_seg, &EbdaData->scsi.hdcount, 0);

    identifier = 0;

    /* Detect BusLogic adapter. */
    outb(BUSLOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(BUSLOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBOXSCSI_DEBUG("scsi_init: BusLogic SCSI adapter detected\n");
        outb(BUSLOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(BUSLOGIC_BIOS_IO_PORT);
    }
    else
    {
        VBOXSCSI_DEBUG("scsi_init: BusLogic SCSI adapter not detected\n");
    }

    /* Detect LsiLogic adapter. */
    outb(LSILOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(LSILOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBOXSCSI_DEBUG("scsi_init: LsiLogic SCSI adapter detected\n");
        outb(LSILOGIC_BIOS_IO_PORT+VBOXSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(LSILOGIC_BIOS_IO_PORT);
    }
    else
    {
        VBOXSCSI_DEBUG("scsi_init: LsiLogic SCSI adapter not detected\n");
    }

    /* Detect LsiLogic SAS adapter. */
    outb(LSILOGIC_SAS_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY, 0x55);
    identifier = inb(LSILOGIC_SAS_BIOS_IO_PORT+VBOXSCSI_REGISTER_IDENTIFY);

    if (identifier == 0x55)
    {
        /* Detected - Enumerate attached devices. */
        VBOXSCSI_DEBUG("scsi_init: LsiLogic SAS adapter detected\n");
        outb(LSILOGIC_SAS_BIOS_IO_PORT+VBOXSCSI_REGISTER_RESET, 0);
        scsi_enumerate_attached_devices(LSILOGIC_SAS_BIOS_IO_PORT);
    }
    else
    {
        VBOXSCSI_DEBUG("scsi_init: LsiLogic SAS adapter not detected\n");
    }
}

