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
#include "biosint.h"
#include "inlines.h"
#include "ebda.h"
#include "ata.h"


#if DEBUG_INT13_HD
#  define BX_DEBUG_INT13_HD(...) BX_DEBUG(__VA_ARGS__)
#else
#  define BX_DEBUG_INT13_HD(...)
#endif

//@todo: put in a header
#define AX      r.gr.u.r16.ax
#define BX      r.gr.u.r16.bx
#define CX      r.gr.u.r16.cx
#define DX      r.gr.u.r16.dx
#define SI      r.gr.u.r16.si
#define DI      r.gr.u.r16.di
#define BP      r.gr.u.r16.bp
#define ELDX    r.gr.u.r16.sp
#define DS      r.ds
#define ES      r.es
#define FLAGS   r.ra.flags.u.r16.flags

void BIOSCALL int13_harddisk(disk_regs_t r)
{
    uint32_t    lba;
    uint16_t    ebda_seg=read_word(0x0040,0x000E);
    uint16_t    cylinder, head, sector;
    uint16_t    npc, nph, npspt, nlc, nlh, nlspt;
    uint16_t    count;
    uint8_t     device, status;
    uint8_t     scsi_device;

    BX_DEBUG_INT13_HD("%s: AX=%04x BX=%04x CX=%04x DX=%04x ES=%04x\n", __func__, AX, BX, CX, DX, ES);
    
    write_byte(0x0040, 0x008e, 0);  // clear completion flag
    
    // basic check : device has to be defined
    if ( (GET_ELDL() < 0x80) || (GET_ELDL() >= 0x80 + BX_MAX_STORAGE_DEVICES) ) {
        BX_DEBUG("%s: function %02x, ELDL out of range %02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13_fail;
    }
    
    // Get the ata channel
    device=read_byte(ebda_seg,(uint16_t)&EbdaData->ata.hdidmap[GET_ELDL()-0x80]);
    
    // basic check : device has to be valid
    if (device >= BX_MAX_STORAGE_DEVICES) {
        BX_DEBUG("%s: function %02x, unmapped device for ELDL=%02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13_fail;
    }

    switch (GET_AH()) {

    case 0x00: /* disk controller reset */
#ifdef VBOX_WITH_SCSI
        /* SCSI controller does not need a reset. */
        if (!VBOX_IS_SCSI_DEVICE(device))
#endif
        ata_reset (device);
        goto int13_success;
        break;

    case 0x01: /* read disk status */
        status = read_byte(0x0040, 0x0074);
        SET_AH(status);
        SET_DISK_RET_STATUS(0);
        /* set CF if error status read */
        if (status) goto int13_fail_nostatus;
        else        goto int13_success_noah;
        break;

    case 0x02: // read disk sectors
    case 0x03: // write disk sectors
    case 0x04: // verify disk sectors

        count       = GET_AL();
        cylinder    = GET_CH();
        cylinder   |= ( ((uint16_t) GET_CL()) << 2) & 0x300;
        sector      = (GET_CL() & 0x3f);
        head        = GET_DH();
        
        /* Segment and offset are in ES:BX. */        
        if ( (count > 128) || (count == 0) ) {
            BX_INFO("%s: function %02x, count out of range!\n", __func__, GET_AH());
            goto int13_fail;
        }

#ifdef VBOX_WITH_SCSI
        if (!VBOX_IS_SCSI_DEVICE(device))
#endif
        {
            nlc   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.cylinders);
            nlh   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.heads);
            nlspt = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.spt);
        }
#ifdef VBOX_WITH_SCSI
        else {
            scsi_device = VBOX_GET_SCSI_DEVICE(device);
    
            nlc   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.cylinders);
            nlh   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.heads);
            nlspt = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.spt);
        }
#endif

        // sanity check on cyl heads, sec
        if( (cylinder >= nlc) || (head >= nlh) || (sector > nlspt )) {
            BX_INFO("%s: function %02x, disk %02x, parameters out of range %04x/%04x/%04x!\n", __func__, GET_AH(), GET_DL(), cylinder, head, sector);
            goto int13_fail;
        }
        
        // FIXME verify
        if ( GET_AH() == 0x04 )
            goto int13_success;

#ifdef VBOX_WITH_SCSI
        if (!VBOX_IS_SCSI_DEVICE(device))
#endif
        {
            nph   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].pchs.heads);
            npspt = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].pchs.spt);
        }
#ifdef VBOX_WITH_SCSI
        else {
            scsi_device = VBOX_GET_SCSI_DEVICE(device);
            nph   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.pchs.heads);
            npspt = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.pchs.spt);
        }
