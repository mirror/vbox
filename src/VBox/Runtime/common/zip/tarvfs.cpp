/* $Id$ */
/** @file
 * IPRT - TAR Virtual Filesystem.
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


/******************************************************************************
 *   Header Files                                                             *
 ******************************************************************************/
#include "internal/iprt.h"
#include <iprt/zip.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/poll.h>
#include <iprt/file.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include "tar.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Tar directory, character device, block device, fifo socket or symbolic link.
 */
typedef struct RTZIPTARBASEOBJ
{
    /** The stream offset of the (first) header.  */
    RTFOFF              offHdr;
    /** The tar header. */
    RTZIPTARHDR         Hdr;
    /** The object info with unix attributes. */
    RTFSOBJINFO         ObjInfo;
} RTZIPTARBASEOBJ;
/** Pointer to a tar filesystem stream base object. */
typedef RTZIPTARBASEOBJ *PRTZIPTARBASEOBJ;


/**
 * Tar file represented as a VFS I/O stream.
 */
typedef struct RTZIPTARIOSTREAM
{
    /** The basic tar object data. */
    RTZIPTARBASEOBJ     BaseObj;
    /** The number of bytes in the file. */
    RTFOFF              cbFile;
    /** The current file position. */
    RTFOFF              offFile;
    /** The number of padding bytes following the file. */
    uint32_t            cbPadding;
    /** Set if we've reached the end of the file. */
    bool                fEndOfStream;
    /** The input I/O stream. */
    RTVFSIOSTREAM       hVfsIos;
} RTZIPTARIOSTREAM;
/** Pointer to a the private data of a tar file I/O stream. */
typedef RTZIPTARIOSTREAM *PRTZIPTARIOSTREAM;


/**
 * Tar filesystem stream private data.
 */
typedef struct RTZIPTARFSSTREAM
{
    /** The input I/O stream. */
    RTVFSIOSTREAM       hVfsIos;

    /** The current object (referenced). */
    RTVFSOBJ            hVfsCurObj;
    /** Pointer to the private data if hVfsCurObj is representing a file. */
    PRTZIPTARIOSTREAM   pCurIosData;

    /** The start offset. */
    RTFOFF              offStart;
    /** The offset of the next header. */
    RTFOFF              offNextHdr;

    /** Set if we've reached the end of the stream. */
    bool                fEndOfStream;
    /** Set if we've encountered a fatal error. */
    int                 rcFatal;
} RTZIPTARFSSTREAM;
/** Pointer to a the private data of a tar filesystem stream. */
typedef RTZIPTARFSSTREAM *PRTZIPTARFSSTREAM;


/**
 * Checks if the TAR header is in the ustar format.
 *
 * @returns true / false.
 * @param   pTar                The TAR header.
 */
DECLINLINE(bool) rtZipTarHdrIsUstar(PCRTZIPTARHDR pTar)
{
    return pTar->Posix.magic[0] == 'u'
        && pTar->Posix.magic[1] == 's'
        && pTar->Posix.magic[2] == 't'
        && pTar->Posix.magic[3] == 'a'
        && pTar->Posix.magic[4] == 'r'
        && pTar->Posix.magic[5] == '\0'
        && pTar->Posix.version[0] == '0'
        && pTar->Posix.version[1] == '0';
}


/**
 * Checks if the TAR header is in the ustar format and has a regular file type.
 *
 * @returns true / false.
 * @param   pTar                The TAR header.
 */
DECLINLINE(bool) rtZipTarHdrIsRegularUstar(PCRTZIPTARHDR pTar)
{
    return rtZipTarHdrIsUstar(pTar)
        && (    (   pTar->Posix.typeflag >= RTZIPTAR_TF_NORMAL
                 && pTar->Posix.typeflag <= RTZIPTAR_TF_CONTIG)
            ||  pTar->Posix.typeflag == RTZIPTAR_TF_OLDNORMAL);
}


/**
 * Checks if the TAR header includes a posix user name field.
 *
 * @returns true / false.
 * @param   pTar                The TAR header.
 */
DECLINLINE(bool) rtZipTarHdrHasPosixUserName(PCRTZIPTARHDR pTar)
{
    return pTar->Posix.uname[0] != '\0'
        && rtZipTarHdrIsUstar(pTar);
}


/**
 * Checks if the TAR header includes a posix group name field.
 *
 * @returns true / false.
 * @param   pTar                The TAR header.
 */
DECLINLINE(bool) rtZipTarHdrHasPosixGroupName(PCRTZIPTARHDR pTar)
{
    return pTar->Posix.gname[0] != '\0'
        && rtZipTarHdrIsUstar(pTar);
}


/**
 * Checks if the TAR header includes a posix compatible path prefix field.
 *
 * @returns true / false.
 * @param   pTar                The TAR header.
 */
DECLINLINE(bool) rtZipTarHdrHasPrefix(PCRTZIPTARHDR pTar)
{
    return pTar->Posix.prefix[0] != '\0'
        && rtZipTarHdrIsUstar(pTar);
}


