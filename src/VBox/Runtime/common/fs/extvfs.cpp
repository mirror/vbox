/* $Id$ */
/** @file
 * IPRT - Ext2/3/4 Virtual Filesystem.
 */

/*
 * Copyright (C) 2012-2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include <iprt/fsvfs.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/avl.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/ext.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The maximum block group cache size (in bytes). */
#if ARCH_BITS >= 64
# define RTFSEXT_MAX_BLOCK_GROUP_CACHE_SIZE _512K
#else
# define RTFSEXT_MAX_BLOCK_GROUP_CACHE_SIZE _128K
#endif
/** The maximum inode cache size (in bytes). */
#if ARCH_BITS >= 64
# define RTFSEXT_MAX_INODE_CACHE_SIZE       _512K
#else
# define RTFSEXT_MAX_INODE_CACHE_SIZE       _128K
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the ext filesystem data. */
typedef struct RTFSEXTVOL *PRTFSEXTVOL;


/**
 * Cached block group descriptor data.
 */
typedef struct RTFSEXTBLKGRP
{
    /** AVL tree node, indexed by the block group number. */
    AVLU32NODECORE    Core;
    /** List node for the LRU list used for eviction. */
    RTLISTNODE        NdLru;
    /** Reference counter. */
    volatile uint32_t cRefs;
    /** Block number where the inode table is store. */
    uint64_t          iBlockInodeTbl;
    /** Pointer to the inode bitmap. */
    uint8_t           *pabInodeBitmap;
    /** Block bitmap - variable in size (depends on the block size
     * and number of blocks per group). */
    uint8_t           abBlockBitmap[1];
} RTFSEXTBLKGRP;
/** Pointer to block group descriptor data. */
typedef RTFSEXTBLKGRP *PRTFSEXTBLKGRP;


/**
 * In-memory inode.
 */
typedef struct RTFSEXTINODE
{
    /** AVL tree node, indexed by the inode number. */
    AVLU32NODECORE    Core;
    /** List node for the inode LRU list used for eviction. */
    RTLISTNODE        NdLru;
    /** Reference counter. */
    volatile uint32_t cRefs;
    /** Block number where the inode is stored. */
    uint64_t          iBlockInode;
    /** Offset in bytes into block where the inode is stored. */
    uint32_t          offInode;
    /** Inode data size. */
    uint64_t          cbData;
} RTFSEXTINODE;
/** Pointer to an in-memory inode. */
typedef RTFSEXTINODE *PRTFSEXTINODE;


/**
 * Open directory instance.
 */
typedef struct RTFSEXTDIR
{
    /** Volume this directory belongs to. */
    PRTFSEXTVOL         pVol;
    /** Set if we've reached the end of the directory enumeration. */
    bool                fNoMoreFiles;
} RTFSEXTDIR;
/** Pointer to an open directory instance. */
typedef RTFSEXTDIR *PRTFSEXTDIR;


/**
 * Ext2/3/4 filesystem volume.
 */
typedef struct RTFSEXTVOL
{
    /** Handle to itself. */
    RTVFS               hVfsSelf;
    /** The file, partition, or whatever backing the ext volume. */
    RTVFSFILE           hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t            cbBacking;

    /** RTVFSMNT_F_XXX. */
    uint32_t            fMntFlags;
    /** RTFSEXTVFS_F_XXX (currently none defined). */
    uint32_t            fExtFlags;

    /** Flag whether the filesystem is 64bit. */
    bool                f64Bit;
    /** Size of one block. */
    size_t              cbBlock;
    /** Number of bits to shift left for fast conversion of block numbers to offsets. */
    uint32_t            cBlockShift;
    /** Number of blocks in one group. */
    uint32_t            cBlocksPerGroup;
    /** Number of inodes in each block group. */
    uint32_t            cInodesPerGroup;
    /** Number of blocks groups in the volume. */
    uint32_t            cBlockGroups;
    /** Size of the block bitmap. */
    size_t              cbBlockBitmap;
    /** Size of the inode bitmap. */
    size_t              cbInodeBitmap;
    /** Size of block group descriptor. */
    size_t              cbBlkGrpDesc;
    /** Size of an inode. */
    size_t              cbInode;

    /** @name Block group cache.
     * @{ */
    /** LRU list anchor. */
    RTLISTANCHOR        LstBlockGroupLru;
    /** Root of the cached block group tree. */
    AVLU32TREE          BlockGroupRoot;
    /** Size of the cached block groups. */
    size_t              cbBlockGroups;
    /** @} */

    /** @name Inode cache.
     * @{ */
    /** LRU list anchor for the inode cache. */
    RTLISTANCHOR        LstInodeLru;
    /** Root of the cached inode tree. */
    AVLU32TREE          InodeRoot;
    /** Size of the cached inodes. */
    size_t              cbInodes;
    /** @} */
} RTFSEXTVOL;



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

#ifdef LOG_ENABLED
/**
 * Logs the ext filesystem superblock.
 *
 * @returns nothing.
 * @param   pSb                 Pointer to the superblock.
 */
