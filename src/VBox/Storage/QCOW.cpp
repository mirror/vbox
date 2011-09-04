/* $Id$ */
/** @file
 * QCOW - QCOW Disk image.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_VD_QCOW
#include <VBox/vd-plugin.h>
#include <VBox/err.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/path.h>
#include <iprt/list.h>

/**
 * The QCOW backend implements support for the qemu copy on write format (short QCOW)
 * There is no official specification available but the format is described
 * at http://people.gnome.org/~markmc/qcow-image-format.html for version 2
 * and http://people.gnome.org/~markmc/qcow-image-format-version-1.html for version 1.
 *
 * Missing things to implement:
 *    - v2 image creation and handling of the reference count table. (Blocker to enable support for V2 images)
 *    - cluster encryption
 *    - cluster compression
 *    - compaction
 *    - resizing
 */

/*******************************************************************************
*   Structures in a QCOW image, big endian                                     *
*******************************************************************************/

#pragma pack(1)
typedef struct QCowHeader
{
    /** Magic value. */
    uint32_t    u32Magic;
    /** Version of the image. */
    uint32_t    u32Version;
    /** Version dependent data. */
    union
    {
        /** Version 1. */
        struct
        {
            /** Backing file offset. */
            uint64_t    u64BackingFileOffset;
            /** Size of the backing file. */
            uint32_t    u32BackingFileSize;
            /** mtime (Modification time?) - can be ignored. */
            uint32_t    u32MTime;
            /** Logical size of the image in bytes. */
            uint64_t    u64Size;
            /** Number of bits in the virtual offset used as a cluster offset. */
            uint8_t     u8ClusterBits;
            /** Number of bits in the virtual offset used for the L2 index. */
            uint8_t     u8L2Bits;
            /** Padding because the header is not packed in the original source. */
            uint16_t    u16Padding;
            /** Used cryptographic method. */
            uint32_t    u32CryptMethod;
            /** Offset of the L1 table in the image in bytes. */
            uint64_t    u64L1TableOffset;
        } v1;
        /** Version 2. */
        struct
        {
            /** Backing file offset. */
            uint64_t    u64BackingFileOffset;
            /** Size of the backing file. */
            uint32_t    u32BackingFileSize;
            /** Number of bits in the virtual offset used as a cluster offset. */
            uint32_t    u32ClusterBits;
            /** Logical size of the image. */
            uint64_t    u64Size;
            /** Used cryptographic method. */
            uint32_t    u32CryptMethod;
            /** Size of the L1 table in entries (each 8bytes big). */
            uint32_t    u32L1Size;
            /** Offset of the L1 table in the image in bytes. */
            uint64_t    u64L1TableOffset;
            /** Start of the refcount table in the image. */
            uint64_t    u64RefcountTableOffset;
            /** Size of the refcount table in clusters. */
            uint32_t    u32RefcountTableClusters;
            /** Number of snapshots in the image. */
            uint32_t    u32NbSnapshots;
            /** Offset of the first snapshot header in the image. */
            uint64_t    u64SnapshotsOffset;
        } v2;
    } Version;
} QCowHeader;
#pragma pack()
/** Pointer to a on disk QCOW header. */
typedef QCowHeader *PQCowHeader;

/** QCOW magic value. */
#define QCOW_MAGIC                            UINT32_C(0x514649fb) /* QFI\0xfb */
/** Size of the V1 header. */
#define QCOW_V1_HDR_SIZE                      (48)
/** Size of the V2 header. */
#define QCOW_V2_HDR_SIZE                      (72)

/*******************************************************************************
*   Constants And Macros, Structures and Typedefs                              *
*******************************************************************************/

/**
 * QCOW L2 cache entry.
 */
typedef struct QCOWL2CACHEENTRY
{
    /** List node for the search list. */
    RTLISTNODE              NodeSearch;
    /** List node for the LRU list. */
    RTLISTNODE              NodeLru;
    /** Reference counter. */
    uint32_t                cRefs;
    /** The offset of the L2 table, used as search key. */
    uint64_t                offL2Tbl;
    /** Pointer to the cached L2 table. */
    uint64_t               *paL2Tbl;
} QCOWL2CACHEENTRY, *PQCOWL2CACHEENTRY;

/** Maximum amount of memory the cache is allowed to use. */
#define QCOW_L2_CACHE_MEMORY_MAX (2*_1M)

/** QCOW default cluster size for image version 2. */
#define QCOW2_CLUSTER_SIZE_DEFAULT (64*_1K)
/** QCOW default cluster size for image version 1. */
#define QCOW_CLUSTER_SIZE_DEFAULT (4*_1K)
/** QCOW default L2 table size in clusters. */
#define QCOW_L2_CLUSTERS_DEFAULT (1)

/**
 * QCOW image data structure.
 */
typedef struct QCOWIMAGE
{
    /** Image name. */
    const char          *pszFilename;
    /** Storage handle. */
    PVDIOSTORAGE        pStorage;

    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE        pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE        pVDIfsImage;
    /** Error interface. */
    PVDINTERFACEERROR   pIfError;
    /** I/O interface. */
    PVDINTERFACEIOINT   pIfIo;

    /** Open flags passed by VBoxHD layer. */
    unsigned            uOpenFlags;
    /** Image flags defined during creation or determined during open. */
    unsigned            uImageFlags;
    /** Total size of the image. */
    uint64_t            cbSize;
    /** Physical geometry of this image. */
    VDGEOMETRY          PCHSGeometry;
    /** Logical geometry of this image. */
    VDGEOMETRY          LCHSGeometry;

    /** Image version. */
    unsigned            uVersion;
    /** MTime field - used only to preserve value in opened images, unmodified otherwise. */
    uint32_t            MTime;

    /** Filename of the backing file if any. */
    char               *pszBackingFilename;
    /** Offset of the filename in the image. */
    uint64_t            offBackingFilename;
    /** Size of the backing filename excluding \0. */
    uint32_t            cbBackingFilename;

    /** Size of the image, multiple of clusters. */
    uint64_t            cbImage;
    /** Cluster size in bytes. */
    uint32_t            cbCluster;
    /** Number of entries in the L1 table. */
    uint32_t            cL1TableEntries;
    /** Size of an L1 rounded to the next cluster size. */
    uint32_t            cbL1Table;
    /** Pointer to the L1 table. */
    uint64_t            *paL1Table;
    /** Offset of the L1 table. */
    uint64_t            offL1Table;

    /** Size of the L2 table in bytes. */
    uint32_t            cbL2Table;
    /** Number of entries in the L2 table. */
    uint32_t            cL2TableEntries;
    /** Memory occupied by the L2 table cache. */
    size_t              cbL2Cache;
    /** The sorted L2 entry list used for searching. */
    RTLISTNODE          ListSearch;
    /** The LRU L2 entry list used for eviction. */
    RTLISTNODE          ListLru;

    /** Offset of the refcount table. */
    uint64_t            offRefcountTable;
    /** Size of the refcount table in bytes. */
    uint32_t            cbRefcountTable;

    /** Offset mask for a cluster. */
    uint64_t            fOffsetMask;
    /** Number of bits to shift to get the L1 index. */
    uint32_t            cL1Shift;
    /** L2 table mask to get the L2 index. */
    uint64_t            fL2Mask;
    /** Number of bits to shift to get the L2 index. */
    uint32_t            cL2Shift;

} QCOWIMAGE, *PQCOWIMAGE;

/**
 * State of the async cluster allocation.
 */
typedef enum QCOWCLUSTERASYNCALLOCSTATE
{
    /** Invalid. */
    QCOWCLUSTERASYNCALLOCSTATE_INVALID = 0,
    /** L2 table allocation. */
    QCOWCLUSTERASYNCALLOCSTATE_L2_ALLOC,
    /** Link L2 table into L1. */
    QCOWCLUSTERASYNCALLOCSTATE_L2_LINK,
    /** Allocate user data cluster. */
    QCOWCLUSTERASYNCALLOCSTATE_USER_ALLOC,
    /** Link user data cluster. */
    QCOWCLUSTERASYNCALLOCSTATE_USER_LINK,
    /** 32bit blowup. */
    QCOWCLUSTERASYNCALLOCSTATE_32BIT_HACK = 0x7fffffff
} QCOWCLUSTERASYNCALLOCSTATE, *PQCOWCLUSTERASYNCALLOCSTATE;

/**
 * Data needed to track async cluster allocation.
 */
typedef struct QCOWCLUSTERASYNCALLOC
{
    /** The state of the cluster allocation. */
    QCOWCLUSTERASYNCALLOCSTATE enmAllocState;
    /** Old image size to rollback in case of an error. */
    uint64_t                   cbImageOld;
    /** L1 index to link if any. */
    uint32_t                   idxL1;
    /** L2 index to link, required in any case. */
    uint32_t                   idxL2;
    /** Start offset of the allocated cluster. */
    uint64_t                   offClusterNew;
    /** L2 cache entry if a L2 table is allocated. */
    PQCOWL2CACHEENTRY          pL2Entry;
    /** Number of bytes to write. */
    size_t                     cbToWrite;
} QCOWCLUSTERASYNCALLOC, *PQCOWCLUSTERASYNCALLOC;

/*******************************************************************************
*   Static Variables                                                           *
*******************************************************************************/

/** NULL-terminated array of supported file extensions. */
static const VDFILEEXTENSION s_aQCowFileExtensions[] =
{
    {"qcow", VDTYPE_HDD},
    {"qcow2", VDTYPE_HDD},
    {NULL,  VDTYPE_INVALID}
};

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/

/**
 * Return power of 2 or 0 if num error.
 *
 * @returns The power of 2 or 0 if the given number is not a power of 2.
 * @param   u32    The number.
 */
static uint32_t qcowGetPowerOfTwo(uint32_t u32)
{
    if (u32 == 0)
        return 0;
    uint32_t uPower2 = 0;
    while ((u32 & 1) == 0)
    {
        u32 >>= 1;
        uPower2++;
    }
    return u32 == 1 ? uPower2 : 0;
}


