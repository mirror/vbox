/* $Id$ $Revision$ */
/** @file
 * vboximg-mount header file
 */

/*
 * Copyright (C) 2018-2019 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VBOX_INCLUDED_SRC_vboximg_mount_vboximg_mount_h
#define VBOX_INCLUDED_SRC_vboximg_mount_vboximg_mount_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#define BOOTSTRAP_CODE_AREA_SIZE    446         /** Impl. specific MBR & EBR block not relevant for our use */
#define GPT_PARTITION_ENTRY_NAME_SIZE 36        /** 72 bytes for GPT partition entry name, but treated as 36 UTF-16 */
#define GUID_STRING_LENGTH            36

#pragma pack(push,1)  /**** THE FOLLOWING STRUCTURES MUST NOT BE PADDED BY THE COMPILER ****/

typedef struct PartitionEntry                   /* Pos & size critical (See http://en.wikipedia.org/wiki/Master_boot_record) */
{
    uint8_t  bootIndicator;                     /** 0x80 = bootable, 0x00 = non-bootable */
    uint8_t  firstHead;                         /** CHS address of first absolute Head     from MBR entry */
    uint8_t  firstSector;                       /** CHS address of first absolute Sector   from MBR entry */
    uint8_t  firstCyl;                          /** CHS address of first absolute Cylinder from MBR entry*/
    uint8_t  type;                              /** partition type */
    uint8_t  lastHead;                          /** CHS address of last absolute Head      from MBR entry */
    uint8_t  lastSector;                        /** CHS address of last absolute Sector    from MBR entry*/
    uint8_t  lastCyl;                           /** CHS address of last absolute Cylinder  from MBR entry */
    uint32_t partitionLba;                      /** LBA of first sector in the partition */
    uint32_t partitionBlkCnt;                   /** partition block count (little endian) */
} MBRPARTITIONENTRY;

typedef struct MasterBootRecord                 /* Pos & size critical (See http://en.wikipedia.org/wiki/Master_boot_record) */
{
    char bootstrapCodeArea[BOOTSTRAP_CODE_AREA_SIZE];
    MBRPARTITIONENTRY partitionEntry[4];
    uint16_t signature;
} MBR_t; /* _t for type to avoid conflict with definition in vd.h */

typedef struct ExtendedBootRecord              /* Pos & size critical (See http://en.wikipedia.org/wiki/Extended_boot_record) */
{
    char unused[BOOTSTRAP_CODE_AREA_SIZE];
    MBRPARTITIONENTRY partitionEntry;          /** EBR-relative offset to 1st sector of partition */
    MBRPARTITIONENTRY chainingPartitionEntry;  /** EBR-relative offset to next EBR */
    MBRPARTITIONENTRY unusedPartitionEntry3;   /** Unused for Logical Partitions table entries (EBR) */
    MBRPARTITIONENTRY unusedPartitionEntry4;   /** Unused for Logical Partitions table entries (EBR) */
    uint16_t signature;                        /** Boot Record signature */
} EBR_t; /* _t for type to avoid possible future conflict with vd.h */

typedef struct gptPartitionTableHeader         /* Pos and size critical (See https://en.wikipedia.org/wiki/GUID_Partition_Table) */
{
    uint64_t      signature;                   /** Equiv values: "EFI PART", 0x5452415020494645ULL little-endian */
    uint32_t      revision;                    /** GPT 1.0 would be 0x00,0x00,x01,0x00 */
    uint32_t      cbHeader;                    /** Typically 0x5C,0x00,0x00,0x00 (e.g. 92, little-endian) */
    uint32_t      crc32OfHeader;               /** From [0:<header size>], little endian, zero while being calculated */
    uint32_t      MBZreserved1;                /** MBZ */
    uint64_t      headerLba;                   /** Location of this header copy */
    uint64_t      backupLba;                   /** Location of the other header copy */
    uint64_t      firstUsableLba;              /** Ending LBA of primary partition table, +1 */
    uint64_t      lastUsableLba;               /** Starting LBA of secondary partition tabel, -1 */
    uint128_t     diskUuid;                    /** Disk UUID */
    uint64_t      partitionEntriesLba;         /** Starting lba of partition entreis array (always 2 in primary copy) */
    uint32_t      cPartitionEntries;           /** Number of partitions in the array */
    uint32_t      cbPartitionEntry;            /** Size of a single partition entry (usually 0x80, e.g. 128) */
    uint32_t      crc32OfPartitionArray;       /** CRC32/zlib of partition array, little-endian */
    uint8_t       MBZreserved2[];              /** Zeros until end of block, 420 bytes for 512 block but can be more */
} PTH_t; /*_t for type, to avoid possible future conflict with vd.h */