/**
 * Converts a numeric header field to the C native type.
 *
 * @returns IPRT status code.
 *
 * @param   pszField            The TAR header field.
 * @param   cchField            The length of the field.
 * @param   fOctalOnly          Must be octal.
 * @param   pi64                Where to store the value.
 */
static int rtZipTarHdrFieldToNum(const char *pszField, size_t cchField, bool fOctalOnly, int64_t *pi64)
{
    size_t const cchFieldOrg = cchField;
    if (   fOctalOnly
        || !(*(unsigned char *)pszField & 0x80))
    {
        /*
         * Skip leading zeros, saving a few slower loops below.
         */
        while (cchField > 0 && *pszField == '0')
            cchField--, pszField++;

        /*
         * Convert octal digits.
         */
        int64_t i64 = 0;
        while (cchField > 0)
        {
            unsigned char uDigit = *pszField - '0';
            if (uDigit >= 8)
                break;
            i64 <<= 3;
            i64 |= uDigit;

            pszField++;
            cchField--;
        }
        *pi64 = i64;

        /*
         * Was it terminated correctly?
         */
        while (cchField > 0)
        {
            char ch = *pszField++;
            if (ch != 0 && ch != ' ')
                return cchField < cchFieldOrg
                     ? VERR_TAR_BAD_NUM_FIELD_TERM
                     : VERR_TAR_BAD_NUM_FIELD;
            cchField--;
        }
    }
    else
    {
        /** @todo implement base-256 encoded fields. */
        return VERR_TAR_BASE_256_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}


/**
 * Calculates the tar header checksums and detects if it's all zeros.
 *
 * @returns true if all zeros, false if not.
 * @param   pHdr                The header to checksum.
 * @param   pi32Unsigned        Where to store the checksum calculated using
 *                              unsigned chars.   This is the one POSIX
 *                              specifies.
 * @param   pi32Signed          Where to store the checksum calculated using
 *                              signed chars.
 *
 * @remarks The reason why we calculate the checksum as both signed and unsigned
 *          has to do with various the char C type being signed on some hosts
 *          and unsigned on others.
 */
static bool rtZipTarCalcChkSum(PCRTZIPTARHDR pHdr, int32_t *pi32Unsigned, int32_t *pi32Signed)
{
    int32_t i32Unsigned = 0;
    int32_t i32Signed   = 0;

    /*
     * Sum up the entire header.
     */
    const char *pch    = (const char *)pHdr;
    const char *pchEnd = pch + sizeof(*pHdr);
    do
    {
        i32Unsigned += *(unsigned char *)pch;
        i32Signed   += *(signed   char *)pch;
    } while (++pch != pchEnd);

    /*
     * Check if it's all zeros and replace the chksum field with spaces.
     */
    bool const fZeroHdr = i32Unsigned == 0;

    pch    = pHdr->Posix.chksum;
    pchEnd = pch + sizeof(pHdr->Posix.chksum);
    do
    {
        i32Unsigned -= *(unsigned char *)pch;
        i32Signed   -= *(signed   char *)pch;
    } while (++pch != pchEnd);

    i32Unsigned += (unsigned char)' ' * sizeof(pHdr->Posix.chksum);
    i32Signed   += (signed   char)' ' * sizeof(pHdr->Posix.chksum);

    *pi32Unsigned = i32Unsigned;
    if (pi32Signed)
        *pi32Signed = i32Signed;
    return fZeroHdr;
}


/**
 * Validates the TAR header.
 *
 * @returns VINF_SUCCESS if valid, appropriate VERR_TAR_XXX if not.
 * @param   pTar                The TAR header.
 * @param   penmType            Where to return the type of header on success.
 */
static int rtZipTarHdrValidate(PCRTZIPTARHDR pTar, PRTZIPTARTYPE penmType)
{
    /*
     * Calc the checksum first since this enables us to detect zero headers.
     */
    int32_t i32ChkSum;
    int32_t i32ChkSumSignedAlt;
    if (rtZipTarCalcChkSum(pTar, &i32ChkSum, &i32ChkSumSignedAlt))
        return VERR_TAR_ZERO_HEADER;

    /*
     * Read the checksum field and match the checksums.
     */
    int64_t i64HdrChkSum;
    int rc = rtZipTarHdrFieldToNum(pTar->Posix.chksum, sizeof(pTar->Posix.chksum), true /*fOctalOnly*/, &i64HdrChkSum);
    if (RT_FAILURE(rc))
        return VERR_TAR_BAD_CHKSUM_FIELD;
    if (   i32ChkSum          != i64HdrChkSum
        && i32ChkSumSignedAlt != i64HdrChkSum) /** @todo test this */
        return VERR_TAR_CHKSUM_MISMATCH;

    /*
     * Detect the tar type.
     */
    RTZIPTARTYPE enmType;
    if (   pTar->Posix.magic[0] == 'u'
        && pTar->Posix.magic[1] == 's'
        && pTar->Posix.magic[2] == 't'
        && pTar->Posix.magic[3] == 'a'
        && pTar->Posix.magic[4] == 'r')
    {
        if (   pTar->Posix.magic[5]   == '\0'
            && pTar->Posix.version[0] == '0'
            && pTar->Posix.version[1] == '0')
            enmType = RTZIPTARTYPE_POSIX;
        else if (   pTar->Posix.magic[5]   == ' '
                && pTar->Posix.version[0] == ' '
                && pTar->Posix.version[1] == '\0')
            enmType = RTZIPTARTYPE_GNU;
        else
            return VERR_TAR_NOT_USTAR_V00;
    }
    else
        enmType = RTZIPTARTYPE_ANCIENT;
    *penmType = enmType;

    /*
     * Perform some basic checks.
     */
    /** @todo more/less? */
    switch (pTar->Posix.typeflag)
    {
        case RTZIPTAR_TF_OLDNORMAL:
        case RTZIPTAR_TF_NORMAL:
        case RTZIPTAR_TF_CONTIG:
        case RTZIPTAR_TF_LINK:
        case RTZIPTAR_TF_SYMLINK:
        case RTZIPTAR_TF_CHR:
        case RTZIPTAR_TF_BLK:
        case RTZIPTAR_TF_FIFO:
        {
            if (!pTar->Posix.name[0])
                return VERR_TAR_EMPTY_NAME;

            /** @todo People claim some (older and newer buggy) tar stores dirs as regular files with a trailing slash. */
            const char *pchEnd = RTStrEnd(&pTar->Posix.name[0], sizeof(pTar->Posix.name));
            pchEnd = pchEnd ? pchEnd - 1 : &pTar->Posix.name[sizeof(pTar->Posix.name) - 1];
            if (*pchEnd == '/')
                return VERR_TAR_NON_DIR_ENDS_WITH_SLASH;
            break;
        }

        case RTZIPTAR_TF_DIR:
            if (!pTar->Posix.name[0])
                return VERR_TAR_EMPTY_NAME;
            break;

        case RTZIPTAR_TF_X_HDR:
        case RTZIPTAR_TF_X_GLOBAL:
            return VERR_TAR_UNSUPPORTED_PAX_TYPE;

        case RTZIPTAR_TF_SOLARIS_XHDR:
            return VERR_TAR_UNSUPPORTED_SOLARIS_HDR_TYPE;

        case RTZIPTAR_TF_GNU_DUMPDIR:
        case RTZIPTAR_TF_GNU_LONGLINK:
        case RTZIPTAR_TF_GNU_LONGNAME:
        case RTZIPTAR_TF_GNU_MULTIVOL:
        case RTZIPTAR_TF_GNU_SPARSE:
        case RTZIPTAR_TF_GNU_VOLDHR:
            return VERR_TAR_UNSUPPORTED_GNU_HDR_TYPE;
    }


    return VINF_SUCCESS;
}


/**
 * Translate a TAR header to an IPRT object info structure with additional UNIX
 * attributes.
 *
 * This completes the validation done by rtZipTarHdrValidate.
 *
 * @returns VINF_SUCCESS if valid, appropriate VERR_TAR_XXX if not.
 * @param   pTar                The TAR header (input).
 * @param   pObjInfo            The object info structure (output).
 */
static int rtZipTarHdrToFsObjInfo(PCRTZIPTARHDR pTar, PRTFSOBJINFO pObjInfo)
{
    /*
     * Zap the whole structure, this takes care of unused space in the union.
     */
    RT_ZERO(*pObjInfo);

    /*
     * Convert the tar field in RTFSOBJINFO order.
     */
    int         rc;
    int64_t     i64Tmp;
#define GET_TAR_NUMERIC_FIELD_RET(a_Var, a_Field) \
        do { \
            rc = rtZipTarHdrFieldToNum(a_Field, sizeof(a_Field), false /*fOctalOnly*/, &i64Tmp); \
            if (RT_FAILURE(rc)) \
                return rc; \
            (a_Var) = i64Tmp; \
            if ((a_Var) != i64Tmp) \
                return VERR_TAR_NUM_VALUE_TOO_LARGE; \
        } while (0)

    GET_TAR_NUMERIC_FIELD_RET(pObjInfo->cbObject,        pTar->Posix.size);
    pObjInfo->cbAllocated = RT_ALIGN_64(pObjInfo->cbObject, 512);
    int64_t c64SecModTime;
    GET_TAR_NUMERIC_FIELD_RET(c64SecModTime,             pTar->Posix.mtime);
    RTTimeSpecSetSeconds(&pObjInfo->ChangeTime,           c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->ModificationTime,     c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->AccessTime,           c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->BirthTime,            c64SecModTime);
    if (c64SecModTime != RTTimeSpecGetSeconds(&pObjInfo->ModificationTime))
        return VERR_TAR_NUM_VALUE_TOO_LARGE;
    GET_TAR_NUMERIC_FIELD_RET(pObjInfo->Attr.fMode,      pTar->Posix.mode);
    pObjInfo->Attr.enmAdditional = RTFSOBJATTRADD_UNIX;
    GET_TAR_NUMERIC_FIELD_RET(pObjInfo->Attr.u.Unix.uid, pTar->Posix.uid);
    GET_TAR_NUMERIC_FIELD_RET(pObjInfo->Attr.u.Unix.gid, pTar->Posix.gid);
    pObjInfo->Attr.u.Unix.cHardlinks    = 1;
    pObjInfo->Attr.u.Unix.INodeIdDevice = 0;
    pObjInfo->Attr.u.Unix.INodeId       = 0;
    pObjInfo->Attr.u.Unix.fFlags        = 0;
    pObjInfo->Attr.u.Unix.GenerationId  = 0;
    pObjInfo->Attr.u.Unix.Device        = 0;
    if (   pTar->Posix.typeflag == RTZIPTAR_TF_CHR
        || pTar->Posix.typeflag == RTZIPTAR_TF_BLK)
    {
        uint32_t uMajor, uMinor;
        GET_TAR_NUMERIC_FIELD_RET(uMajor,               pTar->Posix.devmajor);
        GET_TAR_NUMERIC_FIELD_RET(uMinor,               pTar->Posix.devminor);
        pObjInfo->Attr.u.Unix.Device    = RTDEV_MAKE(uMajor, uMinor);
        if (   uMajor != RTDEV_MAJOR(pObjInfo->Attr.u.Unix.Device)
            || uMinor != RTDEV_MINOR(pObjInfo->Attr.u.Unix.Device))
            return VERR_TAR_DEV_VALUE_TOO_LARGE;
    }

#undef GET_TAR_NUMERIC_FIELD_RET

    /*
     * Massage the result a little bit.
     * Also validate some more now that we've got the numbers to work with.
     */
    if (pObjInfo->Attr.fMode & ~RTFS_UNIX_MASK)
        return VERR_TAR_BAD_MODE_FIELD;

    RTFMODE fModeType = 0;
    switch (pTar->Posix.typeflag)
    {
        case RTZIPTAR_TF_OLDNORMAL:
        case RTZIPTAR_TF_NORMAL:
        case RTZIPTAR_TF_CONTIG:
            fModeType |= RTFS_TYPE_FILE;
            break;

        case RTZIPTAR_TF_LINK:
            if (pObjInfo->cbObject != 0)
                return VERR_TAR_SIZE_NOT_ZERO;
            fModeType |= RTFS_TYPE_FILE; /* no better idea for now */
            break;

        case RTZIPTAR_TF_SYMLINK:
            fModeType |= RTFS_TYPE_SYMLINK;
            break;

        case RTZIPTAR_TF_CHR:
            fModeType |= RTFS_TYPE_DEV_CHAR;
            break;

        case RTZIPTAR_TF_BLK:
            fModeType |= RTFS_TYPE_DEV_BLOCK;
            break;

        case RTZIPTAR_TF_DIR:
            fModeType |= RTFS_TYPE_DIRECTORY;
            break;

        case RTZIPTAR_TF_FIFO:
            fModeType |= RTFS_TYPE_FIFO;
            break;

        default:
            return VERR_TAR_UNKNOWN_TYPE_FLAG; /* Should've been caught in validate. */
    }
    if (   (pObjInfo->Attr.fMode & RTFS_TYPE_MASK)
        && (pObjInfo->Attr.fMode & RTFS_TYPE_MASK) != fModeType)
        return VERR_TAR_MODE_WITH_TYPE;
    pObjInfo->Attr.fMode |= fModeType;

    switch (pTar->Posix.typeflag)
    {
        case RTZIPTAR_TF_CHR:
        case RTZIPTAR_TF_BLK:
        case RTZIPTAR_TF_DIR:
        case RTZIPTAR_TF_FIFO:
            pObjInfo->cbObject    = 0;
            pObjInfo->cbAllocated = 0;
            break;
    }

    return VINF_SUCCESS;
}


/*
 *
 * T h e   V F S   F i l e s y s t e m   S t r e a m   B i t s.
 * T h e   V F S   F i l e s y s t e m   S t r e a m   B i t s.
 * T h e   V F S   F i l e s y s t e m   S t r e a m   B i t s.
 *
 */

/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipTarFssBaseObj_Close(void *pvThis)
{
    PRTZIPTARBASEOBJ pThis = (PRTZIPTARBASEOBJ)pvThis;

    /* Currently there is nothing we really have to do here. */
    pThis->offHdr = -1;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipTarFssBaseObj_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPTARBASEOBJ pThis = (PRTZIPTARBASEOBJ)pvThis;

    /*
     * Copy the desired data.
     */
    switch (enmAddAttr)
    {
        case RTFSOBJATTRADD_NOTHING:
        case RTFSOBJATTRADD_UNIX:
            *pObjInfo = pThis->ObjInfo;
            break;

        case RTFSOBJATTRADD_UNIX_OWNER:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional         = RTFSOBJATTRADD_UNIX_OWNER;
            pObjInfo->Attr.u.UnixOwner.uid       = pThis->ObjInfo.Attr.u.Unix.uid;
            pObjInfo->Attr.u.UnixOwner.szName[0] = '\0';
            if (rtZipTarHdrHasPosixUserName(&pThis->Hdr))
                RTStrCopy(pObjInfo->Attr.u.UnixOwner.szName, sizeof(pObjInfo->Attr.u.UnixOwner.szName), pThis->Hdr.Posix.uname);
            break;

        case RTFSOBJATTRADD_UNIX_GROUP:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional         = RTFSOBJATTRADD_UNIX_GROUP;
            pObjInfo->Attr.u.UnixGroup.gid       = pThis->ObjInfo.Attr.u.Unix.gid;
            pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';
            if (rtZipTarHdrHasPosixGroupName(&pThis->Hdr))
                RTStrCopy(pObjInfo->Attr.u.UnixGroup.szName, sizeof(pObjInfo->Attr.u.UnixGroup.szName), pThis->Hdr.Posix.gname);
            break;

        case RTFSOBJATTRADD_EASIZE:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional = RTFSOBJATTRADD_EASIZE;
            RT_ZERO(pObjInfo->Attr.u);
            break;

        default:
            return VERR_NOT_SUPPORTED;
    }

    return VINF_SUCCESS;
}


/**
 * Tar filesystem base object operations.
 */
static const RTVFSOBJOPS g_rtZipTarFssBaseObjOps =
{
    RTVFSOBJOPS_VERSION,
    RTVFSOBJTYPE_BASE,
    "TarFsStream::Obj",
    rtZipTarFssBaseObj_Close,
    rtZipTarFssBaseObj_QueryInfo,
    RTVFSOBJOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipTarFssIos_Close(void *pvThis)
{
    PRTZIPTARIOSTREAM pThis = (PRTZIPTARIOSTREAM)pvThis;

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;

    return rtZipTarFssBaseObj_Close(&pThis->BaseObj);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipTarFssIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPTARIOSTREAM pThis = (PRTZIPTARIOSTREAM)pvThis;
    return rtZipTarFssBaseObj_QueryInfo(&pThis->BaseObj, pObjInfo, enmAddAttr);
}


/**
 * Reads one segment.
 *
 * @returns IPRT status code.
 * @param   pThis           The instance data.
 * @param   pvBuf           Where to put the read bytes.
 * @param   cbToRead        The number of bytes to read.
 * @param   fBlocking       Whether to block or not.
 * @param   pcbRead         Where to store the number of bytes actually read.
 */
static int rtZipTarFssIos_ReadOneSeg(PRTZIPTARIOSTREAM pThis, void *pvBuf, size_t cbToRead, bool fBlocking, size_t *pcbRead)
{
    /*
     * Fend of reads beyond the end of the stream here.
     */
    if (pThis->fEndOfStream)
        return pcbRead ? VINF_EOF : VERR_EOF;

    Assert(pThis->cbFile >= pThis->offFile);
    uint64_t cbLeft = (uint64_t)(pThis->cbFile - pThis->offFile);
    if (cbToRead > cbLeft)
    {
        if (!pcbRead)
            return VERR_EOF;
        cbToRead = (size_t)cbLeft;
    }

    /*
     * Do the reading.
     */
    size_t cbReadStack = 0;
    if (!pcbRead)
        pcbRead = &cbReadStack;
    int rc = RTVfsIoStrmRead(pThis->hVfsIos, pvBuf, cbToRead, fBlocking, pcbRead);
    pThis->offFile += *pcbRead;
    if (pThis->offFile >= pThis->cbFile)
    {
        Assert(pThis->offFile == pThis->cbFile);
        pThis->fEndOfStream = true;
        RTVfsIoStrmSkip(pThis->hVfsIos, pThis->cbPadding);
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtZipTarFssIos_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTZIPTARIOSTREAM pThis = (PRTZIPTARIOSTREAM)pvThis;
    int               rc;

    if (pSgBuf->cSegs == 1)
        rc = rtZipTarFssIos_ReadOneSeg(pThis, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, fBlocking, pcbRead);
    else
    {
        rc = VINF_SUCCESS;
        size_t  cbRead = 0;
        size_t  cbReadSeg;
        size_t *pcbReadSeg = pcbRead ? &cbReadSeg : NULL;
        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            cbReadSeg = 0;
            rc = rtZipTarFssIos_ReadOneSeg(pThis, pSgBuf->paSegs[iSeg].pvSeg, pSgBuf->paSegs[iSeg].cbSeg, fBlocking, pcbReadSeg);
            if (RT_FAILURE(rc))
                break;
            if (pcbRead)
            {
                cbRead += cbReadSeg;
                if (cbReadSeg != pSgBuf->paSegs[iSeg].cbSeg)
                    break;
            }
        }
        if (pcbRead)
            *pcbRead = cbRead;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnWrite}
 */
static DECLCALLBACK(int) rtZipTarFssIos_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    /* Cannot write to a read-only I/O stream. */
    NOREF(pvThis); NOREF(off); NOREF(pSgBuf); NOREF(fBlocking); NOREF(pcbWritten);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtZipTarFssIos_Flush(void *pvThis)
{
    /* It's a read only stream, nothing dirty to flush. */
    NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtZipTarFssIos_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                                uint32_t *pfRetEvents)
{
    PRTZIPTARIOSTREAM pThis = (PRTZIPTARIOSTREAM)pvThis;

    /* When we've reached the end, read will be set to indicate it. */
    if (   (fEvents & RTPOLL_EVT_READ)
        && pThis->fEndOfStream)
    {
        int rc = RTVfsIoStrmPoll(pThis->hVfsIos, fEvents, 0, fIntr, pfRetEvents);
        if (RT_SUCCESS(rc))
            *pfRetEvents |= RTPOLL_EVT_READ;
        else
            *pfRetEvents = RTPOLL_EVT_READ;
        return VINF_SUCCESS;
    }

    return RTVfsIoStrmPoll(pThis->hVfsIos, fEvents, cMillies, fIntr, pfRetEvents);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtZipTarFssIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTZIPTARIOSTREAM pThis = (PRTZIPTARIOSTREAM)pvThis;
    return pThis->offFile;
}


/**
 * Tar I/O stream operations.
 */
static const RTVFSIOSTREAMOPS g_rtZipTarFssIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "TarFsStream::IoStream",
        rtZipTarFssIos_Close,
        rtZipTarFssIos_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    0,
    rtZipTarFssIos_Read,
    rtZipTarFssIos_Write,
    rtZipTarFssIos_Flush,
    rtZipTarFssIos_PollOne,
    rtZipTarFssIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipTarFssSym_Close(void *pvThis)
{
    PRTZIPTARBASEOBJ pThis = (PRTZIPTARBASEOBJ)pvThis;
    return rtZipTarFssBaseObj_Close(pThis);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipTarFssSym_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPTARBASEOBJ pThis = (PRTZIPTARBASEOBJ)pvThis;
    return rtZipTarFssBaseObj_QueryInfo(pThis, pObjInfo, enmAddAttr);
}

/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtZipTarFssSym_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis); NOREF(fMode); NOREF(fMask);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtZipTarFssSym_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis); NOREF(pAccessTime); NOREF(pModificationTime); NOREF(pChangeTime); NOREF(pBirthTime);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtZipTarFssSym_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis); NOREF(uid); NOREF(gid);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSSYMLINKOPS,pfnRead}
 */
