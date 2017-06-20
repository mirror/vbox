/* $Id$ */
/** @file
 * IPRT - ISO Image Maker, Import Existing Image.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_FS
#include "internal/iprt.h"
#include <iprt/fsisomaker.h>

#include <iprt/avl.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/ctype.h>
#include <iprt/file.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/formats/iso9660.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Max directory depth. */
#define RTFSISOMK_IMPORT_MAX_DEPTH  32


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Block to file translation node.
 */
typedef struct RTFSISOMKIMPBLOCK2FILE
{
    /** AVL tree node containing the first block number of the file.
     * Block number is relative to the start of the import image.  */
    AVLU32NODECORE          Core;
    /** The configuration index of the file. */
    uint32_t                idxObj;
} RTFSISOMKIMPBLOCK2FILE;
/** Pointer to a block-2-file translation node. */
typedef RTFSISOMKIMPBLOCK2FILE *PRTFSISOMKIMPBLOCK2FILE;


/**
 * Directory todo list entry.
 */
typedef struct RTFSISOMKIMPDIR
{
    /** List stuff. */
    RTLISTNODE              Entry;
    /** The directory configuration index with hIsoMaker. */
    uint32_t                idxObj;
    /** The directory data block number. */
    uint32_t                offDirBlock;
    /** The directory size (in bytes). */
    uint32_t                cbDir;
    /** The depth of this directory.  */
    uint8_t                 cDepth;
} RTFSISOMKIMPDIR;
/** Pointer to a directory todo list entry. */
typedef RTFSISOMKIMPDIR *PRTFSISOMKIMPDIR;


/**
 * ISO maker ISO importer state.
 */
typedef struct RTFSISOMKIMPORTER
{
    /** The destination ISO maker. */
    RTFSISOMAKER            hIsoMaker;
    /** RTFSISOMK_IMPORT_F_XXX. */
    uint32_t                fFlags;
    /** The status code of the whole import.
     * This notes down the first error status.  */
    int                     rc;
    /** Pointer to error info return structure. */
    PRTERRINFO              pErrInfo;

    /** The source file. */
    RTVFSFILE               hSrcFile;
    /** The import source index of hSrcFile in hIsoMaker.  UINT32_MAX till adding
     * the first file. */
    uint32_t                idxSrcFile;

    /** The root of the tree for converting data block numbers to files
     * (PRTFSISOMKIMPBLOCK2FILE).   This is essential when importing boot files and
     * the 2nd namespace (joliet, udf, hfs) so that we avoid duplicating data. */
    AVLU32TREE              Block2FileRoot;

    /** The primary volume space size in blocks. */
    uint32_t                cBlocksInPrimaryVolumeSpace;
    /** The primary volume space size in bytes. */
    uint64_t                cbPrimaryVolumeSpace;
    /** The number of volumes in the set. */
    uint32_t                cVolumesInSet;
    /** The primary volume sequence ID. */
    uint32_t                idPrimaryVol;

    /** Set if we've already seen a joliet volume descriptor. */
    bool                    fSeenJoliet;

    /** Pointer to the import results structure (output). */
    PRTFSISOMAKERIMPORTRESULTS pResults;

    /** Sector buffer for volume descriptors and such. */
    union
    {
        uint8_t                     ab[ISO9660_SECTOR_SIZE];
        ISO9660VOLDESCHDR           VolDescHdr;
        ISO9660PRIMARYVOLDESC       PrimVolDesc;
        ISO9660SUPVOLDESC           SupVolDesc;
        ISO9660BOOTRECORDELTORITO   ElToritoDesc;
    }                   uSectorBuf;

    /** Name buffer.  */
    char                szNameBuf[_2K];

    /** A somewhat larger buffer. */
    uint8_t             abBuf[_64K];
} RTFSISOMKIMPORTER;
/** Pointer to an ISO maker ISO importer state. */
typedef RTFSISOMKIMPORTER *PRTFSISOMKIMPORTER;


/** Requested to import an unknown ISO format. */
#define VERR_ISOMK_IMPORT_UNKNOWN_FORMAT                (-24906)
/** Too many volume descriptors in the import ISO. */
#define VERR_ISOMK_IMPORT_TOO_MANY_VOL_DESCS            (-24907)
/** Import ISO contains a bad volume descriptor header.   */
#define VERR_ISOMK_IMPORT_INVALID_VOL_DESC_HDR          (-24907)
/** Import ISO contains more than one primary volume descriptor. */
#define VERR_ISOMK_IMPORT_MULTIPLE_PRIMARY_VOL_DESCS    (-24908)
/** Import ISO contains more than one el torito descriptor. */
#define VERR_ISOMK_IMPORT_MULTIPLE_EL_TORITO_DESCS      (-24909)
/** Import ISO contains more than one joliet volume descriptor. */
#define VERR_ISOMK_IMPORT_MULTIPLE_JOLIET_VOL_DESCS     (-24908)
/** Import ISO starts with supplementary volume descriptor before any
 * primary ones. */
#define VERR_ISOMK_IMPORT_SUPPLEMENTARY_BEFORE_PRIMARY  (-24909)
/** Import ISO contains an unsupported primary volume descriptor version. */
#define VERR_IOSMK_IMPORT_PRIMARY_VOL_DESC_VER          (-24909)
/** Import ISO contains a bad primary volume descriptor. */
#define VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC          (-24910)
/** Import ISO contains an unsupported supplementary volume descriptor
 *  version. */
#define VERR_IOSMK_IMPORT_SUP_VOL_DESC_VER              (-24909)
/** Import ISO contains a bad supplementary volume descriptor. */
#define VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC              (-24910)
/** Import ISO uses a logical block size other than 2KB. */
#define VERR_ISOMK_IMPORT_LOGICAL_BLOCK_SIZE_NOT_2KB    (-24911)
/** Import ISO contains more than volume. */
#define VERR_ISOMK_IMPORT_MORE_THAN_ONE_VOLUME_IN_SET   (-24912)
/** Import ISO uses invalid volume sequence number. */
#define VERR_ISOMK_IMPORT_INVALID_VOLUMNE_SEQ_NO        (-24913)
/** Import ISO has different volume space sizes of primary and supplementary
 * volume descriptors. */
#define VERR_ISOMK_IMPORT_VOLUME_SPACE_SIZE_MISMATCH    (-24913)
/** Import ISO has different volume set sizes of primary and supplementary
 * volume descriptors. */
#define VERR_ISOMK_IMPORT_VOLUME_IN_SET_MISMATCH        (-24913)
/** Import ISO contains a bad root directory record. */
#define VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC              (-24914)
/** Import ISO contains a zero sized root directory. */
#define VERR_ISOMK_IMPORT_ZERO_SIZED_ROOT_DIR           (-24915)
/** Import ISO contains a root directory with a mismatching volume sequence
 *  number. */