/**
 * Converts the image header to the host endianess and performs basic checks.
 *
 * @returns Whether the given header is valid or not.
 * @param   pHeader    Pointer to the header to convert.
 */
static bool qcowHdrConvertToHostEndianess(PQCowHeader pHeader)
{
    pHeader->u32Magic                                = RT_BE2H_U32(pHeader->u32Magic);
    pHeader->u32Version                              = RT_BE2H_U32(pHeader->u32Version);

    if (pHeader->u32Magic != QCOW_MAGIC)
        return false;

    if (pHeader->u32Version == 1)
    {
        pHeader->Version.v1.u64BackingFileOffset     = RT_BE2H_U64(pHeader->Version.v1.u64BackingFileOffset);
        pHeader->Version.v1.u32BackingFileSize       = RT_BE2H_U32(pHeader->Version.v1.u32BackingFileSize);
        pHeader->Version.v1.u32MTime                 = RT_BE2H_U32(pHeader->Version.v1.u32MTime);
        pHeader->Version.v1.u64Size                  = RT_BE2H_U64(pHeader->Version.v1.u64Size);
        pHeader->Version.v1.u32CryptMethod           = RT_BE2H_U32(pHeader->Version.v1.u32CryptMethod);
        pHeader->Version.v1.u64L1TableOffset         = RT_BE2H_U64(pHeader->Version.v1.u64L1TableOffset);
    }
    else if (pHeader->u32Version == 2)
    {
        pHeader->Version.v2.u64BackingFileOffset     = RT_BE2H_U64(pHeader->Version.v2.u64BackingFileOffset);
        pHeader->Version.v2.u32BackingFileSize       = RT_BE2H_U32(pHeader->Version.v2.u32BackingFileSize);
        pHeader->Version.v2.u32ClusterBits           = RT_BE2H_U32(pHeader->Version.v2.u32ClusterBits);
        pHeader->Version.v2.u64Size                  = RT_BE2H_U64(pHeader->Version.v2.u64Size);
        pHeader->Version.v2.u32CryptMethod           = RT_BE2H_U32(pHeader->Version.v2.u32CryptMethod);
        pHeader->Version.v2.u32L1Size                = RT_BE2H_U32(pHeader->Version.v2.u32L1Size);
        pHeader->Version.v2.u64L1TableOffset         = RT_BE2H_U64(pHeader->Version.v2.u64L1TableOffset);
        pHeader->Version.v2.u64RefcountTableOffset   = RT_BE2H_U64(pHeader->Version.v2.u64RefcountTableOffset);
        pHeader->Version.v2.u32RefcountTableClusters = RT_BE2H_U32(pHeader->Version.v2.u32RefcountTableClusters);
        pHeader->Version.v2.u32NbSnapshots           = RT_BE2H_U32(pHeader->Version.v2.u32NbSnapshots);
        pHeader->Version.v2.u64SnapshotsOffset       = RT_BE2H_U64(pHeader->Version.v2.u64SnapshotsOffset);
    }
    else
        return false;

    return true;
}

/**
 * Creates a QCOW header from the given image state.
 *
 * @returns nothing.
 * @param   pImage     Image instance data.
 * @param   pHeader    Pointer to the header to convert.
 * @param   pcbHeader  Where to store the size of the header to write.
 */
static void qcowHdrConvertFromHostEndianess(PQCOWIMAGE pImage, PQCowHeader pHeader,
                                            size_t *pcbHeader)
{
    memset(pHeader, 0, sizeof(QCowHeader));

    pHeader->u32Magic                                = RT_H2BE_U32(QCOW_MAGIC);
    pHeader->u32Version                              = RT_H2BE_U32(pImage->uVersion);
    if (pImage->uVersion == 1)
    {
        pHeader->Version.v1.u64BackingFileOffset     = RT_H2BE_U64(pImage->offBackingFilename);
        pHeader->Version.v1.u32BackingFileSize       = RT_H2BE_U32(pImage->cbBackingFilename);
        pHeader->Version.v1.u32MTime                 = RT_H2BE_U32(pImage->MTime);
        pHeader->Version.v1.u64Size                  = RT_H2BE_U64(pImage->cbSize);
        pHeader->Version.v1.u8ClusterBits            = (uint8_t)qcowGetPowerOfTwo(pImage->cbCluster);
        pHeader->Version.v1.u8L2Bits                 = (uint8_t)qcowGetPowerOfTwo(pImage->cL2TableEntries);
        pHeader->Version.v1.u32CryptMethod           = RT_H2BE_U32(0);
        pHeader->Version.v1.u64L1TableOffset         = RT_H2BE_U64(pImage->offL1Table);
        *pcbHeader = QCOW_V1_HDR_SIZE;
    }
    else if (pImage->uVersion == 2)
    {
        pHeader->Version.v2.u64BackingFileOffset     = RT_H2BE_U64(pImage->offBackingFilename);
        pHeader->Version.v2.u32BackingFileSize       = RT_H2BE_U32(pImage->cbBackingFilename);
        pHeader->Version.v2.u32ClusterBits           = qcowGetPowerOfTwo(pImage->cbCluster);
        pHeader->Version.v2.u64Size                  = RT_H2BE_U64(pImage->cbSize);
        pHeader->Version.v2.u32CryptMethod           = RT_H2BE_U32(0);
        pHeader->Version.v2.u32L1Size                = RT_H2BE_U32(pImage->cL1TableEntries);
        pHeader->Version.v2.u64L1TableOffset         = RT_H2BE_U64(pImage->offL1Table);
        pHeader->Version.v2.u64RefcountTableOffset   = RT_H2BE_U64(pImage->offRefcountTable);
        pHeader->Version.v2.u32RefcountTableClusters = RT_H2BE_U32(pImage->cbRefcountTable / pImage->cbCluster);
        pHeader->Version.v2.u32NbSnapshots           = RT_H2BE_U32(0);
        pHeader->Version.v2.u64SnapshotsOffset       = RT_H2BE_U64((uint64_t)0);
        *pcbHeader = QCOW_V2_HDR_SIZE;
    }
    else
        AssertMsgFailed(("Invalid version of the QCOW image format %d\n", pImage->uVersion));
}

/**
 * Convert table entries from little endian to host endianess.
 *
 * @returns nothing.
 * @param   paTbl       Pointer to the table.
 * @param   cEntries    Number of entries in the table.
 */
static void qcowTableConvertToHostEndianess(uint64_t *paTbl, uint32_t cEntries)
{
    while(cEntries-- > 0)
    {
        *paTbl = RT_BE2H_U64(*paTbl);
        paTbl++;
    }
}

/**
 * Convert table entries from host to little endian format.
 *
 * @returns nothing.
 * @param   paTblImg    Pointer to the table which will store the little endian table.
 * @param   paTbl       The source table to convert.
 * @param   cEntries    Number of entries in the table.
 */
static void qcowTableConvertFromHostEndianess(uint64_t *paTblImg, uint64_t *paTbl,
                                              uint32_t cEntries)
{
    while(cEntries-- > 0)
    {
        *paTblImg = RT_H2BE_U64(*paTbl);
        paTbl++;
        paTblImg++;
    }
}

/**
 * Creates the L2 table cache.
 *
 * @returns VBox status code.
 * @param   pImage    The image instance data.
 */
static int qcowL2TblCacheCreate(PQCOWIMAGE pImage)
{
    pImage->cbL2Cache = 0;
    RTListInit(&pImage->ListSearch);
    RTListInit(&pImage->ListLru);

    return VINF_SUCCESS;
}

/**
 * Destroys the L2 table cache.
 *
 * @returns nothing.
 * @param   pImage    The image instance data.
 */
static void qcowL2TblCacheDestroy(PQCOWIMAGE pImage)
{
    PQCOWL2CACHEENTRY pL2Entry = NULL;
    PQCOWL2CACHEENTRY pL2Next  = NULL;

    RTListForEachSafe(&pImage->ListSearch, pL2Entry, pL2Next, QCOWL2CACHEENTRY, NodeSearch)
    {
        Assert(!pL2Entry->cRefs);

        RTListNodeRemove(&pL2Entry->NodeSearch);
        RTMemPageFree(pL2Entry->paL2Tbl, pImage->cbL2Table);
        RTMemFree(pL2Entry);
    }

    pImage->cbL2Cache       = 0;
    RTListInit(&pImage->ListSearch);
    RTListInit(&pImage->ListLru);
}

/**
 * Returns the L2 table matching the given offset or NULL if none could be found.
 *
 * @returns Pointer to the L2 table cache entry or NULL.
 * @param   pImage    The image instance data.
 * @param   offL2Tbl  Offset of the L2 table to search for.
 */
static PQCOWL2CACHEENTRY qcowL2TblCacheRetain(PQCOWIMAGE pImage, uint64_t offL2Tbl)
{
    PQCOWL2CACHEENTRY pL2Entry = NULL;

    RTListForEach(&pImage->ListSearch, pL2Entry, QCOWL2CACHEENTRY, NodeSearch)
    {
        if (pL2Entry->offL2Tbl == offL2Tbl)
            break;
    }

    if (!RTListNodeIsDummy(&pImage->ListSearch, pL2Entry, QCOWL2CACHEENTRY, NodeSearch))
    {
        /* Update LRU list. */
        RTListNodeRemove(&pL2Entry->NodeLru);
        RTListPrepend(&pImage->ListLru, &pL2Entry->NodeLru);
        pL2Entry->cRefs++;
        return pL2Entry;
    }
    else
        return NULL;
}

/**
 * Releases a L2 table cache entry.
 *
 * @returns nothing.
 * @param   pL2Entry    The L2 cache entry.
 */
static void qcowL2TblCacheEntryRelease(PQCOWL2CACHEENTRY pL2Entry)
{
    Assert(pL2Entry->cRefs > 0);
    pL2Entry->cRefs--;
}

/**
 * Allocates a new L2 table from the cache evicting old entries if required.
 *
 * @returns Pointer to the L2 cache entry or NULL.
 * @param   pImage    The image instance data.
 */
