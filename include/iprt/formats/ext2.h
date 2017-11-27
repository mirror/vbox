/* $Id$ */
/** @file
 * IPRT Filesystem API (FileSys) - ext2/3 format.
 */

/*
 * Copyright (C) 2012-2017 Oracle Corporation
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

#ifndef ___iprt_formats_ext2_h
#define ___iprt_formats_ext2_h

#include <iprt/types.h>
#include <iprt/assertcompile.h>


/*
 * The filesystem structures are from http://wiki.osdev.org/Ext2 and
 * http://www.nongnu.org/ext2-doc/ext2.html
 */

/**
 * Ext2 superblock.
 *
 * Everything is stored little endian on the disk.
 */
typedef struct EXT2SUPERBLOCK
{
    /** Total number of inodes in the filesystem. */
    uint32_t    cInodesTotal;
    /** Total number of blocks in the filesystem. */
    uint32_t    cBlocksTotal;
    /** Number of blocks reserved for the super user. */
    uint32_t    cBlocksRsvdForSuperUser;
    /** Total number of unallocated blocks. */
    uint32_t    cBlocksUnallocated;
    /** Total number of unallocated inodes. */
    uint32_t    cInodesUnallocated;
    /** Block number of block containing the superblock. */
    uint32_t    iBlockOfSuperblock;
    /** Number of bits to shift 1024 to the left to get the block size */
    uint32_t    cBitsShiftLeftBlockSize;
    /** Number of bits to shift 1024 to the left to get the fragment size */
    uint32_t    cBitsShiftLeftFragmentSize;
    /** Number of blocks in each block group. */
    uint32_t    cBlocksPerGroup;
    /** Number of fragments in each block group. */
    uint32_t    cFragmentsPerBlockGroup;
    /** Number of inodes in each block group. */
    uint32_t    cInodesPerBlockGroup;
    /** Last mount time. */
    uint32_t    u32LastMountTime;
    /** Last written time. */
    uint32_t    u32LastWrittenTime;
    /** Number of times the volume was mounted since the last check. */
    uint16_t    cMountsSinceLastCheck;
    /** Number of mounts allowed before a consistency check. */
    uint16_t    cMaxMountsUntilCheck;
    /** Signature to identify a ext2 volume (EXT2_SIGNATURE). */
    uint16_t    u16Signature;
    /** State of the filesystem (clean/errors) */
    uint16_t    u16FilesystemState;
    /** What to do on an error. */
    uint16_t    u16ActionOnError;
    /** Minor version field. */
    uint16_t    u16VersionMinor;
    /** Time of last check. */
    uint32_t    u32LastCheckTime;
    /** Interval between consistency checks. */
    uint32_t    u32CheckInterval;
    /** Operating system ID of the filesystem creator. */
    uint32_t    u32OsIdCreator;
    /** Major version field. */
    uint32_t    u32VersionMajor;
    /** User ID that is allowed to use reserved blocks. */
    uint16_t    u16UidReservedBlocks;
    /** Group ID that is allowed to use reserved blocks. */
    uint16_t    u16GidReservedBlocks;
    /** Reserved fields. */
    uint8_t     abReserved[940];
} EXT2SUPERBLOCK;
AssertCompileSize(EXT2SUPERBLOCK, 1024);
/** Pointer to an ext2 super block. */
typedef EXT2SUPERBLOCK *PEXT2SUPERBLOCK;
/** Pointer to a const ext2 super block. */
typedef EXT2SUPERBLOCK const *PCEXT2SUPERBLOCK;

/** Ext2 signature. */
#define EXT2_SIGNATURE      UINT16_C(0xef53)
/** Clean filesystem state. */
#define EXT2_STATE_CLEAN    UINT16_C(0x0001)
/** Error filesystem state. */
#define EXT2_STATE_ERRORS   UINT16_C(0x0002)

/**
 * Block group descriptor.
 */
typedef struct EXT2BLOCKGROUPDESC
{
    /** Block address of the block bitmap. */
    uint32_t    offBlockBitmap;
    /** Block address of the inode bitmap. */
    uint32_t    offInodeBitmap;
    /** Start block address of the inode table. */
    uint32_t    offInodeTable;
    /** Number of unallocated blocks in group. */
    uint16_t    cBlocksUnallocated;
    /** Number of unallocated inodes in group. */
    uint16_t    cInodesUnallocated;
    /** Number of directories in the group. */
    uint16_t    cDirectories;
    /** Padding. */
    uint16_t    u16Padding;
    /** Reserved. */
    uint8_t     abReserved[12];
} EXT2BLOCKGROUPDESC;
AssertCompileSize(EXT2BLOCKGROUPDESC, 32);
/** Pointer to an ext block group descriptor. */
typedef EXT2BLOCKGROUPDESC *PEXT2BLOCKGROUPDESC;

#endif

