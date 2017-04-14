/* $Id$ */
/** @file
 * IPRT, File Allocation Table (FAT).
 */

/*
 * Copyright (C) 2017 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___iprt_formats_fat_h
#define ___iprt_formats_fat_h

#include <iprt/types.h>
#include <iprt/assert.h>

#pragma pack(1)

/** @name FAT Media byte values
 * @remarks This isn't as simple as it's made out to be here!
 * @{ */
#define FATBPB_MEDIA_FLOPPY_8           UINT8_C(0xe5)
#define FATBPB_MEDIA_FLOPPY_5_DOT_25    UINT8_C(0xed)
#define FATBPB_MEDIA_FLOPPY_3_DOT_5     UINT8_C(0xf0)
/** @} */

typedef struct FATBPB
{
    /** 0x0b / 0x00: The sector size in bytes. */
    uint16_t        cbSector;
    /** 0x0d / 0x02: Number of sectors per cluster. */
    uint8_t         cSectorsPerCluster;
    /** 0x0e / 0x03: Number of reserved sectors before the first FAT. */
    uint16_t        cReservedSectors;
    /** 0x10 / 0x05: Number of FATs. */
    uint8_t         cFats;
    /** 0x11 / 0x06: Max size of the root directory (0 for FAT32). */
    uint16_t        cMaxRootDirEntries;
    /** 0x13 / 0x08: Total sector count, zero if 32-bit count is used. */
    uint16_t        cOldTotalSectors;
    /** 0x15 / 0x0a: Total sector count, zero if 32-bit count is used. */
    uint16_t        bMedia;
} FATBPB;

typedef struct FATBOOTSECTOR
{
    /** 0x000: DOS 2.0+ jump sequence. */
    uint8_t         abJmp[3];
    /** 0x003: OEM name (who formatted this volume). */
    char            achOemName[8];
    /** 0x00b: The BIOS parameter block.
     * This varies a lot in size. */
    union
    {

    } Bpb;
} FATBOOTSECTOR;
typedef FATBOOTSECTOR *PFATBOOTSECTOR;
typedef FATBOOTSECTOR const *PCFATBOOTSECTOR;

#pragma pack()

#endif

