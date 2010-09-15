/* $Id$ */
/** @file
 * VBoxDMG - Intepreter for Apple Disk Images (DMG).
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_DEFAULT /** @todo log group */
#include <VBox/VBoxHDD-Plugin.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/file.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/base64.h>
#ifdef VBOXDMG_TESTING
# include <iprt/initterm.h>
# include <iprt/stream.h>
#endif

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Sector size, multiply with all sector counts to get number of bytes. */
#define VBOXDMG_SECTOR_SIZE 512

/**
 * UDIF checksum structure.
 */
typedef struct VBOXUDIFCKSUM
{
    uint32_t        u32Kind;                    /**< The kind of checksum.  */
    uint32_t        cBits;                      /**< The size of the checksum. */
    union
    {
        uint8_t     au8[128];                   /**< 8-bit view. */
        uint32_t    au32[32];                   /**< 32-bit view. */
    }               uSum;                       /**< The checksum. */
} VBOXUDIFCKSUM;
AssertCompileSize(VBOXUDIFCKSUM, 8 + 128);
typedef VBOXUDIFCKSUM *PVBOXUDIFCKSUM;
typedef const VBOXUDIFCKSUM *PCVBOXUDIFCKSUM;

/** @name Checksum Kind (VBOXUDIFCKSUM::u32Kind)
 * @{ */
/** No checksum. */
#define VBOXUDIFCKSUM_NONE          UINT32_C(0)
/** CRC-32. */
#define VBOXUDIFCKSUM_CRC32         UINT32_C(2)
/** @} */

/**
 * UDIF ID.
 * This is kind of like a UUID only it isn't, but we'll use the UUID
 * representation of it for simplicity.
 */
typedef RTUUID VBOXUDIFID;
AssertCompileSize(VBOXUDIFID, 16);
typedef VBOXUDIFID *PVBOXUDIFID;
typedef const VBOXUDIFID *PCVBOXUDIFID;

/**
 * UDIF footer used by Apple Disk Images (DMG).
 *
 * This is a footer placed 512 bytes from the end of the file. Typically a DMG
 * file starts with the data, which is followed by the block table and then ends
 * with this structure.
 *
 * All fields are stored in big endian format.
 */
#pragma pack(1)
typedef struct VBOXUDIF
{
    uint32_t            u32Magic;               /**< 0x000 - Magic, 'koly' (VBOXUDIF_MAGIC).                       (fUDIFSignature) */
    uint32_t            u32Version;             /**< 0x004 - The UDIF version (VBOXUDIF_VER_CURRENT).              (fUDIFVersion) */
    uint32_t            cbFooter;               /**< 0x008 - The size of the this structure (512).                 (fUDIFHeaderSize) */
    uint32_t            fFlags;                 /**< 0x00c - Flags.                                                (fUDIFFlags) */
    uint64_t            offRunData;             /**< 0x010 - Where the running data fork starts (usually 0).       (fUDIFRunningDataForkOffset) */
    uint64_t            offData;                /**< 0x018 - Where the data fork starts (usually 0).               (fUDIFDataForkOffset) */
    uint64_t            cbData;                 /**< 0x020 - Size of the data fork (in bytes).                     (fUDIFDataForkLength) */
    uint64_t            offRsrc;                /**< 0x028 - Where the resource fork starts (usually cbData or 0). (fUDIFRsrcForkOffset) */
    uint64_t            cbRsrc;                 /**< 0x030 - The size of the resource fork.                        (fUDIFRsrcForkLength)*/
    uint32_t            iSegment;               /**< 0x038 - The segment number of this file.                      (fUDIFSegmentNumber) */
    uint32_t            cSegments;              /**< 0x03c - The number of segments.                               (fUDIFSegmentCount) */
    VBOXUDIFID          SegmentId;              /**< 0x040 - The segment ID.                                       (fUDIFSegmentID) */
    VBOXUDIFCKSUM       DataCkSum;              /**< 0x050 - The data checksum.                                    (fUDIFDataForkChecksum) */
    uint64_t            offXml;                 /**< 0x0d8 - The XML offset (.plist kind of data).                 (fUDIFXMLOffset) */
    uint64_t            cbXml;                  /**< 0x0e0 - The size of the XML.                                  (fUDIFXMLSize) */
    uint8_t             abUnknown[120];         /**< 0x0e8 - Unknown stuff, hdiutil doesn't dump it... */
    VBOXUDIFCKSUM       MasterCkSum;            /**< 0x160 - The master checksum.                                  (fUDIFMasterChecksum) */
    uint32_t            u32Type;                /**< 0x1e8 - The image type.                                       (fUDIFImageVariant) */
    uint64_t            cSectors;               /**< 0x1ec - The sector count. Warning! Unaligned!                 (fUDISectorCount) */
    uint32_t            au32Unknown[3];         /**< 0x1f4 - Unknown stuff, hdiutil doesn't dump it... */
} VBOXUDIF;
#pragma pack(0)
AssertCompileSize(VBOXUDIF, 512);
AssertCompileMemberOffset(VBOXUDIF, cbRsrc,   0x030);
AssertCompileMemberOffset(VBOXUDIF, cbXml,    0x0e0);
AssertCompileMemberOffset(VBOXUDIF, cSectors, 0x1ec);

typedef VBOXUDIF *PVBOXUDIF;
typedef const VBOXUDIF *PCVBOXUDIF;

/** The UDIF magic 'koly' (VBOXUDIF::u32Magic). */
#define VBOXUDIF_MAGIC              UINT32_C(0x6b6f6c79)

/** The current UDIF version (VBOXUDIF::u32Version).
 * This is currently the only we recognizes and will create. */
#define VBOXUDIF_VER_CURRENT        4

/** @name UDIF flags (VBOXUDIF::fFlags).
 * @{ */
/** Flatten image whatever that means.
 * (hdiutil -debug calls it kUDIFFlagsFlattened.) */
#define VBOXUDIF_FLAGS_FLATTENED    RT_BIT_32(0)
/** Internet enabled image.
 * (hdiutil -debug calls it kUDIFFlagsInternetEnabled) */
#define VBOXUDIF_FLAGS_INET_ENABLED RT_BIT_32(2)
/** Mask of known bits. */
#define VBOXUDIF_FLAGS_KNOWN_MASK   (RT_BIT_32(0) | RT_BIT_32(2))
/** @} */

/** @name UDIF Image Types (VBOXUDIF::u32Type).
 * @{ */
/** Device image type. (kUDIFDeviceImageType) */
#define VBOXUDIF_TYPE_DEVICE        1
/** Device image type. (kUDIFPartitionImageType) */
#define VBOXUDIF_TYPE_PARTITION     2
/** @}  */

/**
 * BLKX data.
 *
 * This contains the start offset and size of raw data sotred in the image.
 *
 * All fields are stored in big endian format.
 */
#pragma pack(1)
typedef struct VBOXBLKX
{
    uint32_t            u32Magic;               /**< 0x000 - Magic, 'mish' (VBOXBLKX_MAGIC). */
    uint32_t            u32Version;             /**< 0x004 - The BLKX version (VBOXBLKX_VER_CURRENT). */
    uint64_t            cSectornumberFirst;     /**< 0x008 - The first sector number the block represents in the virtual device. */
    uint64_t            cSectors;               /**< 0x010 - Number of sectors this block represents. */
    uint64_t            offDataStart;           /**< 0x018 - Start offset for raw data. */
    uint32_t            cSectorsDecompress;     /**< 0x020 - Size of the buffer in sectors needed to decompress. */
    uint32_t            u32BlocksDescriptor;    /**< 0x024 - Blocks descriptor. */
    uint8_t             abReserved[24];
    VBOXUDIFCKSUM       BlkxCkSum;              /**< 0x03c - Checksum for the BLKX table. */
    uint32_t            cBlocksRunCount;        /**< 0x    - Number of entries in the blkx run table afterwards. */
} VBOXBLKX;
#pragma pack(0)
AssertCompileSize(VBOXBLKX, 204);

typedef VBOXBLKX *PVBOXBLKX;
typedef const VBOXBLKX *PCVBOXBLKX;

/** The BLKX magic 'mish' (VBOXBLKX::u32Magic). */
#define VBOXBLKX_MAGIC              UINT32_C(0x6d697368)
/** BLKX version (VBOXBLKX::u32Version). */
#define VBOXBLKX_VERSION            UINT32_C(0x00000001)

/** Blocks descriptor type: entire device. */
#define VBOXBLKX_DESC_ENTIRE_DEVICE UINT32_C(0xfffffffe)

/**
 * BLKX table descriptor.
 *
 * All fields are stored in big endian format.
 */
#pragma pack(1)
typedef struct VBOXBLKXDESC
{
    uint32_t            u32Type;                /**< 0x000 - Type of the descriptor. */
    uint32_t            u32Reserved;            /**< 0x004 - Reserved, but contains +beg or +end in case thisi is a comment descriptor. */
    uint64_t            u64SectorStart;         /**< 0x008 - First sector number in the block this entry describes. */
    uint64_t            u64SectorCount;         /**< 0x010 - Number of sectors this entry describes. */
    uint64_t            offData;                /**< 0x018 - Offset in the image where the data starts. */
    uint64_t            cbData;                 /**< 0x020 - Number of bytes in the image. */
} VBOXBLKXDESC;
#pragma pack(0)
AssertCompileSize(VBOXBLKXDESC, 40);