static void rtFsExtSb_Log(PCEXTSUPERBLOCK pSb)
{
    if (LogIs2Enabled())
    {
        RTTIMESPEC Spec;
        char       sz[80];

        Log2(("EXT: Superblock:\n"));
        Log2(("EXT:   cInodesTotal                %RU32\n", RT_LE2H_U32(pSb->cInodesTotal)));
        Log2(("EXT:   cBlocksTotalLow             %RU32\n", RT_LE2H_U32(pSb->cBlocksTotalLow)));
        Log2(("EXT:   cBlocksRsvdForSuperUserLow  %RU32\n", RT_LE2H_U32(pSb->cBlocksRsvdForSuperUserLow)));
        Log2(("EXT:   cBlocksFreeLow              %RU32\n", RT_LE2H_U32(pSb->cBlocksFreeLow)));
        Log2(("EXT:   cInodesFree                 %RU32\n", RT_LE2H_U32(pSb->cInodesFree)));
        Log2(("EXT:   iBlockOfSuperblock          %RU32\n", RT_LE2H_U32(pSb->iBlockOfSuperblock)));
        Log2(("EXT:   cLogBlockSize               %RU32\n", RT_LE2H_U32(pSb->cLogBlockSize)));
        Log2(("EXT:   cLogClusterSize             %RU32\n", RT_LE2H_U32(pSb->cLogClusterSize)));
        Log2(("EXT:   cBlocksPerGroup             %RU32\n", RT_LE2H_U32(pSb->cBlocksPerGroup)));
        Log2(("EXT:   cClustersPerBlockGroup      %RU32\n", RT_LE2H_U32(pSb->cClustersPerBlockGroup)));
        Log2(("EXT:   cInodesPerBlockGroup        %RU32\n", RT_LE2H_U32(pSb->cInodesPerBlockGroup)));
        Log2(("EXT:   u32LastMountTime            %#RX32 %s\n", RT_LE2H_U32(pSb->u32LastMountTime),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pSb->u32LastMountTime)), sz, sizeof(sz))));
        Log2(("EXT:   u32LastWrittenTime          %#RX32 %s\n", RT_LE2H_U32(pSb->u32LastWrittenTime),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pSb->u32LastWrittenTime)), sz, sizeof(sz))));
        Log2(("EXT:   cMountsSinceLastCheck       %RU16\n", RT_LE2H_U32(pSb->cMountsSinceLastCheck)));
        Log2(("EXT:   cMaxMountsUntilCheck        %RU16\n", RT_LE2H_U32(pSb->cMaxMountsUntilCheck)));
        Log2(("EXT:   u16Signature                %#RX16\n", RT_LE2H_U32(pSb->u16Signature)));
        Log2(("EXT:   u16FilesystemState          %#RX16\n", RT_LE2H_U32(pSb->u16FilesystemState)));
        Log2(("EXT:   u16ActionOnError            %#RX16\n", RT_LE2H_U32(pSb->u16ActionOnError)));
        Log2(("EXT:   u16RevLvlMinor              %#RX16\n", RT_LE2H_U32(pSb->u16RevLvlMinor)));
        Log2(("EXT:   u32LastCheckTime            %#RX32 %s\n", RT_LE2H_U32(pSb->u32LastCheckTime),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pSb->u32LastCheckTime)), sz, sizeof(sz))));
        Log2(("EXT:   u32CheckInterval            %RU32\n", RT_LE2H_U32(pSb->u32CheckInterval)));
        Log2(("EXT:   u32OsIdCreator              %#RX32\n", RT_LE2H_U32(pSb->u32OsIdCreator)));
        Log2(("EXT:   u32RevLvl                   %#RX32\n", RT_LE2H_U32(pSb->u32RevLvl)));
        Log2(("EXT:   u16UidReservedBlocks        %#RX16\n", RT_LE2H_U32(pSb->u16UidReservedBlocks)));
        Log2(("EXT:   u16GidReservedBlocks        %#RX16\n", RT_LE2H_U32(pSb->u16GidReservedBlocks)));
        if (RT_LE2H_U32(pSb->u32RevLvl) == EXT_SB_REV_V2_DYN_INODE_SZ)
        {
            Log2(("EXT:   iFirstInodeNonRsvd          %#RX32\n", RT_LE2H_U32(pSb->iFirstInodeNonRsvd)));
            Log2(("EXT:   cbInode                     %#RX16\n", RT_LE2H_U32(pSb->cbInode)));
            Log2(("EXT:   iBlkGrpSb                   %#RX16\n", RT_LE2H_U32(pSb->iBlkGrpSb)));
            Log2(("EXT:   fFeaturesCompat             %#RX32%s%s%s%s%s%s%s%s%s%s\n", RT_LE2H_U32(pSb->fFeaturesCompat),
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_DIR_PREALLOC   ? " dir-prealloc"  : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_IMAGIC_INODES  ? " imagic-inode"  : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_HAS_JOURNAL    ? " has-journal"   : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_EXT_ATTR       ? " ext-attrs"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_RESIZE_INODE   ? " resize-inode"  : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_DIR_INDEX      ? " dir-index"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_LAZY_BG        ? " lazy-bg"       : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_EXCLUDE_INODE  ? " excl-inode"    : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_EXCLUDE_BITMAP ? " excl-bitmap"   : "",
                  RT_LE2H_U32(pSb->fFeaturesCompat) & EXT_SB_FEAT_COMPAT_SPARSE_SUPER2  ? " sparse-super2" : ""));
            Log2(("EXT:   fFeaturesIncompat           %#RX32%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", RT_LE2H_U32(pSb->fFeaturesIncompat),
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_COMPRESSION    ? " compression"   : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_DIR_FILETYPE   ? " dir-filetype"  : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_RECOVER        ? " recovery"      : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_JOURNAL_DEV    ? " journal-dev"   : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_META_BG        ? " meta-bg"       : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_EXTENTS        ? " extents"       : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_64BIT          ? " 64bit"         : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_MMP            ? " mmp"           : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_FLEX_BG        ? " flex-bg"       : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_EXT_ATTR_INODE ? " extattr-inode" : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_DIRDATA        ? " dir-data"      : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_CSUM_SEED      ? " csum-seed"     : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_LARGE_DIR      ? " large-dir"     : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_INLINE_DATA    ? " inline-data"   : "",
                  RT_LE2H_U32(pSb->fFeaturesIncompat) & EXT_SB_FEAT_INCOMPAT_ENCRYPT        ? " encrypt"       : ""));
            Log2(("EXT:   fFeaturesCompatRo           %#RX32%s%s%s%s%s%s%s%s%s%s%s%s%s%s\n", RT_LE2H_U32(pSb->fFeaturesCompatRo),
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_SPARSE_SUPER    ? " sparse-super"  : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_LARGE_FILE      ? " large-file"    : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_BTREE_DIR       ? " btree-dir"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_HUGE_FILE       ? " huge-file"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_GDT_CHSKUM      ? " gdt-chksum"    : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_DIR_NLINK       ? " dir-nlink"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_EXTRA_INODE_SZ  ? " extra-inode"   : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_HAS_SNAPSHOTS   ? " snapshots"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_QUOTA           ? " quota"         : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_BIGALLOC        ? " big-alloc"     : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_METADATA_CHKSUM ? " meta-chksum"   : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_REPLICA         ? " replica"       : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_READONLY        ? " ro"            : "",
                  RT_LE2H_U32(pSb->fFeaturesCompatRo) & EXT_SB_FEAT_COMPAT_RO_PROJECT         ? " project"       : ""));
            Log2(("EXT:   au8Uuid                     <todo>\n"));
            Log2(("EXT:   achVolumeName               %16s\n", &pSb->achVolumeName[0]));
            Log2(("EXT:   achLastMounted              %64s\n", &pSb->achLastMounted[0]));
            Log2(("EXT:   u32AlgoUsageBitmap          %#RX32\n", RT_LE2H_U32(pSb->u32AlgoUsageBitmap)));
            Log2(("EXT:   cBlocksPrealloc             %RU8\n", pSb->cBlocksPrealloc));
            Log2(("EXT:   cBlocksPreallocDirectory    %RU8\n", pSb->cBlocksPreallocDirectory));
            Log2(("EXT:   cGdtEntriesRsvd             %RU16\n", pSb->cGdtEntriesRsvd));
            Log2(("EXT:   au8JournalUuid              <todo>\n"));
            Log2(("EXT:   iJournalInode               %#RX32\n", RT_LE2H_U32(pSb->iJournalInode)));
            Log2(("EXT:   u32JournalDev               %#RX32\n", RT_LE2H_U32(pSb->u32JournalDev)));
            Log2(("EXT:   u32LastOrphan               %#RX32\n", RT_LE2H_U32(pSb->u32LastOrphan)));
            Log2(("EXT:   au32HashSeedHtree[0]        %#RX32\n", RT_LE2H_U32(pSb->au32HashSeedHtree[0])));
            Log2(("EXT:   au32HashSeedHtree[1]        %#RX32\n", RT_LE2H_U32(pSb->au32HashSeedHtree[1])));
            Log2(("EXT:   au32HashSeedHtree[2]        %#RX32\n", RT_LE2H_U32(pSb->au32HashSeedHtree[2])));
            Log2(("EXT:   au32HashSeedHtree[3]        %#RX32\n", RT_LE2H_U32(pSb->au32HashSeedHtree[3])));
            Log2(("EXT:   u8HashVersionDef            %#RX8\n", pSb->u8HashVersionDef));
            Log2(("EXT:   u8JnlBackupType             %#RX8\n", pSb->u8JnlBackupType));
            Log2(("EXT:   cbGroupDesc                 %RU16\n", RT_LE2H_U16(pSb->cbGroupDesc)));
            Log2(("EXT:   fMntOptsDef                 %#RX32\n", RT_LE2H_U32(pSb->fMntOptsDef)));
            Log2(("EXT:   iFirstMetaBg                %#RX32\n", RT_LE2H_U32(pSb->iFirstMetaBg)));
            Log2(("EXT:   u32TimeFsCreation           %#RX32 %s\n", RT_LE2H_U32(pSb->u32TimeFsCreation),
                  RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pSb->u32TimeFsCreation)), sz, sizeof(sz))));
            for (unsigned i = 0; i < RT_ELEMENTS(pSb->au32JnlBlocks); i++)
                Log2(("EXT:   au32JnlBlocks[%u]           %#RX32\n", i, RT_LE2H_U32(pSb->au32JnlBlocks[i])));
            Log2(("EXT:   cBlocksTotalHigh            %#RX32\n", RT_LE2H_U32(pSb->cBlocksTotalHigh)));
            Log2(("EXT:   cBlocksRsvdForSuperUserHigh %#RX32\n", RT_LE2H_U32(pSb->cBlocksRsvdForSuperUserHigh)));
            Log2(("EXT:   cBlocksFreeHigh             %#RX32\n", RT_LE2H_U32(pSb->cBlocksFreeHigh)));
            Log2(("EXT:   cbInodesExtraMin            %#RX16\n", RT_LE2H_U16(pSb->cbInodesExtraMin)));
            Log2(("EXT:   cbNewInodesRsv              %#RX16\n", RT_LE2H_U16(pSb->cbInodesExtraMin)));
            Log2(("EXT:   fFlags                      %#RX32\n", RT_LE2H_U32(pSb->fFlags)));
            Log2(("EXT:   cRaidStride                 %RU16\n", RT_LE2H_U16(pSb->cRaidStride)));
            Log2(("EXT:   cSecMmpInterval             %RU16\n", RT_LE2H_U16(pSb->cSecMmpInterval)));
            Log2(("EXT:   iMmpBlock                   %#RX64\n", RT_LE2H_U64(pSb->iMmpBlock)));
            Log2(("EXT:   cRaidStrideWidth            %#RX32\n", RT_LE2H_U32(pSb->cRaidStrideWidth)));
            Log2(("EXT:   cLogGroupsPerFlex           %RU8\n", pSb->cLogGroupsPerFlex));
            Log2(("EXT:   u8ChksumType                %RX8\n", pSb->u8ChksumType));
            Log2(("EXT:   cKbWritten                  %#RX64\n", RT_LE2H_U64(pSb->cKbWritten)));
            Log2(("EXT:   iSnapshotInode              %#RX32\n", RT_LE2H_U32(pSb->iSnapshotInode)));
            Log2(("EXT:   iSnapshotId                 %#RX32\n", RT_LE2H_U32(pSb->iSnapshotId)));
            Log2(("EXT:   cSnapshotRsvdBlocks         %#RX64\n", RT_LE2H_U64(pSb->cSnapshotRsvdBlocks)));
            Log2(("EXT:   iSnapshotListInode          %#RX32\n", RT_LE2H_U32(pSb->iSnapshotListInode)));
            Log2(("EXT:   cErrorsSeen                 %#RX32\n", RT_LE2H_U32(pSb->cErrorsSeen)));
            Log2(("EXT:   [...]\n")); /** @todo: Missing fields if becoming interesting. */
            Log2(("EXT:   iInodeLostFound             %#RX32\n", RT_LE2H_U32(pSb->iInodeLostFound)));
            Log2(("EXT:   iInodeProjQuota             %#RX32\n", RT_LE2H_U32(pSb->iInodeProjQuota)));
            Log2(("EXT:   u32ChksumSeed               %#RX32\n", RT_LE2H_U32(pSb->u32ChksumSeed)));
            Log2(("EXT:   [...]\n")); /** @todo: Missing fields if becoming interesting. */
            Log2(("EXT:   u32Chksum                   %#RX32\n", RT_LE2H_U32(pSb->u32Chksum)));
        }
    }
}