static PQCOWL2CACHEENTRY qcowL2TblCacheEntryAlloc(PQCOWIMAGE pImage)
{
    PQCOWL2CACHEENTRY pL2Entry = NULL;
    int rc = VINF_SUCCESS;

    if (pImage->cbL2Cache + pImage->cbL2Table <= QCOW_L2_CACHE_MEMORY_MAX)
    {
        /* Add a new entry. */
        pL2Entry = (PQCOWL2CACHEENTRY)RTMemAllocZ(sizeof(QCOWL2CACHEENTRY));
        if (pL2Entry)
        {
            pL2Entry->paL2Tbl = (uint64_t *)RTMemPageAllocZ(pImage->cbL2Table);
            if (RT_UNLIKELY(!pL2Entry->paL2Tbl))
            {
                RTMemFree(pL2Entry);
                pL2Entry = NULL;
            }
            else
            {
                pL2Entry->cRefs    = 1;
                pImage->cbL2Cache += pImage->cbL2Table;
            }
        }
    }
    else
    {
        /* Evict the last not in use entry and use it */
        Assert(!RTListIsEmpty(&pImage->ListLru));

        RTListForEachReverse(&pImage->ListLru, pL2Entry, QCOWL2CACHEENTRY, NodeLru)
        {
            if (!pL2Entry->cRefs)
                break;
        }

        if (!RTListNodeIsDummy(&pImage->ListSearch, pL2Entry, QCOWL2CACHEENTRY, NodeSearch))
        {
            RTListNodeRemove(&pL2Entry->NodeSearch);
            RTListNodeRemove(&pL2Entry->NodeLru);
            pL2Entry->offL2Tbl = 0;
            pL2Entry->cRefs    = 1;
        }
        else
            pL2Entry = NULL;
    }

    return pL2Entry;
}

/**
 * Frees a L2 table cache entry.
 *
 * @returns nothing.
 * @param   pImage    The image instance data.
 * @param   pL2Entry  The L2 cache entry to free.
 */
static void qcowL2TblCacheEntryFree(PQCOWIMAGE pImage, PQCOWL2CACHEENTRY pL2Entry)
{
    Assert(!pL2Entry->cRefs);
    RTMemPageFree(pL2Entry->paL2Tbl, pImage->cbL2Table);
    RTMemFree(pL2Entry);

    pImage->cbL2Cache -= pImage->cbL2Table;
}

/**
 * Inserts an entry in the L2 table cache.
 *
 * @returns nothing.
 * @param   pImage    The image instance data.
 * @param   pL2Entry  The L2 cache entry to insert.
 */
static void qcowL2TblCacheEntryInsert(PQCOWIMAGE pImage, PQCOWL2CACHEENTRY pL2Entry)
{
    PQCOWL2CACHEENTRY pIt = NULL;

    Assert(pL2Entry->offL2Tbl > 0);

    /* Insert at the top of the LRU list. */
    RTListPrepend(&pImage->ListLru, &pL2Entry->NodeLru);

    if (RTListIsEmpty(&pImage->ListSearch))
    {
        RTListAppend(&pImage->ListSearch, &pL2Entry->NodeSearch);
    }
    else
    {
        /* Insert into search list. */
        pIt = RTListGetFirst(&pImage->ListSearch, QCOWL2CACHEENTRY, NodeSearch);
        if (pIt->offL2Tbl > pL2Entry->offL2Tbl)
            RTListPrepend(&pImage->ListSearch, &pL2Entry->NodeSearch);
        else
        {
            bool fInserted = false;

            RTListForEach(&pImage->ListSearch, pIt, QCOWL2CACHEENTRY, NodeSearch)
            {
                Assert(pIt->offL2Tbl != pL2Entry->offL2Tbl);
                if (pIt->offL2Tbl < pL2Entry->offL2Tbl)
                {
                    RTListNodeInsertAfter(&pIt->NodeSearch, &pL2Entry->NodeSearch);
                    fInserted = true;
                    break;
                }
            }
             Assert(fInserted);
        }
    }
}

/**
 * Fetches the L2 from the given offset trying the LRU cache first and
 * reading it from the image after a cache miss.
 *
 * @returns VBox status code.
 * @param   pImage    Image instance data.
 * @param   offL2Tbl  The offset of the L2 table in the image.
 * @param   ppL2Entry Where to store the L2 table on success.
 */
