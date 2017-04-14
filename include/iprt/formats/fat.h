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
/* incomplete, figure out as needed... */
/** @} */

/**
 * The DOS 2.0 BIOS parameter block (BPB).
 *
 * This was the first DOS version with a BPB.
 */
typedef struct FATBPB20
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
    uint16_t        cTotalSectors16;
    /** 0x15 / 0x0a: Media ID. */
    uint8_t         bMedia;
    /** 0x16 / 0x0b: Number of sectors per FAT (0 for FAT32). */
    uint16_t        cSectorsPerFat;
} FATBPB20;
AssertCompileSize(FATBPB20, 0xd);
/** Pointer to a DOS 2.0 BPB. */
typedef FATBPB20 *PFATBPB20;
/** Pointer to a const DOS 2.0 BPB. */
typedef FATBPB20 const *PCFATBPB20;


/**
 * The DOS 3.0 BPB changes that survived.
 */
typedef struct FATBPB30CMN
{
    /** DOS v2.0 BPB.   */
    FATBPB20        Bpb20;
    /** 0x18 / 0x0d: Sectors per track. Zero means reserved and not used. */
    uint16_t        cSectorsPerTrack;
    /** 0x1a / 0x0f: Number of heads. Zero means reserved and not used. */
    uint16_t        cTracksPerCylinder;
} FATBPB30CMN;
AssertCompileSize(FATBPB30CMN, 0x11);

/**
 * The DOS 3.0 BPB.
 */
typedef struct FATBPB30
{
    /** DOS v3.0 BPB bits that survived.   */
    FATBPB30CMN     Core30;
    /** 0x1c / 0x11: Number of hidden sectors preceeding the volume. This is zero
     * on unpartitioned media. */
    uint16_t        cHiddenSectors;
} FATBPB30;
AssertCompileSize(FATBPB30, 0x13);
/** Pointer to a DOS 3.0 BPB. */
typedef FATBPB30 *PFATBPB30;
/** Pointer to a const DOS 3.0 BPB. */
typedef FATBPB30 const *PCFATBPB30;

/**
 * The DOS 3.0 BPB, flattened structure.
 */
typedef struct FATBPB30FLAT
{
    /** @name New in DOS 2.0
     * @{ */
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
    uint16_t        cTotalSectors16;
    /** 0x15 / 0x0a: Media ID. */
    uint8_t         bMedia;
    /** 0x16 / 0x0b: Number of sectors per FAT (0 for FAT32). */
    uint16_t        cSectorsPerFat;
    /** @} */
    /** @name New in DOS 3.0
     * @{  */
    /** 0x18 / 0x0d: Sectors per track. Zero means reserved and not used. */
    uint16_t        cSectorsPerTrack;
    /** 0x1a / 0x0f: Number of heads. Zero means reserved and not used. */
    uint16_t        cTracksPerCylinder;
    /** 0x1c / 0x11: Number of hidden sectors preceeding the volume. This is zero
     * on unpartitioned media. */
    uint16_t        cHiddenSectors;
    /** @} */
} FATBPB30FLAT;
AssertCompileSize(FATBPB30FLAT, 0x13);
/** Pointer to a flattened DOS 3.0 BPB. */
typedef FATBPB30FLAT *PFATBPB30FLAT;
/** Pointer to a const flattened DOS 3.0 BPB. */
typedef FATBPB30FLAT const *PCFATBPB30FLAT;


/**
 * The DOS 3.2 BPB.
 */
typedef struct FATBPB32
{
    /** DOS v3.0 BPB.   */
    FATBPB30        Bpb30;
    /** 0x1e / 0x13: Number of sectors, including the hidden ones.  This is ZERO
     * in DOS 3.31+. */
    uint16_t        cAnotherTotalSectors;
} FATBPB32;
AssertCompileSize(FATBPB32, 0x15);
/** Pointer to a DOS 3.2 BPB. */
typedef FATBPB32 *PFATBPB32;
/** Pointer to const a DOS 3.2 BPB. */
typedef FATBPB32 const *PCFATBPB32;

/**
 * The DOS 3.2 BPB, flattened structure.
 */
typedef struct FATBPB32FLAT
{
    /** @name New in DOS 2.0
     * @{ */
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
    uint16_t        cTotalSectors16;
    /** 0x15 / 0x0a: Media ID. */
    uint8_t         bMedia;
    /** 0x16 / 0x0b: Number of sectors per FAT (0 for FAT32). */
    uint16_t        cSectorsPerFat;
    /** @} */
    /** @name New in DOS 3.0
     * @{  */
    /** 0x18 / 0x0d: Sectors per track. Zero means reserved and not used. */
    uint16_t        cSectorsPerTrack;
    /** 0x1a / 0x0f: Number of heads. Zero means reserved and not used. */
    uint16_t        cTracksPerCylinder;
    /** 0x1c / 0x11: Number of hidden sectors preceeding the volume. This is zero
     * on unpartitioned media. */
    uint16_t        cHiddenSectors;
    /** @} */
    /** @name New in DOS 3.2
     * @{  */
    /** 0x1e / 0x13: Number of sectors, including the hidden ones.  This is ZERO
     * in DOS 3.31+. */
    uint16_t        cAnotherTotalSectors;
    /** @} */
} FATBPB32FLAT;
AssertCompileSize(FATBPB32FLAT, 0x15);
/** Pointer to a flattened DOS 3.2 BPB. */
typedef FATBPB32FLAT *PFATBPB32FLAT;
/** Pointer to a const flattened DOS 3.2 BPB. */
typedef FATBPB32FLAT const *PCFATBPB32FLAT;


