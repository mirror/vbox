/* $Id$ */
/** @file
 * IPRT - XFS Virtual Filesystem.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#include <iprt/err.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/xfs.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The maximum allocation group cache size (in bytes). */
#if ARCH_BITS >= 64
# define RTFSXFS_MAX_AG_CACHE_SIZE          _512K
#else
# define RTFSXFS_MAX_AG_CACHE_SIZE          _128K
#endif
/** The maximum inode cache size (in bytes). */
#if ARCH_BITS >= 64
# define RTFSXFS_MAX_INODE_CACHE_SIZE       _512K
#else
# define RTFSXFS_MAX_INODE_CACHE_SIZE       _128K
#endif
/** The maximum extent tree cache size (in bytes). */
#if ARCH_BITS >= 64
# define RTFSXFS_MAX_BLOCK_CACHE_SIZE       _512K
#else
# define RTFSXFS_MAX_BLOCK_CACHE_SIZE       _128K
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/** Pointer to the XFS filesystem data. */
typedef struct RTFSXFSVOL *PRTFSXFSVOL;


/**
 * Cached allocation group descriptor data.
 */
typedef struct RTFSXFSAG
{
    /** AVL tree node, indexed by the allocation group number. */
    AVLU32NODECORE    Core;
    /** List node for the LRU list used for eviction. */
    RTLISTNODE        NdLru;
    /** Reference counter. */
    volatile uint32_t cRefs;
    /** @todo */
} RTFSXFSAG;
/** Pointer to allocation group descriptor data. */
typedef RTFSXFSAG *PRTFSXFSAG;


/**
 * In-memory inode.
 */
typedef struct RTFSXFSINODE
{
    /** AVL tree node, indexed by the inode number. */
    AVLU32NODECORE    Core;
    /** List node for the inode LRU list used for eviction. */
    RTLISTNODE        NdLru;
    /** Reference counter. */
    volatile uint32_t cRefs;
    /** Byte offset in the backing file where the inode is stored.. */
    uint64_t          offInode;
    /** Inode data. */
    RTFSOBJINFO       ObjInfo;
    /** @todo */
} RTFSXFSINODE;
/** Pointer to an in-memory inode. */
typedef RTFSXFSINODE *PRTFSXFSINODE;


/**
 * Block cache entry.
 */
typedef struct RTFSXFSBLOCKENTRY
{
    /** AVL tree node, indexed by the filesystem block number. */
    AVLU64NODECORE    Core;
    /** List node for the inode LRU list used for eviction. */
    RTLISTNODE        NdLru;
    /** Reference counter. */
    volatile uint32_t cRefs;
    /** The block data. */
    uint8_t           abData[1];
} RTFSXFSBLOCKENTRY;
/** Pointer to a block cache entry. */
typedef RTFSXFSBLOCKENTRY *PRTFSXFSBLOCKENTRY;


/**
 * Open directory instance.
 */
typedef struct RTFSXFSDIR
{
    /** Volume this directory belongs to. */
    PRTFSXFSVOL         pVol;
    /** The underlying inode structure. */
    PRTFSXFSINODE       pInode;
    /** Set if we've reached the end of the directory enumeration. */
    bool                fNoMoreFiles;
    /** Current offset into the directory where the next entry should be read. */
    uint64_t            offEntry;
    /** Next entry index (for logging purposes). */
    uint32_t            idxEntry;
} RTFSXFSDIR;
/** Pointer to an open directory instance. */
typedef RTFSXFSDIR *PRTFSXFSDIR;


/**
 * Open file instance.
 */
typedef struct RTFSXFSFILE
{
    /** Volume this directory belongs to. */
    PRTFSXFSVOL         pVol;
    /** The underlying inode structure. */
    PRTFSXFSINODE       pInode;
    /** Current offset into the file for I/O. */
    RTFOFF              offFile;
} RTFSXFSFILE;
/** Pointer to an open file instance. */
typedef RTFSXFSFILE *PRTFSXFSFILE;


/**
 * XFS filesystem volume.
 */
typedef struct RTFSXFSVOL
{
    /** Handle to itself. */
    RTVFS               hVfsSelf;
    /** The file, partition, or whatever backing the ext volume. */
    RTVFSFILE           hVfsBacking;
    /** The size of the backing thingy. */
    uint64_t            cbBacking;

    /** RTVFSMNT_F_XXX. */
    uint32_t            fMntFlags;
    /** RTFSXFSVFS_F_XXX (currently none defined). */
    uint32_t            fXfsFlags;

    /** Size of one block. */
    size_t              cbBlock;
    /** Number of bits to shift for converting a block number to byte offset. */
    uint32_t            cBlockShift;
    /** Number of blocks per allocation group. */
    XFSAGNUMBER         cBlocksPerAg;

    /** @name Allocation group cache.
     * @{ */
    /** LRU list anchor. */
    RTLISTANCHOR        LstAgLru;
    /** Root of the cached allocation group tree. */
    AVLU32TREE          AgRoot;
    /** Size of the cached allocation groups. */
    size_t              cbAgs;
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

    /** @name Block cache.
     * @{ */
    /** LRU list anchor for the block cache. */
    RTLISTANCHOR        LstBlockLru;
    /** Root of the cached block tree. */
    AVLU64TREE          BlockRoot;
    /** Size of cached blocks. */
    size_t              cbBlocks;
    /** @} */
} RTFSXFSVOL;



/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static int rtFsXfsVol_OpenDirByInode(PRTFSXFSVOL pThis, uint32_t iInode, PRTVFSDIR phVfsDir);

#ifdef LOG_ENABLED
/**
 * Logs the XFS filesystem superblock.
 *
 * @returns nothing.
 * @param   pSb                 Pointer to the superblock.
 */