typedef struct gptPartitionEntry
{
    uint128_t     partitionTypeGuid;           /** Partition type UUID (RFC4122: 128 bits, big endian, network byte order) */
    uint128_t     uniquePartitionGuid;         /** UUID unique to this partition */
    uint64_t      firstLba;                    /** First LBA of partition (little-endian) */
    uint64_t      lastLba;                     /** Last LBA of partition (inclusive, usually odd) */
    uint64_t      attrFlags;                   /** Attribute flags */
    uint16_t      partitionName[GPT_PARTITION_ENTRY_NAME_SIZE];  /** Partition Name, 72-bytes (e.g. 36 UTF-16LE code units) */
} GPTPARTITIONENTRY;

/* GPT partition attributes */

#define GPT_ATTR_REQUIRED_BY_PLATORM        0  /* bit number */
#define GPT_ATTR_EFI_SHOULD_IGNORE          1  /* bit number */
#define GPT_LEGACY_BIOS_BOOTABLE            2  /* bit number */
#define GPT_ATTR_PARTITION_READONLY         60 /* bit number */
#define GPT_ATTR_SHADOW_COPY                61 /* bit number */
#define GPT_ATTR_HIDDEN                     62 /* bit number */
#define GPT_ATTR_NO_DRIVE_LETTER            63 /* bit number */

#pragma pack(pop) /**** End of packed data declarations ****/