/**
 * The DOS 3.31 BPB.
 */
typedef struct FATBPB331
{
    /** DOS v3.0 BPB bits that survived.   */
    FATBPB30CMN     Core30;
    /** 0x1c / 0x11: Number of hidden sectors preceeding the volume.  This is zero
     * on unpartitioned media.  Values higher than 65535 are complicated due to
     * the field overlapping FATBPB32::cAnotherTotalSectors */
    uint32_t        cHiddenSectors;
    /** 0x20 / 0x15: Total logical sectors.  Used if count >= 64K, otherwise
     *  FATBPB20::cTotalSectors16 is used.  Zero if 64-bit value used with FAT32. */
    uint32_t        cTotalSectors32;
} FATBPB331;
AssertCompileSize(FATBPB331, 0x19);
/** Pointer to a DOS 3.31 BPB. */
typedef FATBPB331 *PFATBPB331;
/** Pointer to a const DOS 3.31 BPB. */
typedef FATBPB331 const *PCFATBPB331;

/**
 * The DOS 3.31 BPB, flattened structure.
 */
typedef struct FATBPB331FLAT
{
    /** @name New in DOS 2.0
     * @{ */
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
    uint16_t        cTotalSectors16;
    /** 0x15 / 0x0a: Media ID. */
    uint8_t         bMedia;
    /** 0x16 / 0x0b: Number of sectors per FAT (0 for FAT32). */
    uint16_t        cSectorsPerFat;
    /** @} */
    /** @name New in DOS 3.0
     * @{ */
    /** 0x18 / 0x0d: Sectors per track. Zero means reserved and not used. */
    uint16_t        cSectorsPerTrack;
    /** 0x1a / 0x0f: Number of heads. Zero means reserved and not used. */
    uint16_t        cTracksPerCylinder;
    /** @} */
    /** @name New in DOS 3.31
     * @{ */
    /** 0x1c / 0x11: Number of hidden sectors preceeding the volume.  This is zero
     * on unpartitioned media.  Values higher than 65535 are complicated due to
     * the field overlapping FATBPB32::cAnotherTotalSectors */
    uint32_t        cHiddenSectors;
    /** 0x20 / 0x15: Total logical sectors.  Used if count >= 64K, otherwise
     *  FATBPB20::cTotalSectors16 is used.  Zero if 64-bit value used with FAT32. */
    uint32_t        cTotalSectors32;
    /** @} */
} FATBPB331FLAT;
AssertCompileSize(FATBPB331FLAT, 0x19);
/** Pointer to a flattened DOS 3.31 BPB. */
typedef FATBPB331FLAT *PFATBPB331FLAT;
/** Pointer to a const flattened DOS 3.31 BPB. */
typedef FATBPB331FLAT *PFATBPB331FLAT;


/**
 * Extended BIOS parameter block (EBPB).
 */
typedef struct FATEBPB
{
    /** The BPB.  */
    FATBPB331FLAT   Bpb;

    /** 0x24 / 0x19: BIOS INT13 pysical drive number. */
    uint8_t         bInt13Drive;
    /** 0x25 / 0x1a: Reserved. NT used bit 0 for indicating dirty FS, and bit 1
     * for surface scan. */
    uint8_t         bReserved;
    /** 0x26 / 0x1b: Extended boot signature, FATEBPB_SIGNATURE or
     * FATEBPB_SIGNATURE_OLD. */
    uint8_t         bExtSignature;
    /** 0x27 / 0x1c: The volume serial number. */
    uint32_t        uSerialNumber;
    /** 0x2b / 0x20: The volume label (space padded).
     * @remarks Not available with FATEBPB_SIGNATURE_OLD  */
    char            achLabel[11];
    /** 0x36 / 0x2b: The file system type (space padded).
     * @remarks Not available with FATEBPB_SIGNATURE_OLD  */
    char            achType[8];
} FATEBPB;
AssertCompileSize(FATEBPB, 0x33);
/** Pointer to an extended BIOS parameter block. */
typedef FATEBPB *PFATEBPB;
/** Pointer to a const extended BIOS parameter block. */
typedef FATEBPB const *PCFATEBPB;

/** FATEBPB::bExtSignature value. */
#define FATEBPB_SIGNATURE       UINT8_C(0x29)
/** FATEBPB::bExtSignature value used by OS/2 1.0-1.1 and PC DOS 3.4.  These
 * does not have the volume and file system type. */