static DECLCALLBACK(int) rtZipTarFssSym_Read(void *pvThis, char *pszTarget, size_t cbTarget)
{
    PRTZIPTARBASEOBJ pThis = (PRTZIPTARBASEOBJ)pvThis;
    return RTStrCopy(pszTarget, cbTarget, pThis->Hdr.Posix.linkname);
}


/**
 * Tar symbolic (and hardlink) operations.
 */
static const RTVFSSYMLINKOPS g_rtZipTarFssSymOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_SYMLINK,
        "TarFsStream::Symlink",
        rtZipTarFssSym_Close,
        rtZipTarFssSym_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSSYMLINKOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSSYMLINKOPS, Obj) - RT_OFFSETOF(RTVFSSYMLINKOPS, ObjSet),
        rtZipTarFssSym_SetMode,
        rtZipTarFssSym_SetTimes,
        rtZipTarFssSym_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtZipTarFssSym_Read,
    RTVFSSYMLINKOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipTarFss_Close(void *pvThis)
{
    PRTZIPTARFSSTREAM pThis = (PRTZIPTARFSSTREAM)pvThis;

    RTVfsObjRelease(pThis->hVfsCurObj);
    pThis->hVfsCurObj  = NIL_RTVFSOBJ;
    pThis->pCurIosData = NULL;

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipTarFss_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPTARFSSTREAM pThis = (PRTZIPTARFSSTREAM)pvThis;
    /* Take the lazy approach here, with the sideffect of providing some info
       that is actually kind of useful. */
    return RTVfsIoStrmQueryInfo(pThis->hVfsIos, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSFSSTREAMOPS,pfnNext}
 */
static DECLCALLBACK(int) rtZipTarFss_Next(void *pvThis, char **ppszName, RTVFSOBJTYPE *penmType, PRTVFSOBJ phVfsObj)
{
    PRTZIPTARFSSTREAM pThis = (PRTZIPTARFSSTREAM)pvThis;

    /*
     * Dispense with the current object.
     */
    if (pThis->hVfsCurObj != NIL_RTVFSOBJ)
    {
        if (pThis->pCurIosData)
        {
            pThis->pCurIosData->fEndOfStream = true;
            pThis->pCurIosData->offFile      = pThis->pCurIosData->cbFile;
            pThis->pCurIosData = NULL;
        }

        RTVfsObjRelease(pThis->hVfsCurObj);
        pThis->hVfsCurObj = NIL_RTVFSOBJ;
    }

    /*
     * Check if we've already reached the end in some way.
     */
    if (pThis->fEndOfStream)
        return VERR_EOF;
    if (pThis->rcFatal != VINF_SUCCESS)
        return pThis->rcFatal;

    /*
     * Make sure the input stream is in the right place.
     */
    RTFOFF off = RTVfsIoStrmTell(pThis->hVfsIos);
    while (   off >= 0
           && off < pThis->offNextHdr)
    {
        int rc = RTVfsIoStrmSkip(pThis->hVfsIos, pThis->offNextHdr - off);
        if (RT_FAILURE(rc))
        {
            /** @todo Ignore if we're at the end of the stream? */
            return pThis->rcFatal = rc;
        }

        off = RTVfsIoStrmTell(pThis->hVfsIos);
    }

    if (off < 0)
        return pThis->rcFatal = (int)off;
    if (off > pThis->offNextHdr)
        return pThis->rcFatal = VERR_INTERNAL_ERROR_3;

    /*
     * Read the next header.
     */
    size_t      cbRead;
    RTZIPTARHDR Hdr;
    int rc = RTVfsIoStrmRead(pThis->hVfsIos, &Hdr, sizeof(Hdr), true /*fBlocking*/, &cbRead);
    if (RT_FAILURE(rc))
        return pThis->rcFatal = rc;
    if (rc == VINF_EOF && cbRead == 0)
    {
        pThis->fEndOfStream = true;
        return VERR_EOF;
    }
    if (cbRead != sizeof(Hdr))
        return pThis->rcFatal = VERR_TAR_UNEXPECTED_EOS;

    pThis->offNextHdr = off + sizeof(Hdr);

    /*
     * Validate the header and convert to binary object info.
     * We pick up the start of the zero headers here in the failure path.
     */
    RTZIPTARTYPE enmTarType;
    rc = rtZipTarHdrValidate(&Hdr, &enmTarType);
    if (RT_FAILURE_NP(rc))
    {
        if (rc == VERR_TAR_ZERO_HEADER)
        {
            int rc2 = RTVfsIoStrmRead(pThis->hVfsIos, &Hdr, sizeof(Hdr), true /*fBlocking*/, &cbRead);
            if (RT_FAILURE(rc2))
                return pThis->rcFatal = rc2;
            if (ASMMemIsAllU32(&Hdr, sizeof(Hdr), 0) == NULL)
            {
                pThis->fEndOfStream = true;
                if (RTVfsIoStrmIsAtEnd(pThis->hVfsIos))
                    return VERR_EOF;

                /* Just drain the stream because blocksize may dictate that
                   there is a whole bunch of stuff comming up. */
                for (uint32_t i = 0; i < _32K / 512; i++)
                {
                    rc = RTVfsIoStrmRead(pThis->hVfsIos, &Hdr, sizeof(Hdr), true /*fBlocking*/, &cbRead);
                    if (rc == VINF_EOF)
                        return VERR_EOF;
                    if (RT_FAILURE(rc))
                        break;
                    Assert(cbRead == sizeof(Hdr));
                }
            }
        }

        return pThis->rcFatal = rc;
    }

    RTFSOBJINFO Info;
    rc = rtZipTarHdrToFsObjInfo(&Hdr, &Info);
    if (RT_FAILURE(rc))
        return pThis->rcFatal = rc;

    /*
     * Create an object of the appropriate type.
     */
    RTVFSOBJTYPE    enmType;
    RTVFSOBJ        hVfsObj;
    switch (Hdr.Posix.typeflag)
    {
        /*
         * Files are represented by a VFS I/O stream.
         */
        case RTZIPTAR_TF_NORMAL:
        case RTZIPTAR_TF_OLDNORMAL:
        case RTZIPTAR_TF_CONTIG:
        {
            RTVFSIOSTREAM       hVfsIos;
            PRTZIPTARIOSTREAM   pIosData;
            rc = RTVfsNewIoStream(&g_rtZipTarFssIosOps,
                                  sizeof(*pIosData),
                                  RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                  NIL_RTVFS,
                                  NIL_RTVFSLOCK,
                                  &hVfsIos,
                                  (void **)&pIosData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pIosData->BaseObj.offHdr  = off;
            pIosData->BaseObj.Hdr     = Hdr;
            pIosData->BaseObj.ObjInfo = Info;
            pIosData->cbFile          = Info.cbObject;
            pIosData->offFile         = 0;
            pIosData->cbPadding       = Info.cbAllocated - Info.cbObject;
            pIosData->fEndOfStream    = false;
            pIosData->hVfsIos         = pThis->hVfsIos;
            RTVfsIoStrmRetain(pThis->hVfsIos);

            pThis->pCurIosData = pIosData;
            pThis->offNextHdr += pIosData->cbFile + pIosData->cbPadding;

            enmType = RTVFSOBJTYPE_IO_STREAM;
            hVfsObj = RTVfsObjFromIoStream(hVfsIos);
            RTVfsIoStrmRelease(hVfsIos);
            break;
        }

        /*
         * We represent hard links using a symbolic link object.  This fits
         * best with the way TAR stores it and there is currently no better
         * fitting VFS type alternative.
         */
        case RTZIPTAR_TF_LINK:
        case RTZIPTAR_TF_SYMLINK:
        {
            RTVFSSYMLINK        hVfsSym;
            PRTZIPTARBASEOBJ    pBaseObjData;
            rc = RTVfsNewSymlink(&g_rtZipTarFssSymOps,
                                 sizeof(*pBaseObjData),
                                 NIL_RTVFS,
                                 NIL_RTVFSLOCK,
                                 &hVfsSym,
                                 (void **)&pBaseObjData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pBaseObjData->offHdr  = off;
            pBaseObjData->Hdr     = Hdr;
            pBaseObjData->ObjInfo = Info;

            enmType = RTVFSOBJTYPE_SYMLINK;
            hVfsObj = RTVfsObjFromSymlink(hVfsSym);
            RTVfsSymlinkRelease(hVfsSym);
            break;
        }

        /*
         * All other objects are repesented using a VFS base object since they
         * carry no data streams (unless some tar extension implements extended
         * attributes / alternative streams).
         */
        case RTZIPTAR_TF_CHR:
        case RTZIPTAR_TF_BLK:
        case RTZIPTAR_TF_DIR:
        case RTZIPTAR_TF_FIFO:
        {
            PRTZIPTARBASEOBJ pBaseObjData;
            rc = RTVfsNewBaseObj(&g_rtZipTarFssBaseObjOps,
                                 sizeof(*pBaseObjData),
                                 NIL_RTVFS,
                                 NIL_RTVFSLOCK,
                                 &hVfsObj,
                                 (void **)&pBaseObjData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pBaseObjData->offHdr  = off;
            pBaseObjData->Hdr     = Hdr;
            pBaseObjData->ObjInfo = Info;

            enmType = RTVFSOBJTYPE_BASE;
            break;
        }

        default:
            AssertFailed();
            return pThis->rcFatal = VERR_INTERNAL_ERROR_5;
    }
    pThis->hVfsCurObj = hVfsObj;

    /*
     * Set the return data and we're done.
     */
    if (ppszName)
    {
        if (rtZipTarHdrHasPrefix(&Hdr))
        {
            *ppszName = NULL;
            rc = RTStrAAppendExN(ppszName, 3,
                                 Hdr.Posix.prefix, sizeof(Hdr.Posix.prefix),
                                 "/", 1,
                                 Hdr.Posix.name, sizeof(Hdr.Posix.name));
        }
        else
        {
            *ppszName = RTStrDupN(Hdr.Posix.name, sizeof(Hdr.Posix.name));
            rc = *ppszName ? VINF_SUCCESS : VERR_NO_STR_MEMORY;
        }
        if (RT_FAILURE(rc))
            return rc;
    }

    if (phVfsObj)
    {
        RTVfsObjRetain(hVfsObj);
        *phVfsObj = hVfsObj;
    }

    if (penmType)
        *penmType = enmType;

    return VINF_SUCCESS;
}



/**
 * Tar filesystem stream operations.
 */
static const RTVFSFSSTREAMOPS rtZipTarFssOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_FS_STREAM,
        "TarFsStream",
        rtZipTarFss_Close,
        rtZipTarFss_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSFSSTREAMOPS_VERSION,
    0,
    rtZipTarFss_Next,
    RTVFSFSSTREAMOPS_VERSION
};


RTDECL(int) RTZipTarFsStreamFromIoStream(RTVFSIOSTREAM hVfsIosIn, uint32_t fFlags, PRTVFSFSSTREAM phVfsFss)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(phVfsFss, VERR_INVALID_HANDLE);
    *phVfsFss = NIL_RTVFSFSSTREAM;
    AssertPtrReturn(hVfsIosIn, VERR_INVALID_HANDLE);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    RTFOFF const offStart = RTVfsIoStrmTell(hVfsIosIn);
    AssertReturn(offStart >= 0, (int)offStart);

    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Retain the input stream and create a new filesystem stream handle.
     */
    PRTZIPTARFSSTREAM pThis;
    RTVFSFSSTREAM     hVfsFss;
    int rc = RTVfsNewFsStream(&rtZipTarFssOps, sizeof(*pThis), NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFss, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsIos          = hVfsIosIn;
        pThis->hVfsCurObj       = NIL_RTVFSOBJ;
        pThis->pCurIosData      = NULL;
        pThis->offStart         = offStart;
        pThis->offNextHdr       = offStart;
        pThis->fEndOfStream     = false;
        pThis->rcFatal          = VINF_SUCCESS;

        /* Don't check if it's a TAR stream here, do that in the
           rtZipTarFss_Next. */

        *phVfsFss = hVfsFss;
        return VINF_SUCCESS;
    }

    RTVfsIoStrmRelease(hVfsIosIn);
    return rc;
}