#define VERR_ISOMK_IMPORT_ROOT_VOLUME_SEQ_NO            (-24916)
/** Import ISO contains a root directory with an out of bounds data extent. */
#define VERR_ISOMK_IMPORT_ROOT_DIR_EXTENT_OUT_OF_BOUNDS (-24917)
/** Import ISO contains a root directory with a bad record length. */
#define VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC_LENGTH       (-24918)
/** Import ISO contains a root directory without the directory flag set. */
#define VERR_ISOMK_IMPORT_ROOT_DIR_WITHOUT_DIR_FLAG     (-24919)
/** Import ISO contains a root directory with multiple extents. */
#define VERR_ISOMK_IMPORT_ROOT_DIR_IS_MULTI_EXTENT      (-24920)
/** Import ISO contains a too deep directory subtree. */
#define VERR_ISOMK_IMPORT_TOO_DEEP_DIR_TREE             (-24921)

/** Import ISO contains a bad directory record. */
#define VERR_ISOMK_IMPORT_BAD_DIR_REC                   (-24922)
/** Import ISO directory record with a mismatching volume sequence number. */
#define VERR_ISOMK_IMPORT_DIR_REC_VOLUME_SEQ_NO         (-24923)
/** Import ISO directory with an extent that is out of bounds. */
#define VERR_ISOMK_IMPORT_DIR_REC_EXTENT_OUT_OF_BOUNDS  (-24924)
/** Import ISO directory with a bad record length. */
#define VERR_ISOMK_IMPORT_BAD_DIR_REC_LENGTH            (-24925)
/** Import ISO directory with a bad name length. */
#define VERR_ISOMK_IMPORT_DOT_DIR_REC_BAD_NAME_LENGTH   (-24926)
/** Import ISO directory with a bad name. */
#define VERR_ISOMK_IMPORT_DOT_DIR_REC_BAD_NAME          (-24927)



/**
 * Wrapper around RTErrInfoSetV.
 *
 * @returns rc
 * @param   pThis               The importer instance.
 * @param   rc                  The status code to set.
 * @param   pszFormat           The format string detailing the error.
 * @param   va                  Argument to the format string.
 */
static int rtFsIsoImpErrorV(PRTFSISOMKIMPORTER pThis, int rc, const char *pszFormat, va_list va)
{
    va_list vaCopy;
    va_copy(vaCopy, va);
    LogRel(("RTFsIsoMkImport error %Rrc: %N\n", rc, pszFormat, &vaCopy));
    va_end(vaCopy);

    if (RT_SUCCESS(pThis->rc))
    {
        pThis->rc = rc;
        rc = RTErrInfoSetV(pThis->pErrInfo, rc, pszFormat, va);
    }

    pThis->pResults->cErrors++;
    return rc;
}


/**
 * Wrapper around RTErrInfoSetF.
 *
 * @returns rc
 * @param   pThis               The importer instance.
 * @param   rc                  The status code to set.
 * @param   pszFormat           The format string detailing the error.
 * @param   ...                 Argument to the format string.
 */