typedef VBOXBLKXDESC *PVBOXBLKXDESC;
typedef const VBOXBLKXDESC *PCVBOXBLKXDESC;

/** Raw image data type. */
#define VBOXBLKXDESC_TYPE_RAW        1
/** Ignore type. */
#define VBOXBLKXDESC_TYPE_IGNORE     2
/** Comment type. */
#define VBOXBLKXDESC_TYPE_COMMENT    UINT32_C(0x7ffffffe)
/** Terminator type. */
#define VBOXBLKXDESC_TYPE_TERMINATOR UINT32_C(0xffffffff)

/**
 * UDIF Resource Entry.
 */
typedef struct VBOXUDIFRSRCENTRY
{
    /** The ID. */
    int32_t             iId;
    /** Attributes. */
    uint32_t            fAttributes;
    /** The name. */
    char               *pszName;
    /** The CoreFoundation name. Can be NULL. */
    char               *pszCFName;
    /** The size of the data. */
    size_t              cbData;
    /** The raw data. */
    uint8_t            *pbData;
} VBOXUDIFRSRCENTRY;
/** Pointer to an UDIF resource entry. */
typedef VBOXUDIFRSRCENTRY *PVBOXUDIFRSRCENTRY;
/** Pointer to a const UDIF resource entry. */
typedef VBOXUDIFRSRCENTRY const *PCVBOXUDIFRSRCENTRY;

/**
 * UDIF Resource Array.
 */
typedef struct VBOXUDIFRSRCARRAY
{
    /** The array name. */
    char                szName[12];
    /** The number of occupied entries. */
    uint32_t            cEntries;
    /** The array entries.
     * A lazy bird ASSUME there are no more than 4 entries in any DMG. Increase the
     * size if DMGs with more are found.
     * r=aeichner: Saw one with 6 here (image of a whole DVD) */
    VBOXUDIFRSRCENTRY   aEntries[10];
} VBOXUDIFRSRCARRAY;
/** Pointer to a UDIF resource array. */
typedef VBOXUDIFRSRCARRAY *PVBOXUDIFRSRCARRAY;
/** Pointer to a const UDIF resource array. */
typedef VBOXUDIFRSRCARRAY const *PCVBOXUDIFRSRCARRAY;

/**
 * DMG extent types.
 */
typedef enum DMGEXTENTTYPE
{
    /** Null, never used. */
    DMGEXTENTTYPE_NULL = 0,
    /** Raw image data. */
    DMGEXTENTTYPE_RAW,
    /** Zero extent, reads return 0 and writes have no effect. */
    DMGEXTENTTYPE_ZERO,
    /** 32bit hack. */
    DMGEXTENTTYPE_32BIT_HACK = 0x7fffffff
} DMGEXTENTTYPE, *PDMGEXTENTTYPE;

/**
 * DMG extent mapping a virtual image block to real file offsets.
 */
typedef struct DMGEXTENT
{
    /** Next DMG extent, sorted by virtual sector count. */
    struct DMGEXTENT    *pNext;
    /** Extent type. */
    DMGEXTENTTYPE        enmType;
    /** First byte this extent describes. */
    uint64_t             offExtent;
    /** Number of bytes this extent describes. */
    uint64_t             cbExtent;
    /** Start offset in the real file. */
    uint64_t             offFileStart;
} DMGEXTENT;
/** Pointer to an DMG extent. */
typedef DMGEXTENT *PDMGEXTENT;

/**
 * VirtualBox Apple Disk Image (DMG) interpreter instance data.
 */
typedef struct DMGIMAGE
{
    /** Pointer to the per-disk VD interface list. */
    PVDINTERFACE        pVDIfsDisk;
    /** Pointer to the per-image VD interface list. */
    PVDINTERFACE        pVDIfsImage;

    /** Storage handle. */
    PVDIOSTORAGE        pStorage;
    /** Size of the image. */
    uint64_t            cbFile;
    /** I/O interface. */
    PVDINTERFACE        pInterfaceIO;
    /** Async I/O interface callbacks. */
    PVDINTERFACEIO      pInterfaceIOCallbacks;
    /** Error callback. */
    PVDINTERFACE        pInterfaceError;
    /** Opaque data for error callback. */
    PVDINTERFACEERROR   pInterfaceErrorCallbacks;
    /** Flags the image was opened with. */
    uint32_t            uOpenFlags;
    /** Image flags. */
    unsigned            uImageFlags;
    /** The filename.
     * Kept around for logging and delete-on-close purposes. */
    const char         *pszFilename;
    /** The resources.
     * A lazy bird ASSUME there are only two arrays in the resource-fork section in
     * the XML, namely 'blkx' and 'plst'. These have been assigned fixed indexes. */
    VBOXUDIFRSRCARRAY   aRsrcs[2];
    /** The UDIF footer. */
    VBOXUDIF            Ftr;

    /** Total size of the virtual image. */
    uint64_t            cbSize;
    /** Physical geometry of this image. */
    PDMMEDIAGEOMETRY    PCHSGeometry;
    /** Logical geometry of this image. */
    PDMMEDIAGEOMETRY    LCHSGeometry;
    /** First extent in the list. */
    PDMGEXTENT          pExtentFirst;
    /** Last extent in the list. */
    PDMGEXTENT          pExtentLast;
} DMGIMAGE;
/** Pointer to an instance of the DMG Image Interpreter. */
typedef DMGIMAGE *PDMGIMAGE;

/** @name Resources indexes (into VBOXDMG::aRsrcs).
 * @{ */
#define VBOXDMG_RSRC_IDX_BLKX   0
#define VBOXDMG_RSRC_IDX_PLST   1
/** @} */


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def VBOXDMG_PRINTF
 * Wrapper for RTPrintf/LogRel.
 */
#ifdef VBOXDMG_TESTING
# define VBOXDMG_PRINTF(a)  RTPrintf a
#else
# define VBOXDMG_PRINTF(a)  LogRel(a)
#endif

/** @def VBOXDMG_VALIDATE
 * For validating a struct thing and log/print what's wrong.
 */