struct PartitionDesc
{
    uint8_t type;
    const char *desc;
} g_partitionDescTable[] = {
        { 0x00, "Empty" }, { 0x01, "FAT12" }, { 0x02, "XENIX" }, { 0x03, "XENIX" }, { 0x04, "FAT16 <32M" }, { 0x05, "Extended" },
        { 0x06, "FAT16" }, { 0x07, "HPFS; NTFS; exFAT; WinCE; QNX" }, { 0x08, "AIX; DOS 3.x; OS/2; QNX 1.x" },
        { 0x09, "AIX; Coherent" }, { 0x0a, "OS/2 Boot Mgr; Coherent" }, { 0x0b, "W95 FAT32" }, { 0x0c, "W95 FAT32 (LBA)" },
        { 0x0e, "W95 FAT16 (LBA)" }, { 0x0f, "W95 Ext'd (LBA)" }, { 0x10, "OPUS" }, { 0x11, "FAT12" }, { 0x12, "Diag. & FW; Service; Compaq; EISA; AST" },
        { 0x14, "FAT16; MAVERICK; OS/2" }, { 0x16, "FAT16" }, { 0x17, "HPFS; NTFS; exFAT; IFS" },
        { 0x18, "AST" }, { 0x1b, "OS/2 BootMgr" }, { 0x1c, "OS/2 BootMgr FAT16/LBA; ASUS Recovery FAT32/LBA" },
        { 0x1e, "OS/2 BootMgr Ext. LBA" }, { 0x20, "Windows Mobile" },  { 0x21, "HP Vol. Expansion; FSo2" }, { 0x22, "FSo2 ext. part" },
        { 0x23, "Windows Mobile "}, { 0x24, "NEC" }, {0x25, "Windows Mobile" }, {  0x27, "NTFS; MirOS, RooterBoot" },
        { 0x2a, "AtheOS" }, { 0x2b, "Syllable OS" }, { 0x32, "NOS" }, { 0x35, "JFS" }, { 0x38, "Plan; THEOS" },
        { 0x39, "Plan; THEOS" }, { 0x3c, "PartitionMagic" }, { 0x40, "Venix; PICK" }, { 0x41, "PPC; Linux; SFS" }, { 0x42, "Linux; Win 2000" },
        { 0x43, "Linux" }, { 0x44, "Wildfile" }, { 0x45, "Priam; Boot-US; EMUEL/ELAN (L2)" }, { 0x46, "EMUEL/ELAN (L2)" },
        { 0x47, "EMUEL/ELAN (L2)" }, {0x48, "EMUEL/ELAN" }, { 0x4a, "AdaOS; ALFS/THIN" }, { 0x4c, "ETH Oberon" }, { 0x4d, "QNX4.x Primary" },
        { 0x4e, "QNX4.x 2nd part " }, { 0x4f, "QNX4.x 3rd part; ETH Oberon boot " }, { 0x50, "OnTrack; LynxOS; Novel" },
        { 0x51, "OnTrack" }, { 0x52, "CP/M; System V/AT, V/386" }, { 0x53, "OnTrack" }, { 0x54, "OnTrack" }, { 0x55, "EZ-Drive" },
        { 0x56, "AT&T; EZ-Drive; VFeature" }, {0x57, "DrivePro" }, { 0x5c, "Priam" }, { 0x61, "SpeedStor" },
        { 0x63, "GNU; SCO Unix; ISC; UnixWare; SYSV/386; ix; MtXinu BSD 4.3 on Mach" }, { 0x64, "Netware, SpeedStor; PC-ARMOUR" },
        { 0x65, "Netware" }, { 0x66, "Netware" }, { 0x67, "Netware" }, { 0x68, "Netware" }, { 0x69, "Netware" }, { 0x70, "DiskSecure" },
        { 0x72, "APTI FAT12 (CHS, SFN)" }, { 0x75, "PC/IX" }, { 0x77, "VNDI, M2FS, M2CS" }, { 0x78, "XOSL bootloader" }, { 0x79, "APTI FAT16 (LBA, SFN)" },
        { 0x7a, "APTI FAT16 (LBA, SFN)" }, { 0x7b, "APTI FAT16 (LBA, SFN)" }, { 0x7c, "APTI FAT16 (LBA, SFN)" }, { 0x7d, "APTI FAT16 (LBA, SFN)" },
        { 0x7e, "PrimoCache" }, { 0x7f, "Alt OS Dev. Partition Std." }, { 0x80, "MINIX (old)" }, { 0x81, "Minix" },
        { 0x82, "Linux swap; Solaris; PRIMOS" }, { 0x83, "Linux" }, { 0x84, "OS/2; Rapid Start Tech." }, { 0x85, "Linux Ext." },
        { 0x86, "NTFS" }, { 0x87, "NTFS" }, { 0x88, "Linux" }, { 0x8e, "Linux" }, {0x90, "Free FDISK" }, { 0x91, "Free FDISK" },
        { 0x92, "Free FDISK" }, { 0x93, "Amoeba" }, { 0x94, "Amoeba" }, { 0x95, "EXOPC" }, { 0x96, "CHRP" }, { 0x97, "Free FDISK" },
        { 0x98, "ROM-DOS; Free FDISK" }, { 0x9a, "Free FDISK" }, { 0x9b, "Free FDISK" },
        { 0x9e, "VSTa; ForthOS" }, { 0x9f, "BSD/OS / BSDI" }, { 0xa0, "Phoenix; IBM; Toshiba; Sony" }, { 0xa1, "HP Volume Expansion; Phoenix; NEC" },
        { 0xa5, "FreeBSD" }, { 0xa6, "OpenBSD" }, { 0xa7, "NeXTSTEP" }, { 0xa8, "Darwin; macOS" }, { 0xa9, "NetBSD" }, { 0xab, "Darwin; GO! OS" },
        { 0xad, "RISC OS" },  { 0xae, "ShagOS"}, { 0xaf, "HFS / ShagOS" }, { 0xb0, "Boot-Star" }, {0xb1, "HP Vol. Expansion; QNX 6.x" },
        { 0xb2, "QNX 6.x" }, { 0xB3, "HP Vol. Expansion; QNX 6.x" }, { 0xb7, "BSDI; Win NT 4 Server" }, { 0xb8, "BSDI" },
        { 0xbb, "BootWizard; OS Selector;  Acronis True Image; Win NT 4 Server" }, { 0xbc, "Acronis" }, { 0xbe, "Solaris" }, { 0xbf, "Solaris" },
        { 0xc0, "DR-DOS; Multiuser DOS; REAL/32"}, { 0xc1, "DRDOS/sec" }, { 0xc2, "Power Boot" }, { 0xc3, "Power Boot" }, { 0xc4, "DRDOS/sec" },
        { 0xc6, "DRDOS/sec" }, { 0xc7, "Syrinx; Win NT Server" }, { 0xcb, "Win NT 4 Server" }, { 0xcc, "DR-DOS 7.0x; Win NT 4 Server" },
        { 0xcd, "CTOS" }, { 0xce, "DR-DOS 7.0x" }, { 0xcf, "DR-DOS 7.0x" }, { 0xd1, "Multiuser DOS" }, { 0xd4, "Multiuser DOS" },
        { 0xd5, "Multiuser DOS" }, { 0xd6, "Multiuser DOS" }, { 0xd8, "CP/M-86" }, { 0xda, "Non-FS data; Powercopy Backup" },
        { 0xdb, "CP/M; Concurrent DOS; CTOS; D800; DRMK " }, { 0xdd, "CTOS" }, { 0xde, "Dell" }, { 0xdf, "BootIt" }, { 0xe0, "Aviion" },
        { 0xe1, "SpeedStor" }, { 0xe2, "SpeedStor"}, { 0xe3, "SpeedStor" }, { 0xe4, "SpeedStor" }, { 0xe5, "Tandy MS-DOS" }, { 0xe6, "SpeedStor" },
        { 0xe8, "LUKS" },  { 0xea, "Rufus" }, { 0xeb, "BeOS" }, { 0xec, "SkyOS" }, { 0xed, "EFS; Sprytix" },  { 0xed, "EFS" },
        { 0xee, "GPT" }, { 0xef, "EFI" }, { 0xf0, "Linux; PA-RISC" }, { 0xf1, "SpeedStor" }, { 0xf2, "Sperry MS-DOS 3.x" },
        { 0xf3, "SpeedStor" }, { 0xf4, "SpeedStor; Prologue" }, { 0xf5, "Prologue" }, { 0xf6, "SpeedStor" }, { 0xf7, "O.S.G.; X1" },
        { 0xf9, "Linux" }, { 0xfa, "Bochs" }, { 0xfb, "VMware" }, { 0xfc, "VMware" }, { 0xfd, "Linux; FreeDOS" },
        { 0xfe, "SpeedStor; LANstep; PS/2; Win NT; Linux" }, { 0xff, "Xenis; BBT" },
};