static int rtFsIsoImpError(PRTFSISOMKIMPORTER pThis, int rc, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    rc = rtFsIsoImpErrorV(pThis, rc, pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Callback for destroying a RTFSISOMKIMPBLOCK2FILE node.
 *
 * @returns VINF_SUCCESS
 * @param   pNode               The node to destroy.
 * @param   pvUser              Ignored.
 */
static DECLCALLBACK(int) rtFsIsoMakerImportDestroyData2File(PAVLU32NODECORE pNode, void *pvUser)
{
    RT_NOREF(pvUser);
    RTMemFree(pNode);
    return VINF_SUCCESS;
}


/**
 * Validates a directory record.
 *
 * @returns IPRT status code (safe to ignore, see pThis->rc).
 * @param   pThis               The importer instance.
 * @param   pDirRec             The root directory record to validate.
 * @param   cbMax               The maximum size.
 */
static int rtFsIsoImportValidateDirRec(PRTFSISOMKIMPORTER pThis, PCISO9660DIRREC pDirRec, uint32_t cbMax)
{
    /*
     * Validate dual fields.
     */
    if (RT_LE2H_U32(pDirRec->cbData.le) != RT_BE2H_U32(pDirRec->cbData.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_DIR_REC,
                               "Invalid dir rec size field: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pDirRec->cbData.be), RT_LE2H_U32(pDirRec->cbData.le));

    if (RT_LE2H_U32(pDirRec->offExtent.le) != RT_BE2H_U32(pDirRec->offExtent.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_DIR_REC,
                               "Invalid dir rec extent field: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pDirRec->offExtent.be), RT_LE2H_U32(pDirRec->offExtent.le));

    if (RT_LE2H_U16(pDirRec->VolumeSeqNo.le) != RT_BE2H_U16(pDirRec->VolumeSeqNo.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_DIR_REC,
                               "Invalid dir rec volume sequence ID field: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pDirRec->VolumeSeqNo.be), RT_LE2H_U16(pDirRec->VolumeSeqNo.le));

    /*
     * Check values.
     */
    if (ISO9660_GET_ENDIAN(&pDirRec->VolumeSeqNo) != pThis->idPrimaryVol)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_DIR_REC_VOLUME_SEQ_NO,
                               "Expected dir rec to have same volume sequence number as primary volume: %#x, expected %#x",
                               ISO9660_GET_ENDIAN(&pDirRec->VolumeSeqNo), pThis->idPrimaryVol);

    if (ISO9660_GET_ENDIAN(&pDirRec->offExtent) >= pThis->cBlocksInPrimaryVolumeSpace)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_DIR_REC_EXTENT_OUT_OF_BOUNDS,
                               "Invalid dir rec extent: %#RX32, max %#RX32",
                               ISO9660_GET_ENDIAN(&pDirRec->offExtent), pThis->cBlocksInPrimaryVolumeSpace);

    if (pDirRec->cbDirRec < RT_OFFSETOF(ISO9660DIRREC, achFileId) + pDirRec->bFileIdLength)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_DIR_REC_LENGTH,
                               "Root dir record size is too small: %#x (min %#x)",
                               pDirRec->cbDirRec, RT_OFFSETOF(ISO9660DIRREC, achFileId) + pDirRec->bFileIdLength);
    if (pDirRec->cbDirRec > cbMax)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_DIR_REC_LENGTH,
                               "Root dir record size is too big: %#x (max %#x)", pDirRec->cbDirRec, cbMax);
    return VINF_SUCCESS;
}


/**
 * Validates a dot or dot-dot directory record.
 *
 * @returns IPRT status code (safe to ignore, see pThis->rc).
 * @param   pThis               The importer instance.
 * @param   pDirRec             The root directory record to validate.
 * @param   cbMax               The maximum size.
 * @param   bName               The name byte (0x00: '.', 0x01: '..').
 */
static int rtFsIsoImportValidateDotDirRec(PRTFSISOMKIMPORTER pThis, PCISO9660DIRREC pDirRec, uint32_t cbMax, uint8_t bName)
{
    int rc = rtFsIsoImportValidateDirRec(pThis, pDirRec, cbMax);
    if (RT_SUCCESS(rc))
    {
        if (pDirRec->bFileIdLength != 1)
            return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_DOT_DIR_REC_BAD_NAME_LENGTH,
                                   "Invalid dot dir rec file id length: %u", pDirRec->bFileIdLength);
        if ((uint8_t)pDirRec->achFileId[0] != bName)
            return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_DOT_DIR_REC_BAD_NAME,
                                   "Invalid dot dir rec file id: %#x, expected %#x", pDirRec->achFileId[0], bName);
    }
    return rc;
}


static int rtFsIsoImportProcessIso9660TreeWorker(PRTFSISOMKIMPORTER pThis, uint32_t idxDir,
                                                 uint32_t offDirBlock, uint32_t cbDir, uint8_t cDepth, bool fUnicode,
                                                 PRTLISTANCHOR pTodoList)
{
    /*
     * Restrict the depth to try avoid loops.
     */
    if (cDepth > RTFSISOMK_IMPORT_MAX_DEPTH)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_TOO_DEEP_DIR_TREE, "Dir at %#x LB %#x is too deep", offDirBlock, cbDir);

    /*
     * Read the first chunk into the big buffer.
     */
    uint32_t cbChunk = RT_MIN(cbDir, sizeof(pThis->abBuf));
    uint64_t off     = (uint64_t)offDirBlock * ISO9660_SECTOR_SIZE;
    int rc = RTVfsFileReadAt(pThis->hSrcFile, off, pThis->abBuf, cbChunk, NULL);
    if (RT_FAILURE(rc))
        return rtFsIsoImpError(pThis, rc, "Error reading directory at %#RX64 (%#RX32 / %#RX32): %Rrc", off, cbChunk, cbDir);

    cbDir -= cbChunk;
    off   += cbChunk;

    /*
     * Skip the current and parent directory entries.
     */
    PCISO9660DIRREC pDirRec = (PCISO9660DIRREC)&pThis->abBuf[0];
    rc = rtFsIsoImportValidateDotDirRec(pThis, pDirRec, cbChunk, 0x00);
    if (RT_FAILURE(rc))
        return rc;

    cbChunk -= pDirRec->cbDirRec;
    pDirRec = (PCISO9660DIRREC)((uintptr_t)pDirRec + pDirRec->cbDirRec);
    rc = rtFsIsoImportValidateDotDirRec(pThis, pDirRec, cbChunk, 0x01);
    if (RT_FAILURE(rc))
        return rc;

    cbChunk -= pDirRec->cbDirRec;
    pDirRec  = (PCISO9660DIRREC)((uintptr_t)pDirRec + pDirRec->cbDirRec);

    /*
     * Work our way thru all the directory records.
     */
    Log3(("rtFsIsoImportProcessIso9660TreeWorker: Starting at @%#RX64 LB %#RX32 (out of %#RX32) in %#x\n",
          off - cbChunk, cbChunk, cbChunk + cbDir, idxDir));
    while (cbChunk > 0)
    {
        /*
         * Do we need to read some more?
         */
        if (   cbChunk > UINT8_MAX
            || cbDir == 0)
        { /* No, we don't. */ }
        else
        {
            pDirRec = (PCISO9660DIRREC)memmove(&pThis->abBuf[ISO9660_SECTOR_SIZE - cbChunk], pDirRec, cbChunk);

            Assert(!(off & (ISO9660_SECTOR_SIZE - 1)));
            uint32_t cbToRead = RT_MIN(cbDir, sizeof(pThis->abBuf) - ISO9660_SECTOR_SIZE);
            rc = RTVfsFileReadAt(pThis->hSrcFile, off, &pThis->abBuf[ISO9660_SECTOR_SIZE], cbToRead, NULL);
            if (RT_FAILURE(rc))
                return rtFsIsoImpError(pThis, rc, "Error reading %#RX32 bytes at %#RX64 (dir): %Rrc", off, cbToRead);

            Log3(("rtFsIsoImportProcessIso9660TreeWorker: Read %#zx more bytes @%#RX64, now got @%#RX64 LB %#RX32\n",
                  cbToRead, off, off - cbChunk, cbChunk + cbToRead));
            off     += cbToRead;
            cbDir   -= cbToRead;
            cbChunk += cbToRead;
        }

        /* If null length, skip to the next sector.  May have to read some then. */
        if (pDirRec->cbDirRec == 0)
        {
            uint64_t offChunk = off - cbChunk;
            uint32_t cbSkip   = ISO9660_SECTOR_SIZE - ((uint32_t)offChunk & (ISO9660_SECTOR_SIZE - 1));
            if (cbSkip < cbChunk)
            {
                pDirRec  = (PCISO9660DIRREC)((uintptr_t)pDirRec + cbSkip);
                cbChunk -= cbSkip;
                if (   cbChunk <= UINT8_MAX
                    && cbDir == 0)
                {
                    Log3(("rtFsIsoImportProcessIso9660TreeWorker: cbDirRec=0 --> Restart loop\n"));
                    continue;
                }
                Log3(("rtFsIsoImportProcessIso9660TreeWorker: cbDirRec=0 --> jumped %#RX32 to @%#RX64 LB %#RX32\n",
                      cbSkip, off - cbChunk, cbChunk));
            }
            /* ASSUMES we're working in multiples of sectors! */
            else if (cbDir == 0)
                break;
            else
            {
                Assert(!(off & (ISO9660_SECTOR_SIZE - 1)));
                uint32_t cbToRead = RT_MIN(cbDir, sizeof(pThis->abBuf));
                rc = RTVfsFileReadAt(pThis->hSrcFile, off, pThis->abBuf, cbToRead, NULL);
                if (RT_FAILURE(rc))
                    return rtFsIsoImpError(pThis, rc, "Error reading %#RX32 bytes at %#RX64 (dir): %Rrc", off, cbToRead);

                Log3(("rtFsIsoImportProcessIso9660TreeWorker: cbDirRec=0 --> Read %#zx more bytes @%#RX64, now got @%#RX64 LB %#RX32\n",
                      cbToRead, off, off - cbChunk, cbChunk + cbToRead));
                off     += cbToRead;
                cbDir   -= cbToRead;
                cbChunk += cbToRead;
                pDirRec = (PCISO9660DIRREC)&pThis->abBuf[0];
            }
        }

        /*
         * Validate the directory record.  Give up if not valid since we're
         * likely to get error with subsequent record too.
         */
        uint8_t const         cbSys = pDirRec->cbDirRec - RT_UOFFSETOF(ISO9660DIRREC, achFileId)
                                    - pDirRec->bFileIdLength - !(pDirRec->bFileIdLength & 1);
        uint8_t const * const pbSys = (uint8_t const *)&pDirRec->achFileId[pDirRec->bFileIdLength + !(pDirRec->bFileIdLength & 1)];
        Log3(("pDirRec=&abBuf[%#07zx]: @%#010RX64 cb=%#04x ff=%#04x off=%#010RX32 cb=%#010RX32 cbSys=%#x id=%.*Rhxs\n",
              (uintptr_t)pDirRec - (uintptr_t)&pThis->abBuf[0], off - cbChunk, pDirRec->cbDirRec, pDirRec->fFileFlags,
              ISO9660_GET_ENDIAN(&pDirRec->offExtent), ISO9660_GET_ENDIAN(&pDirRec->cbData), cbSys,
              pDirRec->bFileIdLength, pDirRec->achFileId));
        rc = rtFsIsoImportValidateDirRec(pThis, pDirRec, cbChunk);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Convert the name into the name buffer (szNameBuf).
         */
        if (!fUnicode)
        {
            memcpy(pThis->szNameBuf, pDirRec->achFileId, pDirRec->bFileIdLength);
            pThis->szNameBuf[pDirRec->bFileIdLength] = '\0';
            rc = RTStrValidateEncoding(pThis->szNameBuf);
        }
        else
        {
            char *pszDst = pThis->szNameBuf;
            rc = RTUtf16BigToUtf8Ex((PRTUTF16)pDirRec->achFileId, pDirRec->bFileIdLength / sizeof(RTUTF16),
                                    &pszDst, sizeof(pThis->szNameBuf), NULL);
        }
        if (RT_SUCCESS(rc))
        {
            /* Drop the version from the name. */
            /** @todo preserve the file version on import. */
            size_t cchName = strlen(pThis->szNameBuf);
            if (   !(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
                && cchName > 2
                && RT_C_IS_DIGIT(pThis->szNameBuf[cchName - 1]))
            {
                uint32_t offName = 2;
                while (   offName <= 5
                       && offName + 1 < cchName
                       && RT_C_IS_DIGIT(pThis->szNameBuf[cchName - offName]))
                    offName++;
                if (   offName + 1 < cchName
                    && pThis->szNameBuf[cchName - offName] == ';')
                    pThis->szNameBuf[cchName - offName] = '\0';
            }
            Log3(("  --> name='%s'\n", pThis->szNameBuf));

            /** @todo rock ridge. */
            if (cbSys > 0)
            {
                RT_NOREF(pbSys);
            }
            /*
             * Add the object.
             */
            PRTFSISOMKIMPBLOCK2FILE pBlock2File = NULL;
            uint32_t                idxObj      = UINT32_MAX;
            if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
            {
                rc = RTFsIsoMakerAddUnnamedDir(pThis->hIsoMaker, &idxObj);
                Log3(("  --> added directory #%#x'\n", idxObj));
                if (RT_SUCCESS(rc))
                    pThis->pResults->cAddedDirs++;
            }
            else
            {
                /* Add the common source file if we haven't done that already. */
                if (pThis->idxSrcFile != UINT32_MAX)
                { /* likely */ }
                else
                {
                    rc = RTFsIsoMakerAddCommonSourceFile(pThis->hIsoMaker, pThis->hSrcFile, &pThis->idxSrcFile);
                    if (RT_FAILURE(rc))
                        return rtFsIsoImpError(pThis, rc, "RTFsIsoMakerAddCommonSourceFile failed: %Rrc", rc);
                    Assert(pThis->idxSrcFile != UINT32_MAX);
                }

                if (ISO9660_GET_ENDIAN(&pDirRec->cbData) > 0) /* no data tracking for zero byte files */
                    pBlock2File = (PRTFSISOMKIMPBLOCK2FILE)RTAvlU32Get(&pThis->Block2FileRoot, ISO9660_GET_ENDIAN(&pDirRec->offExtent));
                if (!pBlock2File)
                {
                    rc = RTFsIsoMakerAddUnnamedFileWithCommonSrc(pThis->hIsoMaker, pThis->idxSrcFile,
                                                                 ISO9660_GET_ENDIAN(&pDirRec->offExtent) * (uint64_t)ISO9660_SECTOR_SIZE,
                                                                 ISO9660_GET_ENDIAN(&pDirRec->cbData), NULL /*pObjInfo*/, &idxObj);
                    Log3(("  --> added new file #%#x\n", idxObj));
                    if (RT_SUCCESS(rc))
                    {
                        pThis->pResults->cAddedFiles++;
                        pThis->pResults->cbAddedDataBlocks += RT_ALIGN_32(ISO9660_GET_ENDIAN(&pDirRec->cbData), ISO9660_SECTOR_SIZE);
                    }
                }
                else
                {
                    idxObj = pBlock2File->idxObj;
                    Log3(("  --> existing file #%#x'\n", idxObj));
                    rc = VINF_SUCCESS;
                }
            }

            if (RT_SUCCESS(rc))
            {
                /*
                 * Enter the object into the namespace.
                 */
                rc = RTFsIsoMakerObjSetNameAndParent(pThis->hIsoMaker, idxObj, idxDir,
                                                     !fUnicode ? RTFSISOMAKER_NAMESPACE_ISO_9660 : RTFSISOMAKER_NAMESPACE_JOLIET,
                                                     pThis->szNameBuf);
                if (RT_SUCCESS(rc))
                {
                    pThis->pResults->cAddedNames++;

                    /*
                     * Remember the data location if this is a file, if it's a
                     * directory push it onto the traversal stack.
                     */
                    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY)
                    {
                        PRTFSISOMKIMPDIR pImpDir = (PRTFSISOMKIMPDIR)RTMemAlloc(sizeof(*pImpDir));
                        AssertReturn(pImpDir, rtFsIsoImpError(pThis, VERR_NO_MEMORY, "Could not allocate RTFSISOMKIMPDIR"));
                        pImpDir->cbDir       = ISO9660_GET_ENDIAN(&pDirRec->cbData);
                        pImpDir->offDirBlock = ISO9660_GET_ENDIAN(&pDirRec->offExtent);
                        pImpDir->idxObj      = idxObj;
                        pImpDir->cDepth      = cDepth + 1;
                        RTListAppend(pTodoList, &pImpDir->Entry);
                    }
                    else if (   !pBlock2File
                             && ISO9660_GET_ENDIAN(&pDirRec->cbData) > 0 /* no data tracking for zero byte files */)
                    {
                        pBlock2File = (PRTFSISOMKIMPBLOCK2FILE)RTMemAlloc(sizeof(*pBlock2File));
                        AssertReturn(pBlock2File, rtFsIsoImpError(pThis, VERR_NO_MEMORY, "Could not allocate RTFSISOMKIMPBLOCK2FILE"));
                        pBlock2File->idxObj   = idxObj;
                        pBlock2File->Core.Key = ISO9660_GET_ENDIAN(&pDirRec->offExtent);
                        bool fRc = RTAvlU32Insert(&pThis->Block2FileRoot, &pBlock2File->Core);
                        Assert(fRc); RT_NOREF(fRc);
                    }
                }
                else
                    rtFsIsoImpError(pThis, rc, "Invalid name at %#RX64: %.Rhxs",
                                    off - cbChunk, pDirRec->bFileIdLength, pDirRec->achFileId);
            }
            else
                rtFsIsoImpError(pThis, rc, "Error adding '%s' (fFileFlags=%#x): %Rrc",
                                pThis->szNameBuf, pDirRec->fFileFlags, rc);
        }
        else
            rtFsIsoImpError(pThis, rc, "Invalid name at %#RX64: %.Rhxs",
                            off - cbChunk, pDirRec->bFileIdLength, pDirRec->achFileId);

        /*
         * Advance to the next directory record.
         */
        cbChunk -= pDirRec->cbDirRec;
        pDirRec = (PCISO9660DIRREC)((uintptr_t)pDirRec + pDirRec->cbDirRec);
    }

    return VINF_SUCCESS;
}


static int rtFsIsoImportProcessIso9660Tree(PRTFSISOMKIMPORTER pThis, uint32_t offDirBlock, uint32_t cbDir, bool fUnicode)
{
    /*
     * Make sure we've got a root in the namespace.
     */
    uint32_t idxDir = RTFsIsoMakerGetObjIdxForPath(pThis->hIsoMaker,
                                                   !fUnicode ? RTFSISOMAKER_NAMESPACE_ISO_9660 : RTFSISOMAKER_NAMESPACE_JOLIET,
                                                    "/");
    if (idxDir == UINT32_MAX)
    {
        idxDir = RTFSISOMAKER_CFG_IDX_ROOT;
        int rc = RTFsIsoMakerObjSetPath(pThis->hIsoMaker, RTFSISOMAKER_CFG_IDX_ROOT,
                                        !fUnicode ? RTFSISOMAKER_NAMESPACE_ISO_9660 : RTFSISOMAKER_NAMESPACE_JOLIET, "/");
        if (RT_FAILURE(rc))
            return rtFsIsoImpError(pThis, rc, "RTFsIsoMakerObjSetPath failed on root dir: %Rrc", rc);
    }
    Assert(idxDir == RTFSISOMAKER_CFG_IDX_ROOT);

    /*
     * Directories.
     */
    int          rc     = VINF_SUCCESS;
    uint8_t      cDepth = 0;
    RTLISTANCHOR TodoList;
    RTListInit(&TodoList);
    for (;;)
    {
        int rc2 = rtFsIsoImportProcessIso9660TreeWorker(pThis, idxDir, offDirBlock, cbDir, cDepth, fUnicode, &TodoList);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;

        /*
         * Pop the next directory.
         */
        PRTFSISOMKIMPDIR pNext = RTListRemoveLast(&TodoList, RTFSISOMKIMPDIR, Entry);
        if (!pNext)
            break;
        idxDir      = pNext->idxObj;
        offDirBlock = pNext->offDirBlock;
        cbDir       = pNext->cbDir;
        cDepth      = pNext->cDepth;
        RTMemFree(pNext);
    }

    return rc;
}


/**
 * Validates a root directory record.
 *
 * @returns IPRT status code (safe to ignore, see pThis->rc).
 * @param   pThis               The importer instance.
 * @param   pDirRec             The root directory record to validate.
 */
static int rtFsIsoImportValidateRootDirRec(PRTFSISOMKIMPORTER pThis, PCISO9660DIRREC pDirRec)
{
    /*
     * Validate dual fields.
     */
    if (RT_LE2H_U32(pDirRec->cbData.le) != RT_BE2H_U32(pDirRec->cbData.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC,
                               "Invalid root dir size: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pDirRec->cbData.be), RT_LE2H_U32(pDirRec->cbData.le));

    if (RT_LE2H_U32(pDirRec->offExtent.le) != RT_BE2H_U32(pDirRec->offExtent.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC,
                               "Invalid root dir extent: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pDirRec->offExtent.be), RT_LE2H_U32(pDirRec->offExtent.le));

    if (RT_LE2H_U16(pDirRec->VolumeSeqNo.le) != RT_BE2H_U16(pDirRec->VolumeSeqNo.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC,
                               "Invalid root dir volume sequence ID: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pDirRec->VolumeSeqNo.be), RT_LE2H_U16(pDirRec->VolumeSeqNo.le));

    /*
     * Check values.
     */
    if (ISO9660_GET_ENDIAN(&pDirRec->VolumeSeqNo) != pThis->idPrimaryVol)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_ROOT_VOLUME_SEQ_NO,
                               "Expected root dir to have same volume sequence number as primary volume: %#x, expected %#x",
                               ISO9660_GET_ENDIAN(&pDirRec->VolumeSeqNo), pThis->idPrimaryVol);

    if (ISO9660_GET_ENDIAN(&pDirRec->cbData) == 0)
        return RTErrInfoSet(pThis->pErrInfo, VERR_ISOMK_IMPORT_ZERO_SIZED_ROOT_DIR, "Zero sized root dir");

    if (ISO9660_GET_ENDIAN(&pDirRec->offExtent) >= pThis->cBlocksInPrimaryVolumeSpace)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_ROOT_DIR_EXTENT_OUT_OF_BOUNDS,
                               "Invalid root dir extent: %#RX32, max %#RX32",
                               ISO9660_GET_ENDIAN(&pDirRec->offExtent), pThis->cBlocksInPrimaryVolumeSpace);

    if (pDirRec->cbDirRec < RT_OFFSETOF(ISO9660DIRREC, achFileId))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_ROOT_DIR_REC_LENGTH,
                               "Root dir record size is too small: %#x (min %#x)",
                               pDirRec->cbDirRec, RT_OFFSETOF(ISO9660DIRREC, achFileId));

    if (!(pDirRec->fFileFlags & ISO9660_FILE_FLAGS_DIRECTORY))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_ROOT_DIR_WITHOUT_DIR_FLAG,
                               "Root dir is not flagged as directory: %#x", pDirRec->fFileFlags);
    if (pDirRec->fFileFlags & ISO9660_FILE_FLAGS_MULTI_EXTENT)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_ROOT_DIR_IS_MULTI_EXTENT,
                               "Root dir is cannot be multi-extent: %#x", pDirRec->fFileFlags);

    return VINF_SUCCESS;
}


/**
 * Processes a primary volume descriptor, importing all files described by its
 * namespace.
 *
 * @returns IPRT status code (safe to ignore, see pThis->rc).
 * @param   pThis               The importer instance.
 * @param   pVolDesc            The primary volume descriptor.
 */
static int rtFsIsoImportProcessPrimaryDesc(PRTFSISOMKIMPORTER pThis, PISO9660PRIMARYVOLDESC pVolDesc)
{
    /*
     * Validate dual fields first.
     */
    if (pVolDesc->bFileStructureVersion != ISO9660_FILE_STRUCTURE_VERSION)
        return rtFsIsoImpError(pThis, VERR_IOSMK_IMPORT_PRIMARY_VOL_DESC_VER,
                               "Unsupported file structure version: %#x", pVolDesc->bFileStructureVersion);

    if (RT_LE2H_U16(pVolDesc->cbLogicalBlock.le) != RT_BE2H_U16(pVolDesc->cbLogicalBlock.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC,
                               "Mismatching logical block size: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->cbLogicalBlock.be), RT_LE2H_U16(pVolDesc->cbLogicalBlock.le));
    if (RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le) != RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC,
                               "Mismatching volume space size: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be), RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le));
    if (RT_LE2H_U16(pVolDesc->cVolumesInSet.le) != RT_BE2H_U16(pVolDesc->cVolumesInSet.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC,
                               "Mismatching volumes in set: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->cVolumesInSet.be), RT_LE2H_U16(pVolDesc->cVolumesInSet.le));
    if (RT_LE2H_U16(pVolDesc->VolumeSeqNo.le) != RT_BE2H_U16(pVolDesc->VolumeSeqNo.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC,
                               "Mismatching volume sequence no.: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->VolumeSeqNo.le));
    if (RT_LE2H_U32(pVolDesc->cbPathTable.le) != RT_BE2H_U32(pVolDesc->cbPathTable.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_PRIMARY_VOL_DESC,
                               "Mismatching path table size: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pVolDesc->cbPathTable.be), RT_LE2H_U32(pVolDesc->cbPathTable.le));

    /*
     * Validate field values against our expectations.
     */
    if (ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock) != ISO9660_SECTOR_SIZE)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_LOGICAL_BLOCK_SIZE_NOT_2KB,
                               "Unsupported block size: %#x", ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock));

    if (ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet) != 1)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_MORE_THAN_ONE_VOLUME_IN_SET,
                               "Volumes in set: %#x", ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet));

    if (ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo) != 1)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_INVALID_VOLUMNE_SEQ_NO,
                               "Unexpected volume sequence number: %#x", ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo));

    /*
     * Gather info we need.
     */
    pThis->cBlocksInPrimaryVolumeSpace  = ISO9660_GET_ENDIAN(&pVolDesc->VolumeSpaceSize);
    pThis->cbPrimaryVolumeSpace         = pThis->cBlocksInPrimaryVolumeSpace * (uint64_t)ISO9660_SECTOR_SIZE;
    pThis->cVolumesInSet                = ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet);
    pThis->idPrimaryVol                 = ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo);

    /*
     * Validate the root directory record.
     */
    int rc = rtFsIsoImportValidateRootDirRec(pThis, &pVolDesc->RootDir.DirRec);
    if (RT_SUCCESS(rc))
    {
        /*
         * Process the directory tree.  Start by establishing a root directory.
         */
        if (!(pThis->fFlags & RTFSISOMK_IMPORT_F_NO_PRIMARY_ISO))
            rc = rtFsIsoImportProcessIso9660Tree(pThis, ISO9660_GET_ENDIAN(&pVolDesc->RootDir.DirRec.offExtent),
                                                 ISO9660_GET_ENDIAN(&pVolDesc->RootDir.DirRec.cbData), false /*fUnicode*/);
    }

    return rc;
}