static void rtFsXfsSb_Log(PCXFSSUPERBLOCK pSb)
{
    if (LogIs2Enabled())
    {
        Log2(("XFS: Superblock:\n"));
        Log2(("XFS:   u32Magic                    %#RX32\n", RT_BE2H_U32(pSb->u32Magic)));
        Log2(("XFS:   cbBlock                     %RU32\n", RT_BE2H_U32(pSb->cbBlock)));
        Log2(("XFS:   cBlocks                     %RU64\n", RT_BE2H_U64(pSb->cBlocks)));
        Log2(("XFS:   cBlocksRtDev                %RU64\n", RT_BE2H_U64(pSb->cBlocksRtDev)));
        Log2(("XFS:   cExtentsRtDev               %RU64\n", RT_BE2H_U64(pSb->cExtentsRtDev)));
        Log2(("XFS:   abUuid                      <todo>\n"));
        Log2(("XFS:   uBlockJournal               %#RX64\n", RT_BE2H_U64(pSb->uBlockJournal)));
        Log2(("XFS:   uInodeRoot                  %#RX64\n", RT_BE2H_U64(pSb->uInodeRoot)));
        Log2(("XFS:   uInodeBitmapRtExt           %#RX64\n", RT_BE2H_U64(pSb->uInodeBitmapRtExt)));
        Log2(("XFS:   uInodeBitmapSummary         %#RX64\n", RT_BE2H_U64(pSb->uInodeBitmapSummary)));
        Log2(("XFS:   cRtExtent                   %RU32\n", RT_BE2H_U32(pSb->cRtExtent)));
        Log2(("XFS:   cAgBlocks                   %RU32\n", RT_BE2H_U32(pSb->cAgBlocks)));
        Log2(("XFS:   cAg                         %RU32\n", RT_BE2H_U32(pSb->cAg)));
        Log2(("XFS:   cRtBitmapBlocks             %RU32\n", RT_BE2H_U32(pSb->cRtBitmapBlocks)));
        Log2(("XFS:   cJournalBlocks              %RU32\n", RT_BE2H_U32(pSb->cJournalBlocks)));
        Log2(("XFS:   fVersion                    %#RX16%s%s%s%s%s%s%s%s%s%s%s\n", RT_BE2H_U16(pSb->fVersion),
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_ATTR   ? " attr"   : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_NLINK  ? " nlink"  : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_QUOTA  ? " quota"  : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_ALIGN  ? " align"  : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_DALIGN ? " dalign" : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_SHARED ? " shared" : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_LOGV2  ? " logv2"  : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_SECTOR ? " sector" : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_EXTFLG ? " extflg" : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_DIRV2  ? " dirv2"  : "",
              RT_BE2H_U16(pSb->fVersion) & XFS_SB_VERSION_F_FEAT2  ? " feat2"  : ""));
        Log2(("XFS:   cbSector                    %RU16\n", RT_BE2H_U16(pSb->cbSector)));
        Log2(("XFS:   cbInode                     %RU16\n", RT_BE2H_U16(pSb->cbInode)));
        Log2(("XFS:   cIndoesPerBlock             %RU16\n", RT_BE2H_U16(pSb->cInodesPerBlock)));
        Log2(("XFS:   achFsName                   %12s\n", &pSb->achFsName[0]));
        Log2(("XFS:   cBlockSzLog                 %RU8\n", pSb->cBlockSzLog));
        Log2(("XFS:   cSectorSzLog                %RU8\n", pSb->cSectorSzLog));
        Log2(("XFS:   cInodeSzLog                 %RU8\n", pSb->cInodeSzLog));
        Log2(("XFS:   cInodesPerBlockLog          %RU8\n", pSb->cInodesPerBlockLog));
        Log2(("XFS:   cAgBlocksLog                %RU8\n", pSb->cAgBlocksLog));
        Log2(("XFS:   cExtentsRtDevLog            %RU8\n", pSb->cExtentsRtDevLog));
        Log2(("XFS:   fInProgress                 %RU8\n", pSb->fInProgress));
        Log2(("XFS:   cInodeMaxPct                %RU8\n", pSb->cInodeMaxPct));
        Log2(("XFS:   cInodesGlobal               %#RX64\n", RT_BE2H_U64(pSb->cInodesGlobal)));
        Log2(("XFS:   cInodesGlobalFree           %#RX64\n", RT_BE2H_U64(pSb->cInodesGlobalFree)));
        Log2(("XFS:   cBlocksFree                 %#RX64\n", RT_BE2H_U64(pSb->cBlocksFree)));
        Log2(("XFS:   cExtentsRtFree              %#RX64\n", RT_BE2H_U64(pSb->cExtentsRtFree)));
        Log2(("XFS:   uInodeQuotaUsr              %#RX64\n", RT_BE2H_U64(pSb->uInodeQuotaUsr)));
        Log2(("XFS:   uInodeQuotaGrp              %#RX64\n", RT_BE2H_U64(pSb->uInodeQuotaGrp)));
        Log2(("XFS:   fQuotaFlags                 %#RX16\n", RT_BE2H_U16(pSb->fQuotaFlags)));
        Log2(("XFS:   fFlagsMisc                  %#RX8\n", pSb->fFlagsMisc));
        Log2(("XFS:   uSharedVn                   %#RX8\n", pSb->uSharedVn));
        Log2(("XFS:   cBlocksInodeAlignment       %#RX32\n", RT_BE2H_U32(pSb->cBlocksInodeAlignment)));
        Log2(("XFS:   cBlocksRaidStripe           %#RX32\n", RT_BE2H_U32(pSb->cBlocksRaidStripe)));
        Log2(("XFS:   cBlocksRaidWidth            %#RX32\n", RT_BE2H_U32(pSb->cBlocksRaidWidth)));
        Log2(("XFS:   cDirBlockAllocLog           %RU8\n", pSb->cDirBlockAllocLog));
        Log2(("XFS:   cLogDevSubVolSectorSzLog    %RU8\n", pSb->cLogDevSubVolSectorSzLog));
        Log2(("XFS:   cLogDevSectorSzLog          %RU16\n", RT_BE2H_U16(pSb->cLogDevSectorSzLog)));
        Log2(("XFS:   cLogDevRaidStripe           %RU32\n", RT_BE2H_U32(pSb->cLogDevRaidStripe)));
        Log2(("XFS:   fFeatures2                  %#RX32\n", RT_BE2H_U32(pSb->fFeatures2)));
        Log2(("XFS:   fFeaturesRw                 %#RX32\n", RT_BE2H_U32(pSb->fFeaturesRw)));
        Log2(("XFS:   fFeaturesRo                 %#RX32\n", RT_BE2H_U32(pSb->fFeaturesRo)));
        Log2(("XFS:   fFeaturesIncompatRw         %#RX32\n", RT_BE2H_U32(pSb->fFeaturesIncompatRw)));
        Log2(("XFS:   fFeaturesJrnlIncompatRw     %#RX32\n", RT_BE2H_U32(pSb->fFeaturesJrnlIncompatRw)));
        Log2(("XFS:   u32Chksum                   %#RX32\n", RT_BE2H_U32(pSb->u32Chksum)));
        Log2(("XFS:   u32SparseInodeAlignment     %#RX32\n", RT_BE2H_U32(pSb->u32SparseInodeAlignment)));
        Log2(("XFS:   uInodeProjectQuota          %#RX64\n", RT_BE2H_U64(pSb->uInodeProjectQuota)));
        Log2(("XFS:   uJrnlSeqSbUpdate            %#RX64\n", RT_BE2H_U64(pSb->uJrnlSeqSbUpdate)));
        Log2(("XFS:   abUuidMeta                  <todo>\n"));
        Log2(("XFS:   uInodeRm                    %#RX64\n", RT_BE2H_U64(pSb->uInodeRm)));
    }
}


#if 0
/**
 * Logs a XFS filesystem inode.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   iInode              Inode number.
 * @param   pInode              Pointer to the inode.
 */
static void rtFsXfsInode_Log(PRTFSXFSVOL pThis, XFSINO iInode, PCXFSINODE pInode)
{
    if (LogIs2Enabled())
    {

    }
}


/**
 * Logs a XFS filesystem directory entry.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   idxDirEntry         Directory entry index number.
 * @param   pDirEntry           The directory entry.
 */
static void rtFsXfsDirEntry_Log(PRTFSXFSVOL pThis, uint32_t idxDirEntry, PCXFSDIRENTRYEX pDirEntry)
{
    if (LogIs2Enabled())
    {
    }
}
#endif
#endif


/**
 * Converts a block number to a byte offset.
 *
 * @returns Offset in bytes for the given block number.
 * @param   pThis               The XFS volume instance.
 * @param   iBlock              The block number to convert.
 */
DECLINLINE(uint64_t) rtFsXfsBlockIdxToDiskOffset(PRTFSXFSVOL pThis, uint64_t iBlock)
{
    return iBlock << pThis->cBlockShift;
}


/**
 * Converts a byte offset to a block number.
 *
 * @returns Block number.
 * @param   pThis               The XFS volume instance.
 * @param   iBlock              The offset to convert.
 */
DECLINLINE(uint64_t) rtFsXfsDiskOffsetToBlockIdx(PRTFSXFSVOL pThis, uint64_t off)
{
    return off >> pThis->cBlockShift;
}


/**
 * Allocates a new block group.
 *
 * @returns Pointer to the new block group descriptor or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   cbAlloc             How much to allocate.
 * @param   iBlockGroup         Block group number.
 */
static PRTFSXFSBLOCKENTRY rtFsXfsVol_BlockAlloc(PRTFSXFSVOL pThis, size_t cbAlloc, uint64_t iBlock)
{
    PRTFSXFSBLOCKENTRY pBlock = (PRTFSXFSBLOCKENTRY)RTMemAllocZ(cbAlloc);
    if (RT_LIKELY(pBlock))
    {
        pBlock->Core.Key = iBlock;
        pBlock->cRefs    = 0;
        pThis->cbBlocks  += cbAlloc;
    }

    return pBlock;
}


/**
 * Returns a new block entry utilizing the cache if possible.
 *
 * @returns Pointer to the new block entry or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   iBlock              Block number.
 */
