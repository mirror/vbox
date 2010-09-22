/* $Id$ */
/** @file
 * VCICacheCore - VirtualBox Cache Image, Core Code.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_VD_RAW /** @todo logging group */
#include <VBox/VBoxHDD-CachePlugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/file.h>

/*******************************************************************************
* On disk data structures                                                      *
*******************************************************************************/

/** @note All structures which are written to the disk are written in camel case
 * and packed. */

/** Block size used internally, because we cache sectors the smallest unit we
 * have to care about is 512 bytes. */
#define VCI_BLOCK_SIZE             512

/** Convert block number/size to byte offset/size. */
#define VCI_BLOCK2BYTE(u)          ((uint64_t)(u) << 9)

/** Convert byte offset/size to block number/size. */
#define VCI_BYTE2BLOCK(u)          ((u) >> 9)

/**
 * The VCI header - at the beginning of the file.
 *
 * All entries a stored in little endian order.
 */
#pragma pack(1)
typedef struct VciHdr
{
    /** The signature to identify a cache image. */
    uint32_t    u32Signature;
    /** Version of the layout of metadata in the cache. */
    uint32_t    u32Version;
    /** Maximum size of the cache file in blocks.
     *  This includes all metadata. */
    uint64_t    cBlocksCache;
    /** Flag indicating whether the cache was closed cleanly. */
    uint8_t     fUncleanShutdown;
    /** Cache type. */
    uint32_t    u32CacheType;
    /** Offset of the B+-Tree root in the image in blocks. */
    uint64_t    offTreeRoot;
    /** Offset of the block allocation bitmap in blocks. */
    uint64_t    offBlkMap;
    /** Size of the block allocation bitmap in blocks. */
    uint32_t    cBlkMap;
    /** UUID of the image. */
    RTUUID      uuidImage;
    /** Modifcation UUID for the cache. */
    RTUUID      uuidModification;
    /** Reserved for future use. */
    uint8_t     abReserved[951];
} VciHdr, *PVciHdr;
#pragma pack(0)
AssertCompileSize(VciHdr, 2 * VCI_BLOCK_SIZE);

/** VCI signature to identify a valid image. */
#define VCI_HDR_SIGNATURE          UINT32_C(0x56434900) /* VCI\0 */
/** Current version we support. */
#define VCI_HDR_VERSION            UINT32_C(0x00000001)

/** Value for an unclean cache shutdown. */
#define VCI_HDR_UNCLEAN_SHUTDOWN   UINT8_C(0x01)
/** Value for a clean cache shutdown. */
#define VCI_HDR_CLEAN_SHUTDOWN     UINT8_C(0x00)

/** Cache type: Dynamic image growing to the maximum value. */
#define VCI_HDR_CACHE_TYPE_DYNAMIC UINT32_C(0x00000001)
/** Cache type: Fixed image, space is preallocated. */
#define VCI_HDR_CACHE_TYPE_FIXED   UINT32_C(0x00000002)

/**
 * On disk representation of an extent describing a range of cached data.
 *
 * All entries a stored in little endian order.
 */
#pragma pack(1)
typedef struct VciCacheExtent
{
    /** Block address of the previous extent in the LRU list. */
    uint64_t    u64ExtentPrev;
    /** Block address of the next extent in the LRU list. */
    uint64_t    u64ExtentNext;
    /** Flags (for compression, encryption etc.) - currently unused and should be always 0. */
    uint8_t     u8Flags;
    /** Reserved */
    uint8_t     u8Reserved;
    /** First block of cached data the extent represents. */
    uint64_t    u64BlockOffset;
    /** Number of blocks the extent represents. */
    uint32_t    u32Blocks;
    /** First block in the image where the data is stored. */
    uint64_t    u64BlockAddr;
} VciCacheExtent, *PVciCacheExtent;
#pragma pack(0)
AssertCompileSize(VciCacheExtent, 38);

/**
 * On disk representation of an internal node.
 *
 * All entries a stored in little endian order.
 */
#pragma pack(1)
typedef struct VciTreeNodeInternal
{
    /** First block of cached data the internal node represents. */
    uint64_t    u64BlockOffset;
    /** Number of blocks the internal node represents. */
    uint32_t    u32Blocks;
    /** Block address in the image where the next node in the tree is stored. */
    uint64_t    u64ChildAddr;
} VciTreeNodeInternal, *PVciTreeNodeInternal;
#pragma pack(0)
AssertCompileSize(VciTreeNodeInternal, 20);

/**
 * On-disk representation of a node in the B+-Tree.
 *
 * All entries a stored in little endian order.
 */
#pragma pack(1)
typedef struct VciTreeNode
{
    /** Type of the node (root, internal, leaf). */
    uint8_t     u8Type;
    /** Data in the node. */
    uint8_t     au8Data[4095];
} VciTreeNode, *PVciTreeNode;
#pragma pack(0)
AssertCompileSize(VciTreeNode, 8 * VCI_BLOCK_SIZE);