static int rtFsIsoImportProcessSupplementaryDesc(PRTFSISOMKIMPORTER pThis, PISO9660SUPVOLDESC pVolDesc)
{
    /*
     * Validate dual fields first.
     */
    if (pVolDesc->bFileStructureVersion != ISO9660_FILE_STRUCTURE_VERSION)
        return rtFsIsoImpError(pThis, VERR_IOSMK_IMPORT_SUP_VOL_DESC_VER,
                               "Unsupported file structure version: %#x", pVolDesc->bFileStructureVersion);

    if (RT_LE2H_U16(pVolDesc->cbLogicalBlock.le) != RT_BE2H_U16(pVolDesc->cbLogicalBlock.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC,
                               "Mismatching logical block size: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->cbLogicalBlock.be), RT_LE2H_U16(pVolDesc->cbLogicalBlock.le));
    if (RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le) != RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC,
                               "Mismatching volume space size: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pVolDesc->VolumeSpaceSize.be), RT_LE2H_U32(pVolDesc->VolumeSpaceSize.le));
    if (RT_LE2H_U16(pVolDesc->cVolumesInSet.le) != RT_BE2H_U16(pVolDesc->cVolumesInSet.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC,
                               "Mismatching volumes in set: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->cVolumesInSet.be), RT_LE2H_U16(pVolDesc->cVolumesInSet.le));
    if (RT_LE2H_U16(pVolDesc->VolumeSeqNo.le) != RT_BE2H_U16(pVolDesc->VolumeSeqNo.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC,
                               "Mismatching volume sequence no.: {%#RX16,%#RX16}",
                               RT_BE2H_U16(pVolDesc->VolumeSeqNo.be), RT_LE2H_U16(pVolDesc->VolumeSeqNo.le));
    if (RT_LE2H_U32(pVolDesc->cbPathTable.le) != RT_BE2H_U32(pVolDesc->cbPathTable.be))
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_BAD_SUP_VOL_DESC,
                               "Mismatching path table size: {%#RX32,%#RX32}",
                               RT_BE2H_U32(pVolDesc->cbPathTable.be), RT_LE2H_U32(pVolDesc->cbPathTable.le));

    /*
     * Validate field values against our expectations.
     */
    if (ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock) != ISO9660_SECTOR_SIZE)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_LOGICAL_BLOCK_SIZE_NOT_2KB,
                               "Unsupported block size: %#x", ISO9660_GET_ENDIAN(&pVolDesc->cbLogicalBlock));

    if (ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet) != pThis->cVolumesInSet)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_VOLUME_IN_SET_MISMATCH, "Volumes in set: %#x, expected %#x",
                               ISO9660_GET_ENDIAN(&pVolDesc->cVolumesInSet), pThis->cVolumesInSet);

    if (ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo) != pThis->idPrimaryVol)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_INVALID_VOLUMNE_SEQ_NO,
                               "Unexpected volume sequence number: %#x (expected %#x)",
                               ISO9660_GET_ENDIAN(&pVolDesc->VolumeSeqNo), pThis->idPrimaryVol);

    if (ISO9660_GET_ENDIAN(&pVolDesc->VolumeSpaceSize) != pThis->cBlocksInPrimaryVolumeSpace)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_INVALID_VOLUMNE_SEQ_NO,
                               "Volume space size differs between primary and supplementary descriptors: %#x, primary %#x",
                               ISO9660_GET_ENDIAN(&pVolDesc->VolumeSpaceSize), pThis->cBlocksInPrimaryVolumeSpace);

    /*
     * Validate the root directory record.
     */
    int rc = rtFsIsoImportValidateRootDirRec(pThis, &pVolDesc->RootDir.DirRec);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Is this a joliet descriptor? Ignore if not.
     */
    uint8_t uJolietLevel = 0;
    if (   pVolDesc->abEscapeSequences[0] == ISO9660_JOLIET_ESC_SEQ_0
        && pVolDesc->abEscapeSequences[1] == ISO9660_JOLIET_ESC_SEQ_1)
        switch (pVolDesc->abEscapeSequences[2])
        {
            case ISO9660_JOLIET_ESC_SEQ_2_LEVEL_1: uJolietLevel = 1; break;
            case ISO9660_JOLIET_ESC_SEQ_2_LEVEL_2: uJolietLevel = 2; break;
            case ISO9660_JOLIET_ESC_SEQ_2_LEVEL_3: uJolietLevel = 3; break;
            default: Log(("rtFsIsoImportProcessSupplementaryDesc: last joliet escape sequence byte doesn't match: %#x\n",
                          pVolDesc->abEscapeSequences[2]));
        }
    if (uJolietLevel == 0)
        return VINF_SUCCESS;

    /*
     * Only one joliet descriptor.
     */
    if (pThis->fSeenJoliet)
        return rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_MULTIPLE_JOLIET_VOL_DESCS,
                               "More than one Joliet volume descriptor is not supported");
    pThis->fSeenJoliet = true;

    /*
     * Process the directory tree.
     */
    if (!(pThis->fFlags & RTFSISOMK_IMPORT_F_NO_JOLIET))
        return rtFsIsoImportProcessIso9660Tree(pThis, ISO9660_GET_ENDIAN(&pVolDesc->RootDir.DirRec.offExtent),
                                               ISO9660_GET_ENDIAN(&pVolDesc->RootDir.DirRec.cbData), true /*fUnicode*/);
    return VINF_SUCCESS;
}