#endif

        // if needed, translate lchs to lba, and execute command
#ifdef VBOX_WITH_SCSI
        if (( (nph != nlh) || (npspt != nlspt)) || VBOX_IS_SCSI_DEVICE(device)) {
            lba = ((((uint32_t)cylinder * (uint32_t)nlh) + (uint32_t)head) * (uint32_t)nlspt) + (uint32_t)sector - 1;
            sector = 0; // this forces the command to be lba
        }
#else
        if (( (nph != nlh) || (npspt != nlspt)) ) {
            lba = ((((uint32_t)cylinder * (uint32_t)nlh) + (uint32_t)head) * (uint32_t)nlspt) + (uint32_t)sector - 1;
            sector = 0; // this forces the command to be lba
        }
#endif

        if ( GET_AH() == 0x02 )
        {
#ifdef VBOX_WITH_SCSI
            if (VBOX_IS_SCSI_DEVICE(device))
                status=scsi_read_sectors(VBOX_GET_SCSI_DEVICE(device), count, lba, MK_FP(ES, BX));
            else
#endif
            {
                write_word(ebda_seg,(uint16_t)&EbdaData->ata.devices[device].blksize,count * 0x200);
                status=ata_cmd_data_in(device, ATA_CMD_READ_MULTIPLE, count, cylinder, head, sector, lba, MK_FP(ES, BX));
                write_word(ebda_seg,(uint16_t)&EbdaData->ata.devices[device].blksize,0x200);
            }
        } else {
#ifdef VBOX_WITH_SCSI
            if (VBOX_IS_SCSI_DEVICE(device))
                status=scsi_write_sectors(VBOX_GET_SCSI_DEVICE(device), count, lba, MK_FP(ES, BX));
            else
#endif
                status=ata_cmd_data_out(device, ATA_CMD_WRITE_SECTORS, count, cylinder, head, sector, lba, MK_FP(ES, BX));
        }

        // Set nb of sector transferred
        SET_AL(read_word(ebda_seg, (uint16_t)&EbdaData->ata.trsfsectors));
        
        if (status != 0) {
            BX_INFO("%s: function %02x, error %02x !\n", __func__, GET_AH(), status);
            SET_AH(0x0c);
            goto int13_fail_noah;
        }
        
        goto int13_success;
        break;

    case 0x05: /* format disk track */
          BX_INFO("format disk track called\n");
          goto int13_success;
          return;
          break;

    case 0x08: /* read disk drive parameters */

        // Get logical geometry from table
#ifdef VBOX_WITH_SCSI
        if (!VBOX_IS_SCSI_DEVICE(device))
#endif
        {
            nlc   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.cylinders);
            nlh   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.heads);
            nlspt = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].lchs.spt);
        }
#ifdef VBOX_WITH_SCSI
        else {
            scsi_device = VBOX_GET_SCSI_DEVICE(device);
            nlc   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.cylinders);
            nlh   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.heads);
            nlspt = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.lchs.spt);
        }
#endif

        count = read_byte(ebda_seg, (uint16_t)&EbdaData->ata.hdcount);
        /* Maximum cylinder number is just one less than the number of cylinders. */
        nlc = nlc - 1; /* 0 based , last sector not used */
        SET_AL(0);
        SET_CH(nlc & 0xff);
        SET_CL(((nlc >> 2) & 0xc0) | (nlspt & 0x3f));
        SET_DH(nlh - 1);
        SET_DL(count); /* FIXME returns 0, 1, or n hard drives */
        
        // FIXME should set ES & DI
        // @todo: Actually, the above comment is nonsense.
        
        goto int13_success;
        break;

    case 0x10: /* check drive ready */
        // should look at 40:8E also???

        // Read the status from controller
        status = inb(read_word(ebda_seg, (uint16_t)&EbdaData->ata.channels[device/2].iobase1) + ATA_CB_STAT);
        if ( (status & ( ATA_CB_STAT_BSY | ATA_CB_STAT_RDY )) == ATA_CB_STAT_RDY ) {
            goto int13_success;
        } else {
            SET_AH(0xAA);
            goto int13_fail_noah;
        }
        break;

    case 0x15: /* read disk drive size */

      // Get physical geometry from table
#ifdef VBOX_WITH_SCSI
        if (!VBOX_IS_SCSI_DEVICE(device))