static PRTFSXFSBLOCKENTRY rtFsXfsVol_BlockGetNew(PRTFSXFSVOL pThis, uint64_t iBlock)
{
    PRTFSXFSBLOCKENTRY pBlock = NULL;
    size_t cbAlloc = RT_UOFFSETOF_DYN(RTFSXFSBLOCKENTRY, abData[pThis->cbBlock]);
    if (pThis->cbBlocks + cbAlloc <= RTFSXFS_MAX_BLOCK_CACHE_SIZE)
        pBlock = rtFsXfsVol_BlockAlloc(pThis, cbAlloc, iBlock);
    else
    {
        pBlock = RTListRemoveLast(&pThis->LstBlockLru, RTFSXFSBLOCKENTRY, NdLru);
        if (!pBlock)
            pBlock = rtFsXfsVol_BlockAlloc(pThis, cbAlloc, iBlock);
        else
        {
            /* Remove the block group from the tree because it gets a new key. */
            PAVLU64NODECORE pCore = RTAvlU64Remove(&pThis->BlockRoot, pBlock->Core.Key);
            Assert(pCore == &pBlock->Core); RT_NOREF(pCore);
        }
    }

    Assert(!pBlock->cRefs);
    pBlock->Core.Key = iBlock;
    pBlock->cRefs    = 1;

    return pBlock;
}


/**
 * Frees the given block.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pBlock              The block to free.
 */
static void rtFsXfsVol_BlockFree(PRTFSXFSVOL pThis, PRTFSXFSBLOCKENTRY pBlock)
{
    Assert(!pBlock->cRefs);

    /*
     * Put it into the cache if the limit wasn't exceeded, otherwise the block group
     * is freed right away.
     */
    if (pThis->cbBlocks <= RTFSXFS_MAX_BLOCK_CACHE_SIZE)
    {
        /* Put onto the LRU list. */
        RTListPrepend(&pThis->LstBlockLru, &pBlock->NdLru);
    }
    else
    {
        /* Remove from the tree and free memory. */
        PAVLU64NODECORE pCore = RTAvlU64Remove(&pThis->BlockRoot, pBlock->Core.Key);
        Assert(pCore == &pBlock->Core); RT_NOREF(pCore);
        RTMemFree(pBlock);
        pThis->cbBlocks -= RT_UOFFSETOF_DYN(RTFSXFSBLOCKENTRY, abData[pThis->cbBlock]);
    }
}


/**
 * Gets the specified block data from the volume.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   iBlock              The filesystem block to load.
 * @param   ppBlock             Where to return the pointer to the block entry on success.
 * @param   ppvData             Where to return the pointer to the block data on success.
 */
static int rtFsXfsVol_BlockLoad(PRTFSXFSVOL pThis, uint64_t iBlock, PRTFSXFSBLOCKENTRY *ppBlock, void **ppvData)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the block group from the cache first. */
    PRTFSXFSBLOCKENTRY pBlock = (PRTFSXFSBLOCKENTRY)RTAvlU64Get(&pThis->BlockRoot, iBlock);
    if (!pBlock)
    {
        /* Slow path, load from disk. */
        pBlock = rtFsXfsVol_BlockGetNew(pThis, iBlock);
        if (RT_LIKELY(pBlock))
        {
            uint64_t offRead = rtFsXfsBlockIdxToDiskOffset(pThis, iBlock);
            rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead, &pBlock->abData[0], pThis->cbBlock, NULL);
            if (RT_SUCCESS(rc))
            {
                bool fIns = RTAvlU64Insert(&pThis->BlockRoot, &pBlock->Core);
                Assert(fIns); RT_NOREF(fIns);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Remove from current LRU list position and add to the beginning. */
        uint32_t cRefs = ASMAtomicIncU32(&pBlock->cRefs);
        if (cRefs == 1) /* Blocks get removed from the LRU list if they are referenced. */
            RTListNodeRemove(&pBlock->NdLru);
    }

    if (RT_SUCCESS(rc))
    {
        *ppBlock = pBlock;
        *ppvData = &pBlock->abData[0];
    }
    else if (pBlock)
    {
        ASMAtomicDecU32(&pBlock->cRefs);
        rtFsXfsVol_BlockFree(pThis, pBlock); /* Free the block. */
    }

    return rc;
}


/**
 * Releases a reference of the given block.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pBlock              The block to release.
 */
static void rtFsXfsVol_BlockRelease(PRTFSXFSVOL pThis, PRTFSXFSBLOCKENTRY pBlock)
{
    uint32_t cRefs = ASMAtomicDecU32(&pBlock->cRefs);
    if (!cRefs)
        rtFsXfsVol_BlockFree(pThis, pBlock);
}


/**
 * Allocates a new alloction group.
 *
 * @returns Pointer to the new allocation group descriptor or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   iAG                 Allocation group number.
 */
static PRTFSXFSAG rtFsXfsAg_Alloc(PRTFSXFSVOL pThis, uint32_t iAg)
{
    PRTFSXFSAG pAg = (PRTFSXFSAG)RTMemAllocZ(sizeof(RTFSXFSAG));
    if (RT_LIKELY(pAg))
    {
        pAg->Core.Key       = iAg;
        pAg->cRefs          = 0;
        pThis->cbAgs       += sizeof(RTFSXFSAG);
    }

    return pAg;
}


/**
 * Frees the given allocation group.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pAg                 The allocation group to free.
 */
static void rtFsXfsAg_Free(PRTFSXFSVOL pThis, PRTFSXFSAG pAg)
{
    Assert(!pAg->cRefs);

    /*
     * Put it into the cache if the limit wasn't exceeded, otherwise the allocation group
     * is freed right away.
     */
    if (pThis->cbAgs <= RTFSXFS_MAX_AG_CACHE_SIZE)
    {
        /* Put onto the LRU list. */
        RTListPrepend(&pThis->LstAgLru, &pAg->NdLru);
    }
    else
    {
        /* Remove from the tree and free memory. */
        PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->AgRoot, pAg->Core.Key);
        Assert(pCore == &pAg->Core); RT_NOREF(pCore);
        RTMemFree(pAg);
        pThis->cbAgs -= sizeof(RTFSXFSAG);
    }
}


/**
 * Returns a new block group utilizing the cache if possible.
 *
 * @returns Pointer to the new block group descriptor or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   iAg                 Allocation group number.
 */
static PRTFSXFSAG rtFsXfsAg_GetNew(PRTFSXFSVOL pThis, uint32_t iAg)
{
    PRTFSXFSAG pAg = NULL;
    if (pThis->cbAgs + sizeof(RTFSXFSAG) <= RTFSXFS_MAX_AG_CACHE_SIZE)
        pAg = rtFsXfsAg_Alloc(pThis, iAg);
    else
    {
        pAg = RTListRemoveLast(&pThis->LstAgLru, RTFSXFSAG, NdLru);
        if (!pAg)
            pAg = rtFsXfsAg_Alloc(pThis, iAg);
        else
        {
            /* Remove the block group from the tree because it gets a new key. */
            PAVLU32NODECORE pCore = RTAvlU32Remove(&pThis->AgRoot, pAg->Core.Key);
            Assert(pCore == &pAg->Core); RT_NOREF(pCore);
        }
    }

    Assert(!pAg->cRefs);
    pAg->Core.Key = iAg;
    pAg->cRefs    = 1;

    return pAg;
}


/**
 * Loads the given allocation group number and returns it on success.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   iAg                 The allocation group to load.
 * @param   ppAg                Where to store the allocation group on success.
 */