static int rtFsIsoImportProcessElToritoDesc(PRTFSISOMKIMPORTER pThis, PISO9660BOOTRECORDELTORITO pElTorito)
{
    RT_NOREF(pThis, pElTorito);
    return VINF_SUCCESS;
}


/**
 * Imports an existing ISO.
 *
 * Just like other source files, the existing image must remain present and
 * unmodified till the ISO maker is done with it.
 *
 * @returns IRPT status code.
 * @param   hIsoMaker           The ISO maker handle.
 * @param   pszIso              Path to the existing image to import / clone.
 *                              This is fed to RTVfsChainOpenFile.
 * @param   fFlags              Reserved for the future, MBZ.
 * @param   poffError           Where to return the position in @a pszIso
 *                              causing trouble when opening it for reading.
 *                              Optional.
 * @param   pErrInfo            Where to return additional error information.
 *                              Optional.
 */
RTDECL(int) RTFsIsoMakerImport(RTFSISOMAKER hIsoMaker, const char *pszIso, uint32_t fFlags,
                               PRTFSISOMAKERIMPORTRESULTS pResults, PRTERRINFO pErrInfo)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pResults, VERR_INVALID_POINTER);
    pResults->cAddedNames       = 0;
    pResults->cAddedDirs        = 0;
    pResults->cbAddedDataBlocks = 0;
    pResults->cAddedFiles       = 0;
    pResults->cBootCatEntries   = UINT32_MAX;
    pResults->cbSysArea         = 0;
    pResults->cErrors           = 0;
    pResults->offError          = UINT32_MAX;
    AssertReturn(!(fFlags & ~RTFSISOMK_IMPORT_F_VALID_MASK), VERR_INVALID_FLAGS);

    /*
     * Open the input file and start working on it.
     */
    RTVFSFILE hSrcFile;
    int rc = RTVfsChainOpenFile(pszIso, RTFILE_O_READ | RTFILE_O_OPEN | RTFILE_O_DENY_WRITE,
                                &hSrcFile, &pResults->offError, pErrInfo);
    if (RT_FAILURE(rc))
        return rc;
    pResults->offError = UINT32_MAX;


    /*
     * Allocate and init the importer state.
     */
    PRTFSISOMKIMPORTER pThis = (PRTFSISOMKIMPORTER)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->hIsoMaker        = hIsoMaker;
        pThis->fFlags           = fFlags;
        pThis->rc               = VINF_SUCCESS;
        pThis->pErrInfo         = pErrInfo;
        pThis->hSrcFile         = hSrcFile;
        pThis->idxSrcFile       = UINT32_MAX;
        //pThis->Block2FileRoot = NULL;
        //pThis->cBlocksInPrimaryVolumeSpace = 0;
        //pThis->cbPrimaryVolumeSpace = 0
        //pThis->cVolumesInSet  = 0;
        //pThis->idPrimaryVol   = 0;
        //pThis->fSeenJoliet    = false;
        pThis->pResults         = pResults;

        /*
         * Check if this looks like a plausible ISO by checking out the first volume descriptor.
         */
        rc = RTVfsFileReadAt(hSrcFile, _32K, &pThis->uSectorBuf.PrimVolDesc, sizeof(pThis->uSectorBuf.PrimVolDesc), NULL);
        if (RT_SUCCESS(rc))
        {
            if (   pThis->uSectorBuf.VolDescHdr.achStdId[0] == ISO9660VOLDESC_STD_ID_0
                && pThis->uSectorBuf.VolDescHdr.achStdId[1] == ISO9660VOLDESC_STD_ID_1
                && pThis->uSectorBuf.VolDescHdr.achStdId[2] == ISO9660VOLDESC_STD_ID_2
                && pThis->uSectorBuf.VolDescHdr.achStdId[3] == ISO9660VOLDESC_STD_ID_3
                && pThis->uSectorBuf.VolDescHdr.achStdId[4] == ISO9660VOLDESC_STD_ID_4
                && (   pThis->uSectorBuf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_PRIMARY
                    || pThis->uSectorBuf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_BOOT_RECORD) )
            {
                /*
                 * Process the volume descriptors using the sector buffer, starting
                 * with the one we've already got sitting there.  We postpone processing
                 * the el torito one till after the others, so we can name files and size
                 * referenced in it.
                 */
                uint32_t cPrimaryVolDescs = 0;
                uint32_t iElTorito        = UINT32_MAX;
                uint32_t iVolDesc         = 0;
                for (;;)
                {
                    switch (pThis->uSectorBuf.VolDescHdr.bDescType)
                    {
                        case ISO9660VOLDESC_TYPE_PRIMARY:
                            cPrimaryVolDescs++;
                            if (cPrimaryVolDescs == 1)
                                rtFsIsoImportProcessPrimaryDesc(pThis, &pThis->uSectorBuf.PrimVolDesc);
                            else
                                rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_MULTIPLE_PRIMARY_VOL_DESCS,
                                                "Only a single primary volume descriptor is currently supported");
                            break;

                        case ISO9660VOLDESC_TYPE_SUPPLEMENTARY:
                            if (cPrimaryVolDescs > 0)
                                rtFsIsoImportProcessSupplementaryDesc(pThis, &pThis->uSectorBuf.SupVolDesc);
                            else
                                rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_SUPPLEMENTARY_BEFORE_PRIMARY,
                                                "Primary volume descriptor expected before any supplementary descriptors!");
                            break;

                        case ISO9660VOLDESC_TYPE_BOOT_RECORD:
                            if (strcmp(pThis->uSectorBuf.ElToritoDesc.achBootSystemId,
                                       ISO9660BOOTRECORDELTORITO_BOOT_SYSTEM_ID) == 0)
                            {
                                if (iElTorito == UINT32_MAX)
                                    iElTorito = iVolDesc;
                                else
                                    rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_MULTIPLE_EL_TORITO_DESCS,
                                                    "Only a single El Torito descriptor exepcted!");
                            }
                            break;

                        case ISO9660VOLDESC_TYPE_PARTITION:
                            /* ignore for now */
                            break;

                        case ISO9660VOLDESC_TYPE_TERMINATOR:
                            AssertFailed();
                            break;
                    }


                    /*
                     * Read the next volume descriptor and check the signature.
                     */
                    iVolDesc++;
                    if (iVolDesc >= 32)
                    {
                        rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_TOO_MANY_VOL_DESCS, "Parses at most 32 volume descriptors");
                        break;
                    }

                    rc = RTVfsFileReadAt(hSrcFile, _32K + iVolDesc * ISO9660_SECTOR_SIZE,
                                         &pThis->uSectorBuf, sizeof(pThis->uSectorBuf), NULL);
                    if (RT_FAILURE(rc))
                    {
                        rtFsIsoImpError(pThis, rc, "Error reading the volume descriptor #%u at %#RX32: %Rrc",
                                        iVolDesc, _32K + iVolDesc * ISO9660_SECTOR_SIZE, rc);
                        break;
                    }

                    if (   pThis->uSectorBuf.VolDescHdr.achStdId[0] != ISO9660VOLDESC_STD_ID_0
                        || pThis->uSectorBuf.VolDescHdr.achStdId[1] != ISO9660VOLDESC_STD_ID_1
                        || pThis->uSectorBuf.VolDescHdr.achStdId[2] != ISO9660VOLDESC_STD_ID_2
                        || pThis->uSectorBuf.VolDescHdr.achStdId[3] != ISO9660VOLDESC_STD_ID_3
                        || pThis->uSectorBuf.VolDescHdr.achStdId[4] != ISO9660VOLDESC_STD_ID_4)
                    {
                        rtFsIsoImpError(pThis, VERR_ISOMK_IMPORT_INVALID_VOL_DESC_HDR,
                                        "Invalid volume descriptor header #%u at %#RX32: %.*Rhxs",
                                        iVolDesc, _32K + iVolDesc * ISO9660_SECTOR_SIZE,
                                        (int)sizeof(pThis->uSectorBuf.VolDescHdr), &pThis->uSectorBuf.VolDescHdr);
                        break;
                    }
                    /** @todo UDF support. */
                    if (pThis->uSectorBuf.VolDescHdr.bDescType == ISO9660VOLDESC_TYPE_TERMINATOR)
                        break;
                }

                /*
                 * Process the system area.
                 */
                if (RT_SUCCESS(pThis->rc) || pThis->idxSrcFile != UINT32_MAX)
                {
                    rc = RTVfsFileReadAt(hSrcFile, 0, pThis->abBuf, _32K, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        if (!ASMMemIsAllU8(pThis->abBuf, _32K, 0))
                        {
                            /* Drop zero sectors from the end. */
                            uint32_t cbSysArea = _32K;
                            while (   cbSysArea >= ISO9660_SECTOR_SIZE
                                   && ASMMemIsAllU8(&pThis->abBuf[cbSysArea - ISO9660_SECTOR_SIZE], ISO9660_SECTOR_SIZE, 0))
                                cbSysArea -= ISO9660_SECTOR_SIZE;

                            /** @todo HFS */
                            pThis->pResults->cbSysArea = cbSysArea;
                            rc = RTFsIsoMakerSetSysAreaContent(hIsoMaker, pThis->abBuf, cbSysArea, 0);
                            if (RT_FAILURE(rc))
                                rtFsIsoImpError(pThis, rc, "RTFsIsoMakerSetSysAreaContent failed: %Rrc", rc);
                        }
                    }
                    else
                        rtFsIsoImpError(pThis, rc, "Error reading the system area (0..32KB): %Rrc", rc);
                }

                /*
                 * Do the El Torito descriptor.
                 */
                if (   iElTorito != UINT32_MAX
                    && !(pThis->fFlags & RTFSISOMK_IMPORT_F_NO_BOOT)
                    && (RT_SUCCESS(pThis->rc) || pThis->idxSrcFile != UINT32_MAX))
                {
                    rc = RTVfsFileReadAt(hSrcFile, _32K + iElTorito * ISO9660_SECTOR_SIZE,
                                         &pThis->uSectorBuf, sizeof(pThis->uSectorBuf), NULL);
                    if (RT_SUCCESS(rc))
                        rtFsIsoImportProcessElToritoDesc(pThis, &pThis->uSectorBuf.ElToritoDesc);
                    else
                        rtFsIsoImpError(pThis, rc, "Error reading the El Torito volume descriptor at %#RX32: %Rrc",
                                        _32K + iElTorito * ISO9660_SECTOR_SIZE, rc);
                }

                /*
                 * Return the first error status.
                 */
                rc = pThis->rc;
            }
            else
                rc = RTErrInfoSetF(pErrInfo, VERR_ISOMK_IMPORT_UNKNOWN_FORMAT, "Invalid volume descriptor header: %.*Rhxs",
                                   (int)sizeof(pThis->uSectorBuf.VolDescHdr), &pThis->uSectorBuf.VolDescHdr);
        }

        /*
         * Destroy the state.
         */
        RTAvlU32Destroy(&pThis->Block2FileRoot, rtFsIsoMakerImportDestroyData2File, NULL);
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;
    RTVfsFileRelease(hSrcFile);
    return rc;

}