#endif
        {
            npc   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].pchs.cylinders);
            nph   = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].pchs.heads);
            npspt = read_word(ebda_seg, (uint16_t)&EbdaData->ata.devices[device].pchs.spt);
        }
#ifdef VBOX_WITH_SCSI
        else {
            scsi_device = VBOX_GET_SCSI_DEVICE(device);
            npc   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.pchs.cylinders);
            nph   = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.pchs.heads);
            npspt = read_word(ebda_seg, (uint16_t)&EbdaData->scsi.devices[scsi_device].device_info.pchs.spt);
        }
#endif

        // Compute sector count seen by int13
        lba = (uint32_t)npc * (uint32_t)nph * (uint32_t)npspt;
        CX = lba >> 16;
        DX = lba & 0xffff;
        
        SET_AH(3);  // hard disk accessible
        goto int13_success_noah;
        break;

    case 0x09: /* initialize drive parameters */
    case 0x0c: /* seek to specified cylinder */
    case 0x0d: /* alternate disk reset */
    case 0x11: /* recalibrate */
    case 0x14: /* controller internal diagnostic */
        BX_INFO("%s: function %02xh unimplemented, returns success\n", __func__, GET_AH());
        goto int13_success;
        break;

    case 0x0a: /* read disk sectors with ECC */
    case 0x0b: /* write disk sectors with ECC */
    case 0x18: // set media type for format
    default:
        BX_INFO("%s: function %02xh unsupported, returns fail\n", __func__, GET_AH());
        goto int13_fail;
        break;
    }

int13_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
int13_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
int13_fail_nostatus:
    SET_CF();     // error occurred
    return;

int13_success:
    SET_AH(0x00); // no error
int13_success_noah:
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}