static int rtFsXfsAg_Load(PRTFSXFSVOL pThis, uint32_t iAg, PRTFSXFSAG *ppAg)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the allocation group from the cache first. */
    PRTFSXFSAG pAg = (PRTFSXFSAG)RTAvlU32Get(&pThis->AgRoot, iAg);
    if (!pAg)
    {
        /* Slow path, load from disk. */
        pAg = rtFsXfsAg_GetNew(pThis, iAg);
        if (RT_LIKELY(pAg))
        {
#if 0 /** @todo */
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
#endif
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
    {
        /* Remove from current LRU list position and add to the beginning. */
        uint32_t cRefs = ASMAtomicIncU32(&pAg->cRefs);
        if (cRefs == 1) /* Block groups get removed from the LRU list if they are referenced. */
            RTListNodeRemove(&pAg->NdLru);
    }

    if (RT_SUCCESS(rc))
        *ppAg = pAg;
    else if (pAg)
    {
        ASMAtomicDecU32(&pAg->cRefs);
        rtFsXfsAg_Free(pThis, pAg); /* Free the allocation group. */
    }

    return rc;
}


/**
 * Releases a reference of the given allocation group.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pAg                 The allocation group to release.
 */
static void rtFsXfsAg_Release(PRTFSXFSVOL pThis, PRTFSXFSAG pAg)
{
    uint32_t cRefs = ASMAtomicDecU32(&pAg->cRefs);
    if (!cRefs)
        rtFsXfsAg_Free(pThis, pAg);
}


/**
 * Allocates a new inode.
 *
 * @returns Pointer to the new inode or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   iInode              Inode number.
 */
static PRTFSXFSINODE rtFsXfsInode_Alloc(PRTFSXFSVOL pThis, uint32_t iInode)
{
    PRTFSXFSINODE pInode = (PRTFSXFSINODE)RTMemAllocZ(sizeof(RTFSXFSINODE));
    if (RT_LIKELY(pInode))
    {
        pInode->Core.Key = iInode;
        pInode->cRefs    = 0;
        pThis->cbInodes  += sizeof(RTFSXFSINODE);
    }

    return pInode;
}


/**
 * Frees the given inode.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pInode              The inode to free.
 */
static void rtFsXfsInode_Free(PRTFSXFSVOL pThis, PRTFSXFSINODE pInode)
{
    Assert(!pInode->cRefs);

    /*
     * Put it into the cache if the limit wasn't exceeded, otherwise the inode
     * is freed right away.
     */
    if (pThis->cbInodes <= RTFSXFS_MAX_INODE_CACHE_SIZE)
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
        pThis->cbInodes -= sizeof(RTFSXFSINODE);
    }
}


/**
 * Returns a new inodep utilizing the cache if possible.
 *
 * @returns Pointer to the new inode or NULL if out of memory.
 * @param   pThis               The XFS volume instance.
 * @param   iInode              Inode number.
 */