static int qcowL2TblCacheFetch(PQCOWIMAGE pImage, uint64_t offL2Tbl, PQCOWL2CACHEENTRY *ppL2Entry)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc(("pImage=%#p offL2Tbl=%llu ppL2Entry=%#p\n", pImage, offL2Tbl, ppL2Entry));

    /* Try to fetch the L2 table from the cache first. */
    PQCOWL2CACHEENTRY pL2Entry = qcowL2TblCacheRetain(pImage, offL2Tbl);
    if (!pL2Entry)
    {
        LogFlowFunc(("Reading L2 table from image\n"));
        pL2Entry = qcowL2TblCacheEntryAlloc(pImage);

        if (pL2Entry)
        {
            /* Read from the image. */
            pL2Entry->offL2Tbl = offL2Tbl;
            rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, offL2Tbl,
                                       pL2Entry->paL2Tbl, pImage->cbL2Table, NULL);
            if (RT_SUCCESS(rc))
            {
#if defined(RT_LITTLE_ENDIAN)
                qcowTableConvertToHostEndianess(pL2Entry->paL2Tbl, pImage->cL2TableEntries);
#endif
                qcowL2TblCacheEntryInsert(pImage, pL2Entry);
            }
            else
            {
                qcowL2TblCacheEntryRelease(pL2Entry);
                qcowL2TblCacheEntryFree(pImage, pL2Entry);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
        *ppL2Entry = pL2Entry;

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Fetches the L2 from the given offset trying the LRU cache first and
 * reading it from the image after a cache miss - version for async I/O.
 *
 * @returns VBox status code.
 * @param   pImage    Image instance data.
 * @param   pIoCtx    The I/O context.
 * @param   offL2Tbl  The offset of the L2 table in the image.
 * @param   ppL2Entry Where to store the L2 table on success.
 */
static int qcowL2TblCacheFetchAsync(PQCOWIMAGE pImage, PVDIOCTX pIoCtx,
                                    uint64_t offL2Tbl, PQCOWL2CACHEENTRY *ppL2Entry)
{
    int rc = VINF_SUCCESS;

    /* Try to fetch the L2 table from the cache first. */
    PQCOWL2CACHEENTRY pL2Entry = qcowL2TblCacheRetain(pImage, offL2Tbl);
    if (!pL2Entry)
    {
        pL2Entry = qcowL2TblCacheEntryAlloc(pImage);

        if (pL2Entry)
        {
            /* Read from the image. */
            PVDMETAXFER pMetaXfer;

            pL2Entry->offL2Tbl = offL2Tbl;
            rc = vdIfIoIntFileReadMetaAsync(pImage->pIfIo, pImage->pStorage,
                                            offL2Tbl, pL2Entry->paL2Tbl,
                                            pImage->cbL2Table, pIoCtx,
                                            &pMetaXfer, NULL, NULL);
            if (RT_SUCCESS(rc))
            {
                vdIfIoIntMetaXferRelease(pImage->pIfIo, pMetaXfer);
#if defined(RT_LITTLE_ENDIAN)
                qcowTableConvertToHostEndianess(pL2Entry->paL2Tbl, pImage->cL2TableEntries);
#endif
                qcowL2TblCacheEntryInsert(pImage, pL2Entry);
            }
            else
            {
                qcowL2TblCacheEntryRelease(pL2Entry);
                qcowL2TblCacheEntryFree(pImage, pL2Entry);
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
        *ppL2Entry = pL2Entry;

    return rc;
}

/**
 * Sets the L1, L2 and offset bitmasks and L1 and L2 bit shift members.
 *
 * @returns nothing.
 * @param   pImage    The image instance data.
 */
static void qcowTableMasksInit(PQCOWIMAGE pImage)
{
    uint32_t cClusterBits, cL2TableBits;

    cClusterBits = qcowGetPowerOfTwo(pImage->cbCluster);
    cL2TableBits = qcowGetPowerOfTwo(pImage->cL2TableEntries);

    Assert(cClusterBits + cL2TableBits < 64);

    pImage->fOffsetMask = ((uint64_t)pImage->cbCluster - 1);
    pImage->fL2Mask     = ((uint64_t)pImage->cL2TableEntries - 1) << cClusterBits;
    pImage->cL2Shift    = cClusterBits;
    pImage->cL1Shift    = cClusterBits + cL2TableBits;
}

/**
 * Converts a given logical offset into the 
 *
 * @returns nothing.
 * @param   pImage         The image instance data.
 * @param   off            The logical offset to convert.
 * @param   pidxL1         Where to store the index in the L1 table on success.
 * @param   pidxL2         Where to store the index in the L2 table on success.
 * @param   poffCluster    Where to store the offset in the cluster on success.
 */
DECLINLINE(void) qcowConvertLogicalOffset(PQCOWIMAGE pImage, uint64_t off, uint32_t *pidxL1,
                                          uint32_t *pidxL2, uint32_t *poffCluster)
{
    AssertPtr(pidxL1);
    AssertPtr(pidxL2);
    AssertPtr(poffCluster);

    *poffCluster = off & pImage->fOffsetMask;
    *pidxL1      = off >> pImage->cL1Shift;
    *pidxL2      = (off & pImage->fL2Mask) >> pImage->cL2Shift;
}

/**
 * Converts Cluster size to a byte size.
 *
 * @returns Number of bytes derived from the given number of clusters.
 * @param   pImage    The image instance data.
 * @param   cClusters The clusters to convert.
 */
DECLINLINE(uint64_t) qcowCluster2Byte(PQCOWIMAGE pImage, uint64_t cClusters)
{
    return cClusters * pImage->cbCluster;
}

/**
 * Converts number of bytes to cluster size rounding to the next cluster.
 *
 * @returns Number of bytes derived from the given number of clusters.
 * @param   pImage    The image instance data.
 * @param   cb        Number of bytes to convert.
 */
DECLINLINE(uint64_t) qcowByte2Cluster(PQCOWIMAGE pImage, uint64_t cb)
{
    return cb / pImage->cbCluster + (cb % pImage->cbCluster ? 1 : 0);
}

/**
 * Allocates a new cluster in the image.
 *
 * @returns The start offset of the new cluster in the image.
 * @param   pImage    The image instance data.
 * @param   cCLusters Number of clusters to allocate.
 */
DECLINLINE(uint64_t) qcowClusterAllocate(PQCOWIMAGE pImage, uint32_t cClusters)
{
    uint64_t offCluster;

    offCluster = pImage->cbImage;
    pImage->cbImage += cClusters*pImage->cbCluster;

    return offCluster;
}

/**
 * Returns the real image offset for a given cluster or an error if the cluster is not
 * yet allocated.
 *
 * @returns VBox status code.
 *          VERR_VD_BLOCK_FREE if the cluster is not yet allocated.
 * @param   pImage        The image instance data.
 * @param   idxL1         The L1 index.
 * @param   idxL2         The L2 index.
 * @param   offCluster    Offset inside the cluster.
 * @param   poffImage     Where to store the image offset on success;
 */
static int qcowConvertToImageOffset(PQCOWIMAGE pImage, uint32_t idxL1, uint32_t idxL2,
                                    uint32_t offCluster, uint64_t *poffImage)
{
    int rc = VERR_VD_BLOCK_FREE;
    LogFlowFunc(("pImage=%#p idxL1=%u idxL2=%u offCluster=%u poffImage=%#p\n",
                 pImage, idxL1, idxL2, offCluster, poffImage));

    AssertReturn(idxL1 < pImage->cL1TableEntries, VERR_INVALID_PARAMETER);
    AssertReturn(idxL2 < pImage->cL2TableEntries, VERR_INVALID_PARAMETER);

    if (pImage->paL1Table[idxL1])
    {
        PQCOWL2CACHEENTRY pL2Entry;

        rc = qcowL2TblCacheFetch(pImage, pImage->paL1Table[idxL1], &pL2Entry);
        if (RT_SUCCESS(rc))
        {
            LogFlowFunc(("cluster start offset %llu\n", pL2Entry->paL2Tbl[idxL2]));
            /* Get real file offset. */
            if (pL2Entry->paL2Tbl[idxL2])
                *poffImage = pL2Entry->paL2Tbl[idxL2] + offCluster;
            else
                rc = VERR_VD_BLOCK_FREE;

            qcowL2TblCacheEntryRelease(pL2Entry);
        }
    }

    LogFlowFunc(("returns rc=%Rrc\n", rc));
    return rc;
}

/**
 * Returns the real image offset for a given cluster or an error if the cluster is not
 * yet allocated- version for async I/O.
 *
 * @returns VBox status code.
 *          VERR_VD_BLOCK_FREE if the cluster is not yet allocated.
 * @param   pImage        The image instance data.
 * @param   pIoCtx        The I/O context.
 * @param   idxL1         The L1 index.
 * @param   idxL2         The L2 index.
 * @param   offCluster    Offset inside the cluster.
 * @param   poffImage     Where to store the image offset on success;
 */
static int qcowConvertToImageOffsetAsync(PQCOWIMAGE pImage, PVDIOCTX pIoCtx,
                                         uint32_t idxL1, uint32_t idxL2,
                                         uint32_t offCluster, uint64_t *poffImage)
{
    int rc = VERR_VD_BLOCK_FREE;

    AssertReturn(idxL1 < pImage->cL1TableEntries, VERR_INVALID_PARAMETER);
    AssertReturn(idxL2 < pImage->cL2TableEntries, VERR_INVALID_PARAMETER);

    if (pImage->paL1Table[idxL1])
    {
        PQCOWL2CACHEENTRY pL2Entry;

        rc = qcowL2TblCacheFetchAsync(pImage, pIoCtx, pImage->paL1Table[idxL1],
                                     &pL2Entry);
        if (RT_SUCCESS(rc))
        {
            /* Get real file offset. */
            if (pL2Entry->paL2Tbl[idxL2])
                *poffImage = pL2Entry->paL2Tbl[idxL2] + offCluster;
            else
                rc = VERR_VD_BLOCK_FREE;

            qcowL2TblCacheEntryRelease(pL2Entry);
        }
    }

    return rc;
}


/**
 * Internal. Flush image data to disk.
 */
static int qcowFlushImage(PQCOWIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (   pImage->pStorage
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        QCowHeader Header;

#if defined(RT_LITTLE_ENDIAN)
        uint64_t *paL1TblImg = (uint64_t *)RTMemAllocZ(pImage->cbL1Table);
        if (paL1TblImg)
        {
            qcowTableConvertFromHostEndianess(paL1TblImg, pImage->paL1Table,
                                              pImage->cL1TableEntries);
            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                        pImage->offL1Table, paL1TblImg,
                                        pImage->cbL1Table, NULL);
            RTMemFree(paL1TblImg);
        }
        else
            rc = VERR_NO_MEMORY;
#else
        /* Write L1 table directly. */
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, pImage->offL1Table,
                                    pImage->paL1Table, pImage->cbL1Table, NULL);
#endif
        if (RT_SUCCESS(rc))
        {
            /* Write header. */
            size_t cbHeader = 0;
            qcowHdrConvertFromHostEndianess(pImage, &Header, &cbHeader);
            rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, 0, &Header,
                                        cbHeader, NULL);
            if (RT_SUCCESS(rc))
                rc = vdIfIoIntFileFlushSync(pImage->pIfIo, pImage->pStorage);
        }
    }

    return rc;
}

/**
 * Flush image data to disk - version for async I/O.
 *
 * @returns VBox status code.
 * @param   pImage    The image instance data.
 * @param   pIoCtx    The I/o context
 */
static int qcowFlushImageAsync(PQCOWIMAGE pImage, PVDIOCTX pIoCtx)
{
    int rc = VINF_SUCCESS;

    if (   pImage->pStorage
        && !(pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY))
    {
        QCowHeader Header;

#if defined(RT_LITTLE_ENDIAN)
        uint64_t *paL1TblImg = (uint64_t *)RTMemAllocZ(pImage->cbL1Table);
        if (paL1TblImg)
        {
            qcowTableConvertFromHostEndianess(paL1TblImg, pImage->paL1Table,
                                             pImage->cL1TableEntries);
            rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                             pImage->offL1Table, paL1TblImg,
                                             pImage->cbL1Table, pIoCtx, NULL, NULL);
            RTMemFree(paL1TblImg);
        }
        else
            rc = VERR_NO_MEMORY;
#else
        /* Write L1 table directly. */
        rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                         pImage->offL1Table, pImage->paL1Table,
                                         pImage->cbL1Table, pIoCtx, NULL, NULL);
#endif
        if (RT_SUCCESS(rc) || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        {
            /* Write header. */
            size_t cbHeader = 0;
            qcowHdrConvertFromHostEndianess(pImage, &Header, &cbHeader);
            rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                             0, &Header, cbHeader,
                                             pIoCtx, NULL, NULL);
            if (RT_SUCCESS(rc) || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                rc = vdIfIoIntFileFlushAsync(pImage->pIfIo, pImage->pStorage,
                                             pIoCtx, NULL, NULL);
        }
    }

    return rc;
}

/**
 * Internal. Free all allocated space for representing an image except pImage,
 * and optionally delete the image from disk.
 */
static int qcowFreeImage(PQCOWIMAGE pImage, bool fDelete)
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
                qcowFlushImage(pImage);

            vdIfIoIntFileClose(pImage->pIfIo, pImage->pStorage);
            pImage->pStorage = NULL;
        }

        if (pImage->paL1Table)
            RTMemFree(pImage->paL1Table);

        if (pImage->pszBackingFilename)
            RTMemFree(pImage->pszBackingFilename);

        qcowL2TblCacheDestroy(pImage);

        if (fDelete && pImage->pszFilename)
            vdIfIoIntFileDelete(pImage->pIfIo, pImage->pszFilename);
    }

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/**
 * Internal: Open an image, constructing all necessary data structures.
 */