void BIOSCALL int13_harddisk_ext(disk_regs_t r)
{
    uint32_t            lba;
    uint16_t            ebda_seg = read_word(0x0040,0x000E);
    uint16_t            segment, offset;
    uint16_t            npc, nph, npspt;
    uint16_t            size, count;
    uint8_t             device, status;
    uint8_t             scsi_device;
    ebda_data_t __far   *ebda_data;
    int13ext_t __far    *i13_ext;
    dpt_t __far         *dpt;

    ebda_data = ebda_seg :> 0;

    BX_DEBUG_INT13_HD("%s: AX=%04x BX=%04x CX=%04x DX=%04x ES=%04x\n", __func__, AX, BX, CX, DX, ES);
    
    write_byte(0x0040, 0x008e, 0);  // clear completion flag
    
    // basic check : device has to be defined
    if ( (GET_ELDL() < 0x80) || (GET_ELDL() >= 0x80 + BX_MAX_STORAGE_DEVICES) ) {
        BX_DEBUG("%s: function %02x, ELDL out of range %02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13x_fail;
    }
    
    // Get the ata channel
    device = ebda_data->ata.hdidmap[GET_ELDL()-0x80];
    
    // basic check : device has to be valid
    if (device >= BX_MAX_STORAGE_DEVICES) {
        BX_DEBUG("%s: function %02x, unmapped device for ELDL=%02x\n", __func__, GET_AH(), GET_ELDL());
        goto int13x_fail;
    }

    switch (GET_AH()) {
    case 0x41: // IBM/MS installation check
        BX=0xaa55;     // install check
        SET_AH(0x30);  // EDD 3.0
        CX=0x0007;     // ext disk access and edd, removable supported
        goto int13x_success_noah;
        break;

    case 0x42: // IBM/MS extended read
    case 0x43: // IBM/MS extended write
    case 0x44: // IBM/MS verify
    case 0x47: // IBM/MS extended seek

        /* Get a pointer to the extended structure. */
        i13_ext = DS :> (int13ext_t *)SI;

        count   = i13_ext->count;
        segment = i13_ext->segment;
        offset  = i13_ext->offset;
        
        // Can't use 64 bits lba
        lba = i13_ext->lba2;
        if (lba != 0L) {
            BX_PANIC("%s: function %02x. Can't use 64bits lba\n", __func__, GET_AH());
            goto int13x_fail;
        }
        
        // Get 32 bits lba and check
        lba = i13_ext->lba1;

#ifdef VBOX_WITH_SCSI
        if (VBOX_IS_SCSI_DEVICE(device))
        {
            if (lba >= ebda_data->scsi.devices[VBOX_GET_SCSI_DEVICE(device)].device_info.sectors ) {
                BX_INFO("%s: function %02x. LBA out of range\n", __func__, GET_AH());
                goto int13x_fail;
            }
        }
        else
#endif
        if (lba >= ebda_data->ata.devices[device].sectors) {
              BX_INFO("%s: function %02x. LBA out of range\n", __func__, GET_AH());
              goto int13x_fail;
        }

        // If verify or seek
        if (( GET_AH() == 0x44 ) || ( GET_AH() == 0x47 ))
            goto int13x_success;
        
        // Execute the command
        if ( GET_AH() == 0x42 ) {
#ifdef VBOX_WITH_SCSI
            if (VBOX_IS_SCSI_DEVICE(device))
                status=scsi_read_sectors(VBOX_GET_SCSI_DEVICE(device), count, lba, MK_FP(segment, offset));
            else {
#endif
                if (lba + count >= 268435456)
                    status=ata_cmd_data_in(device, ATA_CMD_READ_SECTORS_EXT, count, 0, 0, 0, lba, MK_FP(segment, offset));
                else {
                    ebda_data->ata.devices[device].blksize = count * 0x200;
                    status=ata_cmd_data_in(device, ATA_CMD_READ_MULTIPLE, count, 0, 0, 0, lba, MK_FP(segment, offset));
                    ebda_data->ata.devices[device].blksize = 0x200;
                }
            }
        } else {
#ifdef VBOX_WITH_SCSI
            if (VBOX_IS_SCSI_DEVICE(device))
                status=scsi_write_sectors(VBOX_GET_SCSI_DEVICE(device), count, lba, MK_FP(segment, offset));
            else {
#endif
                if (lba + count >= 268435456)
                    status=ata_cmd_data_out(device, ATA_CMD_WRITE_SECTORS_EXT, count, 0, 0, 0, lba, MK_FP(segment, offset));
                else
                    status=ata_cmd_data_out(device, ATA_CMD_WRITE_SECTORS, count, 0, 0, 0, lba, MK_FP(segment, offset));
            }
        }

        count = ebda_data->ata.trsfsectors;
        i13_ext->count = count;
        
        if (status != 0) {
            BX_INFO("%s: function %02x, error %02x !\n", __func__, GET_AH(), status);
            SET_AH(0x0c);
            goto int13x_fail_noah;
        }
        
        goto int13x_success;
        break;

    case 0x45: // IBM/MS lock/unlock drive
    case 0x49: // IBM/MS extended media change
        goto int13x_success;   // Always success for HD
        break;

    case 0x46: // IBM/MS eject media
        SET_AH(0xb2);          // Volume Not Removable
        goto int13x_fail_noah; // Always fail for HD
        break;

    case 0x48: // IBM/MS get drive parameters
        dpt = DS :> (dpt_t *)SI;
        size = dpt->size;

        // Buffer is too small
        if(size < 0x1a)
            goto int13x_fail;
        
        // EDD 1.x
        if(size >= 0x1a) {
            uint16_t   blksize;

#ifdef VBOX_WITH_SCSI
            if (!VBOX_IS_SCSI_DEVICE(device))
#endif
            {
                npc     = ebda_data->ata.devices[device].pchs.cylinders;
                nph     = ebda_data->ata.devices[device].pchs.heads;
                npspt   = ebda_data->ata.devices[device].pchs.spt;
                lba     = ebda_data->ata.devices[device].sectors;
                blksize = ebda_data->ata.devices[device].blksize;
            }
#ifdef VBOX_WITH_SCSI
            else {
                scsi_device = VBOX_GET_SCSI_DEVICE(device);
                npc     = ebda_data->scsi.devices[scsi_device].device_info.pchs.cylinders;
                nph     = ebda_data->scsi.devices[scsi_device].device_info.pchs.heads;
                npspt   = ebda_data->scsi.devices[scsi_device].device_info.pchs.spt;
                lba     = ebda_data->scsi.devices[scsi_device].device_info.sectors;
                blksize = ebda_data->scsi.devices[scsi_device].device_info.blksize;
            }
#endif
            dpt->size      = 0x1a;
            dpt->infos     = 0x02;  // geometry is valid
            dpt->cylinders = npc;
            dpt->heads     = nph;
            dpt->spt       = npspt;
            dpt->blksize   = blksize;
            dpt->sector_count1 = lba;   // FIXME should be Bit64
            dpt->sector_count2 = 0;
        }

        // EDD 2.x
        if(size >= 0x1e) {
            uint8_t     channel, irq, mode, checksum, i, translation;
            uint16_t    iobase1, iobase2, options;
            
            dpt->size = 0x1e;
            dpt->dpte_segment = ebda_seg;
            dpt->dpte_offset  = (uint16_t)&EbdaData->ata.dpte;
            
            // Fill in dpte
            channel = device / 2;
            iobase1 = ebda_data->ata.channels[channel].iobase1;
            iobase2 = ebda_data->ata.channels[channel].iobase2;
            irq     = ebda_data->ata.channels[channel].irq;
            mode    = ebda_data->ata.devices[device].mode;
            translation = ebda_data->ata.devices[device].translation;
            
            options  = (translation == ATA_TRANSLATION_NONE ? 0 : 1 << 3);  // chs translation
            options |= (1 << 4);    // lba translation
            options |= (mode == ATA_MODE_PIO32 ? 1 : 0 << 7);
            options |= (translation==ATA_TRANSLATION_LBA ? 1 : 0 << 9);
            options |= (translation==ATA_TRANSLATION_RECHS ? 3 : 0 << 9);
            
            ebda_data->ata.dpte.iobase1  = iobase1;
            ebda_data->ata.dpte.iobase2  = iobase2;
            ebda_data->ata.dpte.prefix   = (0xe | (device % 2)) << 4;
            ebda_data->ata.dpte.unused   = 0xcb;
            ebda_data->ata.dpte.irq      = irq;
            ebda_data->ata.dpte.blkcount = 1;
            ebda_data->ata.dpte.dma      = 0;
            ebda_data->ata.dpte.pio      = 0;
            ebda_data->ata.dpte.options  = options;
            ebda_data->ata.dpte.reserved = 0;
            ebda_data->ata.dpte.revision = 0x11;
            
            checksum = 0;
            for (i=0; i<15; i++)
                checksum += read_byte(ebda_seg, (uint16_t)&EbdaData->ata.dpte + i);
            checksum = -checksum;
            ebda_data->ata.dpte.checksum = checksum;
        }

        // EDD 3.x
        if(size >= 0x42) {
            uint8_t     channel, iface, checksum, i;
            uint16_t    iobase1;

            channel = device / 2;
            iface   = ebda_data->ata.channels[channel].iface;
            iobase1 = ebda_data->ata.channels[channel].iobase1;
            
            dpt->size       = 0x42;
            dpt->key        = 0xbedd;
            dpt->dpi_length = 0x24;
            dpt->reserved1  = 0;
            dpt->reserved2  = 0;
            
            if (iface == ATA_IFACE_ISA) {
                dpt->host_bus[0] = 'I';
                dpt->host_bus[1] = 'S';
                dpt->host_bus[2] = 'A';
                dpt->host_bus[3] = 0;
            }
            else {
                // FIXME PCI
            }
            dpt->iface_type[0] = 'A';
            dpt->iface_type[1] = 'T';
            dpt->iface_type[2] = 'A';
            dpt->iface_type[3] = 0;
            
            if (iface == ATA_IFACE_ISA) {
                ((uint16_t __far *)dpt->iface_path)[0] = iobase1;
                ((uint16_t __far *)dpt->iface_path)[1] = 0;
                ((uint32_t __far *)dpt->iface_path)[1] = 0;
            }
            else {
                // FIXME PCI
            }
            ((uint16_t __far *)dpt->device_path)[0] = device & 1; // device % 2; @todo: correct?
            ((uint16_t __far *)dpt->device_path)[1] = 0;
            ((uint32_t __far *)dpt->device_path)[1] = 0;
            
            checksum = 0;
            for (i = 30; i < 64; i++)
                checksum += read_byte(DS, SI + i);
            checksum = -checksum;
            dpt->checksum = checksum;
        }

        goto int13x_success;
        break;

    case 0x4e: // // IBM/MS set hardware configuration
        // DMA, prefetch, PIO maximum not supported
        switch (GET_AL()) {
        case 0x01:
        case 0x03:
        case 0x04:
        case 0x06:
            goto int13x_success;
            break;
        default :
            goto int13x_fail;
        }
        break;

    case 0x50: // IBM/MS send packet command
    default:
        BX_INFO("%s: function %02xh unsupported, returns fail\n", __func__, GET_AH());
        goto int13x_fail;
        break;
    }

int13x_fail:
    SET_AH(0x01); // defaults to invalid function in AH or invalid parameter
int13x_fail_noah:
    SET_DISK_RET_STATUS(GET_AH());
    SET_CF();     // error occurred
    return;

int13x_success:
    SET_AH(0x00); // no error
int13x_success_noah:
    SET_DISK_RET_STATUS(0x00);
    CLEAR_CF();   // no error
    return;
}
