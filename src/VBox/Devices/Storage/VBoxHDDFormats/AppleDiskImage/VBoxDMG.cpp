/* $Id$ */
/** @file
 * VBoxDMG - Intepreter for Apple Disk Images (DMG).
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DEFAULT /** @todo log group */
#include <VBox/VBoxHDD.h>
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
     * size if DMGs with more are found. */
    VBOXUDIFRSRCENTRY   aEntries[4];
} VBOXUDIFRSRCARRAY;
/** Pointer to a UDIF resource array. */
typedef VBOXUDIFRSRCARRAY *PVBOXUDIFRSRCARRAY;
/** Pointer to a const UDIF resource array. */
typedef VBOXUDIFRSRCARRAY const *PCVBOXUDIFRSRCARRAY;



/**
 * VirtualBox Apple Disk Image (DMG) interpreter instance data.
 */
typedef struct VBOXDMG
{
    /** The open image file. */
    RTFILE              hFile;
    /** The current file size. */
    uint64_t            cbFile;
    /** Flags the image was opened with. */
    uint32_t            fOpenFlags;
    /** The filename.
     * Kept around for logging and delete-on-close purposes. */
    const char         *pszFilename;
    /** The resources.
     * A lazy bird ASSUME there are only two arrays in the resource-fork section in
     * the XML, namely 'blkx' and 'plst'. These have been assigned fixed indexes. */
    VBOXUDIFRSRCARRAY   aRsrcs[2];
    /** The UDIF footer. */
    VBOXUDIF            Ftr;
} VBOXDMG;
/** Pointer to an instance of the DMG Image Interpreter. */
typedef VBOXDMG *PVBOXDMG;

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
    VBOXDMG_VALIDATE(pFtr->iSegment  == 0,      ("iSegment=%RU32 cSegments=%RU32\n", pFtr->iSegment, pFtr->cSegments));
    VBOXDMG_VALIDATE(pFtr->cbXml    <= offFtr && pFtr->offXml + pFtr->cbXml <= offFtr, ("cbXml=%#RX64 offXml=%#RX64 offFtr=%#RX64\n", pFtr->cbXml, pFtr->offXml, offFtr));
    VBOXDMG_VALIDATE(pFtr->offXml    < offFtr,  ("offXml=%#RX64\n", pFtr->offXml));
    VBOXDMG_VALIDATE(pFtr->cbXml     > 128,     ("cbXml=%#RX64\n", pFtr->cbXml));
    VBOXDMG_VALIDATE(pFtr->cbXml     < _1M,     ("cbXml=%#RX64\n", pFtr->cbXml));
    VBOXDMG_VALIDATE(pFtr->u32Type == VBOXUDIF_TYPE_DEVICE || pFtr->u32Type == VBOXUDIF_TYPE_PARTITION,  ("u32Type=%RU32\n", pFtr->u32Type));
    VBOXDMG_VALIDATE(pFtr->cSectors != 0,       ("cSectors=%#RX64\n", pFtr->cSectors));
    vboxdmgUdifCkSumIsValid(&pFtr->DataCkSum, "DataCkSum");
    vboxdmgUdifCkSumIsValid(&pFtr->MasterCkSum, "MasterCkSum");

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
    PVBOXDMG pThis = (PVBOXDMG)pvBackendData;

    if (pThis->hFile != NIL_RTFILE)
    {
        /*
         * If writable, flush changes, fix checksums, ++.
         */
        /** @todo writable images. */

        /*
         * Close the file.
         */
        RTFileClose(pThis->hFile);
        pThis->hFile = NIL_RTFILE;
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
static const char *vboxdmgOpenXmlToRsrc(PVBOXDMG pThis, char const *pszXml)
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
 * Worker for vboxdmgOpen that reads in and validates all the necessary
 * structures from the image.
 *
 * This worker is really just a construct to avoid gotos and do-break-while-zero
 * uglyness.
 *
 * @returns VBox status code.
 * @param   pThis       The DMG instance data.
 */
static int vboxdmgOpenWorker(PVBOXDMG pThis)
{
    /*
     * Read the footer.
     */
    int rc = RTFileGetSize(pThis->hFile, &pThis->cbFile);
    if (RT_FAILURE(rc))
        return rc;
    if (pThis->cbFile < 1024)
        return VERR_VD_GEN_INVALID_HEADER;
    rc = RTFileReadAt(pThis->hFile, pThis->cbFile - sizeof(pThis->Ftr), &pThis->Ftr, sizeof(pThis->Ftr), NULL);
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

    /*
     * Read and parse the XML portion.
     */
    size_t cchXml = (size_t)pThis->Ftr.cbXml;
    char *pszXml = (char *)RTMemAlloc(cchXml + 1);
    if (!pszXml)
        return VERR_NO_MEMORY;
    rc = RTFileReadAt(pThis->hFile, pThis->Ftr.offXml, pszXml, cchXml, NULL);
    if (RT_SUCCESS(rc))
    {
        pszXml[cchXml] = '\0';
        const char *pszError = vboxdmgOpenXmlToRsrc(pThis, pszXml);
        if (!pszError)
        {
            /*
             * What is next?
             */
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
static DECLCALLBACK(int) vboxdmgOpen(const char *pszFilename, unsigned fOpenFlags,
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
    PVBOXDMG pThis = (PVBOXDMG)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->fOpenFlags  = fOpenFlags;
    pThis->pszFilename = pszFilename;
    pThis->hFile       = NIL_RTFILE;
    //pThis->cbFile      = 0;

    int rc = RTFileOpen(&pThis->hFile, pszFilename,
                        (fOpenFlags & VD_OPEN_FLAGS_READONLY ? RTFILE_O_READ : RTFILE_O_READWRITE)
                        | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE);
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
static DECLCALLBACK(int) vboxdmgCheckIfValid(const char *pszFilename)
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




#ifdef VBOXDMG_TESTING
int main(int argc, char **argv)
{
    RTR3Init();

    const char *psz = argv[1];
    if (!psz || argv[2])
    {
        RTPrintf("syntax: tstVBoxDMG <image>\n");
        return 1;
    }

    RTPrintf("tstVBoxDMG: TESTING '%s'...\n", psz);

    /* test 1 - vboxdmgCheckIfValid */
    int rc = vboxdmgCheckIfValid(psz);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVBoxDMG: vboxdmgCheckIfValid failed, rc=%Rrc\n", rc);
        return 1;
    }
    RTPrintf("tstVBoxDMG: vboxdmgCheckIfValid succeeded (rc=%Rrc)\n", rc);


    /* test 1 - vboxdmgOpen(RDONLY) & Close. */
    void *pvOpaque = NULL;
    rc = vboxdmgOpen(psz, VD_OPEN_FLAGS_READONLY, NULL, NULL, &pvOpaque);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVBoxDMG: vboxdmgOpen(RDONLY) failed, rc=%Rrc\n", rc);
        return 1;
    }

    rc = vboxdmgClose(pvOpaque, false /* fDelete */);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstVBoxDMG: vboxdmgClose(RDONLY) failed, rc=%Rrc\n", rc);
        return 1;
    }


    RTPrintf("tstVBoxDMG: SUCCESS\n");
    return 0;
}
#endif