static int qcowOpenImage(PQCOWIMAGE pImage, unsigned uOpenFlags)
{
    int rc;

    pImage->uOpenFlags = uOpenFlags;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    /*
     * Open the image.
     */
    rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename,
                           VDOpenFlagsToFileOpenFlags(uOpenFlags,
                                                      false /* fCreate */),
                           &pImage->pStorage);
    if (RT_FAILURE(rc))
    {
        /* Do NOT signal an appropriate error here, as the VD layer has the
         * choice of retrying the open if it failed. */
        goto out;
    }

    uint64_t cbFile;
    QCowHeader Header;
    rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pStorage, &cbFile);
    if (RT_FAILURE(rc))
        goto out;
    if (cbFile > sizeof(Header))
    {
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, 0, &Header, sizeof(Header), NULL);
        if (   RT_SUCCESS(rc)
            && qcowHdrConvertToHostEndianess(&Header))
        {
            pImage->cbImage = cbFile;

            if (Header.u32Version == 1)
            {
                if (!Header.Version.v1.u32CryptMethod)
                {
                    pImage->uVersion           = 1;
                    pImage->offBackingFilename = Header.Version.v1.u64BackingFileOffset;
                    pImage->cbBackingFilename  = Header.Version.v1.u32BackingFileSize;
                    pImage->MTime              = Header.Version.v1.u32MTime;
                    pImage->cbSize             = Header.Version.v1.u64Size;
                    pImage->cbCluster          = RT_BIT_32(Header.Version.v1.u8ClusterBits);
                    pImage->cL2TableEntries    = RT_BIT_32(Header.Version.v1.u8L2Bits);
                    pImage->cbL2Table          = RT_ALIGN_64(pImage->cL2TableEntries * sizeof(uint64_t), pImage->cbCluster);
                    pImage->offL1Table         = Header.Version.v1.u64L1TableOffset;
                    pImage->cL1TableEntries    = pImage->cbSize / (pImage->cbCluster * pImage->cL2TableEntries);
                    if (pImage->cbSize % (pImage->cbCluster * pImage->cL2TableEntries))
                        pImage->cL1TableEntries++;
                    pImage->cbL1Table          = RT_ALIGN_64(pImage->cL1TableEntries * sizeof(uint64_t), pImage->cbCluster);
                }
                else
                    rc = vdIfError(pImage->pIfError, VERR_NOT_SUPPORTED, RT_SRC_POS,
                                   N_("QCow: Encrypted image '%s' is not supported"),
                                   pImage->pszFilename);
            }
            else if (Header.u32Version == 2)
            {
                if (Header.Version.v2.u32CryptMethod)
                    rc = vdIfError(pImage->pIfError, VERR_NOT_SUPPORTED, RT_SRC_POS,
                                   N_("QCow: Encrypted image '%s' is not supported"),
                                   pImage->pszFilename);
                else if (Header.Version.v2.u32NbSnapshots)
                    rc = vdIfError(pImage->pIfError, VERR_NOT_SUPPORTED, RT_SRC_POS,
                                   N_("QCow: Image '%s' contains snapshots which is not supported"),
                                   pImage->pszFilename);
                else
                {
                    rc = vdIfError(pImage->pIfError, VERR_NOT_SUPPORTED, RT_SRC_POS,
                                   N_("QCow: Image '%s' uses version 2 which is not supported"),
                                   pImage->pszFilename);
#if 0 /** @todo: Add support for the reference count table. */
                    pImage->uVersion           = 2;
                    pImage->offBackingFilename = Header.Version.v2.u64BackingFileOffset;
                    pImage->cbBackingFilename  = Header.Version.v2.u32BackingFileSize;
                    pImage->cbSize             = Header.Version.v2.u64Size;
                    pImage->cbCluster          = RT_BIT_32(Header.Version.v2.u32ClusterBits);
                    pImage->cL2TableEntries    = pImage->cbCluster / sizeof(uint8_t);
                    pImage->cbL2Table          = pImage->cbCluster;
                    pImage->offL1Table         = Header.Version.v2.u64L1TableOffset;
                    pImage->cL1TableEntries    = 0; /** @todo */
                    pImage->cbL1Table          = RT_ALIGN_64(pImage->cL1TableEntries * sizeof(uint64_t), pImage->cbCluster);
                    pImage->offRefcountTable   = Header.Version.v2.u64RefcountTableOffset;
                    pImage->cbRefcountTable    = qcowCluster2Byte(pImage, Header.Version.v2.u32RefcountTableClusters);
#endif
                }
            }
            else
                AssertMsgFailed(("Invalid version of image %d\n", Header.u32Version));

            if (   RT_SUCCESS(rc)
                && pImage->cbBackingFilename
                && pImage->offBackingFilename)
            {
                /* Load backing filename from image. */
                pImage->pszFilename = (char *)RTMemAllocZ(pImage->cbBackingFilename + 1); /* +1 for \0 terminator. */
                if (pImage->pszFilename)
                {
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                               pImage->offBackingFilename, pImage->pszBackingFilename,
                                               pImage->cbBackingFilename, NULL);
                }
                else
                    rc = VERR_NO_MEMORY;
            }

            if (   RT_SUCCESS(rc)
                && pImage->cbRefcountTable
                && pImage->offRefcountTable)
            {
                /* Load refcount table. */
                AssertMsgFailed(("TODO\n"));
            }

            if (RT_SUCCESS(rc))
            {
                qcowTableMasksInit(pImage);

                /* Allocate L1 table. */
                pImage->paL1Table = (uint64_t *)RTMemAllocZ(pImage->cbL1Table);
                if (pImage->paL1Table)
                {
                    /* Read from the image. */
                    rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage,
                                               pImage->offL1Table, pImage->paL1Table,
                                               pImage->cbL1Table, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        qcowTableConvertToHostEndianess(pImage->paL1Table, pImage->cL1TableEntries);
                        rc = qcowL2TblCacheCreate(pImage);
                        if (RT_FAILURE(rc))
                            rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                           N_("QCow: Creating the L2 table cache for image '%s' failed"),
                                           pImage->pszFilename);
                    }
                    else
                        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS,
                                       N_("QCow: Reading the L1 table for image '%s' failed"),
                                       pImage->pszFilename);
                }
                else
                    rc = vdIfError(pImage->pIfError, VERR_NO_MEMORY, RT_SRC_POS,
                                   N_("QCow: Out of memory allocating L1 table for image '%s'"),
                                   pImage->pszFilename);
            }
        }
        else if (RT_SUCCESS(rc))
            rc = VERR_VD_GEN_INVALID_HEADER;
    }
    else
        rc = VERR_VD_GEN_INVALID_HEADER;

out:
    if (RT_FAILURE(rc))
        qcowFreeImage(pImage, false);
    return rc;
}

/**
 * Internal: Create a qcow image.
 */
static int qcowCreateImage(PQCOWIMAGE pImage, uint64_t cbSize,
                           unsigned uImageFlags, const char *pszComment,
                           PCVDGEOMETRY pPCHSGeometry,
                           PCVDGEOMETRY pLCHSGeometry, unsigned uOpenFlags,
                           PFNVDPROGRESS pfnProgress, void *pvUser,
                           unsigned uPercentStart, unsigned uPercentSpan)
{
    int rc;
    int32_t fOpen;

    if (uImageFlags & VD_IMAGE_FLAGS_FIXED)
    {
        rc = vdIfError(pImage->pIfError, VERR_VD_INVALID_TYPE, RT_SRC_POS, N_("QCow: cannot create fixed image '%s'"), pImage->pszFilename);
        goto out;
    }

    pImage->uOpenFlags   = uOpenFlags & ~VD_OPEN_FLAGS_READONLY;
    pImage->uImageFlags  = uImageFlags;
    pImage->PCHSGeometry = *pPCHSGeometry;
    pImage->LCHSGeometry = *pLCHSGeometry;

    pImage->pIfError = VDIfErrorGet(pImage->pVDIfsDisk);
    pImage->pIfIo = VDIfIoIntGet(pImage->pVDIfsImage);
    AssertPtrReturn(pImage->pIfIo, VERR_INVALID_PARAMETER);

    /* Create image file. */
    fOpen = VDOpenFlagsToFileOpenFlags(pImage->uOpenFlags, true /* fCreate */);
    rc = vdIfIoIntFileOpen(pImage->pIfIo, pImage->pszFilename, fOpen, &pImage->pStorage);
    if (RT_FAILURE(rc))
    {
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("QCow: cannot create image '%s'"), pImage->pszFilename);
        goto out;
    }

    /* Init image state. */
    pImage->uVersion           = 1; /* We create only version 1 images at the moment. */
    pImage->cbSize             = cbSize;
    pImage->cbCluster          = QCOW_CLUSTER_SIZE_DEFAULT;
    pImage->cbL2Table          = qcowCluster2Byte(pImage, QCOW_L2_CLUSTERS_DEFAULT);
    pImage->cL2TableEntries    = pImage->cbL2Table / sizeof(uint64_t);
    pImage->cL1TableEntries    = cbSize / (pImage->cbCluster * pImage->cL2TableEntries);
    if (cbSize % (pImage->cbCluster * pImage->cL2TableEntries))
        pImage->cL1TableEntries++;
    pImage->cbL1Table          = pImage->cL1TableEntries * sizeof(uint64_t);
    pImage->offL1Table         = QCOW_V1_HDR_SIZE;
    pImage->cbBackingFilename  = 0;
    pImage->offBackingFilename = 0;
    pImage->cbImage            = RT_ALIGN_64(QCOW_V1_HDR_SIZE + pImage->cbL1Table, pImage->cbCluster);
    qcowTableMasksInit(pImage);

    /* Init L1 table. */
    pImage->paL1Table = (uint64_t *)RTMemAllocZ(pImage->cbL1Table);
    if (!pImage->paL1Table)
    {
        rc = vdIfError(pImage->pIfError, VERR_NO_MEMORY, RT_SRC_POS, N_("QCow: cannot allocate memory for L1 table of image '%s'"),
                       pImage->pszFilename);
        goto out;
    }

    rc = qcowL2TblCacheCreate(pImage);
    if (RT_FAILURE(rc))
    {
        rc = vdIfError(pImage->pIfError, rc, RT_SRC_POS, N_("QCow: Failed to create L2 cache for image '%s'"),
                       pImage->pszFilename);
        goto out;
    }

    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan * 98 / 100);

    rc = qcowFlushImage(pImage);
    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage, pImage->cbImage);

out:
    if (RT_SUCCESS(rc) && pfnProgress)
        pfnProgress(pvUser, uPercentStart + uPercentSpan);

    if (RT_FAILURE(rc))
        qcowFreeImage(pImage, rc != VERR_ALREADY_EXISTS);
    return rc;
}

/**
 * Rollback anything done during async cluster allocation.
 *
 * @returns VBox status code.
 * @param   pImage           The image instance data.
 * @param   pIoCtx           The I/O context.
 * @param   pClusterAlloc    The cluster allocation to rollback.
 */
static int qcowAsyncClusterAllocRollback(PQCOWIMAGE pImage, PVDIOCTX pIoCtx, PQCOWCLUSTERASYNCALLOC pClusterAlloc)
{
    int rc = VINF_SUCCESS;

    switch (pClusterAlloc->enmAllocState)
    {
        case QCOWCLUSTERASYNCALLOCSTATE_L2_ALLOC:
        case QCOWCLUSTERASYNCALLOCSTATE_L2_LINK:
        {
            /* Assumption right now is that the L1 table is not modified if the link fails. */
            rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage, pClusterAlloc->cbImageOld);
            qcowL2TblCacheEntryRelease(pClusterAlloc->pL2Entry); /* Release L2 cache entry. */
            qcowL2TblCacheEntryFree(pImage, pClusterAlloc->pL2Entry); /* Free it, it is not in the cache yet. */
        }
        case QCOWCLUSTERASYNCALLOCSTATE_USER_ALLOC:
        case QCOWCLUSTERASYNCALLOCSTATE_USER_LINK:
        {
            /* Assumption right now is that the L2 table is not modified if the link fails. */
            rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage, pClusterAlloc->cbImageOld);
            qcowL2TblCacheEntryRelease(pClusterAlloc->pL2Entry); /* Release L2 cache entry. */
        }
        default:
            AssertMsgFailed(("Invalid cluster allocation state %d\n", pClusterAlloc->enmAllocState));
            rc = VERR_INVALID_STATE;
    }

    RTMemFree(pClusterAlloc);
    return rc;
}