#ifdef VBOXDMG_TESTING
# define VBOXDMG_VALIDATE(expr, logstuff) \
    do { \
        if (!(expr)) \
        { \
            RTPrintf("tstVBoxDMG: validation failed: %s\ntstVBoxDMG: ", #expr); \
            RTPrintf logstuff; \
            fRc = false; \
        } \
    } while (0)
#else
# define VBOXDMG_VALIDATE(expr, logstuff) \
    do { \
        if (!(expr)) \
        { \
            LogRel(("vboxdmg: validation failed: %s\nvboxdmg: ", #expr)); \
            LogRel(logstuff); \
            fRc = false; \
        } \
    } while (0)
#endif


/** VBoxDMG: Unable to parse the XML. */
#define VERR_VD_DMG_XML_PARSE_ERROR         (-3280)


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void vboxdmgUdifFtrHost2FileEndian(PVBOXUDIF pUdif);
static void vboxdmgUdifFtrFile2HostEndian(PVBOXUDIF pUdif);

static void vboxdmgUdifIdHost2FileEndian(PVBOXUDIFID pId);
static void vboxdmgUdifIdFile2HostEndian(PVBOXUDIFID pId);

static void vboxdmgUdifCkSumHost2FileEndian(PVBOXUDIFCKSUM pCkSum);
static void vboxdmgUdifCkSumFile2HostEndian(PVBOXUDIFCKSUM pCkSum);
static bool vboxdmgUdifCkSumIsValid(PCVBOXUDIFCKSUM pCkSum, const char *pszPrefix);

/**
 * Internal: signal an error to the frontend.
 */
DECLINLINE(int) dmgError(PDMGIMAGE pImage, int rc, RT_SRC_POS_DECL,
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

static int dmgFileOpen(PDMGIMAGE pImage, bool fReadonly, bool fCreate)
{
    AssertMsg(!(fReadonly && fCreate), ("Image can't be opened readonly while being created\n"));

    unsigned uOpenFlags = fReadonly ? VD_INTERFACEASYNCIO_OPEN_FLAGS_READONLY : 0;

    if (fCreate)
        uOpenFlags |= VD_INTERFACEASYNCIO_OPEN_FLAGS_CREATE;

    return pImage->pInterfaceIOCallbacks->pfnOpen(pImage->pInterfaceIO->pvUser,
                                                  pImage->pszFilename,
                                                  uOpenFlags,
                                                  &pImage->pStorage);
}

static int dmgFileClose(PDMGIMAGE pImage)
{
    int rc = VINF_SUCCESS;

    if (pImage->pStorage)
        rc = pImage->pInterfaceIOCallbacks->pfnClose(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage);

    pImage->pStorage = NULL;

    return rc;
}

static int dmgFileFlushSync(PDMGIMAGE pImage)
{
    return pImage->pInterfaceIOCallbacks->pfnFlushSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage);
}

static int dmgFileGetSize(PDMGIMAGE pImage, uint64_t *pcbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnGetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage,
                                                     pcbSize);
}

static int dmgFileSetSize(PDMGIMAGE pImage, uint64_t cbSize)
{
    return pImage->pInterfaceIOCallbacks->pfnSetSize(pImage->pInterfaceIO->pvUser,
                                                     pImage->pStorage,
                                                     cbSize);
}

static int dmgFileWriteSync(PDMGIMAGE pImage, uint64_t off, const void *pcvBuf, size_t cbWrite, size_t *pcbWritten)
{
    return pImage->pInterfaceIOCallbacks->pfnWriteSync(pImage->pInterfaceIO->pvUser,
                                                       pImage->pStorage,
                                                       off, cbWrite, pcvBuf,
                                                       pcbWritten);
}

static int dmgFileReadSync(PDMGIMAGE pImage, uint64_t off, void *pvBuf, size_t cbRead, size_t *pcbRead)
{
    return pImage->pInterfaceIOCallbacks->pfnReadSync(pImage->pInterfaceIO->pvUser,
                                                      pImage->pStorage,
                                                      off, cbRead, pvBuf,
                                                      pcbRead);
}

static bool dmgFileOpened(PDMGIMAGE pImage)
{
    return pImage->pStorage != NULL;
}

/**
 * Swaps endian.
 * @param   pUdif       The structure.
 */
static void vboxdmgSwapEndianUdif(PVBOXUDIF pUdif)
{
#ifndef RT_BIG_ENDIAN
    pUdif->u32Magic   = RT_BSWAP_U32(pUdif->u32Magic);
    pUdif->u32Version = RT_BSWAP_U32(pUdif->u32Version);
    pUdif->cbFooter   = RT_BSWAP_U32(pUdif->cbFooter);
    pUdif->fFlags     = RT_BSWAP_U32(pUdif->fFlags);
    pUdif->offRunData = RT_BSWAP_U64(pUdif->offRunData);
    pUdif->offData    = RT_BSWAP_U64(pUdif->offData);
    pUdif->cbData     = RT_BSWAP_U64(pUdif->cbData);
    pUdif->offRsrc    = RT_BSWAP_U64(pUdif->offRsrc);
    pUdif->cbRsrc     = RT_BSWAP_U64(pUdif->cbRsrc);
    pUdif->iSegment   = RT_BSWAP_U32(pUdif->iSegment);
    pUdif->cSegments  = RT_BSWAP_U32(pUdif->cSegments);
    pUdif->offXml     = RT_BSWAP_U64(pUdif->offXml);
    pUdif->cbXml      = RT_BSWAP_U64(pUdif->cbXml);
    pUdif->u32Type    = RT_BSWAP_U32(pUdif->u32Type);
    pUdif->cSectors   = RT_BSWAP_U64(pUdif->cSectors);
#endif
}


/**
 * Swaps endian from host cpu to file.
 * @param   pUdif       The structure.
 */
static void vboxdmgUdifFtrHost2FileEndian(PVBOXUDIF pUdif)
{
    vboxdmgSwapEndianUdif(pUdif);
    vboxdmgUdifIdHost2FileEndian(&pUdif->SegmentId);
    vboxdmgUdifCkSumHost2FileEndian(&pUdif->DataCkSum);
    vboxdmgUdifCkSumHost2FileEndian(&pUdif->MasterCkSum);
}


/**
 * Swaps endian from file to host cpu.
 * @param   pUdif       The structure.
 */
static void vboxdmgUdifFtrFile2HostEndian(PVBOXUDIF pUdif)
{
    vboxdmgSwapEndianUdif(pUdif);
    vboxdmgUdifIdFile2HostEndian(&pUdif->SegmentId);
    vboxdmgUdifCkSumFile2HostEndian(&pUdif->DataCkSum);
    vboxdmgUdifCkSumFile2HostEndian(&pUdif->MasterCkSum);
}

/**
 * Swaps endian from file to host cpu.
 * @param   pBlkx       The blkx structure.
 */
static void vboxdmgBlkxFile2HostEndian(PVBOXBLKX pBlkx)
{
    pBlkx->u32Magic            = RT_BE2H_U32(pBlkx->u32Magic);
    pBlkx->u32Version          = RT_BE2H_U32(pBlkx->u32Version);
    pBlkx->cSectornumberFirst  = RT_BE2H_U64(pBlkx->cSectornumberFirst);
    pBlkx->cSectors            = RT_BE2H_U64(pBlkx->cSectors);
    pBlkx->offDataStart        = RT_BE2H_U64(pBlkx->offDataStart);
    pBlkx->cSectorsDecompress  = RT_BE2H_U32(pBlkx->cSectorsDecompress);
    pBlkx->u32BlocksDescriptor = RT_BE2H_U32(pBlkx->u32BlocksDescriptor);
    pBlkx->cBlocksRunCount     = RT_BE2H_U32(pBlkx->cBlocksRunCount);
    vboxdmgUdifCkSumFile2HostEndian(&pBlkx->BlkxCkSum);
}

/**
 * Swaps endian from file to host cpu.
 * @param   pBlkxDesc   The blkx descriptor structure.
 */
static void vboxdmgBlkxDescFile2HostEndian(PVBOXBLKXDESC pBlkxDesc)
{
    pBlkxDesc->u32Type        = RT_BE2H_U32(pBlkxDesc->u32Type);
    pBlkxDesc->u32Reserved    = RT_BE2H_U32(pBlkxDesc->u32Reserved);
    pBlkxDesc->u64SectorStart = RT_BE2H_U64(pBlkxDesc->u64SectorStart);
    pBlkxDesc->u64SectorCount = RT_BE2H_U64(pBlkxDesc->u64SectorCount);
    pBlkxDesc->offData        = RT_BE2H_U64(pBlkxDesc->offData);
    pBlkxDesc->cbData         = RT_BE2H_U64(pBlkxDesc->cbData);
}

/**
 * Validates an UDIF footer structure.
 *
 * @returns true if valid, false and LogRel()s on failure.
 * @param   pFtr        The UDIF footer to validate.
 * @param   offFtr      The offset of the structure.
 */
static bool vboxdmgUdifFtrIsValid(PCVBOXUDIF pFtr, uint64_t offFtr)
{
    bool fRc = true;

    VBOXDMG_VALIDATE(!(pFtr->fFlags & ~VBOXUDIF_FLAGS_KNOWN_MASK), ("fFlags=%#RX32 fKnown=%RX32\n", pFtr->fFlags, VBOXUDIF_FLAGS_KNOWN_MASK));
    VBOXDMG_VALIDATE(pFtr->offRunData < offFtr, ("offRunData=%#RX64\n", pFtr->offRunData));
    VBOXDMG_VALIDATE(pFtr->cbData    <= offFtr && pFtr->offData + pFtr->cbData <= offFtr, ("cbData=%#RX64 offData=%#RX64 offFtr=%#RX64\n", pFtr->cbData, pFtr->offData, offFtr));
    VBOXDMG_VALIDATE(pFtr->offData    < offFtr, ("offData=%#RX64\n", pFtr->offData));
    VBOXDMG_VALIDATE(pFtr->cbRsrc    <= offFtr && pFtr->offRsrc + pFtr->cbRsrc <= offFtr, ("cbRsrc=%#RX64 offRsrc=%#RX64 offFtr=%#RX64\n", pFtr->cbRsrc, pFtr->offRsrc, offFtr));
    VBOXDMG_VALIDATE(pFtr->offRsrc    < offFtr, ("offRsrc=%#RX64\n", pFtr->offRsrc));
    VBOXDMG_VALIDATE(pFtr->cSegments <= 1,      ("cSegments=%RU32\n", pFtr->cSegments));
    VBOXDMG_VALIDATE(pFtr->iSegment  == 0 || pFtr->iSegment == 1, ("iSegment=%RU32 cSegments=%RU32\n", pFtr->iSegment, pFtr->cSegments));
    VBOXDMG_VALIDATE(pFtr->cbXml    <= offFtr && pFtr->offXml + pFtr->cbXml <= offFtr, ("cbXml=%#RX64 offXml=%#RX64 offFtr=%#RX64\n", pFtr->cbXml, pFtr->offXml, offFtr));
    VBOXDMG_VALIDATE(pFtr->offXml    < offFtr,  ("offXml=%#RX64\n", pFtr->offXml));
    VBOXDMG_VALIDATE(pFtr->cbXml     > 128,     ("cbXml=%#RX64\n", pFtr->cbXml));
    VBOXDMG_VALIDATE(pFtr->cbXml     < _1M,     ("cbXml=%#RX64\n", pFtr->cbXml));
    VBOXDMG_VALIDATE(pFtr->u32Type == VBOXUDIF_TYPE_DEVICE || pFtr->u32Type == VBOXUDIF_TYPE_PARTITION,  ("u32Type=%RU32\n", pFtr->u32Type));
    VBOXDMG_VALIDATE(pFtr->cSectors != 0,       ("cSectors=%#RX64\n", pFtr->cSectors));
    fRc &= vboxdmgUdifCkSumIsValid(&pFtr->DataCkSum, "DataCkSum");
    fRc &= vboxdmgUdifCkSumIsValid(&pFtr->MasterCkSum, "MasterCkSum");

    return fRc;
}


static bool vboxdmgBlkxIsValid(PCVBOXBLKX pBlkx)
{
    bool fRc = true;

    fRc &= vboxdmgUdifCkSumIsValid(&pBlkx->BlkxCkSum, "BlkxCkSum");
    VBOXDMG_VALIDATE(pBlkx->u32Magic == VBOXBLKX_MAGIC, ("u32Magic=%#RX32 u32MagicExpected=%#RX32\n", pBlkx->u32Magic, VBOXBLKX_MAGIC));
    VBOXDMG_VALIDATE(pBlkx->u32Version == VBOXBLKX_VERSION, ("u32Version=%#RX32 u32VersionExpected=%#RX32\n", pBlkx->u32Magic, VBOXBLKX_VERSION));

    return fRc;
}

/**
 * Swaps endian from host cpu to file.
 * @param   pId         The structure.
 */
static void vboxdmgUdifIdHost2FileEndian(PVBOXUDIFID pId)
{
    NOREF(pId);
}


/**
 * Swaps endian from file to host cpu.
 * @param   pId         The structure.
 */
static void vboxdmgUdifIdFile2HostEndian(PVBOXUDIFID pId)
{
    vboxdmgUdifIdHost2FileEndian(pId);
}


/**
 * Swaps endian.
 * @param   pCkSum      The structure.
 */
static void vboxdmgSwapEndianUdifCkSum(PVBOXUDIFCKSUM pCkSum, uint32_t u32Kind, uint32_t cBits)
{
#ifdef RT_BIG_ENDIAN
    NOREF(pCkSum);
    NOREF(u32Kind);
    NOREF(cBits);
#else
    switch (u32Kind)
    {
        case VBOXUDIFCKSUM_NONE:
            /* nothing to do here */
            break;

        case VBOXUDIFCKSUM_CRC32:
            Assert(cBits == 32);
            pCkSum->u32Kind      = RT_BSWAP_U32(pCkSum->u32Kind);
            pCkSum->cBits        = RT_BSWAP_U32(pCkSum->cBits);
            pCkSum->uSum.au32[0] = RT_BSWAP_U32(pCkSum->uSum.au32[0]);
            break;

        default:
            AssertMsgFailed(("%x\n", u32Kind));
            break;
    }
    NOREF(cBits);
#endif
}


/**
 * Swaps endian from host cpu to file.
 * @param   pCkSum      The structure.
 */
static void vboxdmgUdifCkSumHost2FileEndian(PVBOXUDIFCKSUM pCkSum)
{
    vboxdmgSwapEndianUdifCkSum(pCkSum, pCkSum->u32Kind, pCkSum->cBits);
}


/**
 * Swaps endian from file to host cpu.
 * @param   pCkSum      The structure.
 */
static void vboxdmgUdifCkSumFile2HostEndian(PVBOXUDIFCKSUM pCkSum)
{
    vboxdmgSwapEndianUdifCkSum(pCkSum, RT_BE2H_U32(pCkSum->u32Kind), RT_BE2H_U32(pCkSum->cBits));
}


/**
 * Validates an UDIF checksum structure.
 *
 * @returns true if valid, false and LogRel()s on failure.
 * @param   pCkSum      The checksum structure.
 * @param   pszPrefix   The message prefix.
 * @remarks This does not check the checksummed data.
 */
static bool vboxdmgUdifCkSumIsValid(PCVBOXUDIFCKSUM pCkSum, const char *pszPrefix)
{
    bool fRc = true;

    switch (pCkSum->u32Kind)
    {
        case VBOXUDIFCKSUM_NONE:
            VBOXDMG_VALIDATE(pCkSum->cBits == 0, ("%s/NONE: cBits=%d\n", pszPrefix, pCkSum->cBits));
            break;

        case VBOXUDIFCKSUM_CRC32:
            VBOXDMG_VALIDATE(pCkSum->cBits == 32, ("%s/NONE: cBits=%d\n", pszPrefix, pCkSum->cBits));
            break;

        default:
            VBOXDMG_VALIDATE(0, ("%s: u32Kind=%#RX32\n", pszPrefix, pCkSum->u32Kind));
            break;
    }
    return fRc;
}


/** @copydoc VBOXHDDBACKEND::pfnClose */
static DECLCALLBACK(int) vboxdmgClose(void *pvBackendData, bool fDelete)
{
    PDMGIMAGE pThis = (PDMGIMAGE)pvBackendData;

    if (dmgFileOpened(pThis))
    {
        /*
         * If writable, flush changes, fix checksums, ++.
         */
        /** @todo writable images. */

        /*
         * Close the file.
         */
        dmgFileClose(pThis);
    }

    /*
     * Delete the file if requested, then free the remaining resources.
     */
    int rc = VINF_SUCCESS;
    if (fDelete)
        rc = RTFileDelete(pThis->pszFilename);

    for (unsigned iRsrc = 0; iRsrc < RT_ELEMENTS(pThis->aRsrcs); iRsrc++)
        for (unsigned i = 0; i < pThis->aRsrcs[iRsrc].cEntries; i++)
        {
            if (pThis->aRsrcs[iRsrc].aEntries[i].pbData)
            {
                RTMemFree(pThis->aRsrcs[iRsrc].aEntries[i].pbData);
                pThis->aRsrcs[iRsrc].aEntries[i].pbData = NULL;
            }
            if (pThis->aRsrcs[iRsrc].aEntries[i].pszName)
            {
                RTMemFree(pThis->aRsrcs[iRsrc].aEntries[i].pszName);
                pThis->aRsrcs[iRsrc].aEntries[i].pszName = NULL;
            }
            if (pThis->aRsrcs[iRsrc].aEntries[i].pszCFName)
            {
                RTMemFree(pThis->aRsrcs[iRsrc].aEntries[i].pszCFName);
                pThis->aRsrcs[iRsrc].aEntries[i].pszCFName = NULL;
            }
        }
    RTMemFree(pThis);

    return rc;
}


#define STARTS_WITH(pszString, szStart) \
        ( strncmp(pszString, szStart, sizeof(szStart) - 1) == 0 )

#define STARTS_WITH_WORD(pszString, szWord) \
        (   STARTS_WITH(pszString, szWord) \
         && !RT_C_IS_ALNUM((pszString)[sizeof(szWord) - 1]) )

#define SKIP_AHEAD(psz, szWord) \
        do { \
            (psz) = RTStrStripL((psz) + sizeof(szWord) - 1); \
        } while (0)

#define REQUIRE_WORD(psz, szWord) \
        do { \
            if (!STARTS_WITH_WORD(psz, szWord)) \
                return psz; \
            (psz) = RTStrStripL((psz) + sizeof(szWord) - 1); \
        } while (0)

#define REQUIRE_TAG(psz, szTag) \
        do { \
            if (!STARTS_WITH(psz, "<" szTag ">")) \
                return psz; \
            (psz) = RTStrStripL((psz) + sizeof("<" szTag ">") - 1); \
        } while (0)

#define REQUIRE_TAG_NO_STRIP(psz, szTag) \
        do { \
            if (!STARTS_WITH(psz, "<" szTag ">")) \
                return psz; \
            (psz) += sizeof("<" szTag ">") - 1; \
        } while (0)

#define REQUIRE_END_TAG(psz, szTag) \
        do { \
            if (!STARTS_WITH(psz, "</" szTag ">")) \
                return psz; \
            (psz) = RTStrStripL((psz) + sizeof("</" szTag ">") - 1); \
        } while (0)


/**
 * Finds the next tag end.
 *
 * @returns Pointer to a '>' or '\0'.
 * @param   pszCur      The current position.
 */
static const char *vboxdmgXmlFindTagEnd(const char *pszCur)
{
    /* Might want to take quoted '>' into account? */
    char ch;
    while ((ch = *pszCur) != '\0' && ch != '>')
        pszCur++;
    return pszCur;
}


/**
 * Finds the end tag.
 *
 * Does not deal with '<tag attr="1"/>' style tags.
 *
 * @returns Pointer to the first char in the end tag. NULL if another tag
 *          was encountered first or if we hit the end of the file.
 * @param   ppszCur     The current position (IN/OUT).
 * @param   pszTag      The tag name.
 */
static const char *vboxdmgXmlFindEndTag(const char **ppszCur, const char *pszTag)
{
    const char         *psz = *ppszCur;
    char                ch;
    while ((ch = *psz))
    {
        if (ch == '<')
        {
            size_t const cchTag = strlen(pszTag);
            if (    psz[1] == '/'
                &&  !memcmp(&psz[2], pszTag, cchTag)
                &&  psz[2 + cchTag] == '>')
            {
                *ppszCur = psz + 2 + cchTag + 1;
                return psz;
            }
            break;
        }
        psz++;
    }
    return NULL;
}


/**
 * Reads a signed 32-bit value.
 *
 * @returns NULL on success, pointer to the offending text on failure.
 * @param   ppszCur     The text position (IN/OUT).
 * @param   pi32        Where to store the value.
 */
static const char *vboxdmgXmlParseS32(const char **ppszCur, int32_t *pi32)
{
    const char *psz = *ppszCur;

    /*
     * <string>-1</string>
     */
    REQUIRE_TAG_NO_STRIP(psz, "string");

    char *pszNext;
    int rc = RTStrToInt32Ex(psz, &pszNext, 0, pi32);
    if (rc != VWRN_TRAILING_CHARS)
        return *ppszCur;
    psz = pszNext;

    REQUIRE_END_TAG(psz, "string");
    *ppszCur = psz;
    return NULL;
}


/**
 * Reads an unsigned 32-bit value.
 *
 * @returns NULL on success, pointer to the offending text on failure.
 * @param   ppszCur     The text position (IN/OUT).
 * @param   pu32        Where to store the value.
 */
static const char *vboxdmgXmlParseU32(const char **ppszCur, uint32_t *pu32)
{
    const char *psz = *ppszCur;

    /*
     * <string>0x00ff</string>
     */
    REQUIRE_TAG_NO_STRIP(psz, "string");

    char *pszNext;
    int rc = RTStrToUInt32Ex(psz, &pszNext, 0, pu32);
    if (rc != VWRN_TRAILING_CHARS)
        return *ppszCur;
    psz = pszNext;

    REQUIRE_END_TAG(psz, "string");
    *ppszCur = psz;
    return NULL;
}


/**
 * Reads a string value.
 *
 * @returns NULL on success, pointer to the offending text on failure.
 * @param   ppszCur     The text position (IN/OUT).
 * @param   ppszString  Where to store the pointer to the string. The caller
 *                      must free this using RTMemFree.
 */
static const char *vboxdmgXmlParseString(const char **ppszCur, char **ppszString)
{
    const char *psz = *ppszCur;

    /*
     * <string>Driver Descriptor Map (DDM : 0)</string>
     */
    REQUIRE_TAG_NO_STRIP(psz, "string");

    const char *pszStart = psz;
    const char *pszEnd = vboxdmgXmlFindEndTag(&psz, "string");
    if (!pszEnd)
        return *ppszCur;
    psz = RTStrStripL(psz);

    *ppszString = (char *)RTMemDupEx(pszStart, pszEnd - pszStart, 1);
    if (!*ppszString)
        return *ppszCur;

    *ppszCur = psz;
    return NULL;
}


/**
 * Parses the BASE-64 coded data tags.
 *
 * @returns NULL on success, pointer to the offending text on failure.
 * @param   ppszCur     The text position (IN/OUT).
 * @param   ppbData     Where to store the pointer to the data we've read. The
 *                      caller must free this using RTMemFree.
 * @param   pcbData     The number of bytes we're returning.
 */
static const char *vboxdmgXmlParseData(const char **ppszCur, uint8_t **ppbData, size_t *pcbData)
{
    const char *psz = *ppszCur;

    /*
     * <data>   AAAAA...    </data>
     */
    REQUIRE_TAG(psz, "data");

    const char *pszStart = psz;
    ssize_t cbData = RTBase64DecodedSize(pszStart, (char **)&psz);
    if (cbData == -1)
        return *ppszCur;
    const char *pszEnd = psz;

    REQUIRE_END_TAG(psz, "data");

    *ppbData = (uint8_t *)RTMemAlloc(cbData);
    if (!*ppbData)
        return *ppszCur;
    char *pszIgnored;
    int rc = RTBase64Decode(pszStart, *ppbData, cbData, pcbData, &pszIgnored);
    if (RT_FAILURE(rc))
    {
        RTMemFree(*ppbData);
        *ppbData = NULL;
        return *ppszCur;
    }

    *ppszCur = psz;
    return NULL;
}


/**
 * Parses the XML resource-fork in a rather presumptive manner.
 *
 * This function is supposed to construct the VBOXDMG::aRsrcs instance data
 * parts.
 *
 * @returns NULL on success, pointer to the problematic text on failure.
 * @param   pThis       The DMG instance data.
 * @param   pszXml      The XML text to parse, UTF-8.
 * @param   cch         The size of the the XML text.
 */
static const char *vboxdmgOpenXmlToRsrc(PDMGIMAGE pThis, char const *pszXml)
{
    const char *psz = pszXml;

    /*
     * Verify the ?xml, !DOCTYPE and plist tags.
     */
    SKIP_AHEAD(psz, "");

    /* <?xml version="1.0" encoding="UTF-8"?> */
    REQUIRE_WORD(psz, "<?xml");
    while (*psz != '?')
    {
        if (!*psz)
            return psz;
        if (STARTS_WITH_WORD(psz, "version="))
        {
            SKIP_AHEAD(psz, "version=");
            REQUIRE_WORD(psz, "\"1.0\"");
        }
        else if (STARTS_WITH_WORD(psz, "encoding="))
        {
            SKIP_AHEAD(psz, "encoding=");
            REQUIRE_WORD(psz, "\"UTF-8\"");
        }
        else
            return psz;
    }
    SKIP_AHEAD(psz, "?>");

    /* <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd"> */
    REQUIRE_WORD(psz, "<!DOCTYPE");
    REQUIRE_WORD(psz, "plist");
    REQUIRE_WORD(psz, "PUBLIC");
    psz = vboxdmgXmlFindTagEnd(psz);
    REQUIRE_WORD(psz, ">");

    /* <plist version="1.0"> */
    REQUIRE_WORD(psz, "<plist");
    REQUIRE_WORD(psz, "version=");
    REQUIRE_WORD(psz, "\"1.0\"");
    REQUIRE_WORD(psz, ">");

    /*
     * Decend down to the 'resource-fork' dictionary.
     * ASSUME it's the only top level dictionary.
     */
    /* <dict> <key>resource-fork</key> */
    REQUIRE_TAG(psz, "dict");
    REQUIRE_WORD(psz, "<key>resource-fork</key>");

    /*
     * Parse the keys in the resource-fork dictionary.
     * ASSUME that there are just two, 'blkx' and 'plst'.
     */
    REQUIRE_TAG(psz, "dict");
    while (!STARTS_WITH_WORD(psz, "</dict>"))
    {
        /*
         * Parse the key and Create the resource-fork entry.
         */
        unsigned iRsrc;
        if (STARTS_WITH_WORD(psz, "<key>blkx</key>"))
        {
            REQUIRE_WORD(psz, "<key>blkx</key>");
            iRsrc = VBOXDMG_RSRC_IDX_BLKX;
            strcpy(&pThis->aRsrcs[iRsrc].szName[0], "blkx");
        }
        else if (STARTS_WITH_WORD(psz, "<key>plst</key>"))
        {
            REQUIRE_WORD(psz, "<key>plst</key>");
            iRsrc = VBOXDMG_RSRC_IDX_PLST;
            strcpy(&pThis->aRsrcs[iRsrc].szName[0], "plst");
        }
        else
            return psz;

        /*
         * Decend into the array and add the elements to the resource entry.
         */
        /* <array> */
        REQUIRE_TAG(psz, "array");
        while (!STARTS_WITH_WORD(psz, "</array>"))
        {
            REQUIRE_TAG(psz, "dict");
            uint32_t i = pThis->aRsrcs[iRsrc].cEntries;
            if (i == RT_ELEMENTS(pThis->aRsrcs[iRsrc].aEntries))
                return psz;

            while (!STARTS_WITH_WORD(psz, "</dict>"))
            {

                /* switch on the key. */
                const char *pszErr;
                if (STARTS_WITH_WORD(psz, "<key>Attributes</key>"))
                {
                    REQUIRE_WORD(psz, "<key>Attributes</key>");
                    pszErr = vboxdmgXmlParseU32(&psz, &pThis->aRsrcs[iRsrc].aEntries[i].fAttributes);
                }
                else if (STARTS_WITH_WORD(psz, "<key>ID</key>"))
                {
                    REQUIRE_WORD(psz, "<key>ID</key>");
                    pszErr = vboxdmgXmlParseS32(&psz, &pThis->aRsrcs[iRsrc].aEntries[i].iId);
                }
                else if (STARTS_WITH_WORD(psz, "<key>Name</key>"))
                {
                    REQUIRE_WORD(psz, "<key>Name</key>");
                    pszErr = vboxdmgXmlParseString(&psz, &pThis->aRsrcs[iRsrc].aEntries[i].pszName);
                }
                else if (STARTS_WITH_WORD(psz, "<key>CFName</key>"))
                {
                    REQUIRE_WORD(psz, "<key>CFName</key>");
                    pszErr = vboxdmgXmlParseString(&psz, &pThis->aRsrcs[iRsrc].aEntries[i].pszCFName);
                }
                else if (STARTS_WITH_WORD(psz, "<key>Data</key>"))
                {
                    REQUIRE_WORD(psz, "<key>Data</key>");
                    pszErr = vboxdmgXmlParseData(&psz, &pThis->aRsrcs[iRsrc].aEntries[i].pbData, &pThis->aRsrcs[iRsrc].aEntries[i].cbData);
                }
                else
                    pszErr = psz;
                if (pszErr)
                    return pszErr;
            } /* while not </dict> */
            REQUIRE_END_TAG(psz, "dict");

            pThis->aRsrcs[iRsrc].cEntries++;
        } /* while not </array> */
        REQUIRE_END_TAG(psz, "array");

    } /* while not </dict> */
    REQUIRE_END_TAG(psz, "dict");

    /*
     * ASSUMING there is only the 'resource-fork', we'll now see the end of
     * the outer dict, plist and text.
     */
    /* </dict> </plist> */
    REQUIRE_END_TAG(psz, "dict");
    REQUIRE_END_TAG(psz, "plist");

    /* the end */
    if (*psz)
        return psz;

    return NULL;
}

#undef REQUIRE_END_TAG
#undef REQUIRE_TAG_NO_STRIP
#undef REQUIRE_TAG
#undef REQUIRE_WORD
#undef SKIP_AHEAD
#undef STARTS_WITH_WORD
#undef STARTS_WITH

/**
 * Returns the data attached to a resource.
 *
 * @returns VBox status code.
 * @param   pThis        The DMG instance data.
 * @param   pcszRsrcName Name of the resource to get.
 */
static int dmgGetRsrcData(PDMGIMAGE pThis, const char *pcszRsrcName,
                          PCVBOXUDIFRSRCARRAY *ppcRsrc)
{
    int rc = VERR_NOT_FOUND;

    for (unsigned i = 0; i < RT_ELEMENTS(pThis->aRsrcs); i++)
    {
        if (!strcmp(pThis->aRsrcs[i].szName, pcszRsrcName))
        {
            *ppcRsrc = &pThis->aRsrcs[i];
            rc = VINF_SUCCESS;
            break;
        }
    }

    return rc;
}

/**
 * Creates a new extent from the given blkx descriptor.
 *
 * @returns VBox status code.
 * @param   pThis     DMG instance data.
 * @param   pBlkxDesc The blkx descriptor.
 */
static int vboxdmgExtentCreateFromBlkxDesc(PDMGIMAGE pThis, uint64_t offDevice, PVBOXBLKXDESC pBlkxDesc)
{
    int rc = VINF_SUCCESS;
    DMGEXTENTTYPE enmExtentTypeNew;
    PDMGEXTENT pExtentNew = NULL;

    if (pBlkxDesc->u32Type == VBOXBLKXDESC_TYPE_RAW)
        enmExtentTypeNew = DMGEXTENTTYPE_RAW;
    else if (pBlkxDesc->u32Type == VBOXBLKXDESC_TYPE_IGNORE)
        enmExtentTypeNew = DMGEXTENTTYPE_ZERO;
    else
        AssertMsgFailed(("This method supports only raw or zero extents!\n"));

#if 0
    pExtentNew = pThis->pExtentLast;
    if (   pExtentNew
        && pExtentNew->enmType == enmExtentTypeNew
        && pExtentNew->offExtent + pExtentNew->cbExtent == offDevice + pBlkxDesc->u64SectorStart * VBOXDMG_SECTOR_SIZE;
        && pExtentNew->offFileStart + pExtentNew->cbExtent == pBlkxDesc->offData)
    {
        /* Increase the last extent. */
        pExtentNew->cbExtent += pBlkxDesc->cbData;
    }
    else
#endif
    {
        /* Create a new extent. */
        pExtentNew = (PDMGEXTENT)RTMemAllocZ(sizeof(DMGEXTENT));
        if (pExtentNew)
        {
            pExtentNew->pNext        = NULL;
            pExtentNew->enmType      = enmExtentTypeNew;
            pExtentNew->offExtent    = offDevice + pBlkxDesc->u64SectorStart * VBOXDMG_SECTOR_SIZE;
            pExtentNew->offFileStart = pBlkxDesc->offData;
            pExtentNew->cbExtent     = pBlkxDesc->u64SectorCount * VBOXDMG_SECTOR_SIZE;
            Assert(   pBlkxDesc->cbData == pBlkxDesc->u64SectorCount * VBOXDMG_SECTOR_SIZE
                   || enmExtentTypeNew == DMGEXTENTTYPE_ZERO);

            if (!pThis->pExtentLast)
            {
                pThis->pExtentFirst = pExtentNew;
                pThis->pExtentLast  = pExtentNew;
            }
            else
            {
                pThis->pExtentLast->pNext = pExtentNew;
                pThis->pExtentLast        = pExtentNew;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    return rc;
}

/**
 * Find the extent for the given offset.
 */
static PDMGEXTENT dmgExtentGetFromOffset(PDMGIMAGE pThis, uint64_t uOffset)
{
    PDMGEXTENT pExtent = pThis->pExtentFirst;

    while (   pExtent
           && (   uOffset < pExtent->offExtent
               || uOffset - pExtent->offExtent >= pExtent->cbExtent))
        pExtent = pExtent->pNext;

    return pExtent;
}

/**
 * Goes through the BLKX structure and creates the necessary extents.
 */
static int vboxdmgBlkxParse(PDMGIMAGE pThis, PVBOXBLKX pBlkx)
{
    int rc = VINF_SUCCESS;
    PVBOXBLKXDESC pBlkxDesc = (PVBOXBLKXDESC)(pBlkx + 1);

    for (unsigned i = 0; i < pBlkx->cBlocksRunCount; i++)
    {
        vboxdmgBlkxDescFile2HostEndian(pBlkxDesc);

        switch (pBlkxDesc->u32Type)
        {
            case VBOXBLKXDESC_TYPE_RAW:
            case VBOXBLKXDESC_TYPE_IGNORE:
            {
                rc = vboxdmgExtentCreateFromBlkxDesc(pThis, pBlkx->cSectornumberFirst * VBOXDMG_SECTOR_SIZE, pBlkxDesc);
                break;
            }
            case VBOXBLKXDESC_TYPE_COMMENT:
            case VBOXBLKXDESC_TYPE_TERMINATOR:
                break;
            default:
                rc = VERR_VD_GEN_INVALID_HEADER;
                break;
        }

        if (   pBlkxDesc->u32Type == VBOXBLKXDESC_TYPE_TERMINATOR
            || RT_FAILURE(rc))
                break;

        pBlkxDesc++;
    }

    return rc;
}

/**
 * Worker for vboxdmgOpen that reads in and validates all the necessary
 * structures from the image.
 *
 * This worker is really just a construct to avoid gotos and do-break-while-zero
 * uglyness.
 *
 * @returns VBox status code.
 * @param   pThis       The DMG instance data.
 */
static int vboxdmgOpenWorker(PDMGIMAGE pThis)
{
    /*
     * Read the footer.
     */
    int rc = dmgFileGetSize(pThis, &pThis->cbFile);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->cbFile < 1024)
        return VERR_VD_GEN_INVALID_HEADER;
    rc = dmgFileReadSync(pThis, pThis->cbFile - sizeof(pThis->Ftr), &pThis->Ftr, sizeof(pThis->Ftr), NULL);
    if (RT_FAILURE(rc))
        return rc;
    vboxdmgUdifFtrFile2HostEndian(&pThis->Ftr);

    /*
     * Do we recognize the footer structure? If so, is it valid?
     */
    if (pThis->Ftr.u32Magic != VBOXUDIF_MAGIC)
        return VERR_VD_GEN_INVALID_HEADER;
    if (pThis->Ftr.u32Version != VBOXUDIF_VER_CURRENT)
        return VERR_VD_GEN_INVALID_HEADER;
    if (pThis->Ftr.cbFooter != sizeof(pThis->Ftr))
        return VERR_VD_GEN_INVALID_HEADER;

    if (!vboxdmgUdifFtrIsValid(&pThis->Ftr, pThis->cbFile - sizeof(pThis->Ftr)))
    {
        VBOXDMG_PRINTF(("Bad DMG: '%s' cbFile=%RTfoff\n", pThis->pszFilename, pThis->cbFile));
        return VERR_VD_GEN_INVALID_HEADER;
    }

    pThis->cbSize = pThis->Ftr.cSectors * VBOXDMG_SECTOR_SIZE;

    /*
     * Read and parse the XML portion.
     */
    size_t cchXml = (size_t)pThis->Ftr.cbXml;
    char *pszXml = (char *)RTMemAlloc(cchXml + 1);
    if (!pszXml)
        return VERR_NO_MEMORY;
    rc = dmgFileReadSync(pThis, pThis->Ftr.offXml, pszXml, cchXml, NULL);
    if (RT_SUCCESS(rc))
    {
        pszXml[cchXml] = '\0';
        const char *pszError = vboxdmgOpenXmlToRsrc(pThis, pszXml);
        if (!pszError)
        {
            PCVBOXUDIFRSRCARRAY pRsrcBlkx = NULL;

            rc = dmgGetRsrcData(pThis, "blkx", &pRsrcBlkx);
            if (RT_SUCCESS(rc))
            {
                for (unsigned idxBlkx = 0; idxBlkx < pRsrcBlkx->cEntries; idxBlkx++)
                {
                    PVBOXBLKX pBlkx = NULL;

                    if (pRsrcBlkx->aEntries[idxBlkx].cbData < sizeof(VBOXBLKX))
                    {
                        rc = VERR_VD_GEN_INVALID_HEADER;
                        break;
                    }

                    pBlkx = (PVBOXBLKX)RTMemAllocZ(pRsrcBlkx->aEntries[idxBlkx].cbData);
                    if (!pBlkx)
                    {
                        rc = VERR_NO_MEMORY;
                        break;
                    }

                    memcpy(pBlkx, pRsrcBlkx->aEntries[idxBlkx].pbData, pRsrcBlkx->aEntries[idxBlkx].cbData);

                    vboxdmgBlkxFile2HostEndian(pBlkx);

                    if (   vboxdmgBlkxIsValid(pBlkx)
                        && pRsrcBlkx->aEntries[idxBlkx].cbData == pBlkx->cBlocksRunCount * sizeof(VBOXBLKXDESC) + sizeof(VBOXBLKX))
                        rc = vboxdmgBlkxParse(pThis, pBlkx);
                    else
                        rc = VERR_VD_GEN_INVALID_HEADER;

                    if (RT_FAILURE(rc))
                    {
                        RTMemFree(pBlkx);
                        break;
                    }
                }
            }
            else
                rc = VERR_VD_GEN_INVALID_HEADER;
        }
        else
        {
            VBOXDMG_PRINTF(("**** XML DUMP BEGIN ***\n%s\n**** XML DUMP END ****\n", pszXml));
            VBOXDMG_PRINTF(("**** Bad XML at %#lx (%lu) ***\n%.256s\n**** Bad XML END ****\n",
                            (unsigned long)(pszError - pszXml), (unsigned long)(pszError - pszXml), pszError));
            rc = VERR_VD_DMG_XML_PARSE_ERROR;
        }
    }
    RTMemFree(pszXml);
    if (RT_FAILURE(rc))
        return rc;


    return VINF_SUCCESS;
}


/** @copydoc VBOXHDDBACKEND::pfnOpen */
static DECLCALLBACK(int) dmgOpen(const char *pszFilename, unsigned fOpenFlags,
                                 PVDINTERFACE pVDIfsDisk, PVDINTERFACE pVDIfsImage,
                                 void **ppvBackendData)
{
    /*
     * Reject combinations we don't currently support.
     *
     * There is no point in being paranoid about the input here as we're just a
     * simple backend and can expect the caller to be the only user and already
     * have validate what it passes thru to us.
     */
    if (!(fOpenFlags & VD_OPEN_FLAGS_READONLY))
        return VERR_NOT_SUPPORTED;

    /*
     * Create the basic instance data structure and open the file,
     * then hand it over to a worker function that does all the rest.
     */
    PDMGIMAGE pThis = (PDMGIMAGE)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->uOpenFlags  = fOpenFlags;
    pThis->pszFilename = pszFilename;
    pThis->pStorage    = NULL;
    pThis->pVDIfsDisk  = pVDIfsDisk;
    pThis->pVDIfsImage = pVDIfsImage;

    pThis->pInterfaceError = VDInterfaceGet(pThis->pVDIfsDisk, VDINTERFACETYPE_ERROR);
    if (pThis->pInterfaceError)
        pThis->pInterfaceErrorCallbacks = VDGetInterfaceError(pThis->pInterfaceError);

    /* Try to get I/O interface. */
    pThis->pInterfaceIO = VDInterfaceGet(pThis->pVDIfsImage, VDINTERFACETYPE_IO);
    AssertPtr(pThis->pInterfaceIO);
    pThis->pInterfaceIOCallbacks = VDGetInterfaceIO(pThis->pInterfaceIO);
    AssertPtr(pThis->pInterfaceIOCallbacks);

    int rc = dmgFileOpen(pThis, (fOpenFlags & VD_OPEN_FLAGS_READONLY ? true : false),
                         false);
    if (RT_SUCCESS(rc))
    {
        rc = vboxdmgOpenWorker(pThis);
        if (RT_SUCCESS(rc))
        {
            *ppvBackendData = pThis;
            return rc;
        }
    }

    /* We failed, let the close method clean up. */
    vboxdmgClose(pThis, false /* fDelete */);
    return rc;
}


/** @copydoc VBOXHDDBACKEND::pfnCheckIfValid */
static DECLCALLBACK(int) dmgCheckIfValid(const char *pszFilename, PVDINTERFACE pVDIfsDisk)
{
    /*
     * Open the file and read the footer.
     */
    RTFILE hFile;
    int rc = RTFileOpen(&hFile, pszFilename, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    VBOXUDIF Ftr;
    uint64_t offFtr;
    rc = RTFileSeek(hFile, -(RTFOFF)sizeof(Ftr), RTFILE_SEEK_END, &offFtr);
    if (RT_SUCCESS(rc))
    {
        rc = RTFileRead(hFile, &Ftr, sizeof(Ftr), NULL);
        if (RT_SUCCESS(rc))
        {
            vboxdmgUdifFtrFile2HostEndian(&Ftr);

            /*
             * Do we recognize this stuff? Does it look valid?
             */
            if (    Ftr.u32Magic    == VBOXUDIF_MAGIC
                &&  Ftr.u32Version  == VBOXUDIF_VER_CURRENT
                &&  Ftr.cbFooter    == sizeof(Ftr))
            {
                if (vboxdmgUdifFtrIsValid(&Ftr, offFtr))
                    rc = VINF_SUCCESS;
                else
                {
                    VBOXDMG_PRINTF(("Bad DMG: '%s' offFtr=%RTfoff\n", pszFilename, offFtr));
                    rc = VERR_VD_GEN_INVALID_HEADER;
                }

            }
            else
                rc = VERR_VD_GEN_INVALID_HEADER;
        }
    }

    RTFileClose(hFile);
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnCreate */
static int dmgCreate(const char *pszFilename, uint64_t cbSize,
                     unsigned uImageFlags, const char *pszComment,
                     PCPDMMEDIAGEOMETRY pPCHSGeometry,
                     PCPDMMEDIAGEOMETRY pLCHSGeometry, PCRTUUID pUuid,
                     unsigned uOpenFlags, unsigned uPercentStart,
                     unsigned uPercentSpan, PVDINTERFACE pVDIfsDisk,
                     PVDINTERFACE pVDIfsImage, PVDINTERFACE pVDIfsOperation,
                     void **ppBackendData)
{
    LogFlowFunc(("pszFilename=\"%s\" cbSize=%llu uImageFlags=%#x pszComment=\"%s\" pPCHSGeometry=%#p pLCHSGeometry=%#p Uuid=%RTuuid uOpenFlags=%#x uPercentStart=%u uPercentSpan=%u pVDIfsDisk=%#p pVDIfsImage=%#p pVDIfsOperation=%#p ppBackendData=%#p",
                 pszFilename, cbSize, uImageFlags, pszComment, pPCHSGeometry, pLCHSGeometry, pUuid, uOpenFlags, uPercentStart, uPercentSpan, pVDIfsDisk, pVDIfsImage, pVDIfsOperation, ppBackendData));
    return VERR_NOT_SUPPORTED;
}

/** @copydoc VBOXHDDBACKEND::pfnRename */
static int dmgRename(void *pBackendData, const char *pszFilename)
{
    LogFlowFunc(("pBackendData=%#p pszFilename=%#p\n", pBackendData, pszFilename));
    int rc = VERR_NOT_SUPPORTED;

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnClose */
static int dmgClose(void *pBackendData, bool fDelete)
{
    LogFlowFunc(("pBackendData=%#p fDelete=%d\n", pBackendData, fDelete));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    /* Freeing a never allocated image (e.g. because the open failed) is
     * not signalled as an error. After all nothing bad happens. */
    if (pImage)
        vboxdmgClose(pImage, fDelete);

    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnRead */
static int dmgRead(void *pBackendData, uint64_t uOffset, void *pvBuf,
                   size_t cbToRead, size_t *pcbActuallyRead)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToRead=%zu pcbActuallyRead=%#p\n", pBackendData, uOffset, pvBuf, cbToRead, pcbActuallyRead));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    PDMGEXTENT pExtent = NULL;
    int rc = VINF_SUCCESS;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToRead % 512 == 0);

    if (   uOffset + cbToRead > pImage->cbSize
        || cbToRead == 0)
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

    pExtent = dmgExtentGetFromOffset(pImage, uOffset);

    if (pExtent)
    {
        uint64_t offExtentRel = uOffset - pExtent->offExtent;

        /* Remain in this extent. */
        cbToRead = RT_MIN(cbToRead, pExtent->cbExtent - offExtentRel);

        switch (pExtent->enmType)
        {
            case DMGEXTENTTYPE_RAW:
            {
                rc = dmgFileReadSync(pImage, pExtent->offFileStart + offExtentRel, pvBuf, cbToRead, NULL);
                break;
            }
            case DMGEXTENTTYPE_ZERO:
            {
                memset(pvBuf, 0, cbToRead);
                break;
            }
            default:
                AssertMsgFailed(("Invalid extent type\n"));
        }

        if (RT_SUCCESS(rc))
            *pcbActuallyRead = cbToRead;
    }
    else
        rc = VERR_INVALID_PARAMETER;

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnWrite */
static int dmgWrite(void *pBackendData, uint64_t uOffset, const void *pvBuf,
                    size_t cbToWrite, size_t *pcbWriteProcess,
                    size_t *pcbPreRead, size_t *pcbPostRead, unsigned fWrite)
{
    LogFlowFunc(("pBackendData=%#p uOffset=%llu pvBuf=%#p cbToWrite=%zu pcbWriteProcess=%#p pcbPreRead=%#p pcbPostRead=%#p\n",
                 pBackendData, uOffset, pvBuf, cbToWrite, pcbWriteProcess, pcbPreRead, pcbPostRead));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc = VERR_NOT_IMPLEMENTED;

    Assert(pImage);
    Assert(uOffset % 512 == 0);
    Assert(cbToWrite % 512 == 0);

    if (pImage->uOpenFlags & VD_OPEN_FLAGS_READONLY)
    {
        rc = VERR_VD_IMAGE_READ_ONLY;
        goto out;
    }

    AssertMsgFailed(("Not implemented\n"));

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnFlush */
static int dmgFlush(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc = VINF_SUCCESS;

    AssertMsgFailed(("Not implemented\n"));
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetVersion */
static unsigned dmgGetVersion(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return 1;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetSize */
static uint64_t dmgGetSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;

    Assert(pImage);

    if (pImage)
        return pImage->cbSize;
    else
        return 0;
}

/** @copydoc VBOXHDDBACKEND::pfnGetFileSize */
static uint64_t dmgGetFileSize(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    uint64_t cb = 0;

    Assert(pImage);

    if (pImage)
    {
        uint64_t cbFile;
        if (dmgFileOpened(pImage))
        {
            int rc = dmgFileGetSize(pImage, &cbFile);
            if (RT_SUCCESS(rc))
                cb += cbFile;
        }
    }

    LogFlowFunc(("returns %lld\n", cb));
    return cb;
}

/** @copydoc VBOXHDDBACKEND::pfnGetPCHSGeometry */
static int dmgGetPCHSGeometry(void *pBackendData,
                              PPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p\n", pBackendData, pPCHSGeometry));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static int dmgSetPCHSGeometry(void *pBackendData,
                              PCPDMMEDIAGEOMETRY pPCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pPCHSGeometry=%#p PCHS=%u/%u/%u\n",
                 pBackendData, pPCHSGeometry, pPCHSGeometry->cCylinders, pPCHSGeometry->cHeads, pPCHSGeometry->cSectors));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static int dmgGetLCHSGeometry(void *pBackendData,
                              PPDMMEDIAGEOMETRY pLCHSGeometry)
{
     LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p\n", pBackendData, pLCHSGeometry));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static int dmgSetLCHSGeometry(void *pBackendData,
                               PCPDMMEDIAGEOMETRY pLCHSGeometry)
{
    LogFlowFunc(("pBackendData=%#p pLCHSGeometry=%#p LCHS=%u/%u/%u\n",
                 pBackendData, pLCHSGeometry, pLCHSGeometry->cCylinders, pLCHSGeometry->cHeads, pLCHSGeometry->cSectors));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static unsigned dmgGetImageFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    unsigned uImageFlags;

    Assert(pImage);

    if (pImage)
        uImageFlags = pImage->uImageFlags;
    else
        uImageFlags = 0;

    LogFlowFunc(("returns %#x\n", uImageFlags));
    return uImageFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnGetOpenFlags */
static unsigned dmgGetOpenFlags(void *pBackendData)
{
    LogFlowFunc(("pBackendData=%#p\n", pBackendData));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    unsigned uOpenFlags;

    Assert(pImage);

    if (pImage)
        uOpenFlags = pImage->uOpenFlags;
    else
        uOpenFlags = 0;

    LogFlowFunc(("returns %#x\n", uOpenFlags));
    return uOpenFlags;
}

/** @copydoc VBOXHDDBACKEND::pfnSetOpenFlags */
static int dmgSetOpenFlags(void *pBackendData, unsigned uOpenFlags)
{
    LogFlowFunc(("pBackendData=%#p\n uOpenFlags=%#x", pBackendData, uOpenFlags));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    /* Image must be opened and the new flags must be valid. Just readonly and
     * info flags are supported. */
    if (!pImage || (uOpenFlags & ~(VD_OPEN_FLAGS_READONLY | VD_OPEN_FLAGS_INFO)))
    {
        rc = VERR_INVALID_PARAMETER;
        goto out;
    }

#if 0
    /* Implement this operation via reopening the image. */
    rawFreeImage(pImage, false);
    rc = rawOpenImage(pImage, uOpenFlags);
#endif

out:
    LogFlowFunc(("returns %Rrc\n", rc));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnGetComment */
static int dmgGetComment(void *pBackendData, char *pszComment,
                         size_t cbComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=%#p cbComment=%zu\n", pBackendData, pszComment, cbComment));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
    {
        if (pszComment)
            *pszComment = '\0';
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc comment='%s'\n", rc, pszComment));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetComment */
static int dmgSetComment(void *pBackendData, const char *pszComment)
{
    LogFlowFunc(("pBackendData=%#p pszComment=\"%s\"\n", pBackendData, pszComment));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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

/** @copydoc VBOXHDDBACKEND::pfnGetUuid */
static int dmgGetUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetUuid */
static int dmgSetUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    LogFlowFunc(("%RTuuid\n", pUuid));
    Assert(pImage);

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
static int dmgGetModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetModificationUuid */
static int dmgSetModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static int dmgGetParentUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentUuid */
static int dmgSetParentUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static int dmgGetParentModificationUuid(void *pBackendData, PRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p pUuid=%#p\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

    if (pImage)
        rc = VERR_NOT_SUPPORTED;
    else
        rc = VERR_VD_NOT_OPENED;

    LogFlowFunc(("returns %Rrc (%RTuuid)\n", rc, pUuid));
    return rc;
}

/** @copydoc VBOXHDDBACKEND::pfnSetParentModificationUuid */
static int dmgSetParentModificationUuid(void *pBackendData, PCRTUUID pUuid)
{
    LogFlowFunc(("pBackendData=%#p Uuid=%RTuuid\n", pBackendData, pUuid));
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;
    int rc;

    Assert(pImage);

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
static void dmgDump(void *pBackendData)
{
    PDMGIMAGE pImage = (PDMGIMAGE)pBackendData;

    Assert(pImage);
    if (pImage)
    {
        pImage->pInterfaceErrorCallbacks->pfnMessage(pImage->pInterfaceError->pvUser, "Header: Geometry PCHS=%u/%u/%u LCHS=%u/%u/%u cbSector=%llu\n",
                    pImage->PCHSGeometry.cCylinders, pImage->PCHSGeometry.cHeads, pImage->PCHSGeometry.cSectors,
                    pImage->LCHSGeometry.cCylinders, pImage->LCHSGeometry.cHeads, pImage->LCHSGeometry.cSectors,
                    pImage->cbSize / 512);
    }
}

static int dmgGetTimeStamp(void *pvBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int dmgGetParentTimeStamp(void *pvBackendData, PRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int dmgSetParentTimeStamp(void *pvBackendData, PCRTTIMESPEC pTimeStamp)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int dmgGetParentFilename(void *pvBackendData, char **ppszParentFilename)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}

static int dmgSetParentFilename(void *pvBackendData, const char *pszParentFilename)
{
    int rc = VERR_NOT_IMPLEMENTED;
    LogFlow(("%s: returned %Rrc\n", __FUNCTION__, rc));
    return rc;
}


VBOXHDDBACKEND g_DmgBackend =
{
    /* pszBackendName */
    "DMG",
    /* cbSize */
    sizeof(VBOXHDDBACKEND),
    /* uBackendCaps */
    VD_CAP_FILE,
    /* papszFileExtensions */
    NULL,
    /* paConfigInfo */
    NULL,
    /* hPlugin */
    NIL_RTLDRMOD,
    /* pfnCheckIfValid */
    dmgCheckIfValid,
    /* pfnOpen */
    dmgOpen,
    /* pfnCreate */
    dmgCreate,
    /* pfnRename */
    dmgRename,
    /* pfnClose */
    dmgClose,
    /* pfnRead */
    dmgRead,
    /* pfnWrite */
    dmgWrite,
    /* pfnFlush */
    dmgFlush,
    /* pfnGetVersion */
    dmgGetVersion,
    /* pfnGetSize */
    dmgGetSize,
    /* pfnGetFileSize */
    dmgGetFileSize,
    /* pfnGetPCHSGeometry */
    dmgGetPCHSGeometry,
    /* pfnSetPCHSGeometry */
    dmgSetPCHSGeometry,
    /* pfnGetLCHSGeometry */
    dmgGetLCHSGeometry,
    /* pfnSetLCHSGeometry */
    dmgSetLCHSGeometry,
    /* pfnGetImageFlags */
    dmgGetImageFlags,
    /* pfnGetOpenFlags */
    dmgGetOpenFlags,
    /* pfnSetOpenFlags */
    dmgSetOpenFlags,
    /* pfnGetComment */
    dmgGetComment,
    /* pfnSetComment */
    dmgSetComment,
    /* pfnGetUuid */
    dmgGetUuid,
    /* pfnSetUuid */
    dmgSetUuid,
    /* pfnGetModificationUuid */
    dmgGetModificationUuid,
    /* pfnSetModificationUuid */
    dmgSetModificationUuid,
    /* pfnGetParentUuid */
    dmgGetParentUuid,
    /* pfnSetParentUuid */
    dmgSetParentUuid,
    /* pfnGetParentModificationUuid */
    dmgGetParentModificationUuid,
    /* pfnSetParentModificationUuid */
    dmgSetParentModificationUuid,
    /* pfnDump */
    dmgDump,
    /* pfnGetTimeStamp */
    dmgGetTimeStamp,
    /* pfnGetParentTimeStamp */
    dmgGetParentTimeStamp,
    /* pfnSetParentTimeStamp */
    dmgSetParentTimeStamp,
    /* pfnGetParentFilename */
    dmgGetParentFilename,
    /* pfnSetParentFilename */
    dmgSetParentFilename,
    /* pfnIsAsyncIOSupported */
    NULL,
    /* pfnAsyncRead */
    NULL,
    /* pfnAsyncWrite */
    NULL,
    /* pfnAsyncFlush */
    NULL,
    /* pfnComposeLocation */
    genericFileComposeLocation,
    /* pfnComposeName */
    genericFileComposeName,
    /* pfnCompact */
    NULL,
    /* pfnResize */
    NULL
};