/**
 * Logs a ext filesystem block group descriptor.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   iBlockGroup         Block group number.
 * @param   pBlockGroup         Pointer to the block group.
 */
static void rtFsExtBlockGroup_Log(PRTFSEXTVOL pThis, uint32_t iBlockGroup, PCEXTBLOCKGROUPDESC pBlockGroup)
{
    if (LogIs2Enabled())
    {
        uint64_t iBlockStart = (uint64_t)iBlockGroup * pThis->cBlocksPerGroup;
        Log2(("EXT: Block group %#RX32 (blocks %#RX64 to %#RX64):\n",
              iBlockGroup, iBlockStart, iBlockStart + pThis->cBlocksPerGroup - 1));
        Log2(("EXT:   offBlockBitmapLow               %#RX32\n", RT_LE2H_U32(pBlockGroup->v32.offBlockBitmapLow)));
        Log2(("EXT:   offInodeBitmapLow               %#RX32\n", RT_LE2H_U32(pBlockGroup->v32.offInodeBitmapLow)));
        Log2(("EXT:   offInodeTableLow                %#RX32\n", RT_LE2H_U32(pBlockGroup->v32.offInodeTableLow)));
        Log2(("EXT:   cBlocksFreeLow                  %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.cBlocksFreeLow)));
        Log2(("EXT:   cInodesFreeLow                  %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.cInodesFreeLow)));
        Log2(("EXT:   cDirectoriesLow                 %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.cDirectoriesLow)));
        Log2(("EXT:   fFlags                          %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.fFlags)));
        Log2(("EXT:   offSnapshotExclBitmapLow        %#RX32\n", RT_LE2H_U32(pBlockGroup->v32.offSnapshotExclBitmapLow)));
        Log2(("EXT:   u16ChksumBlockBitmapLow         %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.u16ChksumBlockBitmapLow)));
        Log2(("EXT:   u16ChksumInodeBitmapLow         %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.u16ChksumInodeBitmapLow)));
        Log2(("EXT:   cInodeTblUnusedLow              %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.cInodeTblUnusedLow)));
        Log2(("EXT:   u16Chksum                       %#RX16\n", RT_LE2H_U16(pBlockGroup->v32.u16Chksum)));
        if (pThis->cbBlkGrpDesc == sizeof(EXTBLOCKGROUPDESC64))
        {
            Log2(("EXT:   offBlockBitmapHigh              %#RX32\n", RT_LE2H_U32(pBlockGroup->v64.offBlockBitmapHigh)));
            Log2(("EXT:   offInodeBitmapHigh              %#RX32\n", RT_LE2H_U32(pBlockGroup->v64.offInodeBitmapHigh)));
            Log2(("EXT:   offInodeTableHigh               %#RX32\n", RT_LE2H_U32(pBlockGroup->v64.offInodeTableHigh)));
            Log2(("EXT:   cBlocksFreeHigh                 %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.cBlocksFreeHigh)));
            Log2(("EXT:   cInodesFreeHigh                 %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.cInodesFreeHigh)));
            Log2(("EXT:   cDirectoriesHigh                %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.cDirectoriesHigh)));
            Log2(("EXT:   cInodeTblUnusedHigh             %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.cInodeTblUnusedHigh)));
            Log2(("EXT:   offSnapshotExclBitmapHigh       %#RX32\n", RT_LE2H_U32(pBlockGroup->v64.offSnapshotExclBitmapHigh)));
            Log2(("EXT:   u16ChksumBlockBitmapHigh        %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.u16ChksumBlockBitmapHigh)));
            Log2(("EXT:   u16ChksumInodeBitmapHigh        %#RX16\n", RT_LE2H_U16(pBlockGroup->v64.u16ChksumInodeBitmapHigh)));
        }
    }
}


/**
 * Logs a ext filesystem inode.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   iInode              Inode number.
 * @param   pInode              Pointer to the inode.
 */
static void rtFsExtInode_Log(PRTFSEXTVOL pThis, uint32_t iInode, PCEXTINODECOMB pInode)
{
    if (LogIs2Enabled())
    {
        RTTIMESPEC Spec;
        char       sz[80];

        Log2(("EXT: Inode %#RX32:\n", iInode));
        Log2(("EXT:   fMode                               %#RX16\n", RT_LE2H_U16(pInode->Core.fMode)));
        Log2(("EXT:   uUidLow                             %#RX16\n", RT_LE2H_U16(pInode->Core.uUidLow)));
        Log2(("EXT:   cbSizeLow                           %#RX32\n", RT_LE2H_U32(pInode->Core.cbSizeLow)));
        Log2(("EXT:   u32TimeLastAccess                   %#RX32 %s\n", RT_LE2H_U32(pInode->Core.u32TimeLastAccess),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pInode->Core.u32TimeLastAccess)), sz, sizeof(sz))));
        Log2(("EXT:   u32TimeLastChange                   %#RX32 %s\n", RT_LE2H_U32(pInode->Core.u32TimeLastChange),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pInode->Core.u32TimeLastChange)), sz, sizeof(sz))));
        Log2(("EXT:   u32TimeLastModification             %#RX32 %s\n", RT_LE2H_U32(pInode->Core.u32TimeLastModification),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pInode->Core.u32TimeLastModification)), sz, sizeof(sz))));
        Log2(("EXT:   u32TimeDeletion                     %#RX32 %s\n", RT_LE2H_U32(pInode->Core.u32TimeDeletion),
              RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pInode->Core.u32TimeDeletion)), sz, sizeof(sz))));
        Log2(("EXT:   uGidLow                             %#RX16\n", RT_LE2H_U16(pInode->Core.uGidLow)));
        Log2(("EXT:   cHardLinks                          %#RU16\n", RT_LE2H_U16(pInode->Core.cHardLinks)));
        Log2(("EXT:   cBlocksLow                          %#RX32\n", RT_LE2H_U32(pInode->Core.cBlocksLow)));
        Log2(("EXT:   fFlags                              %#RX32\n", RT_LE2H_U32(pInode->Core.fFlags)));
        Log2(("EXT:   Osd1.u32LnxVersion                  %#RX32\n", RT_LE2H_U32(pInode->Core.Osd1.u32LnxVersion)));
        for (unsigned i = 0; i < RT_ELEMENTS(pInode->Core.au32Block); i++)
            Log2(("EXT:   au32Block[%u]                       %#RX32\n", i, RT_LE2H_U32(pInode->Core.au32Block[i])));
        Log2(("EXT:   u32Version                          %#RX32\n", RT_LE2H_U32(pInode->Core.u32Version)));
        Log2(("EXT:   offExtAttrLow                       %#RX32\n", RT_LE2H_U32(pInode->Core.offExtAttrLow)));
        Log2(("EXT:   cbSizeHigh                          %#RX32\n", RT_LE2H_U32(pInode->Core.cbSizeHigh)));
        Log2(("EXT:   u32FragmentAddrObs                  %#RX32\n", RT_LE2H_U32(pInode->Core.u32FragmentAddrObs)));
        Log2(("EXT:   Osd2.Lnx.cBlocksHigh                %#RX32\n", RT_LE2H_U32(pInode->Core.Osd2.Lnx.cBlocksHigh)));
        Log2(("EXT:   Osd2.Lnx.offExtAttrHigh             %#RX32\n", RT_LE2H_U32(pInode->Core.Osd2.Lnx.offExtAttrHigh)));
        Log2(("EXT:   Osd2.Lnx.uUidHigh                   %#RX16\n", RT_LE2H_U16(pInode->Core.Osd2.Lnx.uUidHigh)));
        Log2(("EXT:   Osd2.Lnx.uGidHigh                   %#RX16\n", RT_LE2H_U16(pInode->Core.Osd2.Lnx.uGidHigh)));
        Log2(("EXT:   Osd2.Lnx.u16ChksumLow               %#RX16\n", RT_LE2H_U16(pInode->Core.Osd2.Lnx.u16ChksumLow)));

        if (pThis->cbInode >= sizeof(EXTINODECOMB))
        {
            Log2(("EXT:   cbInodeExtra                        %#RU16\n", RT_LE2H_U16(pInode->Extra.cbInodeExtra)));
            Log2(("EXT:   u16ChksumHigh                       %#RX16\n", RT_LE2H_U16(pInode->Extra.u16ChksumHigh)));
            Log2(("EXT:   u32ExtraTimeLastChange              %#RX32\n", RT_LE2H_U16(pInode->Extra.u32ExtraTimeLastChange)));
            Log2(("EXT:   u32ExtraTimeLastModification        %#RX32\n", RT_LE2H_U16(pInode->Extra.u32ExtraTimeLastModification)));
            Log2(("EXT:   u32ExtraTimeLastAccess              %#RX32\n", RT_LE2H_U16(pInode->Extra.u32ExtraTimeLastAccess)));
            Log2(("EXT:   u32TimeCreation                     %#RX32 %s\n", RT_LE2H_U32(pInode->Extra.u32TimeCreation),
                  RTTimeSpecToString(RTTimeSpecSetSeconds(&Spec, RT_LE2H_U32(pInode->Extra.u32TimeCreation)), sz, sizeof(sz))));
            Log2(("EXT:   u32ExtraTimeCreation                %#RX32\n", RT_LE2H_U16(pInode->Extra.u32ExtraTimeCreation)));
            Log2(("EXT:   u32VersionHigh                      %#RX32\n", RT_LE2H_U16(pInode->Extra.u32VersionHigh)));
            Log2(("EXT:   u32ProjectId                        %#RX32\n", RT_LE2H_U16(pInode->Extra.u32ProjectId)));
        }
    }
}
#endif


/**
 * Converts a block number to a byte offset.
 *
 * @returns Offset in bytes for the given block number.
 * @param   pThis               The ext volume instance.
 * @param   iBlock              The block number to convert.
 */
DECLINLINE(uint64_t) rtFsExtBlockIdxToDiskOffset(PRTFSEXTVOL pThis, uint64_t iBlock)
{
    return iBlock << pThis->cBlockShift;
}


/**
 * Converts a byte offset to a block number.
 *
 * @returns Block number.
 * @param   pThis               The ext volume instance.
 * @param   iBlock              The offset to convert.
 */
DECLINLINE(uint64_t) rtFsExtDiskOffsetToBlockIdx(PRTFSEXTVOL pThis, uint64_t off)
{
    return off >> pThis->cBlockShift;
}


/**
 * Creates the proper block number from the given low and high parts in case a 64bit
 * filesystem is used.
 *
 * @returns 64bit block number.
 * @param   pThis               The ext volume instance.
 * @param   uLow                The lower 32bit part.
 * @param   uHigh               The upper 32bit part.
 */
DECLINLINE(uint64_t) rtFsExtBlockFromLowHigh(PRTFSEXTVOL pThis, uint32_t uLow, uint32_t uHigh)
{
    return pThis->f64Bit ? RT_MAKE_U64(uLow, uHigh): uLow;
}


/**
 * Converts the given high and low parts of the block number to a byte offset.
 *
 * @returns Offset in bytes for the given block number.
 * @param   uLow                The lower 32bit part of the block number.
 * @param   uHigh               The upper 32bit part of the block number.
 */
DECLINLINE(uint64_t) rtFsExtBlockIdxLowHighToDiskOffset(PRTFSEXTVOL pThis, uint32_t uLow, uint32_t uHigh)
{
    uint64_t iBlock = rtFsExtBlockFromLowHigh(pThis, uLow, uHigh);
    return rtFsExtBlockIdxToDiskOffset(pThis, iBlock);
}


/**
 * Allocates a new block group.
 *
 * @returns Pointer to the new block group descriptor or NULL if out of memory.
 * @param   pThis               The ext volume instance.
 * @param   cbAlloc             How much to allocate.
 * @param   iBlockGroup         Block group number.
 */
static PRTFSEXTBLKGRP rtFsExtBlockGroupAlloc(PRTFSEXTVOL pThis, size_t cbAlloc, uint32_t iBlockGroup)
{
    PRTFSEXTBLKGRP pBlockGroup = (PRTFSEXTBLKGRP)RTMemAllocZ(cbAlloc);
    if (RT_LIKELY(pBlockGroup))
    {
        pBlockGroup->Core.Key       = iBlockGroup;
        pBlockGroup->cRefs          = 0;
        pBlockGroup->pabInodeBitmap = &pBlockGroup->abBlockBitmap[pThis->cbBlockBitmap];
        pThis->cbBlockGroups       += cbAlloc;
    }

    return pBlockGroup;
}


/**
 * Frees the given block group.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   pBlockGroup         The block group to free.
 */
static void rtFsExtBlockGroupFree(PRTFSEXTVOL pThis, PRTFSEXTBLKGRP pBlockGroup)
{
    Assert(!pBlockGroup->cRefs);

    /*
     * Put it into the cache if the limit wasn't exceeded, otherwise the block group
     * is freed right away.
     */
    if (pThis->cbBlockGroups <= RTFSEXT_MAX_BLOCK_GROUP_CACHE_SIZE)
    {
        /* Put onto the LRU list. */
        RTListPrepend(&pThis->LstBlockGroupLru, &pBlockGroup->NdLru);
    }
    else
    {
        /* Remove from the tree and free memory. */
        PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->BlockGroupRoot, pBlockGroup->Core.Key);
        Assert(pCore == &pBlockGroup->Core); RT_NOREF(pCore);
        RTMemFree(pBlockGroup);
        pThis->cbBlockGroups -= sizeof(RTFSEXTBLKGRP) + pThis->cbBlockBitmap + pThis->cbInodeBitmap;
    }
}


/**
 * Returns a new block group utilizing the cache if possible.
 *
 * @returns Pointer to the new block group descriptor or NULL if out of memory.
 * @param   pThis               The ext volume instance.
 * @param   iBlockGroup         Block group number.
 */
static PRTFSEXTBLKGRP rtFsExtBlockGroupGetNew(PRTFSEXTVOL pThis, uint32_t iBlockGroup)
{
    PRTFSEXTBLKGRP pBlockGroup = NULL;
    size_t cbAlloc = sizeof(RTFSEXTBLKGRP) + pThis->cbBlockBitmap + pThis->cbInodeBitmap;
    if (pThis->cbBlockGroups + cbAlloc <= RTFSEXT_MAX_BLOCK_GROUP_CACHE_SIZE)
        pBlockGroup = rtFsExtBlockGroupAlloc(pThis, cbAlloc, iBlockGroup);
    else
    {
        pBlockGroup = RTListRemoveLast(&pThis->LstBlockGroupLru, RTFSEXTBLKGRP, NdLru);
        if (!pBlockGroup)
            pBlockGroup = rtFsExtBlockGroupAlloc(pThis, cbAlloc, iBlockGroup);
        else
        {
            /* Remove the block group from the tree because it gets a new key. */
            PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->BlockGroupRoot, pBlockGroup->Core.Key);
            Assert(pCore == &pBlockGroup->Core); RT_NOREF(pCore);
        }
    }

    Assert(!pBlockGroup->cRefs);
    pBlockGroup->Core.Key = iBlockGroup;
    pBlockGroup->cRefs    = 1;

    return pBlockGroup;
}


/**
 * Loads the given block group number and returns it on success.
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   iBlockGroup         The block group to load.
 * @param   ppBlockGroup        Where to store the block group on success.
 */
static int rtFsExtBlockGroupLoad(PRTFSEXTVOL pThis, uint32_t iBlockGroup, PRTFSEXTBLKGRP *ppBlockGroup)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the block group from the cache first. */
    PRTFSEXTBLKGRP pBlockGroup = (PRTFSEXTBLKGRP)RTAvlU32Get(&pThis->BlockGroupRoot, iBlockGroup);
    if (!pBlockGroup)
    {
        /* Slow path, load from disk. */
        pBlockGroup = rtFsExtBlockGroupGetNew(pThis, iBlockGroup);
        if (RT_LIKELY(pBlockGroup))
        {
            uint64_t          offRead =   rtFsExtBlockIdxToDiskOffset(pThis, pThis->cbBlock == _1K ? 2 : 1)
                                        + (uint64_t)iBlockGroup * pThis->cbBlkGrpDesc;
            EXTBLOCKGROUPDESC BlockGroupDesc;
            rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &BlockGroupDesc, pThis->cbBlkGrpDesc, NULL);
            if (RT_SUCCESS(rc))
            {
#ifdef LOG_ENABLED
                rtFsExtBlockGroup_Log(pThis, iBlockGroup, &BlockGroupDesc);
#endif
                pBlockGroup->iBlockInodeTbl =   RT_LE2H_U32(BlockGroupDesc.v32.offInodeTableLow)
                                              | ((pThis->cbBlkGrpDesc == sizeof(EXTBLOCKGROUPDESC64))
                                              ? (uint64_t)RT_LE2H_U32(BlockGroupDesc.v64.offInodeTableHigh) << 32
                                              : 0);

                offRead = rtFsExtBlockIdxLowHighToDiskOffset(pThis, RT_LE2H_U32(BlockGroupDesc.v32.offBlockBitmapLow),
                                                             RT_LE2H_U32(BlockGroupDesc.v64.offBlockBitmapHigh));
                rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &pBlockGroup->abBlockBitmap[0], pThis->cbBlockBitmap, NULL);
                if (RT_SUCCESS(rc))
                {
                    offRead = rtFsExtBlockIdxLowHighToDiskOffset(pThis, RT_LE2H_U32(BlockGroupDesc.v32.offInodeBitmapLow),
                                                                 RT_LE2H_U32(BlockGroupDesc.v64.offInodeBitmapHigh));
                    rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &pBlockGroup->pabInodeBitmap[0], pThis->cbInodeBitmap, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        bool fIns = RTAvlU32Insert(&pThis->BlockGroupRoot, &pBlockGroup->Core);
                        Assert(fIns); RT_NOREF(fIns);
                    }
                }
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Remove from current LRU list position and add to the beginning. */
        uint32_t cRefs = ASMAtomicIncU32(&pBlockGroup->cRefs);
        if (cRefs == 1) /* Block groups get removed from the LRU list if they are referenced. */
            RTListNodeRemove(&pBlockGroup->NdLru);
    }

    if (RT_SUCCESS(rc))
        *ppBlockGroup = pBlockGroup;
    else if (pBlockGroup)
    {
        ASMAtomicDecU32(&pBlockGroup->cRefs);
        rtFsExtBlockGroupFree(pThis, pBlockGroup); /* Free the block group. */
    }

    return rc;
}


/**
 * Releases a reference of the given block group.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   pBlockGroup         The block group to release.
 */
static void rtFsExtBlockGroupRelease(PRTFSEXTVOL pThis, PRTFSEXTBLKGRP pBlockGroup)
{
    uint32_t cRefs = ASMAtomicDecU32(&pBlockGroup->cRefs);
    if (!cRefs)
        rtFsExtBlockGroupFree(pThis, pBlockGroup);
}


/**
 * Allocates a new inode.
 *
 * @returns Pointer to the new inode or NULL if out of memory.
 * @param   pThis               The ext volume instance.
 * @param   iInode              Inode number.
 */
static PRTFSEXTINODE rtFsExtInodeAlloc(PRTFSEXTVOL pThis, uint32_t iInode)
{
    PRTFSEXTINODE pInode = (PRTFSEXTINODE)RTMemAllocZ(sizeof(RTFSEXTINODE));
    if (RT_LIKELY(pInode))
    {
        pInode->Core.Key = iInode;
        pInode->cRefs    = 0;
        pThis->cbInodes  += sizeof(RTFSEXTINODE);
    }

    return pInode;
}


/**
 * Frees the given inode.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   pInode              The inode to free.
 */
static void rtFsExtInodeFree(PRTFSEXTVOL pThis, PRTFSEXTINODE pInode)
{
    Assert(!pInode->cRefs);

    /*
     * Put it into the cache if the limit wasn't exceeded, otherwise the inode
     * is freed right away.
     */
    if (pThis->cbInodes <= RTFSEXT_MAX_INODE_CACHE_SIZE)
    {
        /* Put onto the LRU list. */
        RTListPrepend(&pThis->LstInodeLru, &pInode->NdLru);
    }
    else
    {
        /* Remove from the tree and free memory. */
        PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->InodeRoot, pInode->Core.Key);
        Assert(pCore == &pInode->Core); RT_NOREF(pCore);
        RTMemFree(pInode);
        pThis->cbInodes -= sizeof(RTFSEXTINODE);
    }
}


/**
 * Returns a new inodep utilizing the cache if possible.
 *
 * @returns Pointer to the new inode or NULL if out of memory.
 * @param   pThis               The ext volume instance.
 * @param   iInode              Inode number.
 */
static PRTFSEXTINODE rtFsExtInodeGetNew(PRTFSEXTVOL pThis, uint32_t iInode)
{
    PRTFSEXTINODE pInode = NULL;
    if (pThis->cbInodes + sizeof(RTFSEXTINODE) <= RTFSEXT_MAX_INODE_CACHE_SIZE)
        pInode = rtFsExtInodeAlloc(pThis, iInode);
    else
    {
        pInode = RTListRemoveLast(&pThis->LstInodeLru, RTFSEXTINODE, NdLru);
        if (!pInode)
            pInode = rtFsExtInodeAlloc(pThis, iInode);
        else
        {
            /* Remove the block group from the tree because it gets a new key. */
            PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->InodeRoot, pInode->Core.Key);
            Assert(pCore == &pInode->Core); RT_NOREF(pCore);
        }
    }

    Assert(!pInode->cRefs);
    pInode->Core.Key = iInode;
    pInode->cRefs    = 1;

    return pInode;
}


/**
 * Loads the given inode number and returns it on success.
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   iInode              The inode to load.
 * @param   ppInode             Where to store the inode on success.
 */
static int rtFsExtInodeLoad(PRTFSEXTVOL pThis, uint32_t iInode, PRTFSEXTINODE *ppInode)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the inode from the cache first. */
    PRTFSEXTINODE pInode = (PRTFSEXTINODE)RTAvlU32Get(&pThis->InodeRoot, iInode);
    if (!pInode)
    {
        /* Slow path, load from disk. */
        pInode = rtFsExtInodeGetNew(pThis, iInode);
        if (RT_LIKELY(pInode))
        {
            /* Calculate the block group and load that one first to get at the inode table location. */
            PRTFSEXTBLKGRP pBlockGroup = NULL;
            rc = rtFsExtBlockGroupLoad(pThis, (iInode - 1) / pThis->cInodesPerGroup, &pBlockGroup);
            if (RT_SUCCESS(rc))
            {
                uint32_t idxInodeInTbl = (iInode - 1) % pThis->cInodesPerGroup;
                uint64_t offRead =   rtFsExtBlockIdxToDiskOffset(pThis, pBlockGroup->iBlockInodeTbl)
                                   + idxInodeInTbl * pThis->cbInode;

                /* Release block group here already as it is not required. */
                rtFsExtBlockGroupRelease(pThis, pBlockGroup);

                EXTINODECOMB Inode;
                rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &Inode, RT_MIN(sizeof(Inode), pThis->cbInode), NULL);
                if (RT_SUCCESS(rc))
                {
#ifdef LOG_ENABLED
                    rtFsExtInode_Log(pThis, iInode, &Inode);
#endif
                    /** @todo Fill in data. */
                }
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Remove from current LRU list position and add to the beginning. */
        uint32_t cRefs = ASMAtomicIncU32(&pInode->cRefs);
        if (cRefs == 1) /* Inodes get removed from the LRU list if they are referenced. */
            RTListNodeRemove(&pInode->NdLru);
    }

    if (RT_SUCCESS(rc))
        *ppInode = pInode;
    else if (pInode)
    {
        ASMAtomicDecU32(&pInode->cRefs);
        rtFsExtInodeFree(pThis, pInode); /* Free the inode. */
    }

    return rc;
}


/**
 * Releases a reference of the given inode.
 *
 * @returns nothing.
 * @param   pThis               The ext volume instance.
 * @param   pInode              The inode to release.
 */
static void rtFsExtInodeRelease(PRTFSEXTVOL pThis, PRTFSEXTINODE pInode)
{
    uint32_t cRefs = ASMAtomicDecU32(&pInode->cRefs);
    if (!cRefs)
        rtFsExtInodeFree(pThis, pInode);
}


/*
 *
 * Directory instance methods
 * Directory instance methods
 * Directory instance methods
 *
 */

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsExtDir_Close(void *pvThis)
{
    PRTFSEXTDIR pThis = (PRTFSEXTDIR)pvThis;
    LogFlowFunc(("pThis=%p\n", pThis));
    RT_NOREF(pThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsExtDir_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSEXTDIR pThis = (PRTFSEXTDIR)pvThis;
    LogFlowFunc(("\n"));
    RT_NOREF(pThis, pObjInfo, enmAddAttr);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsExtDir_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsExtDir_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsExtDir_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpen}
 */
static DECLCALLBACK(int) rtFsExtDir_Open(void *pvThis, const char *pszEntry, uint64_t fOpen,
                                         uint32_t fFlags, PRTVFSOBJ phVfsObj)
{
    LogFlowFunc(("pszEntry='%s' fOpen=%#RX64 fFlags=%#x\n", pszEntry, fOpen, fFlags));
    PRTFSEXTDIR  pThis = (PRTFSEXTDIR)pvThis;
    PRTFSEXTVOL  pVol  = pThis->pVol;
    int rc = VINF_SUCCESS;

    RT_NOREF(pThis, pVol, phVfsObj, pszEntry, fFlags);

    /*
     * We cannot create or replace anything, just open stuff.
     */
    if (   (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_OPEN
        || (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_OPEN_CREATE)
    { /* likely */ }
    else
        return VERR_WRITE_PROTECT;

    LogFlow(("rtFsExtDir_Open(%s): returns %Rrc\n", pszEntry, rc));
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtFsExtDir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fMode, phVfsDir);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtFsExtDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, phVfsSymlink);
    LogFlowFunc(("\n"));
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtFsExtDir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                  RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, pszTarget, enmType, phVfsSymlink);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtFsExtDir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    RT_NOREF(pvThis, pszEntry, fType);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRenameEntry}
 */
static DECLCALLBACK(int) rtFsExtDir_RenameEntry(void *pvThis, const char *pszEntry, RTFMODE fType, const char *pszNewName)
{
    RT_NOREF(pvThis, pszEntry, fType, pszNewName);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtFsExtDir_RewindDir(void *pvThis)
{
    PRTFSEXTDIR pThis = (PRTFSEXTDIR)pvThis;
    LogFlowFunc(("\n"));

    pThis->fNoMoreFiles = false;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtFsExtDir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                            RTFSOBJATTRADD enmAddAttr)
{
    PRTFSEXTDIR     pThis = (PRTFSEXTDIR)pvThis;
    int             rc = VINF_SUCCESS;
    LogFlowFunc(("\n"));

    RT_NOREF(pThis, rc, pDirEntry, pcbDirEntry, enmAddAttr);

    /*
     * The End.
     */
    LogFlowFunc(("no more files\n"));
    pThis->fNoMoreFiles = true;
    return VERR_NO_MORE_FILES;
}


/**
 * EXT directory operations.
 */
static const RTVFSDIROPS g_rtFsExtDirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_DIR,
        "EXT Dir",
        rtFsExtDir_Close,
        rtFsExtDir_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_UOFFSETOF(RTVFSDIROPS, ObjSet) - RT_UOFFSETOF(RTVFSDIROPS, Obj),
        rtFsExtDir_SetMode,
        rtFsExtDir_SetTimes,
        rtFsExtDir_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsExtDir_Open,
    NULL /* pfnFollowAbsoluteSymlink */,
    NULL /* pfnOpenFile */,
    NULL /* pfnOpenDir */,
    rtFsExtDir_CreateDir,
    rtFsExtDir_OpenSymlink,
    rtFsExtDir_CreateSymlink,
    NULL /* pfnQueryEntryInfo */,
    rtFsExtDir_UnlinkEntry,
    rtFsExtDir_RenameEntry,
    rtFsExtDir_RewindDir,
    rtFsExtDir_ReadDir,
    RTVFSDIROPS_VERSION,
};


/**
 * Opens a directory by the given inode.
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   iInode              The inode to open.
 * @param   phVfsDir            Where to store the handle to the VFS directory on success.
 */
static int rtFsExtVol_OpenDirByInode(PRTFSEXTVOL pThis, uint32_t iInode, PRTVFSDIR phVfsDir)
{
    PRTFSEXTINODE pInode = NULL;
    int rc = rtFsExtInodeLoad(pThis, iInode, &pInode);
    if (RT_SUCCESS(rc))
        rtFsExtInodeRelease(pThis, pInode);
    RT_NOREF(phVfsDir, g_rtFsExtDirOps);
    return rc;
}


/*
 *
 * Volume level code.
 * Volume level code.
 * Volume level code.
 *
 */


/**
 * Checks whether the block range in the given block group is in use by checking the
 * block bitmap.
 *
 * @returns Flag whether the range is in use.
 * @param   pBlkGrpDesc         The block group to check for.
 * @param   iBlockStart         The starting block to check relative from the beginning of the block group.
 * @param   cBlocks             How many blocks to check.
 */
static bool rtFsExtIsBlockRangeInUse(PRTFSEXTBLKGRP pBlkGrpDesc, uint64_t iBlockStart, size_t cBlocks)
{
    /** @todo: Optimize with ASMBitFirstSet(). */
    while (cBlocks)
    {
        uint32_t idxByte = iBlockStart / 8;
        uint32_t iBit = iBlockStart % 8;

        if (pBlkGrpDesc->abBlockBitmap[idxByte] & RT_BIT(iBit))
            return true;

        cBlocks--;
        iBlockStart++;
    }

    return false;
}


static DECLCALLBACK(int) rtFsExtVolBlockGroupTreeDestroy(PAVLU32NODECORE pCore, void *pvUser)
{
    RT_NOREF(pvUser);

    PRTFSEXTBLKGRP pBlockGroup = (PRTFSEXTBLKGRP)pCore;
    Assert(!pBlockGroup->cRefs);
    RTMemFree(pBlockGroup);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsExtVol_Close(void *pvThis)
{
    PRTFSEXTVOL pThis = (PRTFSEXTVOL)pvThis;

    /* Destroy the block group tree. */
    RTAvlU32Destroy(&pThis->BlockGroupRoot, rtFsExtVolBlockGroupTreeDestroy, pThis);
    pThis->BlockGroupRoot = NULL;
    RTListInit(&pThis->LstBlockGroupLru);

    /*
     * Backing file and handles.
     */
    RTVfsFileRelease(pThis->hVfsBacking);
    pThis->hVfsBacking = NIL_RTVFSFILE;
    pThis->hVfsSelf    = NIL_RTVFS;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsExtVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsExtVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    PRTFSEXTVOL pThis = (PRTFSEXTVOL)pvThis;
    int rc = rtFsExtVol_OpenDirByInode(pThis, EXT_INODE_NR_ROOT_DIR, phVfsDir);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsExtVol_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    int            rc    = VINF_SUCCESS;
    PRTFSEXTVOL    pThis = (PRTFSEXTVOL)pvThis;

    *pfUsed = false;

    uint64_t iBlock  = rtFsExtDiskOffsetToBlockIdx(pThis, off);
    uint64_t cBlocks = rtFsExtDiskOffsetToBlockIdx(pThis, cb) + (cb % pThis->cbBlock ? 1 : 0);
    while (cBlocks > 0)
    {
        uint32_t const iBlockGroup    = iBlock / pThis->cBlocksPerGroup;
        uint32_t const iBlockRelStart = iBlock - iBlockGroup * pThis->cBlocksPerGroup;
        PRTFSEXTBLKGRP pBlockGroup = NULL;

        rc = rtFsExtBlockGroupLoad(pThis, iBlockGroup, &pBlockGroup);
        if (RT_FAILURE(rc))
            break;

        uint64_t cBlocksThis = RT_MIN(cBlocks, iBlockRelStart - pThis->cBlocksPerGroup);
        if (rtFsExtIsBlockRangeInUse(pBlockGroup, iBlockRelStart, cBlocksThis))
        {
            *pfUsed = true;
            break;
        }

        rtFsExtBlockGroupRelease(pThis, pBlockGroup);
        cBlocks -= cBlocksThis;
        iBlock  += cBlocksThis;
    }

    return rc;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsExtVolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "ExtVol",
        /* .pfnClose = */       rtFsExtVol_Close,
        /* .pfnQueryInfo = */   rtFsExtVol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsExtVol_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsExtVol_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};



/**
 * Loads the parameters from the given original ext2 format superblock (EXT_SB_REV_ORIG).
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   pSb                 The superblock to load.
 * @param   pErrInfo            Where to return additional error info.
 */
static int rtFsExtVolLoadAndParseSuperBlockV0(PRTFSEXTVOL pThis, PCEXTSUPERBLOCK pSb, PRTERRINFO pErrInfo)
{
    RT_NOREF(pErrInfo);

    /*
     * Linux never supported a differing cluster (also called fragment) size for
     * the original ext2 layout so we reject such filesystems as it is not clear what
     * the purpose is really.
     */
    if (RT_LE2H_U32(pSb->cLogBlockSize) != RT_LE2H_U32(pSb->cLogClusterSize))
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "EXT filesystem cluster and block size differ");

    pThis->f64Bit          = false;
    pThis->cBlockShift     = 10 + RT_LE2H_U32(pSb->cLogBlockSize);
    pThis->cbBlock         = UINT64_C(1) << pThis->cBlockShift;
    pThis->cbInode         = sizeof(EXTINODE);
    pThis->cbBlkGrpDesc    = sizeof(EXTBLOCKGROUPDESC32);
    pThis->cBlocksPerGroup = RT_LE2H_U32(pSb->cBlocksPerGroup);
    pThis->cInodesPerGroup = RT_LE2H_U32(pSb->cInodesPerBlockGroup);
    pThis->cBlockGroups    = RT_LE2H_U32(pSb->cBlocksTotalLow) / pThis->cBlocksPerGroup;
    pThis->cbBlockBitmap   = pThis->cBlocksPerGroup / 8;
    if (pThis->cBlocksPerGroup % 8)
        pThis->cbBlockBitmap++;
    pThis->cbInodeBitmap   = pThis->cInodesPerGroup / 8;
    if (pThis->cInodesPerGroup % 8)
        pThis->cbInodeBitmap++;

    return VINF_SUCCESS;
}


/**
 * Loads the parameters from the given ext superblock (EXT_SB_REV_V2_DYN_INODE_SZ).
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   pSb                 The superblock to load.
 * @param   pErrInfo            Where to return additional error info.
 */
static int rtFsExtVolLoadAndParseSuperBlockV1(PRTFSEXTVOL pThis, PCEXTSUPERBLOCK pSb, PRTERRINFO pErrInfo)
{
    if (RT_LE2H_U32(pSb->fFeaturesIncompat) != 0)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "EXT filesystem contains unsupported incompatible features: %RX32",
                                   RT_LE2H_U32(pSb->fFeaturesIncompat));
    if (   RT_LE2H_U32(pSb->fFeaturesCompatRo) != 0
        && !(pThis->fMntFlags & RTVFSMNT_F_READ_ONLY))
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "EXT filesystem contains unsupported readonly features: %RX32",
                                   RT_LE2H_U32(pSb->fFeaturesCompatRo));

    return VINF_SUCCESS;
}


/**
 * Loads and parses the superblock of the filesystem.
 *
 * @returns IPRT status code.
 * @param   pThis               The ext volume instance.
 * @param   pErrInfo            Where to return additional error info.
 */
static int rtFsExtVolLoadAndParseSuperblock(PRTFSEXTVOL pThis, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    EXTSUPERBLOCK Sb;
    rc = RTVfsFileReadAt(pThis->hVfsBacking, EXT_SB_OFFSET, &Sb, sizeof(EXTSUPERBLOCK), NULL);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading super block");

    /* Validate the superblock. */
    if (RT_LE2H_U16(Sb.u16Signature) != EXT_SB_SIGNATURE)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not EXT - Signature mismatch: %RX16", RT_LE2H_U16(Sb.u16Signature));

#ifdef LOG_ENABLED
    rtFsExtSb_Log(&Sb);
#endif

    if (RT_LE2H_U16(Sb.u16FilesystemState) == EXT_SB_STATE_ERRORS)
        return RTERRINFO_LOG_SET(pErrInfo, VERR_VFS_UNSUPPORTED_FORMAT, "EXT filesystem contains errors");

    if (RT_LE2H_U32(Sb.u32RevLvl) == EXT_SB_REV_ORIG)
        rc = rtFsExtVolLoadAndParseSuperBlockV0(pThis, &Sb, pErrInfo);
    else
        rc = rtFsExtVolLoadAndParseSuperBlockV1(pThis, &Sb, pErrInfo);

    return rc;
}


RTDECL(int) RTFsExtVolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fExtFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & ~RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fExtFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a VFS instance and initialize the data so rtFsExtVol_Close works.
     */
    RTVFS       hVfs;
    PRTFSEXTVOL pThis;
    int rc = RTVfsNew(&g_rtFsExtVolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsBacking    = hVfsFileIn;
        pThis->hVfsSelf       = hVfs;
        pThis->fMntFlags      = fMntFlags;
        pThis->fExtFlags      = fExtFlags;
        pThis->BlockGroupRoot = NULL;
        pThis->InodeRoot      = NULL;
        pThis->cbBlockGroups  = 0;
        pThis->cbInodes       = 0;
        RTListInit(&pThis->LstBlockGroupLru);
        RTListInit(&pThis->LstInodeLru);

        rc = RTVfsFileGetSize(pThis->hVfsBacking, &pThis->cbBacking);
        if (RT_SUCCESS(rc))
        {
            rc = rtFsExtVolLoadAndParseSuperblock(pThis, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                *phVfs = hVfs;
                return VINF_SUCCESS;
            }
        }

        RTVfsRelease(hVfs);
        *phVfs = NIL_RTVFS;
    }
    else
        RTVfsFileRelease(hVfsFileIn);

    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnValidate}
 */
static DECLCALLBACK(int) rtVfsChainExtVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
                                                   PRTVFSCHAINELEMSPEC pElement, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg);

    /*
     * Basic checks.
     */
    if (pElement->enmTypeIn != RTVFSOBJTYPE_FILE)
        return pElement->enmTypeIn == RTVFSOBJTYPE_INVALID ? VERR_VFS_CHAIN_CANNOT_BE_FIRST_ELEMENT : VERR_VFS_CHAIN_TAKES_FILE;
    if (   pElement->enmType != RTVFSOBJTYPE_VFS
        && pElement->enmType != RTVFSOBJTYPE_DIR)
        return VERR_VFS_CHAIN_ONLY_DIR_OR_VFS;
    if (pElement->cArgs > 1)
        return VERR_VFS_CHAIN_AT_MOST_ONE_ARG;

    /*
     * Parse the flag if present, save in pElement->uProvider.
     */
    bool fReadOnly = (pSpec->fOpenFile & RTFILE_O_ACCESS_MASK) == RTFILE_O_READ;
    if (pElement->cArgs > 0)
    {
        const char *psz = pElement->paArgs[0].psz;
        if (*psz)
        {
            if (!strcmp(psz, "ro"))
                fReadOnly = true;
            else if (!strcmp(psz, "rw"))
                fReadOnly = false;
            else
            {
                *poffError = pElement->paArgs[0].offSpec;
                return RTErrInfoSet(pErrInfo, VERR_VFS_CHAIN_INVALID_ARGUMENT, "Expected 'ro' or 'rw' as argument");
            }
        }
    }

    pElement->uProvider = fReadOnly ? RTVFSMNT_F_READ_ONLY : 0;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnInstantiate}
 */
static DECLCALLBACK(int) rtVfsChainExtVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                      PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                      PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsExtVolOpen(hVfsFileIn, (uint32_t)pElement->uProvider, (uint32_t)(pElement->uProvider >> 32), &hVfs, pErrInfo);
        RTVfsFileRelease(hVfsFileIn);
        if (RT_SUCCESS(rc))
        {
            *phVfsObj = RTVfsObjFromVfs(hVfs);
            RTVfsRelease(hVfs);
            if (*phVfsObj != NIL_RTVFSOBJ)
                return VINF_SUCCESS;
            rc = VERR_VFS_CHAIN_CAST_FAILED;
        }
    }
    else
        rc = VERR_VFS_CHAIN_CAST_FAILED;
    return rc;
}


/**
 * @interface_method_impl{RTVFSCHAINELEMENTREG,pfnCanReuseElement}
 */
static DECLCALLBACK(bool) rtVfsChainExtVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                           PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                           PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (   pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider
        || !pReuseElement->paArgs[0].uProvider)
        return true;
    return false;
}


/** VFS chain element 'ext'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainExtVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "ext",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a EXT file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainExtVol_Validate,
    /* pfnInstantiate = */      rtVfsChainExtVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainExtVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainExtVolReg, rtVfsChainExtVolReg);