/**
 * Updates the state of the async cluster allocation.
 *
 * @returns VBox status code.
 * @param   pBackendData    The opaque backend data.
 * @param   pIoCtx          I/O context associated with this request.
 * @param   pvUser          Opaque user data passed during a read/write request.
 * @param   rcReq           Status code for the completed request.
 */
static DECLCALLBACK(int) qcowAsyncClusterAllocUpdate(void *pBackendData, PVDIOCTX pIoCtx, void *pvUser, int rcReq)
{
    int rc = VINF_SUCCESS;
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    PQCOWCLUSTERASYNCALLOC pClusterAlloc = (PQCOWCLUSTERASYNCALLOC)pvUser;

    if (RT_FAILURE(rcReq))
        return qcowAsyncClusterAllocRollback(pImage, pIoCtx, pClusterAlloc);

    AssertPtr(pClusterAlloc->pL2Entry);

    switch (pClusterAlloc->enmAllocState)
    {
        case QCOWCLUSTERASYNCALLOCSTATE_L2_ALLOC:
        {
            uint64_t offUpdateLe = RT_H2BE_U64(pClusterAlloc->pL2Entry->offL2Tbl);

            /* Update the link in the on disk L1 table now. */
            pClusterAlloc->enmAllocState = QCOWCLUSTERASYNCALLOCSTATE_L2_LINK;
            rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                             pImage->offL1Table + pClusterAlloc->idxL1*sizeof(uint64_t),
                                             &offUpdateLe, sizeof(uint64_t), pIoCtx,
                                             qcowAsyncClusterAllocUpdate, pClusterAlloc);
            if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                break;
            else if (RT_FAILURE(rc))
            {
                /* Rollback. */
                qcowAsyncClusterAllocRollback(pImage, pIoCtx, pClusterAlloc);
                break;
            }
            /* Success, fall through. */
        }
        case QCOWCLUSTERASYNCALLOCSTATE_L2_LINK:
        {
            /* L2 link updated in L1 , save L2 entry in cache and allocate new user data cluster. */
            uint64_t offData = qcowClusterAllocate(pImage, 1);

            /* Update the link in the in memory L1 table now. */
            pImage->paL1Table[pClusterAlloc->idxL1] = pClusterAlloc->pL2Entry->offL2Tbl;
            qcowL2TblCacheEntryInsert(pImage, pClusterAlloc->pL2Entry);

            pClusterAlloc->enmAllocState = QCOWCLUSTERASYNCALLOCSTATE_USER_ALLOC;
            pClusterAlloc->cbImageOld    = offData;
            pClusterAlloc->offClusterNew = offData;

            /* Write data. */
            rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                             offData, pIoCtx, pClusterAlloc->cbToWrite,
                                             qcowAsyncClusterAllocUpdate, pClusterAlloc);
            if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                break;
            else if (RT_FAILURE(rc))
            {
                qcowAsyncClusterAllocRollback(pImage, pIoCtx, pClusterAlloc);
                RTMemFree(pClusterAlloc);
                break;
            }
        }
        case QCOWCLUSTERASYNCALLOCSTATE_USER_ALLOC:
        {
            uint64_t offUpdateLe = RT_H2BE_U64(pClusterAlloc->offClusterNew);

            pClusterAlloc->enmAllocState = QCOWCLUSTERASYNCALLOCSTATE_USER_LINK;

            /* Link L2 table and update it. */
            rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                             pImage->paL1Table[pClusterAlloc->idxL1] + pClusterAlloc->idxL2*sizeof(uint64_t),
                                             &offUpdateLe, sizeof(uint64_t), pIoCtx,
                                             qcowAsyncClusterAllocUpdate, pClusterAlloc);
            if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                break;
            else if (RT_FAILURE(rc))
            {
                qcowAsyncClusterAllocRollback(pImage, pIoCtx, pClusterAlloc);
                RTMemFree(pClusterAlloc);
                break;
            }
        }
        case QCOWCLUSTERASYNCALLOCSTATE_USER_LINK:
        {
            /* Everything done without errors, signal completion. */
            pClusterAlloc->pL2Entry->paL2Tbl[pClusterAlloc->idxL2] = pClusterAlloc->offClusterNew;
            qcowL2TblCacheEntryRelease(pClusterAlloc->pL2Entry);
            RTMemFree(pClusterAlloc);
            rc = VINF_SUCCESS;
            break;
        }
        default:
            AssertMsgFailed(("Invalid async cluster allocation state %d\n",
                             pClusterAlloc->enmAllocState));
    }

    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static int qcowCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk,
                           PVDINTERFACE pVDIfsImage, VDTYPE *penmType)
{
    LogFlowFunc(("pszFilename=\"%s\" pVDIfsDisk=%#p pVDIfsImage=%#p\n", pszFilename, pVDIfsDisk, pVDIfsImage));
    PVDIOSTORAGE pStorage = NULL;
    uint64_t cbFile;
    int rc = VINF_SUCCESS;

    /* Get I/O interface. */
    PVDINTERFACEIOINT pIfIo = VDIfIoIntGet(pVDIfsImage);
    AssertPtrReturn(pIfIo, VERR_INVALID_PARAMETER);

    if (   !VALID_PTR(pszFilename)
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /*
     * Open the file and read the footer.
     */
    rc = vdIfIoIntFileOpen(pIfIo, pszFilename,
                           VDOpenFlagsToFileOpenFlags(VD_OPEN_FLAGS_READONLY,
                                                      false /* fCreate */),
                           &pStorage);
    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileGetSize(pIfIo, pStorage, &cbFile);

    if (   RT_SUCCESS(rc)
        && cbFile > sizeof(QCowHeader))
    {
        QCowHeader Header;

        rc = vdIfIoIntFileReadSync(pIfIo, pStorage, 0, &Header, sizeof(Header), NULL);
        if (   RT_SUCCESS(rc)
            && qcowHdrConvertToHostEndianess(&Header))
        {
            *penmType = VDTYPE_HDD;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEN_INVALID_HEADER;
    }
    else
        rc = VERR_VD_GEN_INVALID_HEADER;

    if (pStorage)
        vdIfIoIntFileClose(pIfIo, pStorage);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnOpen */
static int qcowOpen(const char *pszFilename, unsigned uOpenFlags,
                   PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                   VDTYPE enmType, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" uOpenFlags=%#x pVDIfsDisk=%#p pVDIfsImage=%#p ppBackendData=%#p\n", pszFilename, uOpenFlags, pVDIfsDisk, pVDIfsImage, ppBackendData));
    int rc;
    PQCOWIMAGE pImage;

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


    pImage = (PQCOWIMAGE)RTMemAllocZ(sizeof(QCOWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = qcowOpenImage(pImage, uOpenFlags);
    if (RT_SUCCESS(rc))
        *ppBackendData = pImage;
    else
        RTMemFree(pImage);

out:
    LogFlowFunc(("returns %Rrc (pBackendData=%#p)\n", rc, *ppBackendData));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int qcowCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCVDGEOMETRY pPCHSGeometry, PCVDGEOMETRY pLCHSGeometry,
                     PCRTUUID pUuid, unsigned uOpenFlags,
                     unsigned uPercentStart, unsigned uPercentSpan,
                     PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                     PVDINTERFACE pVDIfsOperation, void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    int rc;
    PQCOWIMAGE pImage;

    PFNVDPROGRESS pfnProgress = NULL;
    void *pvUser = NULL;
    PVDINTERFACEPROGRESS pIfProgress = VDIfProgressGet(pVDIfsOperation);
    if (pIfProgress)
    {
        pfnProgress = pIfProgress->pfnProgress;
        pvUser = pIfProgress->Core.pvUser;
    }

    /* Check open flags. All valid flags are supported. */
    if (uOpenFlags & ~VD_OPEN_FLAGS_MASK)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Check remaining arguments. */
    if (   !VALID_PTR(pszFilename)
        || !*pszFilename
        || !VALID_PTR(pPCHSGeometry)
        || !VALID_PTR(pLCHSGeometry))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pImage = (PQCOWIMAGE)RTMemAllocZ(sizeof(QCOWIMAGE));
    if (!pImage)
    {
        rc = VERR_NO_MEMORY;
        goto out;
    }
    pImage->pszFilename = pszFilename;
    pImage->pStorage = NULL;
    pImage->pVDIfsDisk = pVDIfsDisk;
    pImage->pVDIfsImage = pVDIfsImage;

    rc = qcowCreateImage(pImage, cbSize, uImageFlags, pszComment,
                        pPCHSGeometry, pLCHSGeometry, uOpenFlags,
                        pfnProgress, pvUser, uPercentStart, uPercentSpan);
    if (RT_SUCCESS(rc))
    {
        /* So far the image is opened in read/write mode. Make sure the
         * image is opened in read-only mode if the caller requested that. */
        if (uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            qcowFreeImage(pImage, false);
            rc = qcowOpenImage(pImage, uOpenFlags);
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

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int qcowRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VINF_SUCCESS;
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;

    /* Check arguments. */
    if (   !pImage
        || !pszFilename
        || !*pszFilename)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Close the image. */
    rc = qcowFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;

    /* Rename the file. */
    rc = vdIfIoIntFileMove(pImage->pIfIo, pImage->pszFilename, pszFilename, 0);
    if (RT_FAILURE(rc))
    {
        /* The move failed, try to reopen the original image. */
        int rc2 = qcowOpenImage(pImage, pImage->uOpenFlags);
        if (RT_FAILURE(rc2))
            rc = rc2;

        goto out;
    }

    /* Update pImage with the new information. */
    pImage->pszFilename = pszFilename;

    /* Open the old image with new name. */
    rc = qcowOpenImage(pImage, pImage->uOpenFlags);
    if (RT_FAILURE(rc))
        goto out;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int qcowClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    rc = qcowFreeImage(pImage, fDelete);
    RTMemFree(pImage);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int qcowRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint32_t offCluster = 0;
    uint32_t idxL1      = 0;
    uint32_t idxL2      = 0;
    uint64_t offFile    = 0;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    qcowConvertLogicalOffset(pImage, uOffset, &idxL1, &idxL2, &offCluster);
    LogFlowFunc(("idxL1=%u idxL2=%u offCluster=%u\n", idxL1, idxL2, offCluster));

    /* Clip read size to remain in the cluster. */
    cbToRead = RT_MIN(cbToRead, pImage->cbCluster - offCluster);

    /* Get offset in image. */
    rc = qcowConvertToImageOffset(pImage, idxL1, idxL2, offCluster, &offFile);
    if (RT_SUCCESS(rc))
    {
        LogFlowFunc(("offFile=%llu\n", offFile));
        rc = vdIfIoIntFileReadSync(pImage->pIfIo, pImage->pStorage, offFile,
                                   pvBuf, cbToRead, NULL);
    }

    if (   (RT_SUCCESS(rc) || rc == VERR_VD_BLOCK_FREE)
        && pcbActuallyRead)
        *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int qcowWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess,
                    size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint32_t offCluster = 0;
    uint32_t idxL1      = 0;
    uint32_t idxL2      = 0;
    uint64_t offImage   = 0;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (   uOffset + cbToWrite > pImage->cbSize
        || cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Convert offset to L1, L2 index and cluster offset. */
    qcowConvertLogicalOffset(pImage, uOffset, &idxL1, &idxL2, &offCluster);

    /* Clip write size to remain in the cluster. */
    cbToWrite = RT_MIN(cbToWrite, pImage->cbCluster - offCluster);
    Assert(!(cbToWrite % 512));

    /* Get offset in image. */
    rc = qcowConvertToImageOffset(pImage, idxL1, idxL2, offCluster, &offImage);
    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, offImage,
                                    pvBuf, cbToWrite, NULL);
    else if (rc == VERR_VD_BLOCK_FREE)
    {
        if (   cbToWrite == pImage->cbCluster
            && !(fWrite & VD_WRITE_NO_ALLOC))
        {
            PQCOWL2CACHEENTRY pL2Entry = NULL;

            /* Full cluster write to previously unallocated cluster.
             * Allocate cluster and write data. */
            Assert(!offCluster);

            do
            {
                uint64_t idxUpdateLe = 0;

                /* Check if we have to allocate a new cluster for L2 tables. */
                if (!pImage->paL1Table[idxL1])
                {
                    uint64_t offL2Tbl = qcowClusterAllocate(pImage, qcowByte2Cluster(pImage, pImage->cbL2Table));

                    pL2Entry = qcowL2TblCacheEntryAlloc(pImage);
                    if (!pL2Entry)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    pL2Entry->offL2Tbl = offL2Tbl;
                    memset(pL2Entry->paL2Tbl, 0, pImage->cbL2Table);
                    qcowL2TblCacheEntryInsert(pImage, pL2Entry);

                    /*
                     * Write the L2 table first and link to the L1 table afterwards.
                     * If something unexpected happens the worst case which can happen
                     * is a leak of some clusters.
                     */
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage, offL2Tbl,
                                                pL2Entry->paL2Tbl, pImage->cbL2Table, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    /* Write the L1 link now. */
                    pImage->paL1Table[idxL1] = offL2Tbl;
                    idxUpdateLe = RT_H2BE_U64(offL2Tbl);
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                pImage->offL1Table + idxL1*sizeof(uint64_t),
                                                &idxUpdateLe, sizeof(uint64_t), NULL);
                    if (RT_FAILURE(rc))
                        break;
                }
                else
                    rc = qcowL2TblCacheFetch(pImage, pImage->paL1Table[idxL1], &pL2Entry);

                if (RT_SUCCESS(rc))
                {
                    /* Allocate new cluster for the data. */
                    uint64_t offData = qcowClusterAllocate(pImage, 1);

                    /* Write data. */
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                offData, pvBuf, cbToWrite, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    /* Link L2 table and update it. */
                    pL2Entry->paL2Tbl[idxL2] = offData;
                    idxUpdateLe = RT_H2BE_U64(offData);
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                pImage->paL1Table[idxL1] + idxL2*sizeof(uint64_t),
                                                &idxUpdateLe, sizeof(uint64_t), NULL);
                    qcowL2TblCacheEntryRelease(pL2Entry);
                }

            } while (0);

            *pcbPreRead = 0;
            *pcbPostRead = 0;
        }
        else
        {
            /* Trying to do a partial write to an unallocated cluster. Don't do
             * anything except letting the upper layer know what to do. */
            *pcbPreRead = offCluster;
            *pcbPostRead = pImage->cbCluster - cbToWrite - *pcbPreRead;
        }
    }

    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int qcowFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    rc = qcowFlushImage(pImage);
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned qcowGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;

    AssertPtr(pImage);

    if (pImage)
        return pImage->uVersion;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t qcowGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage && pImage->pStorage)
        cb = pImage->cbSize;

    LogFlowFunc(("returns %llu\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t qcowGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint64_t cb = 0;

    AssertPtr(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (pImage->pStorage)
        {
            int rc = vdIfIoIntFileGetSize(pImage->pIfIo, pImage->pStorage, &cbFile);
            if (RT_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int qcowGetPCHSGeometry(void *pBackendData,
                              PVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->PCHSGeometry.cCylinders)
        {
            *pPCHSGeometry = pImage->PCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (PCHS=%u/%u/%u)\n", rc, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetPCHSGeometry */
static int qcowSetPCHSGeometry(void *pBackendData,
                              PCVDGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n", pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->PCHSGeometry = *pPCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetLCHSGeometry */
static int qcowGetLCHSGeometry(void *pBackendData,
                              PVDGEOMETRY pLCHSGeometry)
{
     LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->LCHSGeometry.cCylinders)
        {
            *pLCHSGeometry = pImage->LCHSGeometry;
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_VD_GEOMETRY_NOT_SET;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (LCHS=%u/%u/%u)\n", rc, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetLCHSGeometry */
static int qcowSetLCHSGeometry(void *pBackendData,
                               PCVDGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n", pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
        {
            rc = VERR_VD_IMAGE_READ_ONLY;
            goto out;
        }

        pImage->LCHSGeometry = *pLCHSGeometry;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetImageFlags */
static unsigned qcowGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    unsigned uImageFlags;

    AssertPtr(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned qcowGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    unsigned uOpenFlags;

    AssertPtr(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int qcowSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO | VD_OPEN_FLAGS_ASYNC_IO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Implement this operation via reopening the image. */
    rc = qcowFreeImage(pImage, false);
    if (RT_FAILURE(rc))
        goto out;
    rc = qcowOpenImage(pImage, uOpenFlags);

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int qcowGetComment(void *pBackendData, char *pszComment,
                          size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int qcowSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int qcowGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int qcowSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetModificationUuid */
static int qcowGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int qcowSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetParentUuid */
static int qcowGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int qcowSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnGetParentModificationUuid */
static int qcowGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc;

    AssertPtr(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int qcowSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
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

/** @copydoc VBOXHDDBACKEND::pfnDump */
static void qcowDump(void *pBackendData)
{
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        vdIfErrorMessage(pImage->pIfError, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cSector=%llu\n",
                         pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                         pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                         pImage->cbSize / 512);
    }
}

/** @copydoc VBOXHDDBACKEND::pfnGetParentFilename */
static int qcowGetParentFilename(void *pBackendData, char **ppszParentFilename)
{
    int rc = VINF_SUCCESS;
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
        if (pImage->pszFilename)
            *ppszParentFilename = RTStrDup(pImage->pszBackingFilename);
        else
            rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentFilename */
static int qcowSetParentFilename(void *pBackendData, const char *pszParentFilename)
{
    int rc = VINF_SUCCESS;
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;

    AssertPtr(pImage);
    if (pImage)
    {
        if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
            rc = VERR_VD_IMAGE_READ_ONLY;
        else if (   pImage->pszBackingFilename
                 && (strlen(pszParentFilename) > pImage->cbBackingFilename))
            rc = VERR_NOT_SUPPORTED; /* The new filename is longer than the old one. */
        else
        {
            if (pImage->pszBackingFilename)
                RTStrFree(pImage->pszBackingFilename);
            pImage->pszBackingFilename = RTStrDup(pszParentFilename);
            if (!pImage->pszBackingFilename)
                rc = VERR_NO_MEMORY;
            else
            {
                if (!pImage->offBackingFilename)
                {
                    /* Allocate new cluster. */
                    uint64_t offData = qcowClusterAllocate(pImage, 1);

                    Assert((offData & UINT32_MAX) == offData);
                    pImage->offBackingFilename = (uint32_t)offData;
                    pImage->cbBackingFilename  = strlen(pszParentFilename);
                    rc = vdIfIoIntFileSetSize(pImage->pIfIo, pImage->pStorage,
                                              offData + pImage->cbCluster);
                }

                if (RT_SUCCESS(rc))
                    rc = vdIfIoIntFileWriteSync(pImage->pIfIo, pImage->pStorage,
                                                pImage->offBackingFilename,
                                                pImage->pszBackingFilename,
                                                strlen(pImage->pszBackingFilename),
                                                NULL);
            }
        }
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

static int qcowAsyncRead(void *pBackendData, uint64_t uOffset, size_t cbToRead,
                        PVDIOCTX pIoCtx, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToRead=%zu pcbActuallyRead=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToRead, pcbActuallyRead));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint32_t offCluster = 0;
    uint32_t idxL1      = 0;
    uint32_t idxL2      = 0;
    uint64_t offFile    = 0;
    int rc;

    AssertPtr(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (!VALID_PTR(pIoCtx) || !cbToRead)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    qcowConvertLogicalOffset(pImage, uOffset, &idxL1, &idxL2, &offCluster);

    /* Clip read size to remain in the cluster. */
    cbToRead = RT_MIN(cbToRead, pImage->cbCluster - offCluster);

    /* Get offset in image. */
    rc = qcowConvertToImageOffsetAsync(pImage, pIoCtx, idxL1, idxL2, offCluster,
                                      &offFile);
    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileReadUserAsync(pImage->pIfIo, pImage->pStorage, offFile,
                                        pIoCtx, cbToRead);

    if (   (   RT_SUCCESS(rc)
            || rc == VERR_VD_BLOCK_FREE
            || rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
        && pcbActuallyRead)
        *pcbActuallyRead = cbToRead;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

static int qcowAsyncWrite(void *pBackendData, uint64_t uOffset, size_t cbToWrite,
                         PVDIOCTX pIoCtx,
                         size_t *pcbWriteProcess, size_t *pcbPreRead,
                         size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pIoCtx=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n",
                 pBackendData, uOffset, pIoCtx, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    uint32_t offCluster = 0;
    uint32_t idxL1      = 0;
    uint32_t idxL2      = 0;
    uint64_t offImage   = 0;
    int rc = VINF_SUCCESS;

    AssertPtr(pImage);
    Assert(!(uOffset % 512));
    Assert(!(cbToWrite % 512));

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    if (!VALID_PTR(pIoCtx) || !cbToWrite)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    if (   uOffset + cbToWrite > pImage->cbSize
        || cbToWrite == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    /* Convert offset to L1, L2 index and cluster offset. */
    qcowConvertLogicalOffset(pImage, uOffset, &idxL1, &idxL2, &offCluster);

    /* Clip write size to remain in the cluster. */
    cbToWrite = RT_MIN(cbToWrite, pImage->cbCluster - offCluster);
    Assert(!(cbToWrite % 512));

    /* Get offset in image. */
    rc = qcowConvertToImageOffsetAsync(pImage, pIoCtx, idxL1, idxL2, offCluster,
                                      &offImage);
    if (RT_SUCCESS(rc))
        rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                         offImage, pIoCtx, cbToWrite, NULL, NULL);
    else if (rc == VERR_VD_BLOCK_FREE)
    {
        if (   cbToWrite == pImage->cbCluster
            && !(fWrite & VD_WRITE_NO_ALLOC))
        {
            PQCOWL2CACHEENTRY pL2Entry = NULL;

            /* Full cluster write to previously unallocated cluster.
             * Allocate cluster and write data. */
            Assert(!offCluster);

            do
            {
                uint64_t idxUpdateLe = 0;

                /* Check if we have to allocate a new cluster for L2 tables. */
                if (!pImage->paL1Table[idxL1])
                {
                    uint64_t offL2Tbl;
                    PQCOWCLUSTERASYNCALLOC pL2ClusterAlloc = NULL;

                    /* Allocate new async cluster allocation state. */
                    pL2ClusterAlloc = (PQCOWCLUSTERASYNCALLOC)RTMemAllocZ(sizeof(QCOWCLUSTERASYNCALLOC));
                    if (RT_UNLIKELY(!pL2ClusterAlloc))
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    pL2Entry = qcowL2TblCacheEntryAlloc(pImage);
                    if (!pL2Entry)
                    {
                        rc = VERR_NO_MEMORY;
                        RTMemFree(pL2ClusterAlloc);
                        break;
                    }

                    offL2Tbl = qcowClusterAllocate(pImage, qcowByte2Cluster(pImage, pImage->cbL2Table));
                    pL2Entry->offL2Tbl = offL2Tbl;
                    memset(pL2Entry->paL2Tbl, 0, pImage->cbL2Table);

                    pL2ClusterAlloc->enmAllocState = QCOWCLUSTERASYNCALLOCSTATE_L2_ALLOC;
                    pL2ClusterAlloc->cbImageOld    = offL2Tbl;
                    pL2ClusterAlloc->offClusterNew = offL2Tbl;
                    pL2ClusterAlloc->idxL1         = idxL1;
                    pL2ClusterAlloc->idxL2         = idxL2;
                    pL2ClusterAlloc->cbToWrite     = cbToWrite;
                    pL2ClusterAlloc->pL2Entry      = pL2Entry;

                    /*
                     * Write the L2 table first and link to the L1 table afterwards.
                     * If something unexpected happens the worst case which can happen
                     * is a leak of some clusters.
                     */
                    rc = vdIfIoIntFileWriteMetaAsync(pImage->pIfIo, pImage->pStorage,
                                                     offL2Tbl, pL2Entry->paL2Tbl, pImage->cbL2Table, pIoCtx,
                                                     qcowAsyncClusterAllocUpdate, pL2ClusterAlloc);
                    if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                        break;
                    else if (RT_FAILURE(rc))
                    {
                        RTMemFree(pL2ClusterAlloc);
                        qcowL2TblCacheEntryFree(pImage, pL2Entry);
                        break;
                    }

                    rc = qcowAsyncClusterAllocUpdate(pImage, pIoCtx, pL2ClusterAlloc, rc);
                }
                else
                {
                    rc = qcowL2TblCacheFetchAsync(pImage, pIoCtx, pImage->paL1Table[idxL1],
                                                 &pL2Entry);

                    if (RT_SUCCESS(rc))
                    {
                        PQCOWCLUSTERASYNCALLOC pDataClusterAlloc = NULL;

                        /* Allocate new async cluster allocation state. */
                        pDataClusterAlloc = (PQCOWCLUSTERASYNCALLOC)RTMemAllocZ(sizeof(QCOWCLUSTERASYNCALLOC));
                        if (RT_UNLIKELY(!pDataClusterAlloc))
                        {
                            rc = VERR_NO_MEMORY;
                            break;
                        }

                        /* Allocate new cluster for the data. */
                        uint64_t offData = qcowClusterAllocate(pImage, 1);

                        pDataClusterAlloc->enmAllocState = QCOWCLUSTERASYNCALLOCSTATE_USER_ALLOC;
                        pDataClusterAlloc->cbImageOld    = offData;
                        pDataClusterAlloc->offClusterNew = offData;
                        pDataClusterAlloc->idxL1         = idxL1;
                        pDataClusterAlloc->idxL2         = idxL2;
                        pDataClusterAlloc->cbToWrite     = cbToWrite;
                        pDataClusterAlloc->pL2Entry      = pL2Entry;

                        /* Write data. */
                        rc = vdIfIoIntFileWriteUserAsync(pImage->pIfIo, pImage->pStorage,
                                                         offData, pIoCtx, cbToWrite,
                                                         qcowAsyncClusterAllocUpdate, pDataClusterAlloc);
                        if (rc == VERR_VD_ASYNC_IO_IN_PROGRESS)
                            break;
                        else if (RT_FAILURE(rc))
                        {
                            RTMemFree(pDataClusterAlloc);
                            break;
                        }

                        rc = qcowAsyncClusterAllocUpdate(pImage, pIoCtx, pDataClusterAlloc, rc);
                    }
                }

            } while (0);

            *pcbPreRead = 0;
            *pcbPostRead = 0;
        }
        else
        {
            /* Trying to do a partial write to an unallocated cluster. Don't do
             * anything except letting the upper layer know what to do. */
            *pcbPreRead = offCluster;
            *pcbPostRead = pImage->cbCluster - cbToWrite - *pcbPreRead;
        }
    }

    if (pcbWriteProcess)
        *pcbWriteProcess = cbToWrite;


out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

static int qcowAsyncFlush(void *pBackendData, PVDIOCTX pIoCtx)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PQCOWIMAGE pImage = (PQCOWIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    Assert(pImage);

    if (VALID_PTR(pIoCtx))
        rc = qcowFlushImageAsync(pImage, pIoCtx);
    else
        rc = VERR_INVALID_PARAMETER;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

VBOXHDDBACKEND g_QCowBackend =
{
    /* pszBackendName */
    "QCOW",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_FILE | VD_CAP_VFS | VD_CAP_CREATE_DYNAMIC | VD_CAP_DIFF | VD_CAP_ASYNC,
    /* paFileExtensions */
    s_aQCowFileExtensions,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    qcowCheckIfValid,
    /* pfnOpen */
    qcowOpen,
    /* pfnCreate */
    qcowCreate,
    /* pfnRename */
    qcowRename,
    /* pfnClose */
    qcowClose,
    /* pfnRead */
    qcowRead,
    /* pfnWrite */
    qcowWrite,
    /* pfnFlush */
    qcowFlush,
    /* pfnGetVersion */
    qcowGetVersion,
    /* pfnGetSize */
    qcowGetSize,
    /* pfnGetFileSize */
    qcowGetFileSize,
    /* pfnGetPCHSGeometry */
    qcowGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    qcowSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    qcowGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    qcowSetLCHSGeometry,
    /* pfnGetImageFlags */
    qcowGetImageFlags,
    /* pfnGetOpenFlags */
    qcowGetOpenFlags,
    /* pfnSetOpenFlags */
    qcowSetOpenFlags,
    /* pfnGetComment */
    qcowGetComment,
    /* pfnSetComment */
    qcowSetComment,
    /* pfnGetUuid */
    qcowGetUuid,
    /* pfnSetUuid */
    qcowSetUuid,
    /* pfnGetModificationUuid */
    qcowGetModificationUuid,
    /* pfnSetModificationUuid */
    qcowSetModificationUuid,
    /* pfnGetParentUuid */
    qcowGetParentUuid,
    /* pfnSetParentUuid */
    qcowSetParentUuid,
    /* pfnGetParentModificationUuid */
    qcowGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    qcowSetParentModificationUuid,
    /* pfnDump */
    qcowDump,
    /* pfnGetTimeStamp */
    NULL,
    /* pfnGetParentTimeStamp */
    NULL,
    /* pfnSetParentTimeStamp */
    NULL,
    /* pfnGetParentFilename */
    qcowGetParentFilename,
    /* pfnSetParentFilename */
    qcowSetParentFilename,
    /* pfnAsyncRead */
    qcowAsyncRead,
    /* pfnAsyncWrite */
    qcowAsyncWrite,
    /* pfnAsyncFlush */
    qcowAsyncFlush,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL,
    /* pfnDiscard */
    NULL
};