typedef struct GptPartitionTypeTable
{
    const char *gptPartitionUuid;
    const char *osType;
    const char *gptPartitionTypeDesc;
} GPTPARTITIONTYPE;

GPTPARTITIONTYPE g_gptPartitionTypes[] =
{
    { "00000000-0000-0000-0000-000000000000", "", "Unused" },
    { "024DEE41-33E7-11D3-9D69-0008C781F39F", "", "MBR scheme" },
    { "C12A7328-F81F-11D2-BA4B-00A0C93EC93B", "", "EFI System" },
    { "21686148-6449-6E6F-744E-656564454649", "", "BIOS boot" },
    { "D3BFE2DE-3DAF-11DF-BA40-E3A556D89593", "", "Intel Fast Flash (iFFS)" },
    { "F4019732-066E-4E12-8273-346C5641494F", "", "Sony boot" },
    { "BFBFAFE7-A34F-448A-9A5B-6213EB736C22", "", "Lenovo boot" },
    { "E3C9E316-0B5C-4DB8-817D-F92DF00215AE", "Windows", "Microsoft Reserved" },
    { "EBD0A0A2-B9E5-4433-87C0-68B6B72699C7", "Windows", "Basic data" },
    { "5808C8AA-7E8F-42E0-85D2-E1E90434CFB3", "Windows", "Logical Disk Manager (LDM) metadata" },
    { "AF9B60A0-1431-4F62-BC68-3311714A69AD", "Windows", "Logical Disk Manager (LDM) data" },
    { "DE94BBA4-06D1-4D40-A16A-BFD50179D6AC", "Windows", "Windows Recovery Environment" },
    { "37AFFC90-EF7D-4E96-91C3-2D7AE055B174", "Windows", "IBM General Parallel File System (GPFS)" },
    { "E75CAF8F-F680-4CEE-AFA3-B001E56EFC2D", "Windows", "Storage Spaces" },
    { "75894C1E-3AEB-11D3-B7C1-7B03A0000000", "HP-UX", "Data" },
    { "E2A1E728-32E3-11D6-A682-7B03A0000000", "HP-UX", "Service" },
    { "0FC63DAF-8483-4772-8E79-3D69D8477DE4", "Linux", "Filesystem Data" },
    { "A19D880F-05FC-4D3B-A006-743F0F84911E", "Linux", "RAID" },
    { "44479540-F297-41B2-9AF7-D131D5F0458A", "Linux", "Root (x86)" },
    { "4F68BCE3-E8CD-4DB1-96E7-FBCAF984B709", "Linux", "Root (x86-64)" },
    { "69DAD710-2CE4-4E3C-B16C-21A1D49ABED3", "Linux", "Root (32-bit ARM)" },
    { "B921B045-1DF0-41C3-AF44-4C6F280D3FAE", "Linux", "Root (64-bit ARM/AArch64)" },
    { "A2A0D0EB-E5B9-3344-87C0-68B6B72699C7", "Linux", "Data" },
    { "AF3DC60F-8384-7247-8E79-3D69D8477DE4", "Linux", "Data" },
    { "0657FD6D-A4AB-43C4-84E5-0933C84B4F4F", "Linux", "Swap" },
    { "E6D6D379-F507-44C2-A23C-238F2A3DF928", "Linux", "LVM" },
    { "933AC7E1-2EB4-4F13-B844-0E14E2AEF915", "Linux", "/home" },
    { "3B8F8425-20E0-4F3B-907F-1A25A76F98E8", "Linux", "/srv " },
    { "7FFEC5C9-2D00-49B7-8941-3EA10A5586B7", "Linux", "Plain dm-crypt" },
    { "CA7D7CCB-63ED-4C53-861C-1742536059CC", "Linux", "LUKS" },
    { "8DA63339-0007-60C0-C436-083AC8230908", "Linux", "Reserved" },
    { "83BD6B9D-7F41-11DC-BE0B-001560B84F0F", "FreeBSD", "Boot" },
    { "516E7CB4-6ECF-11D6-8FF8-00022D09712B", "FreeBSD", "Data" },
    { "516E7CB5-6ECF-11D6-8FF8-00022D09712B", "FreeBSD", "Swap" },
    { "516E7CB6-6ECF-11D6-8FF8-00022D09712B", "FreeBSD",  "Unix File System (UFS)" },
    { "516E7CB8-6ECF-11D6-8FF8-00022D09712B", "FreeBSD", "Vinum volume manager" },
    { "516E7CBA-6ECF-11D6-8FF8-00022D09712B", "FreeBSD", "ZFS" },
    { "48465300-0000-11AA-AA11-00306543ECAC", "macOS Darwin", "Hierarchical File System Plus (HFS+)" },
    { "7C3457EF-0000-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple APFS" },
    { "55465300-0000-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple UFS container" },
    { "6A898CC3-1DD2-11B2-99A6-080020736631", "macOS Darwin", "ZFS" },
    { "52414944-0000-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple RAID" },
    { "52414944-5F4F-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple RAID , offline" },
    { "426F6F74-0000-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple Boot (Recovery HD)" },
    { "4C616265-6C00-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple Label" },
    { "5265636F-7665-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple TV Recovery" },
    { "53746F72-6167-11AA-AA11-00306543ECAC", "macOS Darwin", "Apple Core Storage (i.e. Lion FileVault)" },
    { "B6FA30DA-92D2-4A9A-96F1-871EC6486200", "macOS Darwin", "SoftRAID_Status" },
    { "2E313465-19B9-463F-8126-8A7993773801", "macOS Darwin", "SoftRAID_Scratch" },
    { "FA709C7E-65B1-4593-BFD5-E71D61DE9B02", "macOS Darwin", "SoftRAID_Volume" },
    { "BBBA6DF5-F46F-4A89-8F59-8765B2727503", "macOS Darwin", "SoftRAID_Cache" },
    { "6A82CB45-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Boot" },
    { "6A85CF4D-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Root" },
    { "6A87C46F-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Swap" },
    { "6A8B642B-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Backup" },
    { "6A898CC3-1DD2-11B2-99A6-080020736631", "Solaris illumos", "/usr" },
    { "6A8EF2E9-1DD2-11B2-99A6-080020736631", "Solaris illumos", "/var" },
    { "6A90BA39-1DD2-11B2-99A6-080020736631", "Solaris illumos", "/home" },
    { "6A9283A5-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Alternate sector" },
    { "6A945A3B-1DD2-11B2-99A6-080020736631", "Solaris illumos",  "Reserved" },
    { "6A9630D1-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Reserved" },
    { "6A980767-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Reserved"},
    { "6A96237F-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Reserved" },
    { "6A8D2AC7-1DD2-11B2-99A6-080020736631", "Solaris illumos", "Reserved" },
    { "49F48D32-B10E-11DC-B99B-0019D1879648", "NetBSD", "Swap" },
    { "49F48D5A-B10E-11DC-B99B-0019D1879648", "NetBSD", "FFS" },
    { "49F48D82-B10E-11DC-B99B-0019D1879648", "NetBSD", "LFS" },
    { "49F48DAA-B10E-11DC-B99B-0019D1879648", "NetBSD", "RAID" },
    { "2DB519C4-B10F-11DC-B99B-0019D1879648", "NetBSD", "Concatenated" },
    { "2DB519EC-B10F-11DC-B99B-0019D1879648", "NetBSD", "Encrypted" },
    { "FE3A2A5D-4F32-41A7-B725-ACCC3285A309", "Chrome OS", "kernel" },
    { "3CB8E202-3B7E-47DD-8A3C-7FF2A13CFCEC", "Chrome OS", "rootfs" },
    { "2E0A753D-9E48-43B0-8337-B15192CB1B5E", "Chrome OS", "future use" },
    { "5DFBF5F4-2848-4BAC-AA5E-0D9A20B745A6", "Container Linux", "/usr (coreos-usr)" },
    { "3884DD41-8582-4404-B9A8-E9B84F2DF50E", "Container Linux", "Resizable rootfs (coreos-resize)" },
    { "C95DC21A-DF0E-4340-8D7B-26CBFA9A03E0", "Container Linux",  "OEM customizations (coreos-reserved)" },
    { "BE9067B9-EA49-4F15-B4F6-F36F8C9E1818", "Container Linux", "Root filesystem on RAID (coreos-root-raid)" },
    { "42465331-3BA3-10F1-802A-4861696B7521",  "Haiku", "BFS" },
    { "85D5E45E-237C-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD", "Boot" },
    { "85D5E45A-237C-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD",  "Data" },
    { "85D5E45B-237C-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD", "Swap" },
    { "0394EF8B-237E-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD", "Unix File System (UFS)" },
    { "85D5E45C-237C-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD", "Vinum volume manager" },
    { "85D5E45D-237C-11E1-B4B3-E89A8F7FC3A7", "MidnightBSD", "ZFS" },
    { "45B0969E-9B03-4F30-B4C6-B4B80CEFF106", "Ceph", "Journal" },
    { "45B0969E-9B03-4F30-B4C6-5EC00CEFF106", "Ceph", "dm-crypt journal" },
    { "4FBD7E29-9D25-41B8-AFD0-062C0CEFF05D", "Ceph", "OSD" },
    { "4FBD7E29-9D25-41B8-AFD0-5EC00CEFF05D", "Ceph", "dm-crypt OSD" },
    { "89C57F98-2FE5-4DC0-89C1-F3AD0CEFF2BE", "Ceph", "Disk in creation" },
    { "89C57F98-2FE5-4DC0-89C1-5EC00CEFF2BE", "Ceph", "dm-crypt disk in creation" },
    { "CAFECAFE-9B03-4F30-B4C6-B4B80CEFF106", "Ceph", "Block" },
    { "30CD0809-C2B2-499C-8879-2D6B78529876", "Ceph", "Block DB" },
    { "5CE17FCE-4087-4169-B7FF-056CC58473F9", "Ceph", "Block write-ahead log" },
    { "FB3AABF9-D25F-47CC-BF5E-721D1816496B", "Ceph", "Lockbox for dm-crypt keys" },
    { "4FBD7E29-8AE0-4982-BF9D-5A8D867AF560", "Ceph", "Multipath OSD" },
    { "45B0969E-8AE0-4982-BF9D-5A8D867AF560", "Ceph", "Multipath journal" },
    { "CAFECAFE-8AE0-4982-BF9D-5A8D867AF560", "Ceph", "Multipath block" },
    { "7F4A666A-16F3-47A2-8445-152EF4D03F6C", "Ceph", "Multipath block" },
    { "EC6D6385-E346-45DC-BE91-DA2A7C8B3261", "Ceph", "Multipath block DB" },
    { "01B41E1B-002A-453C-9F17-88793989FF8F", "Ceph", "Multipath block write-ahead log" },
    { "CAFECAFE-9B03-4F30-B4C6-5EC00CEFF106", "Ceph", "dm-crypt block" },
    { "93B0052D-02D9-4D8A-A43B-33A3EE4DFBC3", "Ceph", "dm-crypt block DB" },
    { "306E8683-4FE2-4330-B7C0-00A917C16966", "Ceph", "dm-crypt block write-ahead log" },
    { "45B0969E-9B03-4F30-B4C6-35865CEFF106", "Ceph", "dm-crypt LUKS journal" },
    { "CAFECAFE-9B03-4F30-B4C6-35865CEFF106", "Ceph", "dm-crypt LUKS block" },
    { "166418DA-C469-4022-ADF4-B30AFD37F176", "Ceph", "dm-crypt LUKS block DB" },
    { "86A32090-3647-40B9-BBBD-38D8C573AA86", "Ceph", "dm-crypt LUKS block write-ahead log" },
    { "4FBD7E29-9D25-41B8-AFD0-35865CEFF05D", "Ceph", "dm-crypt LUKS OSD" },
    { "824CC7A0-36A8-11E3-890A-952519AD3F61", "OpenBSD", "Data" },
    { "CEF5A9AD-73BC-4601-89F3-CDEEEEE321A1", "QNX", "Power-safe (QNX6) file system[45]" },
    { "C91818F9-8025-47AF-89D2-F030D7000C2C", "Plan 9" },
    { "9D275380-40AD-11DB-BF97-000C2911D1B8", "VMware ESX",  "vmkcore (coredump )" },
    { "AA31E02A-400F-11DB-9590-000C2911D1B8", "VMware ESX", "VMFS filesystem" },
    { "9198EFFC-31C0-11DB-8F78-000C2911D1B8", "VMware ESX", "VMware Reserved" },
    { "2568845D-2332-4675-BC39-8FA5A4748D15", "Android-IA", "Bootloader" },
    { "114EAFFE-1552-4022-B26E-9B053604CF84", "Android-IA", "Bootloader2" },
    { "49A4D17F-93A3-45C1-A0DE-F50B2EBE2599", "Android-IA", "Boot" },
    { "4177C722-9E92-4AAB-8644-43502BFD5506", "Android-IA", "Recovery" },
    { "EF32A33B-A409-486C-9141-9FFB711F6266", "Android-IA", "Misc" },
    { "20AC26BE-20B7-11E3-84C5-6CFDB94711E9", "Android-IA", "Metadata" },
    { "38F428E6-D326-425D-9140-6E0EA133647C", "Android-IA", "System" },
    { "A893EF21-E428-470A-9E55-0668FD91A2D9", "Android-IA", "Cache" },
    { "DC76DDA9-5AC1-491C-AF42-A82591580C0D", "Android-IA", "Data" },
    { "EBC597D0-2053-4B15-8B64-E0AAC75F4DB1", "Android-IA", "Persistent" },
    { "C5A0AEEC-13EA-11E5-A1B1-001E67CA0C3C", "Android-IA", "Vendor" },
    { "BD59408B-4514-490D-BF12-9878D963F378", "Android-IA", "Config" },
    { "8F68CC74-C5E5-48DA-BE91-A0C8C15E9C80", "Android-IA", "Factory" },
    { "9FDAA6EF-4B3F-40D2-BA8D-BFF16BFB887B", "Android-IA", "Factory (alt)[50]" },
    { "767941D0-2085-11E3-AD3B-6CFDB94711E9", "Android-IA", "Fastboot / Tertiary" },
    { "AC6D7924-EB71-4DF8-B48D-E267B27148FF", "Android-IA", "OEM" },
    { "19A710A2-B3CA-11E4-B026-10604B889DCF", "Android 6.0+ ARM", "Android Meta" },
    { "193D1EA4-B3CA-11E4-B075-10604B889DCF", "Android 6.0+ ARM", "Android EXT" },
    { "7412F7D5-A156-4B13-81DC-867174929325", "ONIE", "Boot" },
    { "D4E6E2CD-4469-46F3-B5CB-1BFF57AFC149", "ONIE", "Config" },
    { "9E1A2D38-C612-4316-AA26-8B49521E5A8B", "PowerPC", "PReP boot" },
    { "BC13C2FF-59E6-4262-A352-B275FD6F7172", "freedesktop.org", "Shared boot loader configuration" },
    { "734E5AFE-F61A-11E6-BC64-92361F002671", "Atari TOS", "Basic data (GEM, BGM, F32)" },
};

#endif /* !VBOX_INCLUDED_SRC_vboximg_mount_vboximg_mount_h */