static PRTFSXFSINODE rtFsXfsInode_GetNew(PRTFSXFSVOL pThis, uint32_t iInode)
{
    PRTFSXFSINODE pInode = NULL;
    if (pThis->cbInodes + sizeof(RTFSXFSINODE) <= RTFSXFS_MAX_INODE_CACHE_SIZE)
        pInode = rtFsXfsInode_Alloc(pThis, iInode);
    else
    {
        pInode = RTListRemoveLast(&pThis->LstInodeLru, RTFSXFSINODE, NdLru);
        if (!pInode)
            pInode = rtFsXfsInode_Alloc(pThis, iInode);
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
 * @param   pThis               The XFS volume instance.
 * @param   iInode              The inode to load.
 * @param   ppInode             Where to store the inode on success.
 */
static int rtFsXfsInode_Load(PRTFSXFSVOL pThis, uint32_t iInode, PRTFSXFSINODE *ppInode)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the inode from the cache first. */
    PRTFSXFSINODE pInode = (PRTFSXFSINODE)RTAvlU32Get(&pThis->InodeRoot, iInode);
    if (!pInode)
    {
        /* Slow path, load from disk. */
        pInode = rtFsXfsInode_GetNew(pThis, iInode);
        if (RT_LIKELY(pInode))
        {
#if 0 /** @todo */
            /* Calculate the block group and load that one first to get at the inode table location. */
            PRTFSEXTBLKGRP pBlockGroup = NULL;
            rc = rtFsEBlockGroupLoad(pThis, (iInode - 1) / pThis->cInodesPerGroup, &pBlockGroup);
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
                    pInode->offInode            = offRead;
                    pInode->fFlags              = RT_LE2H_U32(Inode.Core.fFlags);
                    pInode->ObjInfo.cbObject    =   (uint64_t)RT_LE2H_U32(Inode.Core.cbSizeHigh) << 32
                                                  | (uint64_t)RT_LE2H_U32(Inode.Core.cbSizeLow);
                    pInode->ObjInfo.cbAllocated = (  (uint64_t)RT_LE2H_U16(Inode.Core.Osd2.Lnx.cBlocksHigh) << 32
                                                   | (uint64_t)RT_LE2H_U32(Inode.Core.cBlocksLow)) * pThis->cbBlock;
                    RTTimeSpecSetSeconds(&pInode->ObjInfo.AccessTime, RT_LE2H_U32(Inode.Core.u32TimeLastAccess));
                    RTTimeSpecSetSeconds(&pInode->ObjInfo.ModificationTime, RT_LE2H_U32(Inode.Core.u32TimeLastModification));
                    RTTimeSpecSetSeconds(&pInode->ObjInfo.ChangeTime, RT_LE2H_U32(Inode.Core.u32TimeLastChange));
                    pInode->ObjInfo.Attr.enmAdditional = RTFSOBJATTRADD_UNIX;
                    pInode->ObjInfo.Attr.u.Unix.uid    =   (uint32_t)RT_LE2H_U16(Inode.Core.Osd2.Lnx.uUidHigh) << 16
                                                         | (uint32_t)RT_LE2H_U16(Inode.Core.uUidLow);
                    pInode->ObjInfo.Attr.u.Unix.gid    =   (uint32_t)RT_LE2H_U16(Inode.Core.Osd2.Lnx.uGidHigh) << 16
                                                         | (uint32_t)RT_LE2H_U16(Inode.Core.uGidLow);
                    pInode->ObjInfo.Attr.u.Unix.cHardlinks = RT_LE2H_U16(Inode.Core.cHardLinks);
                    pInode->ObjInfo.Attr.u.Unix.INodeIdDevice = 0;
                    pInode->ObjInfo.Attr.u.Unix.INodeId       = iInode;
                    pInode->ObjInfo.Attr.u.Unix.fFlags        = 0;
                    pInode->ObjInfo.Attr.u.Unix.GenerationId  = RT_LE2H_U32(Inode.Core.u32Version);
                    pInode->ObjInfo.Attr.u.Unix.Device        = 0;
                    if (pThis->cbInode >= sizeof(EXTINODECOMB))
                        RTTimeSpecSetSeconds(&pInode->ObjInfo.BirthTime, RT_LE2H_U32(Inode.Extra.u32TimeCreation));
                    else
                        RTTimeSpecSetSeconds(&pInode->ObjInfo.BirthTime, RT_LE2H_U32(Inode.Core.u32TimeLastChange));
                    for (unsigned i = 0; i < RT_ELEMENTS(pInode->aiBlocks); i++)
                        pInode->aiBlocks[i] = RT_LE2H_U32(Inode.Core.au32Block[i]);

                    /* Fill in the mode. */
                    pInode->ObjInfo.Attr.fMode = 0;
                    uint32_t fInodeMode = RT_LE2H_U32(Inode.Core.fMode);
                    switch (EXT_INODE_MODE_TYPE_GET_TYPE(fInodeMode))
                    {
                        case EXT_INODE_MODE_TYPE_FIFO:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_FIFO;
                            break;
                        case EXT_INODE_MODE_TYPE_CHAR:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_DEV_CHAR;
                            break;
                        case EXT_INODE_MODE_TYPE_DIR:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_DIRECTORY;
                            break;
                        case EXT_INODE_MODE_TYPE_BLOCK:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_DEV_BLOCK;
                            break;
                        case EXT_INODE_MODE_TYPE_REGULAR:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_FILE;
                            break;
                        case EXT_INODE_MODE_TYPE_SYMLINK:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_SYMLINK;
                            break;
                        case EXT_INODE_MODE_TYPE_SOCKET:
                            pInode->ObjInfo.Attr.fMode |= RTFS_TYPE_SOCKET;
                            break;
                        default:
                            rc = VERR_VFS_BOGUS_FORMAT;
                    }
                    if (fInodeMode & EXT_INODE_MODE_EXEC_OTHER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IXOTH;
                    if (fInodeMode & EXT_INODE_MODE_WRITE_OTHER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IWOTH;
                    if (fInodeMode & EXT_INODE_MODE_READ_OTHER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IROTH;
                    if (fInodeMode & EXT_INODE_MODE_EXEC_GROUP)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IXGRP;
                    if (fInodeMode & EXT_INODE_MODE_WRITE_GROUP)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IWGRP;
                    if (fInodeMode & EXT_INODE_MODE_READ_GROUP)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IRGRP;
                    if (fInodeMode & EXT_INODE_MODE_EXEC_OWNER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IXUSR;
                    if (fInodeMode & EXT_INODE_MODE_WRITE_OWNER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IWUSR;
                    if (fInodeMode & EXT_INODE_MODE_READ_OWNER)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_IRUSR;
                    if (fInodeMode & EXT_INODE_MODE_STICKY)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_ISTXT;
                    if (fInodeMode & EXT_INODE_MODE_SET_GROUP_ID)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_ISGID;
                    if (fInodeMode & EXT_INODE_MODE_SET_USER_ID)
                        pInode->ObjInfo.Attr.fMode |= RTFS_UNIX_ISUID;
                }
            }
#endif
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
        rtFsXfsInode_Free(pThis, pInode); /* Free the inode. */
    }

    return rc;
}


/**
 * Releases a reference of the given inode.
 *
 * @returns nothing.
 * @param   pThis               The XFS volume instance.
 * @param   pInode              The inode to release.
 */
static void rtFsXfsInode_Release(PRTFSXFSVOL pThis, PRTFSXFSINODE pInode)
{
    uint32_t cRefs = ASMAtomicDecU32(&pInode->cRefs);
    if (!cRefs)
        rtFsXfsInode_Free(pThis, pInode);
}


/**
 * Worker for various QueryInfo methods.
 *
 * @returns IPRT status code.
 * @param   pInode              The inode structure to return info for.
 * @param   pObjInfo            Where to return object info.
 * @param   enmAddAttr          What additional info to return.
 */
static int rtFsXfsInode_QueryInfo(PRTFSXFSINODE pInode, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_ZERO(*pObjInfo);

    pObjInfo->cbObject           = pInode->ObjInfo.cbObject;
    pObjInfo->cbAllocated        = pInode->ObjInfo.cbAllocated;
    pObjInfo->AccessTime         = pInode->ObjInfo.AccessTime;
    pObjInfo->ModificationTime   = pInode->ObjInfo.ModificationTime;
    pObjInfo->ChangeTime         = pInode->ObjInfo.ChangeTime;
    pObjInfo->BirthTime          = pInode->ObjInfo.BirthTime;
    pObjInfo->Attr.fMode         = pInode->ObjInfo.Attr.fMode;
    pObjInfo->Attr.enmAdditional = enmAddAttr;
    switch (enmAddAttr)
    {
        case RTFSOBJATTRADD_UNIX:
            memcpy(&pObjInfo->Attr.u.Unix, &pInode->ObjInfo.Attr.u.Unix, sizeof(pInode->ObjInfo.Attr.u.Unix));
            break;

        case RTFSOBJATTRADD_UNIX_OWNER:
            pObjInfo->Attr.u.UnixOwner.uid = pInode->ObjInfo.Attr.u.Unix.uid;
            break;

        case RTFSOBJATTRADD_UNIX_GROUP:
            pObjInfo->Attr.u.UnixGroup.gid = pInode->ObjInfo.Attr.u.Unix.gid;
            break;

        default:
            break;
    }

    return VINF_SUCCESS;
}


/**
 * Maps the given inode block to the destination filesystem block.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   pInode              The inode structure to read from.
 * @param   iBlock              The inode block to map.
 * @param   cBlocks             Number of blocks requested.
 * @param   piBlockFs           Where to store the filesystem block on success.
 * @param   pcBlocks            Where to store the number of contiguous blocks on success.
 * @param   pfSparse            Where to store the sparse flag on success.
 *
 * @todo Optimize
 */
static int rtFsXfsInode_MapBlockToFs(PRTFSXFSVOL pThis, PRTFSXFSINODE pInode, uint64_t iBlock, size_t cBlocks,
                                     uint64_t *piBlockFs, size_t *pcBlocks, bool *pfSparse)
{
    RT_NOREF(pThis, pInode, iBlock, cBlocks, piBlockFs, pcBlocks, pfSparse);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Reads data from the given inode at the given byte offset.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   pInode              The inode structure to read from.
 * @param   off                 The byte offset to start reading from.
 * @param   pvBuf               Where to store the read data to.
 * @param   pcbRead             Where to return the amount of data read.
 */
static int rtFsXfsInode_Read(PRTFSXFSVOL pThis, PRTFSXFSINODE pInode, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    int rc = VINF_SUCCESS;
    uint8_t *pbBuf = (uint8_t *)pvBuf;

    if (((uint64_t)pInode->ObjInfo.cbObject < off + cbRead))
    {
        if (!pcbRead)
            return VERR_EOF;
        else
            cbRead = (uint64_t)pInode->ObjInfo.cbObject - off;
    }

    while (   cbRead
           && RT_SUCCESS(rc))
    {
        uint64_t iBlockStart   = rtFsXfsDiskOffsetToBlockIdx(pThis, off);
        uint32_t offBlockStart = off % pThis->cbBlock;

        /* Resolve the inode block to the proper filesystem block. */
        uint64_t iBlockFs = 0;
        size_t cBlocks = 0;
        bool fSparse = false;
        rc = rtFsXfsInode_MapBlockToFs(pThis, pInode, iBlockStart, 1, &iBlockFs, &cBlocks, &fSparse);
        if (RT_SUCCESS(rc))
        {
            Assert(cBlocks == 1);

            size_t cbThisRead = RT_MIN(cbRead, pThis->cbBlock - offBlockStart);

            if (!fSparse)
            {
                uint64_t offRead = rtFsXfsBlockIdxToDiskOffset(pThis, iBlockFs);
                rc = RTVfsFileReadAt(pThis->hVfsBacking, offRead + offBlockStart, pbBuf, cbThisRead, NULL);
            }
            else
                memset(pbBuf, 0, cbThisRead);

            if (RT_SUCCESS(rc))
            {
                pbBuf  += cbThisRead;
                cbRead -= cbThisRead;
                off    += cbThisRead;
                if (pcbRead)
                    *pcbRead += cbThisRead;
            }
        }
    }

    return rc;
}



/*
 *
 * File operations.
 * File operations.
 * File operations.
 *
 */

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtFsXfsFile_Close(void *pvThis)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    LogFlow(("rtFsXfsFile_Close(%p/%p)\n", pThis, pThis->pInode));

    rtFsXfsInode_Release(pThis->pVol, pThis->pInode);
    pThis->pInode = NULL;
    pThis->pVol   = NULL;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsXfsFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    return rtFsXfsInode_QueryInfo(pThis->pInode, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtFsXfsFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    AssertReturn(pSgBuf->cSegs == 1, VERR_INTERNAL_ERROR_3);
    RT_NOREF(fBlocking);

    if (off == -1)
        off = pThis->offFile;
    else
        AssertReturn(off >= 0, VERR_INTERNAL_ERROR_3);

    int rc;
    size_t cbRead = pSgBuf->paSegs[0].cbSeg;
    if (!pcbRead)
    {
        rc = rtFsXfsInode_Read(pThis->pVol, pThis->pInode, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbRead, NULL);
        if (RT_SUCCESS(rc))
            pThis->offFile = off + cbRead;
        Log6(("rtFsExtFile_Read: off=%#RX64 cbSeg=%#x -> %Rrc\n", off, pSgBuf->paSegs[0].cbSeg, rc));
    }
    else
    {
        PRTFSXFSINODE pInode = pThis->pInode;
        if (off >= pInode->ObjInfo.cbObject)
        {
            *pcbRead = 0;
            rc = VINF_EOF;
        }
        else
        {
            if ((uint64_t)off + cbRead <= (uint64_t)pInode->ObjInfo.cbObject)
                rc = rtFsXfsInode_Read(pThis->pVol, pThis->pInode, (uint64_t)off, pSgBuf->paSegs[0].pvSeg, cbRead, NULL);
            else
            {
                /* Return VINF_EOF if beyond end-of-file. */
                cbRead = (size_t)(pInode->ObjInfo.cbObject - off);
                rc = rtFsXfsInode_Read(pThis->pVol, pThis->pInode, off, pSgBuf->paSegs[0].pvSeg, cbRead, NULL);
                if (RT_SUCCESS(rc))
                    rc = VINF_EOF;
            }
            if (RT_SUCCESS(rc))
            {
                pThis->offFile = off + cbRead;
                *pcbRead = cbRead;
            }
            else
                *pcbRead = 0;
        }
        Log6(("rtFsXfsFile_Read: off=%#RX64 cbSeg=%#x -> %Rrc *pcbRead=%#x\n", off, pSgBuf->paSegs[0].cbSeg, rc, *pcbRead));
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtFsXfsFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    RT_NOREF(pvThis, off, pSgBuf, fBlocking, pcbWritten);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtFsXfsFile_Flush(void *pvThis)
{
    RT_NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtFsXfsFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    *poffActual = pThis->offFile;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsXfsFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsXfsFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                              PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsXfsFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtFsXfsFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    RTFOFF offNew;
    switch (uMethod)
    {
        case RTFILE_SEEK_BEGIN:
            offNew = offSeek;
            break;
        case RTFILE_SEEK_END:
            offNew = pThis->pInode->ObjInfo.cbObject + offSeek;
            break;
        case RTFILE_SEEK_CURRENT:
            offNew = (RTFOFF)pThis->offFile + offSeek;
            break;
        default:
            return VERR_INVALID_PARAMETER;
    }
    if (offNew >= 0)
    {
        pThis->offFile = offNew;
        *poffActual    = offNew;
        return VINF_SUCCESS;
    }
    return VERR_NEGATIVE_SEEK;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtFsXfsFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTFSXFSFILE pThis = (PRTFSXFSFILE)pvThis;
    *pcbFile = (uint64_t)pThis->pInode->ObjInfo.cbObject;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSetSize}
 */
static DECLCALLBACK(int) rtFsXfsFile_SetSize(void *pvThis, uint64_t cbFile, uint32_t fFlags)
{
    RT_NOREF(pvThis, cbFile, fFlags);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQueryMaxSize}
 */
static DECLCALLBACK(int) rtFsXfsFile_QueryMaxSize(void *pvThis, uint64_t *pcbMax)
{
    RT_NOREF(pvThis);
    *pcbMax = INT64_MAX; /** @todo */
    return VINF_SUCCESS;
}


/**
 * XFS file operations.
 */
static const RTVFSFILEOPS g_rtFsXfsFileOps =
{
    { /* Stream */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "XFS File",
            rtFsXfsFile_Close,
            rtFsXfsFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        RTVFSIOSTREAMOPS_FEAT_NO_SG,
        rtFsXfsFile_Read,
        rtFsXfsFile_Write,
        rtFsXfsFile_Flush,
        NULL /*PollOne*/,
        rtFsXfsFile_Tell,
        NULL /*pfnSkip*/,
        NULL /*pfnZeroFill*/,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_UOFFSETOF(RTVFSFILEOPS, ObjSet) - RT_UOFFSETOF(RTVFSFILEOPS, Stream.Obj),
        rtFsXfsFile_SetMode,
        rtFsXfsFile_SetTimes,
        rtFsXfsFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsXfsFile_Seek,
    rtFsXfsFile_QuerySize,
    rtFsXfsFile_SetSize,
    rtFsXfsFile_QueryMaxSize,
    RTVFSFILEOPS_VERSION
};


/**
 * Creates a new VFS file from the given regular file inode.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   fOpen               Open flags passed.
 * @param   iInode              The inode for the file.
 * @param   phVfsFile           Where to store the VFS file handle on success.
 * @param   pErrInfo            Where to record additional error information on error, optional.
 * @param   pszWhat             Logging prefix.
 */
static int rtFsXfsVol_NewFile(PRTFSXFSVOL pThis, uint64_t fOpen, uint32_t iInode,
                              PRTVFSFILE phVfsFile, PRTERRINFO pErrInfo, const char *pszWhat)
{
    /*
     * Load the inode and check that it really is a file.
     */
    PRTFSXFSINODE pInode = NULL;
    int rc = rtFsXfsInode_Load(pThis, iInode, &pInode);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_FILE(pInode->ObjInfo.Attr.fMode))
        {
            PRTFSXFSFILE pNewFile;
            rc = RTVfsNewFile(&g_rtFsXfsFileOps, sizeof(*pNewFile), fOpen, pThis->hVfsSelf, NIL_RTVFSLOCK,
                             phVfsFile, (void **)&pNewFile);
            if (RT_SUCCESS(rc))
            {
                pNewFile->pVol    = pThis;
                pNewFile->pInode  = pInode;
                pNewFile->offFile = 0;
            }
        }
        else
            rc = RTERRINFO_LOG_SET_F(pErrInfo, VERR_NOT_A_FILE, "%s: fMode=%#RX32", pszWhat, pInode->ObjInfo.Attr.fMode);

        if (RT_FAILURE(rc))
            rtFsXfsInode_Release(pThis, pInode);
    }

    return rc;
}



/*
 *
 * XFS directory code.
 * XFS directory code.
 * XFS directory code.
 *
 */

/**
 * Looks up an entry in the given directory inode.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   pInode              The directory inode structure to.
 * @param   pszEntry            The entry to lookup.
 * @param   piInode             Where to store the inode number if the entry was found.
 */
static int rtFsXfsDir_Lookup(PRTFSXFSVOL pThis, PRTFSXFSINODE pInode, const char *pszEntry, uint32_t *piInode)
{
    uint64_t offEntry = 0;
    int rc = VERR_FILE_NOT_FOUND;
    uint32_t idxDirEntry = 0;
    size_t cchEntry = strlen(pszEntry);

    if (cchEntry > 255)
        return VERR_FILENAME_TOO_LONG;

    RT_NOREF(pThis, idxDirEntry, offEntry, pInode, piInode);

#if 0 /** @todo */
    while (offEntry < (uint64_t)pInode->ObjInfo.cbObject)
    {
        EXTDIRENTRYEX DirEntry;
        size_t cbThis = RT_MIN(sizeof(DirEntry), (uint64_t)pInode->ObjInfo.cbObject - offEntry);
        int rc2 = rtFsXfsInode_Read(pThis, pInode, offEntry, &DirEntry, cbThis, NULL);
        if (RT_SUCCESS(rc2))
        {
#ifdef LOG_ENABLED
            rtFsExtDirEntry_Log(pThis, idxDirEntry, &DirEntry);
#endif

            uint16_t cbName =   pThis->fFeaturesIncompat & EXT_SB_FEAT_INCOMPAT_DIR_FILETYPE
                              ? DirEntry.Core.u.v2.cbName
                              : RT_LE2H_U16(DirEntry.Core.u.v1.cbName);
            if (   cchEntry == cbName
                && !memcmp(pszEntry, &DirEntry.Core.achName[0], cchEntry))
            {
                *piInode = RT_LE2H_U32(DirEntry.Core.iInodeRef);
                rc = VINF_SUCCESS;
                break;
            }

            offEntry += RT_LE2H_U16(DirEntry.Core.cbRecord);
            idxDirEntry++;
        }
        else
        {
            rc = rc2;
            break;
        }
    }
#endif
    return rc;
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
static DECLCALLBACK(int) rtFsXfsDir_Close(void *pvThis)
{
    PRTFSXFSDIR pThis = (PRTFSXFSDIR)pvThis;
    LogFlowFunc(("pThis=%p\n", pThis));
    rtFsXfsInode_Release(pThis->pVol, pThis->pInode);
    pThis->pInode = NULL;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtFsXfsDir_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTFSXFSDIR pThis = (PRTFSXFSDIR)pvThis;
    LogFlowFunc(("\n"));
    return rtFsXfsInode_QueryInfo(pThis->pInode, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtFsXfsDir_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, fMode, fMask);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtFsXfsDir_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtFsXfsDir_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    LogFlowFunc(("\n"));
    RT_NOREF(pvThis, uid, gid);
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpen}
 */
static DECLCALLBACK(int) rtFsXfsDir_Open(void *pvThis, const char *pszEntry, uint64_t fOpen,
                                         uint32_t fFlags, PRTVFSOBJ phVfsObj)
{
    LogFlowFunc(("pszEntry='%s' fOpen=%#RX64 fFlags=%#x\n", pszEntry, fOpen, fFlags));
    PRTFSXFSDIR  pThis = (PRTFSXFSDIR)pvThis;
    PRTFSXFSVOL  pVol  = pThis->pVol;
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

    /*
     * Lookup the entry.
     */
    uint32_t iInode = 0;
    rc = rtFsXfsDir_Lookup(pVol, pThis->pInode, pszEntry, &iInode);
    if (RT_SUCCESS(rc))
    {
        PRTFSXFSINODE pInode = NULL;
        rc = rtFsXfsInode_Load(pVol, iInode, &pInode);
        if (RT_SUCCESS(rc))
        {
            if (RTFS_IS_DIRECTORY(pInode->ObjInfo.Attr.fMode))
            {
                RTVFSDIR hVfsDir;
                rc = rtFsXfsVol_OpenDirByInode(pVol, iInode, &hVfsDir);
                if (RT_SUCCESS(rc))
                {
                    *phVfsObj = RTVfsObjFromDir(hVfsDir);
                    RTVfsDirRelease(hVfsDir);
                    AssertStmt(*phVfsObj != NIL_RTVFSOBJ, rc = VERR_INTERNAL_ERROR_3);
                }
            }
            else if (RTFS_IS_FILE(pInode->ObjInfo.Attr.fMode))
            {
                RTVFSFILE hVfsFile;
                rc = rtFsXfsVol_NewFile(pVol, fOpen, iInode, &hVfsFile, NULL, pszEntry);
                if (RT_SUCCESS(rc))
                {
                    *phVfsObj = RTVfsObjFromFile(hVfsFile);
                    RTVfsFileRelease(hVfsFile);
                    AssertStmt(*phVfsObj != NIL_RTVFSOBJ, rc = VERR_INTERNAL_ERROR_3);
                }
            }
            else
                rc = VERR_NOT_SUPPORTED;
        }
    }

    LogFlow(("rtFsXfsDir_Open(%s): returns %Rrc\n", pszEntry, rc));
    return rc;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateDir}
 */
static DECLCALLBACK(int) rtFsXfsDir_CreateDir(void *pvThis, const char *pszSubDir, RTFMODE fMode, PRTVFSDIR phVfsDir)
{
    RT_NOREF(pvThis, pszSubDir, fMode, phVfsDir);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnOpenSymlink}
 */
static DECLCALLBACK(int) rtFsXfsDir_OpenSymlink(void *pvThis, const char *pszSymlink, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, phVfsSymlink);
    LogFlowFunc(("\n"));
    return VERR_NOT_SUPPORTED;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnCreateSymlink}
 */
static DECLCALLBACK(int) rtFsXfsDir_CreateSymlink(void *pvThis, const char *pszSymlink, const char *pszTarget,
                                                  RTSYMLINKTYPE enmType, PRTVFSSYMLINK phVfsSymlink)
{
    RT_NOREF(pvThis, pszSymlink, pszTarget, enmType, phVfsSymlink);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnUnlinkEntry}
 */
static DECLCALLBACK(int) rtFsXfsDir_UnlinkEntry(void *pvThis, const char *pszEntry, RTFMODE fType)
{
    RT_NOREF(pvThis, pszEntry, fType);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRenameEntry}
 */
static DECLCALLBACK(int) rtFsXfsDir_RenameEntry(void *pvThis, const char *pszEntry, RTFMODE fType, const char *pszNewName)
{
    RT_NOREF(pvThis, pszEntry, fType, pszNewName);
    LogFlowFunc(("\n"));
    return VERR_WRITE_PROTECT;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnRewindDir}
 */
static DECLCALLBACK(int) rtFsXfsDir_RewindDir(void *pvThis)
{
    PRTFSXFSDIR pThis = (PRTFSXFSDIR)pvThis;
    LogFlowFunc(("\n"));

    pThis->fNoMoreFiles = false;
    pThis->offEntry     = 0;
    pThis->idxEntry     = 0;
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSDIROPS,pfnReadDir}
 */
static DECLCALLBACK(int) rtFsXfsDir_ReadDir(void *pvThis, PRTDIRENTRYEX pDirEntry, size_t *pcbDirEntry,
                                            RTFSOBJATTRADD enmAddAttr)
{
    PRTFSXFSDIR     pThis = (PRTFSXFSDIR)pvThis;
    PRTFSXFSINODE   pInode = pThis->pInode;
    LogFlowFunc(("\n"));

    if (pThis->fNoMoreFiles)
        return VERR_NO_MORE_FILES;

    RT_NOREF(pInode, pDirEntry, pcbDirEntry, enmAddAttr);
    return VERR_NOT_IMPLEMENTED;
}


/**
 * XFS directory operations.
 */
static const RTVFSDIROPS g_rtFsXfsDirOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_DIR,
        "XFS Dir",
        rtFsXfsDir_Close,
        rtFsXfsDir_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSDIROPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_UOFFSETOF(RTVFSDIROPS, ObjSet) - RT_UOFFSETOF(RTVFSDIROPS, Obj),
        rtFsXfsDir_SetMode,
        rtFsXfsDir_SetTimes,
        rtFsXfsDir_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtFsXfsDir_Open,
    NULL /* pfnFollowAbsoluteSymlink */,
    NULL /* pfnOpenFile */,
    NULL /* pfnOpenDir */,
    rtFsXfsDir_CreateDir,
    rtFsXfsDir_OpenSymlink,
    rtFsXfsDir_CreateSymlink,
    NULL /* pfnQueryEntryInfo */,
    rtFsXfsDir_UnlinkEntry,
    rtFsXfsDir_RenameEntry,
    rtFsXfsDir_RewindDir,
    rtFsXfsDir_ReadDir,
    RTVFSDIROPS_VERSION,
};


/**
 * Opens a directory by the given inode.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   iInode              The inode to open.
 * @param   phVfsDir            Where to store the handle to the VFS directory on success.
 */
static int rtFsXfsVol_OpenDirByInode(PRTFSXFSVOL pThis, uint32_t iInode, PRTVFSDIR phVfsDir)
{
    PRTFSXFSINODE pInode = NULL;
    int rc = rtFsXfsInode_Load(pThis, iInode, &pInode);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(pInode->ObjInfo.Attr.fMode))
        {
            PRTFSXFSDIR pNewDir;
            rc = RTVfsNewDir(&g_rtFsXfsDirOps, sizeof(*pNewDir), 0 /*fFlags*/, pThis->hVfsSelf, NIL_RTVFSLOCK,
                             phVfsDir, (void **)&pNewDir);
            if (RT_SUCCESS(rc))
            {
                pNewDir->fNoMoreFiles = false;
                pNewDir->pVol         = pThis;
                pNewDir->pInode       = pInode;
            }
        }
        else
            rc = VERR_VFS_BOGUS_FORMAT;

        if (RT_FAILURE(rc))
            rtFsXfsInode_Release(pThis, pInode);
    }

    return rc;
}



/*
 *
 * Volume level code.
 * Volume level code.
 * Volume level code.
 *
 */

static DECLCALLBACK(int) rtFsXfsVolAgTreeDestroy(PAVLU32NODECORE pCore, void *pvUser)
{
    RT_NOREF(pvUser);

    PRTFSXFSAG pAg = (PRTFSXFSAG)pCore;
    Assert(!pAg->cRefs);
    RTMemFree(pAg);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtFsXfsVolInodeTreeDestroy(PAVLU32NODECORE pCore, void *pvUser)
{
    RT_NOREF(pvUser);

    PRTFSXFSINODE pInode = (PRTFSXFSINODE)pCore;
    Assert(!pInode->cRefs);
    RTMemFree(pInode);
    return VINF_SUCCESS;
}


static DECLCALLBACK(int) rtFsXfsVolBlockTreeDestroy(PAVLU64NODECORE pCore, void *pvUser)
{
    RT_NOREF(pvUser);

    PRTFSXFSBLOCKENTRY pBlock = (PRTFSXFSBLOCKENTRY)pCore;
    Assert(!pBlock->cRefs);
    RTMemFree(pBlock);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnClose}
 */
static DECLCALLBACK(int) rtFsXfsVol_Close(void *pvThis)
{
    PRTFSXFSVOL pThis = (PRTFSXFSVOL)pvThis;

    /* Destroy the block group tree. */
    RTAvlU32Destroy(&pThis->AgRoot, rtFsXfsVolAgTreeDestroy, pThis);
    pThis->AgRoot = NULL;
    RTListInit(&pThis->LstAgLru);

    /* Destroy the inode tree. */
    RTAvlU32Destroy(&pThis->InodeRoot, rtFsXfsVolInodeTreeDestroy, pThis);
    pThis->InodeRoot = NULL;
    RTListInit(&pThis->LstInodeLru);

    /* Destroy the block cache tree. */
    RTAvlU64Destroy(&pThis->BlockRoot, rtFsXfsVolBlockTreeDestroy, pThis);
    pThis->BlockRoot = NULL;
    RTListInit(&pThis->LstBlockLru);

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
static DECLCALLBACK(int) rtFsXfsVol_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    RT_NOREF(pvThis, pObjInfo, enmAddAttr);
    return VERR_WRONG_TYPE;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnOpenRoot}
 */
static DECLCALLBACK(int) rtFsXfsVol_OpenRoot(void *pvThis, PRTVFSDIR phVfsDir)
{
    PRTFSXFSVOL pThis = (PRTFSXFSVOL)pvThis;
    int rc = rtFsXfsVol_OpenDirByInode(pThis, 0 /** @todo */, phVfsDir);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS::Obj,pfnQueryRangeState}
 */
static DECLCALLBACK(int) rtFsXfsVol_QueryRangeState(void *pvThis, uint64_t off, size_t cb, bool *pfUsed)
{
    RT_NOREF(pvThis, off, cb, pfUsed);
    return VERR_NOT_IMPLEMENTED;
}


DECL_HIDDEN_CONST(const RTVFSOPS) g_rtFsXfsVolOps =
{
    /* .Obj = */
    {
        /* .uVersion = */       RTVFSOBJOPS_VERSION,
        /* .enmType = */        RTVFSOBJTYPE_VFS,
        /* .pszName = */        "XfsVol",
        /* .pfnClose = */       rtFsXfsVol_Close,
        /* .pfnQueryInfo = */   rtFsXfsVol_QueryInfo,
        /* .uEndMarker = */     RTVFSOBJOPS_VERSION
    },
    /* .uVersion = */           RTVFSOPS_VERSION,
    /* .fFeatures = */          0,
    /* .pfnOpenRoot = */        rtFsXfsVol_OpenRoot,
    /* .pfnQueryRangeState = */ rtFsXfsVol_QueryRangeState,
    /* .uEndMarker = */         RTVFSOPS_VERSION
};



/**
 * Loads and parses the superblock of the filesystem.
 *
 * @returns IPRT status code.
 * @param   pThis               The XFS volume instance.
 * @param   pErrInfo            Where to return additional error info.
 */
static int rtFsXfsVolLoadAndParseSuperblock(PRTFSXFSVOL pThis, PRTERRINFO pErrInfo)
{
    int rc = VINF_SUCCESS;
    XFSSUPERBLOCK Sb;
    rc = RTVfsFileReadAt(pThis->hVfsBacking, XFS_SB_OFFSET, &Sb, sizeof(XFSSUPERBLOCK), NULL);
    if (RT_FAILURE(rc))
        return RTERRINFO_LOG_SET(pErrInfo, rc, "Error reading super block");

    /* Validate the superblock. */
    if (RT_BE2H_U32(Sb.u32Magic) != XFS_SB_MAGIC)
        return RTERRINFO_LOG_SET_F(pErrInfo, VERR_VFS_UNKNOWN_FORMAT, "Not XFS - Signature mismatch: %RX32", RT_BE2H_U32(Sb.u32Magic));

#ifdef LOG_ENABLED
    rtFsXfsSb_Log(&Sb);
#endif

    /** @todo */
    return rc;
}


RTDECL(int) RTFsXfsVolOpen(RTVFSFILE hVfsFileIn, uint32_t fMntFlags, uint32_t fXfsFlags, PRTVFS phVfs, PRTERRINFO pErrInfo)
{
    AssertPtrReturn(phVfs, VERR_INVALID_POINTER);
    AssertReturn(!(fMntFlags & ~RTVFSMNT_F_VALID_MASK), VERR_INVALID_FLAGS);
    AssertReturn(!fXfsFlags, VERR_INVALID_FLAGS);

    uint32_t cRefs = RTVfsFileRetain(hVfsFileIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create a VFS instance and initialize the data so rtFsExtVol_Close works.
     */
    RTVFS       hVfs;
    PRTFSXFSVOL pThis;
    int rc = RTVfsNew(&g_rtFsXfsVolOps, sizeof(*pThis), NIL_RTVFS, RTVFSLOCK_CREATE_RW, &hVfs, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsBacking    = hVfsFileIn;
        pThis->hVfsSelf       = hVfs;
        pThis->fMntFlags      = fMntFlags;
        pThis->fXfsFlags      = fXfsFlags;
        pThis->AgRoot         = NULL;
        pThis->InodeRoot      = NULL;
        pThis->BlockRoot      = NULL;
        pThis->cbAgs          = 0;
        pThis->cbInodes       = 0;
        pThis->cbBlocks       = 0;
        RTListInit(&pThis->LstAgLru);
        RTListInit(&pThis->LstInodeLru);
        RTListInit(&pThis->LstBlockLru);

        rc = RTVfsFileGetSize(pThis->hVfsBacking, &pThis->cbBacking);
        if (RT_SUCCESS(rc))
        {
            rc = rtFsXfsVolLoadAndParseSuperblock(pThis, pErrInfo);
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
static DECLCALLBACK(int) rtVfsChainXfsVol_Validate(PCRTVFSCHAINELEMENTREG pProviderReg, PRTVFSCHAINSPEC pSpec,
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
static DECLCALLBACK(int) rtVfsChainXfsVol_Instantiate(PCRTVFSCHAINELEMENTREG pProviderReg, PCRTVFSCHAINSPEC pSpec,
                                                      PCRTVFSCHAINELEMSPEC pElement, RTVFSOBJ hPrevVfsObj,
                                                      PRTVFSOBJ phVfsObj, uint32_t *poffError, PRTERRINFO pErrInfo)
{
    RT_NOREF(pProviderReg, pSpec, poffError);

    int         rc;
    RTVFSFILE   hVfsFileIn = RTVfsObjToFile(hPrevVfsObj);
    if (hVfsFileIn != NIL_RTVFSFILE)
    {
        RTVFS hVfs;
        rc = RTFsXfsVolOpen(hVfsFileIn, (uint32_t)pElement->uProvider, (uint32_t)(pElement->uProvider >> 32), &hVfs, pErrInfo);
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
static DECLCALLBACK(bool) rtVfsChainXfsVol_CanReuseElement(PCRTVFSCHAINELEMENTREG pProviderReg,
                                                           PCRTVFSCHAINSPEC pSpec, PCRTVFSCHAINELEMSPEC pElement,
                                                           PCRTVFSCHAINSPEC pReuseSpec, PCRTVFSCHAINELEMSPEC pReuseElement)
{
    RT_NOREF(pProviderReg, pSpec, pReuseSpec);
    if (   pElement->paArgs[0].uProvider == pReuseElement->paArgs[0].uProvider
        || !pReuseElement->paArgs[0].uProvider)
        return true;
    return false;
}


/** VFS chain element 'xfs'. */
static RTVFSCHAINELEMENTREG g_rtVfsChainXfsVolReg =
{
    /* uVersion = */            RTVFSCHAINELEMENTREG_VERSION,
    /* fReserved = */           0,
    /* pszName = */             "xfs",
    /* ListEntry = */           { NULL, NULL },
    /* pszHelp = */             "Open a XFS file system, requires a file object on the left side.\n"
                                "First argument is an optional 'ro' (read-only) or 'rw' (read-write) flag.\n",
    /* pfnValidate = */         rtVfsChainXfsVol_Validate,
    /* pfnInstantiate = */      rtVfsChainXfsVol_Instantiate,
    /* pfnCanReuseElement = */  rtVfsChainXfsVol_CanReuseElement,
    /* uEndMarker = */          RTVFSCHAINELEMENTREG_VERSION
};

RTVFSCHAIN_AUTO_REGISTER_ELEMENT_PROVIDER(&g_rtVfsChainXfsVolReg, rtVfsChainXfsVolReg);