/** Node type: Root of the tree (VciTreeNodeInternal). */
#define VCI_TREE_NODE_TYPE_ROOT     UINT32_C(0x00000001)
/** Node type: Internal node containing links to other nodes (VciTreeNodeInternal). */
#define VCI_TREE_NODE_TYPE_INTERNAL UINT32_C(0x00000002)
/** Node type: Leaf of the tree (VciCacheExtent). */
#define VCI_TREE_NODE_TYPE_LEAF     UINT32_C(0x00000003)

/**
 * VCI block bitmap header.
 *
 * All entries a stored in little endian order.
 */
#pragma pack(1)
typedef struct VciBlkMap
{
    /** Magic of the block bitmap. */
    uint32_t     u32Magic;
    /** Version of the block bitmap. */
    uint32_t     u32Version;
    /** Number of blocks this block map manages. */
    uint64_t     cBlocks;
    /** Number of free blocks. */
    uint64_t     cBlocksFree;
    /** Number of blocks allocated for metadata. */
    uint64_t     cBlocksAllocMeta;
    /** Number of blocks allocated for actual cached data. */
    uint64_t     cBlocksAllocData;
    /** Reserved for future use. */
    uint8_t      au8Reserved[472];
} VciBlkMap, *PVciBlkMap;
#pragma pack(0)
AssertCompileSize(VciBlkMap, VCI_BLOCK_SIZE);

/** The magic which identifies a block map. */
#define VCI_BLKMAP_MAGIC   UINT32_C(0x56424c4b) /* VBLK */
/** Current version. */
#define VCI_BLKMAP_VERSION UINT32_C(0x00000001)

/** Block bitmap entry */
typedef uint8_t VciBlkMapEnt;

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/**
 * Block range descriptor.
 */
typedef struct VCIBLKRANGEDESC
{
    /** Previous entry in the list. */
    struct VCIBLKRANGEDESC    *pPrev;
    /** Next entry in the list. */
    struct VCIBLKRANGEDESC    *pNext;
    /** Start address of the range. */
    uint64_t                   offAddrStart;
    /** Number of blocks in the range. */
    uint64_t                   cBlocks;
    /** Flag whether the range is free or allocated. */
    bool                       fFree;
} VCIBLKRANGEDESC, *PVCIBLKRANGEDESC;

/**
 * Block map for the cache image - in memory structure.
 */
typedef struct VCIBLKMAP
{
    /** Number of blocks the map manages. */
    uint64_t     cBlocks;
    /** Number of blocks allocated for metadata. */
    uint64_t     cBlocksAllocMeta;
    /** Number of blocks allocated for actual cached data. */
    uint64_t     cBlocksAllocData;
    /** Number of free blocks. */
    uint64_t     cBlocksFree;

    /** Pointer to the head of the block range list. */
    PVCIBLKRANGEDESC pRangesHead;
    /** Pointer to the tail of the block range list. */
    PVCIBLKRANGEDESC pRangesTail;

    /** Pointer to the block bitmap. */
    VciBlkMapEnt *pBlkBitmap;
} VCIBLKMAP;
/** Pointer to a block map. */
typedef VCIBLKMAP *PVCIBLKMAP;

/**
 * VCI image data structure.
 */
typedef struct VCICACHE
{
    /** Image name. */
    const char       *pszFilename;
    /** Storage handle. */
    PVDIOSTORAGE      pStorage;
    /** I/O interface. */
    PVDINTERFACE      pInterfaceIO;
    /** Async I/O interface callbacks. */
    PVDINTERFACEIOINT pInterfaceIOCallbacks;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE      pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE      pVDIfsImage;

    /** Error callback. */
    PVDINTERFACE      pInterfaceError;
    /** Opaque data for error callback. */
    PVDINTERFACEERROR pInterfaceErrorCallbacks;

    /** Open flags passed by VBoxHD layer. */
    unsigned          uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned          uImageFlags;
    /** Total size of the image. */
    uint64_t          cbSize;

    /** Offset of the B+-Tree in the image in bytes. */
    uint64_t          offTreeRoot;
    /** @todo Pointer to the root of the tree in memory. */


    /** Offset to the block allocation bitmap in bytes. */
    uint64_t          offBlksBitmap;
    /** Block map. */
    PVCIBLKMAP        pBlkMap;
} VCICACHE, *PVCICACHE;

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const char *const s_apszVciFileExtensions[] =
{
    "vci",
    NULL
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) vciError(PVCICACHE pImage, int rc, RT_SRC_POS_DECL,
                         const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks->pfnError(pImage->pInterfaceError->pvUser, rc, RT_SRC_POS_ARGS,
                                                   pszFormat, va);
    va_end(va);
    return rc;
}