#define FATEBPB_SIGNATURE_OLD   UINT8_C(0x28)

/**FATEBPB::achType value for FAT12. */
#define FATEBPB_TYPE_FAT12      "FAT12   "
/**FATEBPB::achType value for FAT16. */
#define FATEBPB_TYPE_FAT16      "FAT16   "
/**FATEBPB::achType value for FAT12/FAT16. */
#define FATEBPB_TYPE_FAT        "FAT32   "


/**
 * FAT32 Extended BIOS parameter block (EBPB).
 */
typedef struct FAT32EBPB
{
    /** The BPB.  */
    FATBPB331FLAT   Bpb;

    /** 0x24 / 0x19: Number of sectors per FAT.
     * @note To avoid confusion with the FATEBPB signature, values which result in
     *       0x00280000 or 0x00290000 when masked by 0x00ff0000 must not be used. */
    uint32_t        cSectorsPerFat32;
    /** 0x28 / 0x1d: Flags pertaining to FAT mirroring and other stuff. */
    uint16_t        fFlags;
    /** 0x2a / 0x1f: FAT32 version number (FAT32EBPB_VERSION_0_0). */
    uint16_t        uVersion;
    /** 0x2c / 0x21: Cluster number of the root directory. */
    uint32_t        uRootDirCluster;
    /** 0x30 / 0x25: Logical sector number of the information sector. */
    uint16_t        uInfoSectorNo;
    /** 0x32 / 0x27: Logical sector number of boot sector copy. */
    uint16_t        uBootSectorCopySectorNo;
    /** 0x34 / 0x29: Reserved, zero (or 0xf6) filled, preserve. */
    uint8_t         abReserved[12];

    /** 0x40 / 0x35: BIOS INT13 pysical drive number
     * @remarks Same as FATEBPB::bInt13Drive. */
    uint8_t         bInt13Drive;
    /** 0x41 / 0x36: Reserved.
     * @remarks Same as FATEBPB::bReserved. */
    uint8_t         bReserved;
    /** 0x42 / 0x37: Extended boot signature (FATEBPB_SIGNATURE, or
     *  FATEBPB_SIGNATURE_OLD in some special cases).
     * @remarks Same as FATEBPB::bExtSignature. */
    uint8_t         bExtSignature;
    /** 0x43 / 0x38: The volume serial number.
     * @remarks Same as FATEBPB::uSerialNumber. */
    uint32_t        uSerialNumber;
    /** 0x47 / 0x3c: The volume label (space padded).
     * @remarks Not available with FATEBPB_SIGNATURE_OLD
     * @remarks Same as FATEBPB::achLabel. */
    char            achLabel[11];
    /** 0x52 / 0x47: The file system type (space padded), or 64-bit logical sector
     * count if both other count fields are zero.  In the latter case, the type is
     * moved to the OEM name field (FATBOOTSECTOR::achOemName).
     *
     * @remarks Not available with FATEBPB_SIGNATURE_OLD
     * @remarks Same as FATEBPB::achType. */
    union
    {
        char        achType[8];
        uint64_t    cTotalSectors64;
    } u;
} FAT32EBPB;
AssertCompileSize(FAT32EBPB, 0x4f);
/** Pointer to a FAT32 extended BIOS parameter block. */
typedef FAT32EBPB *PFAT32EBPB;
/** Pointer to a const FAT32 extended BIOS parameter block. */
typedef FAT32EBPB const *PCFAT32EBPB;

/** FAT32 version 0.0 (FAT32EBPB::uVersion). */
#define FAT32EBPB_VERSION_0_0   UINT16_C(0x0000)


/**
 * FAT boot sector layout.
 */
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
        FATBPB20        Bpb20;
        FATBPB32FLAT    Bpb30;
        FATBPB32FLAT    Bpb32;
        FATBPB331FLAT   Bpb331;
        FATEBPB         Ebpb;
        FAT32EBPB       Fat32Ebpb;
    } Bpb;
    /** 0x05a: Bootloader code/data/stuff. */
    uint8_t             abStuff[0x1a3];
    /** 0x1fd: Old drive number location (DOS 3.2-3.31). */
    uint8_t             bOldInt13Drive;
    /** 0x1fe: DOS signature (FATBOOTSECTOR_SIGNATURE). */
    uint16_t            uSignature;
} FATBOOTSECTOR;
AssertCompileSize(FATBOOTSECTOR, 0x200);
/** Pointer to a FAT boot sector. */
typedef FATBOOTSECTOR *PFATBOOTSECTOR;
/** Pointer to a const FAT boot sector. */
typedef FATBOOTSECTOR const *PCFATBOOTSECTOR;

/** Boot sector signature (FATBOOTSECTOR::uSignature). */
#define FATBOOTSECTOR_SIGNATURE     UINT16_C(0xaa55)

#pragma pack()

#endif