/**
 * Internal: signal an informational message to the frontend.
 */
DECLINLINE(int) vciMessage(PVCICACHE pImage, const char *pszFormat, ...)
{
    int rc = VINF_SUCCESS;
    va_list va;
    va_start(va, pszFormat);
    if (pImage->pInterfaceError)
        rc = pImage->pInterfaceErrorCallbacks->pfnMessage(pImage->pInterfaceError->pvUser,
                                                          pszFormat, va);
    va_end(va);
    return rc;
}


DECLINLINE(int) vciFileOpen(PVCICACHE pImage, const char *pszFilename,
                            uint32_t fOpen)
{
    return pImage->pInterfaceIOCallbacks->pfnOpen(pImage->pInterfaceIO->pvUser,
                                                  pszFilename, fOpen,
                                                  &pImage->pStorage);
}

DECLINLINE(int) vciFileClose(PVCICACHE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnClose(pImage->pInterfaceIO->pvUser,
                                                   pImage->pStorage);
}

DECLINLINE(int) vciFileDelete(PVCICACHE pImage, const char *pszFilename)
{
    return pImage->pInterfaceIOCallbacks->pfnDelete(pImage->pInterfaceIO->pvUser,
                                                    pszFilename);
}

DECLINLINE(int) vciFileMove(PVCICACHE pImage, const char *pszSrc,
                            const char *pszDst, unsigned fMove)
{
    return pImage->pInterfaceIOCallbacks->pfnMove(pImage->pInterfaceIO->pvUser,
                                                  pszSrc, pszDst, fMove);
}

DECLINLINE(int) vciFileGetFreeSpace(PVCICACHE pImage, const char *pszFilename,
                                    int64_t *pcbFree)
{
    return pImage->pInterfaceIOCallbacks->pfnGetFreeSpace(pImage->pInterfaceIO->pvUser,
                                                          pszFilename, pcbFree);
}

DECLINLINE(int) vciFileGetSize(PVCICACHE pImage, uint64_t *pcbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnGetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, pcbSize);
}

DECLINLINE(int) vciFileSetSize(PVCICACHE pImage, uint64_t cbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnSetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage, cbSize);
}

DECLINLINE(int) vciFileWriteSync(PVCICACHE pImage, uint64_t uOffset,
                                 const void *pvBuffer, size_t cbBuffer)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage, uOffset,
                                                       pvBuffer, cbBuffer, NULL);
}

DECLINLINE(int) vciFileReadSync(PVCICACHE pImage, uint64_t uOffset,
                                void *pvBuffer, size_t cbBuffer)
{
    return pImage->pInterfaceIOCallbacks->pfnReadSync(pImage->pInterfaceIO->pvUser,
                                                      pImage->pStorage, uOffset,
                                                      pvBuffer, cbBuffer, NULL);
}

DECLINLINE(int) vciFileFlushSync(PVCICACHE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage);
}

DECLINLINE(int) vciFileReadUserAsync(PVCICACHE pImage, uint64_t uOffset,
                                     PVDIOCTX pIoCtx, size_t cbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadUserAsync(pImage->pInterfaceIO->pvUser,
                                                           pImage->pStorage,
                                                           uOffset, pIoCtx,
                                                           cbRead);
}

DECLINLINE(int) vciFileWriteUserAsync(PVCICACHE pImage, uint64_t uOffset,
                                      PVDIOCTX pIoCtx, size_t cbWrite,
                                      PFNVDXFERCOMPLETED pfnComplete,
                                      void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteUserAsync(pImage->pInterfaceIO->pvUser,
                                                            pImage->pStorage,
                                                            uOffset, pIoCtx,
                                                            cbWrite,
                                                            pfnComplete,
                                                            pvCompleteUser);
}

DECLINLINE(int) vciFileFlushAsync(PVCICACHE pImage, PVDIOCTX pIoCtx,
                                  PFNVDXFERCOMPLETED pfnComplete,
                                  void *pvCompleteUser)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushAsync(pImage->pInterfaceIO->pvUser,
                                                        pImage->pStorage,
                                                        pIoCtx, pfnComplete,
                                                        pvCompleteUser);
}

/**
 * Internal. Flush image data to disk.
 */
static int vciFlushImage(PVCICACHE pImage)
{
    int rc = VINF_SUCCESS;

    if (   pImage->pStorage
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = vciFileFlushSync(pImage);

    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int vciFreeImage(PVCICACHE pImage, bool fDelete)
{
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
    {
        if (pImage->pStorage)
        {
            /* No point updating the file that is deleted anyway. */
            if (!fDelete)
                vciFlushImage(pImage);

            vciFileClose(pImage);
            pImage->pStorage = NULL;
        }

        if (fDelete && pImage->pszFilename)
            vciFileDelete(pImage, pImage->pszFilename);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Creates a new block map which can manage the given number of blocks.
 *
 * The size of the bitmap is aligned to the VCI block size.
 *
 * @returns VBox status code.
 * @param   cBlocks      The number of blocks the bitmap can manage.
 * @param   ppBlkMap     Where to store the pointer to the block bitmap.
 * @param   pcbBlkMap    Where to store the size of the block bitmap in blocks
 *                       needed on the disk.
 */
static int vciBlkMapCreate(uint64_t cBlocks, PVCIBLKMAP *ppBlkMap, uint32_t *pcBlkMap)
{
    int rc = VINF_SUCCESS;
    uint32_t cbBlkMap = RT_ALIGN_Z(cBlocks / sizeof(VciBlkMapEnt) / 8, VCI_BLOCK_SIZE);
    PVCIBLKMAP pBlkMap = (PVCIBLKMAP)RTMemAllocZ(sizeof(VCIBLKMAP));
    VciBlkMapEnt *pBlkBitmap = (VciBlkMapEnt *)RTMemAllocZ(cbBlkMap);
    PVCIBLKRANGEDESC pFree   = (PVCIBLKRANGEDESC)RTMemAllocZ(sizeof(VCIBLKRANGEDESC));

    LogFlowFunc(("cBlocks=%u ppBlkMap=%#p pcBlkMap=%#p\n", cBlocks, ppBlkMap, pcBlkMap));

    if (pBlkMap && pBlkBitmap && pFree)
    {
        pBlkMap->cBlocks          = cBlocks;
        pBlkMap->cBlocksAllocMeta = 0;
        pBlkMap->cBlocksAllocData = 0;
        pBlkMap->cBlocksFree      = cBlocks;
        pBlkMap->pBlkBitmap       = pBlkBitmap;

        pFree->pPrev = NULL;
        pFree->pNext = NULL;
        pFree->offAddrStart = 0;
        pFree->cBlocks      = cBlocks;
        pFree->fFree        = true;

        pBlkMap->pRangesHead = pFree;
        pBlkMap->pRangesTail = pFree;

        Assert(!((cbBlkMap + sizeof(VciBlkMap) % VCI_BLOCK_SIZE)));
        *ppBlkMap = pBlkMap;
        *pcBlkMap = (cbBlkMap + sizeof(VciBlkMap)) / VCI_BLOCK_SIZE;
    }
    else
    {
        if (pBlkMap)
            RTMemFree(pBlkMap);
        if (pBlkBitmap)
            RTMemFree(pBlkBitmap);
        if (pFree)
            RTMemFree(pFree);

        rc = VERR_NO_MEMORY;
    }

    LogFlowFunc(("returns rc=%Rrc cBlkMap=%u\n", rc, *pcBlkMap));
    return rc;
}

/**
 * Frees a block map.
 *
 * @returns nothing.
 * @param   pBlkMap         The block bitmap to destroy.
 */
static void vciBlkMapDestroy(PVCIBLKMAP pBlkMap)
{
    LogFlowFunc(("pBlkMap=%#p\n", pBlkMap));

    PVCIBLKRANGEDESC pRangeCur = pBlkMap->pRangesHead;

    while (pRangeCur)
    {
        PVCIBLKRANGEDESC pTmp = pRangeCur;

        RTMemFree(pTmp);

        pRangeCur = pRangeCur->pNext;
    }

    RTMemFree(pBlkMap->pBlkBitmap);
    RTMemFree(pBlkMap);

    LogFlowFunc(("returns\n"));
}

/**
 * Loads the block map from the specified medium and creates all necessary
 * in memory structures to manage used and free blocks.
 *
 * @returns VBox status code.
 * @param   pStorage        Storage handle to read the block bitmap from.
 * @param   offBlkMap       Start of the block bitmap in blocks.
 * @param   cBlkMap         Size of the block bitmap on the disk in blocks.
 * @param   ppBlkMap        Where to store the block bitmap on success.
 */
static int vciBlkMapLoad(PVCICACHE pStorage, uint64_t offBlkMap, uint32_t cBlkMap, PVCIBLKMAP *ppBlkMap)
{
    int rc = VINF_SUCCESS;
    VciBlkMap BlkMap;

    LogFlowFunc(("pStorage=%#p offBlkMap=%llu cBlkMap=%u ppBlkMap=%#p\n",
                 pStorage, offBlkMap, cBlkMap, ppBlkMap));

    if (cBlkMap >= VCI_BYTE2BLOCK(sizeof(VciBlkMap)))
    {
        cBlkMap -= VCI_BYTE2BLOCK(sizeof(VciBlkMap));

        rc = vciFileReadSync(pStorage, offBlkMap, &BlkMap, VCI_BYTE2BLOCK(sizeof(VciBlkMap)));
        if (RT_SUCCESS(rc))
        {
            offBlkMap += VCI_BYTE2BLOCK(sizeof(VciBlkMap));

            BlkMap.u32Magic         = RT_LE2H_U32(BlkMap.u32Magic);
            BlkMap.u32Version       = RT_LE2H_U32(BlkMap.u32Version);
            BlkMap.cBlocks          = RT_LE2H_U32(BlkMap.cBlocks);
            BlkMap.cBlocksFree      = RT_LE2H_U32(BlkMap.cBlocksFree);
            BlkMap.cBlocksAllocMeta = RT_LE2H_U32(BlkMap.cBlocksAllocMeta);
            BlkMap.cBlocksAllocData = RT_LE2H_U32(BlkMap.cBlocksAllocData);

            if (   BlkMap.u32Magic == VCI_BLKMAP_MAGIC
                && BlkMap.u32Version == VCI_BLKMAP_VERSION
                && BlkMap.cBlocks == BlkMap.cBlocksFree + BlkMap.cBlocksAllocMeta + BlkMap.cBlocksAllocData
                && BlkMap.cBlocks / 8 == cBlkMap)
            {
                PVCIBLKMAP pBlkMap = (PVCIBLKMAP)RTMemAllocZ(sizeof(VCIBLKMAP));
                if (pBlkMap)
                {
                    pBlkMap->cBlocks          = BlkMap.cBlocks;
                    pBlkMap->cBlocksFree      = BlkMap.cBlocksFree;
                    pBlkMap->cBlocksAllocMeta = BlkMap.cBlocksAllocMeta;
                    pBlkMap->cBlocksAllocData = BlkMap.cBlocksAllocData;

                    pBlkMap->pBlkBitmap = (VciBlkMapEnt *)RTMemAllocZ(pBlkMap->cBlocks / 8);
                    if (pBlkMap->pBlkBitmap)
                    {
                        rc = vciFileReadSync(pStorage, offBlkMap, pBlkMap->pBlkBitmap, cBlkMap);
                        if (RT_SUCCESS(rc))
                        {
                            *ppBlkMap = pBlkMap;
                            LogFlowFunc(("return success\n"));
                            return VINF_SUCCESS;
                        }

                        RTMemFree(pBlkMap->pBlkBitmap);
                    }
                    else
                        rc = VERR_NO_MEMORY;

                    RTMemFree(pBlkMap);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            else
                rc = VERR_VD_GEN_INVALID_HEADER;
        }
        else if (RT_SUCCESS(rc))
            rc = VERR_VD_GEN_INVALID_HEADER;
    }
    else
        rc = VERR_VD_GEN_INVALID_HEADER;

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Saves the block map in the cache image. All neccessary on disk structures
 * are written.
 *
 * @returns VBox status code.
 * @param   pBlkMap         The block bitmap to save.
 * @param   pStorage        Where the block bitmap should be written to.
 * @param   offBlkMap       Start of the block bitmap in blocks.
 * @param   cBlkMap         Size of the block bitmap on the disk in blocks.
 */
static int vciBlkMapSave(PVCIBLKMAP pBlkMap, PVCICACHE pStorage, uint64_t offBlkMap, uint32_t cBlkMap)
{
    int rc = VINF_SUCCESS;
    VciBlkMap BlkMap;

    LogFlowFunc(("pBlkMap=%#p pStorage=%#p offBlkMap=%llu cBlkMap=%u\n",
                 pBlkMap, pStorage, offBlkMap, cBlkMap));

    /* Make sure the number of blocks allocated for us match our expectations. */
    if ((pBlkMap->cBlocks / 8) + VCI_BYTE2BLOCK(sizeof(VciBlkMap)) == cBlkMap)
    {
        /* Setup the header */
        memset(&BlkMap, 0, sizeof(VciBlkMap));

        BlkMap.u32Magic         = RT_H2LE_U32(VCI_BLKMAP_MAGIC);
        BlkMap.u32Version       = RT_H2LE_U32(VCI_BLKMAP_VERSION);
        BlkMap.cBlocks          = RT_H2LE_U32(pBlkMap->cBlocks);
        BlkMap.cBlocksFree      = RT_H2LE_U32(pBlkMap->cBlocksFree);
        BlkMap.cBlocksAllocMeta = RT_H2LE_U32(pBlkMap->cBlocksAllocMeta);
        BlkMap.cBlocksAllocData = RT_H2LE_U32(pBlkMap->cBlocksAllocData);

        rc = vciFileWriteSync(pStorage, offBlkMap, &BlkMap, VCI_BYTE2BLOCK(sizeof(VciBlkMap)));
        if (RT_SUCCESS(rc))
        {
            offBlkMap += VCI_BYTE2BLOCK(sizeof(VciBlkMap));
            rc = vciFileWriteSync(pStorage, offBlkMap, pBlkMap->pBlkBitmap, pBlkMap->cBlocks / 8);
        }
    }
    else
        rc = VERR_INTERNAL_ERROR; /* @todo Better error code. */

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Allocates the given number of blocks in the bitmap and returns the start block address.
 *
 * @returns VBox status code.
 * @param   pBlkMap          The block bitmap to allocate the blocks from.
 * @param   cBlocks          How many blocks to allocate.
 * @param   poffBlockAddr    Where to store the start address of the allocated region.
 */
static int vciBlkMapAllocate(PVCIBLKMAP pBlkMap, uint32_t cBlocks, uint64_t *poffBlockAddr)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBlkMap=%#p cBlocks=%u poffBlockAddr=%#p\n",
                 pBlkMap, cBlocks, poffBlockAddr));

    LogFlowFunc(("returns rc=%Rrc offBlockAddr=%llu\n", rc, *poffBlockAddr));
    return rc;
}

/**
 * Try to extend the space of an already allocated block.
 *
 * @returns VBox status code.
 * @param   pBlkMap          The block bitmap to allocate the blocks from.
 * @param   cBlocksNew       How many blocks the extended block should have.
 * @param   offBlockAddrOld  The start address of the block to reallocate.
 * @param   poffBlockAddr    Where to store the start address of the allocated region.
 */
static int vciBlkMapRealloc(PVCIBLKMAP pBlkMap, uint32_t cBlocksNew, uint64_t offBlockAddrOld,
                            uint64_t *poffBlockAddr)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pBlkMap=%#p cBlocksNew=%u offBlockAddrOld=%llu poffBlockAddr=%#p\n",
                 pBlkMap, cBlocksNew, offBlockAddrOld, poffBlockAddr));

    LogFlowFunc(("returns rc=%Rrc offBlockAddr=%llu\n", rc, *poffBlockAddr));
    return rc;
}

/**
 * Frees a range of blocks.
 *
 * @returns nothing.
 * @param   pBlkMap          The block bitmap.
 * @param   offBlockAddr     Address of the first block to free.
 * @param   cBlocks          How many blocks to free.
 */
static void vciBlkMapFree(PVCIBLKMAP pBlkMap, uint64_t offBlockAddr, uint32_t cBlocks)
{
    LogFlowFunc(("pBlkMap=%#p offBlockAddr=%llu cBlocks=%u\n",
                 pBlkMap, offBlockAddr, cBlocks));

    LogFlowFunc(("returns\n"));
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int vciOpenImage(PVCICACHE pImage, unsigned uOpenFlags)
{
    int rc;

    pImage->uOpenFlags = uOpenFlags;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    /* Get I/O interface. */
    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IOINT);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIOInt(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     */
    rc = vciFileOpen(pImage, pImage->pszFilename,
                     VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                false /* fCreate */));
    if (RT_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }

    rc = vciFileGetSize(pImage, &pImage->cbSize);
    if (RT_FAILURE(rc))
        goto out;
    if (pImage->cbSize % 512)
    {
        rc = VERR_VD_RAW_INVALID_HEADER;
        goto out;
    }
    pImage->uImageFlags |= VD_IMAGE_FLAGS_FIXED;

out:
    if (RT_FAILURE(rc))
        vciFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Create a vci image.
 */
static int vciCreateImage(PVCICACHE pImage, uint64_t cbSize,
                          unsigned uImageFlags, const char *pszComment,
                          unsigned uOpenFlags, PFNVDPROGRESS pfnProgress,
                          void *pvUser, unsigned uPercentStart,
                          unsigned uPercentSpan)
{
    VciHdr Hdr;
    VciTreeNode NodeRoot;
    int rc;
    uint64_t cBlocks = cbSize / VCI_BLOCK_SIZE; /* Size of the cache in blocks. */

    if (uImageFlags & VD_IMAGE_FLAGS_DIFF)
    {
        rc = vciError(pImage, VERR_VD_RAW_INVALID_TYPE, RT_SRC_POS, N_("VCI: cannot create diff image '%s'"), pImage->pszFilename);
        return rc;
    }

    pImage->uImageFlags = uImageFlags;

    pImage->uOpenFlags = uOpenFlags & ~VD_OPEN_FLAGS_READONLY;

    pImage->pInterfaceError = VDInterfaceGet(pImage->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pImage->pInterfaceError)
        pImage->pInterfaceErrorCallbacks = VDGetInterfaceError(pImage->pInterfaceError);

    pImage->pInterfaceIO = VDInterfaceGet(pImage->pVDIfsImage, VDINTERFACETYPE_IOINT);
    AssertPtrReturn(pImage->pInterfaceIO, VERR_INVALID_PARAMETER);
    pImage->pInterfaceIOCallbacks = VDGetInterfaceIOInt(pImage->pInterfaceIO);
    AssertPtrReturn(pImage->pInterfaceIOCallbacks, VERR_INVALID_PARAMETER);

    do
    {
        /* Create image file. */
        rc = vciFileOpen(pImage, pImage->pszFilename,
                         VDOpenFlagsToFileOpenFlags(uOpenFlags & ~VD_OPEN_FLAGS_READONLY,
                                                    true /* fCreate */));
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot create image '%s'"), pImage->pszFilename);
            break;
        }

        /* Allocate block bitmap. */
        uint32_t cBlkMap = 0;
        rc = vciBlkMapCreate(cBlocks, &pImage->pBlkMap, &cBlkMap);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot create block bitmap '%s'"), pImage->pszFilename);
            break;
        }

        /*
         * Allocate space for the header in the block bitmap.
         * Because the block map is empty the header has to start at block 0
         */
        uint64_t offHdr = 0;
        rc = vciBlkMapAllocate(pImage->pBlkMap, VCI_BYTE2BLOCK(sizeof(VciHdr)), &offHdr);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot allocate space for header in block bitmap '%s'"), pImage->pszFilename);
            break;
        }

        Assert(offHdr == 0);

        /*
         * Allocate space for the block map itself.
         */
        uint64_t offBlkMap = 0;
        rc = vciBlkMapAllocate(pImage->pBlkMap, cBlkMap, &offBlkMap);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot allocate space for block map in block map '%s'"), pImage->pszFilename);
            break;
        }

        /*
         * Allocate space for the tree root node.
         */
        uint64_t offTreeRoot = 0;
        rc = vciBlkMapAllocate(pImage->pBlkMap, VCI_BYTE2BLOCK(sizeof(VciTreeNode)), &offTreeRoot);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot allocate space for block map in block map '%s'"), pImage->pszFilename);
            break;
        }

        /*
         * Now that we are here we have all the basic structures and know where to place them in the image.
         * It's time to write it now.
         */

        /* Setup the header. */
        memset(&Hdr, 0, sizeof(VciHdr));
        Hdr.u32Signature     = RT_H2LE_U32(VCI_HDR_SIGNATURE);
        Hdr.u32Version       = RT_H2LE_U32(VCI_HDR_VERSION);
        Hdr.cBlocksCache     = RT_H2LE_U64(cBlocks);
        Hdr.fUncleanShutdown = VCI_HDR_UNCLEAN_SHUTDOWN;
        Hdr.u32CacheType     = uImageFlags & VD_IMAGE_FLAGS_FIXED
                               ? RT_H2LE_U32(VCI_HDR_CACHE_TYPE_FIXED)
                               : RT_H2LE_U32(VCI_HDR_CACHE_TYPE_DYNAMIC);
        Hdr.offTreeRoot      = RT_H2LE_U64(offTreeRoot);
        Hdr.offBlkMap        = RT_H2LE_U64(offBlkMap);
        Hdr.cBlkMap          = RT_H2LE_U64(cBlkMap);

        rc = vciFileWriteSync(pImage, offHdr, &Hdr, VCI_BYTE2BLOCK(sizeof(VciHdr)));
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot write header '%s'"), pImage->pszFilename);
            break;
        }

        rc = vciBlkMapSave(pImage->pBlkMap, pImage, offBlkMap, cBlkMap);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot write block map '%s'"), pImage->pszFilename);
            break;
        }

        /* Setup the root tree. */
        memset(&NodeRoot, 0, sizeof(VciTreeNode));
        NodeRoot.u8Type = RT_H2LE_U32(VCI_TREE_NODE_TYPE_ROOT);

        rc = vciFileWriteSync(pImage, offTreeRoot, &NodeRoot, VCI_BYTE2BLOCK(sizeof(VciTreeNode)));
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot write root node '%s'"), pImage->pszFilename);
            break;
        }

        rc = vciFlushImage(pImage);
        if (RT_FAILURE(rc))
        {
            rc = vciError(pImage, rc, RT_SRC_POS, N_("VCI: cannot flush '%s'"), pImage->pszFilename);
            break;
        }

        pImage->cbSize = cbSize;

    } while (0);

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        vciFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnProbe */
static int vciProbe(const char *pszFilename, PVDINTERFACE pVDIfsCache,
                    PVDINTERFACE pVDIfsImage)
{
    LogFlowFunc(("pszFilename=\"%s\"\n", pszFilename));
    int rc = VINF_SUCCESS;

    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnOpen */
static int vciOpen(const char *pszFilename, unsigned uOpenFlags,
                   PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                   void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc;
    PVCICACHE pImage;

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }


    pImage = (PVCICACHE)RTMemAllocZ(sizeof(VCICACHE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnCreate */
static int vciCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCRTUUID pUuid, unsigned uOpenFlags,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc;
    PVCICACHE pImage;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACE pIfProgress = VDInterfaceGet(pVDIfsOperation,
                                              VDINTERFACETYPE_PROGRESS);
    PVDINTERFACEPROGRESS pCbProgress = NULL;
    if (pIfProgress)
    {
        pCbProgress = VDGetInterfaceProgress(pIfProgress);
        if (pCbProgress)
            pfnProgress = pCbProgress->pfnProgress;
        pvUser = pIfProgress->pvUser;
    }

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PVCICACHE)RTMemAllocZ(sizeof(VCICACHE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = vciCreateImage(pImage, cbSize, uImageFlags, pszComment, uOpenFlags,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            vciFreeImage(pImage, false);
            rc = vciOpenImage(pImage, uOpenFlags);
            if (RT_FAILURE(rc))
            {
                RTMemFree(pImage);
                goto out;
            }
        }
        *ppBackendData = pImage;
    }
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnClose */
static int vciClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    rc = vciFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnRead */
static int vciRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnWrite */
static int vciWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnFlush */
static int vciFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc = VINF_SUCCESS;

    rc = vciFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetVersion */
static unsigned vciGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VDCACHEBACKEND::pfnGetSize */
static uint64_t vciGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VDCACHEBACKEND::pfnGetFileSize */
static uint64_t vciGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->pStorage)
        {
            int rc = vciFileGetSize(pImage, &cbFile);
            if (RT_SUCCESS(rc))
                cb = cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VDCACHEBACKEND::pfnGetImageFlags */
static unsigned vciGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    unsigned uImageFlags;

    AssertPtr(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VDCACHEBACKEND::pfnGetOpenFlags */
static unsigned vciGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    unsigned uOpenFlags;

    AssertPtr(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VDCACHEBACKEND::pfnSetOpenFlags */
static int vciSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    rc = vciFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;
    rc = vciOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetComment */
static int vciGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetComment */
static int vciSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetUuid */
static int vciGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetUuid */
static int vciSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnGetModificationUuid */
static int vciGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnSetModificationUuid */
static int vciSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PVCICACHE pImage = (PVCICACHE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
            rc = VERR_NOT_SUPPORTED;
        else
            rc = VERR_VD_IMAGE_READ_ONLY;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnDump */
static void vciDump(void *pBackendData)
{
    NOREF(pBackendData);
}

/** @copydoc VDCACHEBACKEND::pfnAsyncRead */
static int vciAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    rc = vciFileReadUserAsync(pImage, uOffset, pIoCtx, cbRead);
    if (RT_SUCCESS(rc))
        *pcbActuallyRead = cbRead;

    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnAsyncWrite */
static int vciAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbWrite,
                         PVDIOCTX pIoCtx, size_t *pcbWriteProcess)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    rc = vciFileWriteUserAsync(pImage, uOffset, pIoCtx, cbWrite, NULL, NULL);
    if (RT_SUCCESS(rc))
        *pcbWriteProcess = cbWrite;

    return rc;
}

/** @copydoc VDCACHEBACKEND::pfnAsyncFlush */
static int vciAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;
    PVCICACHE pImage = (PVCICACHE)pBackendData;

    if (!(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
        rc = vciFileFlushAsync(pImage, pIoCtx, NULL, NULL);

    return rc;
}


VDCACHEBACKEND g_VciCacheBackend =
{
    /* pszBackendName */
    "vci",
    /* cbSize */
    sizeof(VDCACHEBACKEND),
    /* uBackendCaps */
    VD_CAP_CREATE_FIXED | VD_CAP_CREATE_DYNAMIC | VD_CAP_FILE | VD_CAP_ASYNC,
    /* papszFileExtensions */
    s_apszVciFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnProbe */
    vciProbe,
    /* pfnOpen */
    vciOpen,
    /* pfnCreate */
    vciCreate,
    /* pfnClose */
    vciClose,
    /* pfnRead */
    vciRead,
    /* pfnWrite */
    vciWrite,
    /* pfnFlush */
    vciFlush,
    /* pfnGetVersion */
    vciGetVersion,
    /* pfnGetSize */
    vciGetSize,
    /* pfnGetFileSize */
    vciGetFileSize,
    /* pfnGetImageFlags */
    vciGetImageFlags,
    /* pfnGetOpenFlags */
    vciGetOpenFlags,
    /* pfnSetOpenFlags */
    vciSetOpenFlags,
    /* pfnGetComment */
    vciGetComment,
    /* pfnSetComment */
    vciSetComment,
    /* pfnGetUuid */
    vciGetUuid,
    /* pfnSetUuid */
    vciSetUuid,
    /* pfnGetModificationUuid */
    vciGetModificationUuid,
    /* pfnSetModificationUuid */
    vciSetModificationUuid,
    /* pfnDump */
    vciDump,
    /* pfnAsyncRead */
    vciAsyncRead,
    /* pfnAsyncWrite */
    vciAsyncWrite,
    /* pfnAsyncFlush */
    vciAsyncFlush,
    /* pfnComposeLocation */
    NULL,
    /* pfnComposeName */
    NULL
};

