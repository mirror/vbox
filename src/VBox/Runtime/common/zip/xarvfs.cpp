/* $Id$ */
/** @file
 * IPRT - XAR Virtual Filesystem.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
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
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/md5.h>
#include <iprt/poll.h>
#include <iprt/file.h>
#include <iprt/sha.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>
#include <iprt/formats/xar.h>
#include <iprt/cpp/xml.h>



/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Hash digest value union for the supported XAR hash functions.
 */
typedef union RTZIPXARHASHDIGEST
{
    uint8_t abMd5[RTMD5_HASH_SIZE];
    uint8_t abSha1[RTSHA1_HASH_SIZE];
} RTZIPXARHASHDIGEST;
/** Pointer to a XAR hash digest union. */
typedef RTZIPXARHASHDIGEST *PRTZIPXARHASHDIGEST;
/** Pointer to a const XAR hash digest union. */
typedef RTZIPXARHASHDIGEST *PCRTZIPXARHASHDIGEST;

/**
 * XAR reader instance data.
 */
typedef struct RTZIPXARREADER
{
    /** The TOC XML element. */
    xml::ElementNode const *pToc;
    /** The TOC XML document. */
    xml::Document          *pDoc;
    /** The current file ID. */
    uint32_t            idCurFile;
} RTZIPXARREADER;
/** Pointer to the XAR reader instance data. */
typedef RTZIPXARREADER *PRTZIPXARREADER;

/**
 * Xar directory, character device, block device, fifo socket or symbolic link.
 */
typedef struct RTZIPXARBASEOBJ
{
    /** The stream offset of the (first) header.  */
    RTFOFF                  offHdr;
    /** Pointer to the reader instance data (resides in the filesystem
     * stream).
     * @todo Fix this so it won't go stale... Back ref from this obj to fss? */
    PRTZIPXARREADER         pXarReader;
    /** The object info with unix attributes. */
    RTFSOBJINFO             ObjInfo;
} RTZIPXARBASEOBJ;
/** Pointer to a XAR filesystem stream base object. */
typedef RTZIPXARBASEOBJ *PRTZIPXARBASEOBJ;


/**
 * Xar file represented as a VFS I/O stream.
 */
typedef struct RTZIPXARIOSTREAM
{
    /** The basic XAR object data. */
    RTZIPXARBASEOBJ         BaseObj;
    /** The number of bytes in the file. */
    RTFOFF                  cbFile;
    /** The current file position. */
    RTFOFF                  offFile;
    /** The number of padding bytes following the file. */
    uint32_t                cbPadding;
    /** Set if we've reached the end of the file. */
    bool                    fEndOfStream;
    /** The input I/O stream. */
    RTVFSIOSTREAM           hVfsIos;
} RTZIPXARIOSTREAM;
/** Pointer to a the private data of a XAR file I/O stream. */
typedef RTZIPXARIOSTREAM *PRTZIPXARIOSTREAM;


/**
 * Xar filesystem stream private data.
 */
typedef struct RTZIPXARFSSTREAM
{
    /** The input I/O stream. */
    RTVFSIOSTREAM           hVfsIos;
    /** The input file, if the stream is actually a file. */
    RTVFSFILE               hVfsFile;

    /** The current object (referenced). */
    RTVFSOBJ                hVfsCurObj;
    /** Pointer to the private data if hVfsCurObj is representing a file. */
    PRTZIPXARIOSTREAM       pCurIosData;

    /** The start offset in the input I/O stream. */
    RTFOFF                  offStart;
    /** The zero offset in the file which all others are relative to. */
    RTFOFF                  offZero;

    /** The hash function we're using (XAR_HASH_XXX). */
    uint8_t                 uHashFunction;
    /** The size of the digest produced by the hash function we're using. */
    uint8_t                 cbHashDigest;

    /** Set if we've reached the end of the stream. */
    bool                    fEndOfStream;
    /** Set if we've encountered a fatal error. */
    int                     rcFatal;


    /** The XAR reader instance data. */
    RTZIPXARREADER          XarReader;
} RTZIPXARFSSTREAM;
/** Pointer to a the private data of a XAR filesystem stream. */
typedef RTZIPXARFSSTREAM *PRTZIPXARFSSTREAM;


/**
 * Hashes a block of data.
 *
 * @param   uHashFunction   The hash function to use.
 * @param   pvSrc           The data to hash.
 * @param   cbSrc           The size of the data to hash.
 * @param   pHashDigest     Where to return the hash digest.
 */
static void rtZipXarCalcHash(uint32_t uHashFunction, void const *pvSrc, size_t cbSrc, PRTZIPXARHASHDIGEST pHashDigest)
{
    switch (uHashFunction)
    {
        case XAR_HASH_SHA1:
            RTSha1(pvSrc, cbSrc, pHashDigest->abSha1);
            break;
        case XAR_HASH_MD5:
            RTMd5(pvSrc, cbSrc, pHashDigest->abMd5);
            break;
        default:
            RT_ZERO(*pHashDigest);
            break;
    }
}


static int rtZipXarGetOffsetSizeFromElem(xml::ElementNode const *pElement, PRTFOFF poff, uint64_t *pcb)
{
    /*
     * The offset.
     */
    xml::ElementNode const *pElem = pElement->findChildElement("offset");
    if (!pElem)
        return VERR_XAR_MISSING_OFFSET_ELEMENT;
    const char *pszValue = pElem->getValue();
    if (!pszValue)
        return VERR_XAR_BAD_OFFSET_ELEMENT;

    int rc = RTStrToInt64Full(pszValue, 0, poff);
    if (   RT_FAILURE(rc)
        || rc == VWRN_NUMBER_TOO_BIG
        || *poff > RTFOFF_MAX / 2 /* make sure to not overflow calculating offsets. */
        || *poff < 0)
        return VERR_XAR_BAD_OFFSET_ELEMENT;

    /*
     * The size.
     */
    pElem = pElement->findChildElement("size");
    if (!pElem)
        return VERR_XAR_MISSING_SIZE_ELEMENT;

    pszValue = pElem->getValue();
    if (!pszValue)
        return VERR_XAR_BAD_SIZE_ELEMENT;

    rc = RTStrToUInt64Full(pszValue, 0, pcb);
    if (   RT_FAILURE(rc)
        || rc == VWRN_NUMBER_TOO_BIG
        || *pcb > UINT64_MAX / 2 /* prevent overflow should be use it for calcuations later. */)
        return VERR_XAR_BAD_SIZE_ELEMENT;

    return VINF_SUCCESS;
}


/**
 * Translate a XAR header to an IPRT object info structure with additional UNIX
 * attributes.
 *
 * This completes the validation done by rtZipXarHdrValidate.
 *
 * @returns VINF_SUCCESS if valid, appropriate VERR_XAR_XXX if not.
 * @param   pThis               The XAR reader instance.
 * @param   pObjInfo            The object info structure (output).
 */
static int rtZipXarReaderGetFsObjInfo(PRTZIPXARREADER pThis, PRTFSOBJINFO pObjInfo)
{
    /*
     * Zap the whole structure, this takes care of unused space in the union.
     */
    RT_ZERO(*pObjInfo);

#if 0
    /*
     * Convert the XAR field in RTFSOBJINFO order.
     */
    int         rc;
    int64_t     i64Tmp;
#define GET_XAR_NUMERIC_FIELD_RET(a_Var, a_Field) \
        do { \
            rc = rtZipXarHdrFieldToNum(a_Field, sizeof(a_Field), false /*fOctalOnly*/, &i64Tmp); \
            if (RT_FAILURE(rc)) \
                return rc; \
            (a_Var) = i64Tmp; \
            if ((a_Var) != i64Tmp) \
                return VERR_XAR_NUM_VALUE_TOO_LARGE; \
        } while (0)

    GET_XAR_NUMERIC_FIELD_RET(pObjInfo->cbObject,        pThis->Hdr.Common.size);
    pObjInfo->cbAllocated = RT_ALIGN_64(pObjInfo->cbObject, 512);
    int64_t c64SecModTime;
    GET_XAR_NUMERIC_FIELD_RET(c64SecModTime,             pThis->Hdr.Common.mtime);
    RTTimeSpecSetSeconds(&pObjInfo->ChangeTime,          c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->ModificationTime,    c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->AccessTime,          c64SecModTime);
    RTTimeSpecSetSeconds(&pObjInfo->BirthTime,           c64SecModTime);
    if (c64SecModTime != RTTimeSpecGetSeconds(&pObjInfo->ModificationTime))
        return VERR_XAR_NUM_VALUE_TOO_LARGE;
    GET_XAR_NUMERIC_FIELD_RET(pObjInfo->Attr.fMode,      pThis->Hdr.Common.mode);
    pObjInfo->Attr.enmAdditional = RTFSOBJATTRADD_UNIX;
    GET_XAR_NUMERIC_FIELD_RET(pObjInfo->Attr.u.Unix.uid, pThis->Hdr.Common.uid);
    GET_XAR_NUMERIC_FIELD_RET(pObjInfo->Attr.u.Unix.gid, pThis->Hdr.Common.gid);
    pObjInfo->Attr.u.Unix.cHardlinks    = 1;
    pObjInfo->Attr.u.Unix.INodeIdDevice = 0;
    pObjInfo->Attr.u.Unix.INodeId       = 0;
    pObjInfo->Attr.u.Unix.fFlags        = 0;
    pObjInfo->Attr.u.Unix.GenerationId  = 0;
    pObjInfo->Attr.u.Unix.Device        = 0;
    switch (pThis->enmType)
    {
        case RTZIPXARTYPE_POSIX:
        case RTZIPXARTYPE_GNU:
            if (   pThis->Hdr.Common.typeflag == RTZIPXAR_TF_CHR
                || pThis->Hdr.Common.typeflag == RTZIPXAR_TF_BLK)
            {
                uint32_t uMajor, uMinor;
                GET_XAR_NUMERIC_FIELD_RET(uMajor,        pThis->Hdr.Common.devmajor);
                GET_XAR_NUMERIC_FIELD_RET(uMinor,        pThis->Hdr.Common.devminor);
                pObjInfo->Attr.u.Unix.Device    = RTDEV_MAKE(uMajor, uMinor);
                if (   uMajor != RTDEV_MAJOR(pObjInfo->Attr.u.Unix.Device)
                    || uMinor != RTDEV_MINOR(pObjInfo->Attr.u.Unix.Device))
                    return VERR_XAR_DEV_VALUE_TOO_LARGE;
            }
            break;

        default:
            if (   pThis->Hdr.Common.typeflag == RTZIPXAR_TF_CHR
                || pThis->Hdr.Common.typeflag == RTZIPXAR_TF_BLK)
                return VERR_XAR_UNKNOWN_TYPE_FLAG;
    }

#undef GET_XAR_NUMERIC_FIELD_RET

    /*
     * Massage the result a little bit.
     * Also validate some more now that we've got the numbers to work with.
     */
    if (   (pObjInfo->Attr.fMode & ~RTFS_UNIX_MASK)
        && pThis->enmType == RTZIPXARTYPE_POSIX)
        return VERR_XAR_BAD_MODE_FIELD;
    pObjInfo->Attr.fMode &= RTFS_UNIX_MASK;

    RTFMODE fModeType = 0;
    switch (pThis->Hdr.Common.typeflag)
    {
        case RTZIPXAR_TF_OLDNORMAL:
        case RTZIPXAR_TF_NORMAL:
        case RTZIPXAR_TF_CONTIG:
        {
            const char *pszEnd = strchr(pThis->szName, '\0');
            if (pszEnd == &pThis->szName[0] || pszEnd[-1] != '/')
                fModeType |= RTFS_TYPE_FILE;
            else
                fModeType |= RTFS_TYPE_DIRECTORY;
            break;
        }

        case RTZIPXAR_TF_LINK:
            if (pObjInfo->cbObject != 0)
#if 0 /* too strict */
                return VERR_XAR_SIZE_NOT_ZERO;
#else
                pObjInfo->cbObject = pObjInfo->cbAllocated = 0;
#endif
            fModeType |= RTFS_TYPE_FILE; /* no better idea for now */
            break;

        case RTZIPXAR_TF_SYMLINK:
            fModeType |= RTFS_TYPE_SYMLINK;
            break;

        case RTZIPXAR_TF_CHR:
            fModeType |= RTFS_TYPE_DEV_CHAR;
            break;

        case RTZIPXAR_TF_BLK:
            fModeType |= RTFS_TYPE_DEV_BLOCK;
            break;

        case RTZIPXAR_TF_DIR:
            fModeType |= RTFS_TYPE_DIRECTORY;
            break;

        case RTZIPXAR_TF_FIFO:
            fModeType |= RTFS_TYPE_FIFO;
            break;

        case RTZIPXAR_TF_GNU_LONGLINK:
        case RTZIPXAR_TF_GNU_LONGNAME:
            /* ASSUMES RTFS_TYPE_XXX uses the same values as GNU stored in the mode field. */
            fModeType = pObjInfo->Attr.fMode & RTFS_TYPE_MASK;
            switch (fModeType)
            {
                case RTFS_TYPE_FILE:
                case RTFS_TYPE_DIRECTORY:
                case RTFS_TYPE_SYMLINK:
                case RTFS_TYPE_DEV_BLOCK:
                case RTFS_TYPE_DEV_CHAR:
                case RTFS_TYPE_FIFO:
                    break;

                default:
                case 0:
                    return VERR_XAR_UNKNOWN_TYPE_FLAG; /** @todo new status code */
            }

        default:
            return VERR_XAR_UNKNOWN_TYPE_FLAG; /* Should've been caught in validate. */
    }
    if (   (pObjInfo->Attr.fMode & RTFS_TYPE_MASK)
        && (pObjInfo->Attr.fMode & RTFS_TYPE_MASK) != fModeType)
        return VERR_XAR_MODE_WITH_TYPE;
    pObjInfo->Attr.fMode &= ~RTFS_TYPE_MASK;
    pObjInfo->Attr.fMode |= fModeType;

    switch (pThis->Hdr.Common.typeflag)
    {
        case RTZIPXAR_TF_CHR:
        case RTZIPXAR_TF_BLK:
        case RTZIPXAR_TF_DIR:
        case RTZIPXAR_TF_FIFO:
            pObjInfo->cbObject    = 0;
            pObjInfo->cbAllocated = 0;
            break;
    }
#endif

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
static DECLCALLBACK(int) rtZipXarFssBaseObj_Close(void *pvThis)
{
    PRTZIPXARBASEOBJ pThis = (PRTZIPXARBASEOBJ)pvThis;

    /* Currently there is nothing we really have to do here. */
    pThis->offHdr = -1;

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipXarFssBaseObj_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPXARBASEOBJ pThis = (PRTZIPXARBASEOBJ)pvThis;

#if 0
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
            if (rtZipXarReaderHasUserName(pThis->pXarReader))
                RTStrCopy(pObjInfo->Attr.u.UnixOwner.szName, sizeof(pObjInfo->Attr.u.UnixOwner.szName),
                          pThis->pXarReader->Hdr.Common.uname);
            break;

        case RTFSOBJATTRADD_UNIX_GROUP:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional         = RTFSOBJATTRADD_UNIX_GROUP;
            pObjInfo->Attr.u.UnixGroup.gid       = pThis->ObjInfo.Attr.u.Unix.gid;
            pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';
            if (rtZipXarReaderHasGroupName(pThis->pXarReader))
                RTStrCopy(pObjInfo->Attr.u.UnixGroup.szName, sizeof(pObjInfo->Attr.u.UnixGroup.szName),
                          pThis->pXarReader->Hdr.Common.gname);
            break;

        case RTFSOBJATTRADD_EASIZE:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional = RTFSOBJATTRADD_EASIZE;
            RT_ZERO(pObjInfo->Attr.u);
            break;

        default:
            return VERR_NOT_SUPPORTED;
    }
#endif

    return VINF_SUCCESS;
}


/**
 * Xar filesystem base object operations.
 */
static const RTVFSOBJOPS g_rtZipXarFssBaseObjOps =
{
    RTVFSOBJOPS_VERSION,
    RTVFSOBJTYPE_BASE,
    "XarFsStream::Obj",
    rtZipXarFssBaseObj_Close,
    rtZipXarFssBaseObj_QueryInfo,
    RTVFSOBJOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipXarFssIos_Close(void *pvThis)
{
    PRTZIPXARIOSTREAM pThis = (PRTZIPXARIOSTREAM)pvThis;

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;

    return rtZipXarFssBaseObj_Close(&pThis->BaseObj);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipXarFssIos_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPXARIOSTREAM pThis = (PRTZIPXARIOSTREAM)pvThis;
    return rtZipXarFssBaseObj_QueryInfo(&pThis->BaseObj, pObjInfo, enmAddAttr);
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
static int rtZipXarFssIos_ReadOneSeg(PRTZIPXARIOSTREAM pThis, void *pvBuf, size_t cbToRead, bool fBlocking, size_t *pcbRead)
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
static DECLCALLBACK(int) rtZipXarFssIos_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTZIPXARIOSTREAM pThis = (PRTZIPXARIOSTREAM)pvThis;
    int               rc;
    AssertReturn(off == -1, VERR_INVALID_PARAMETER);

    if (pSgBuf->cSegs == 1)
        rc = rtZipXarFssIos_ReadOneSeg(pThis, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, fBlocking, pcbRead);
    else
    {
        rc = VINF_SUCCESS;
        size_t  cbRead = 0;
        size_t  cbReadSeg;
        size_t *pcbReadSeg = pcbRead ? &cbReadSeg : NULL;
        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            cbReadSeg = 0;
            rc = rtZipXarFssIos_ReadOneSeg(pThis, pSgBuf->paSegs[iSeg].pvSeg, pSgBuf->paSegs[iSeg].cbSeg, fBlocking, pcbReadSeg);
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
static DECLCALLBACK(int) rtZipXarFssIos_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    /* Cannot write to a read-only I/O stream. */
    NOREF(pvThis); NOREF(off); NOREF(pSgBuf); NOREF(fBlocking); NOREF(pcbWritten);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtZipXarFssIos_Flush(void *pvThis)
{
    /* It's a read only stream, nothing dirty to flush. */
    NOREF(pvThis);
    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtZipXarFssIos_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                                uint32_t *pfRetEvents)
{
    PRTZIPXARIOSTREAM pThis = (PRTZIPXARIOSTREAM)pvThis;

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
static DECLCALLBACK(int) rtZipXarFssIos_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTZIPXARIOSTREAM pThis = (PRTZIPXARIOSTREAM)pvThis;
    *poffActual = pThis->offFile;
    return VINF_SUCCESS;
}


/**
 * Xar I/O stream operations.
 */
static const RTVFSIOSTREAMOPS g_rtZipXarFssIosOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "XarFsStream::IoStream",
        rtZipXarFssIos_Close,
        rtZipXarFssIos_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    0,
    rtZipXarFssIos_Read,
    rtZipXarFssIos_Write,
    rtZipXarFssIos_Flush,
    rtZipXarFssIos_PollOne,
    rtZipXarFssIos_Tell,
    NULL /*Skip*/,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipXarFssSym_Close(void *pvThis)
{
    PRTZIPXARBASEOBJ pThis = (PRTZIPXARBASEOBJ)pvThis;
    return rtZipXarFssBaseObj_Close(pThis);
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipXarFssSym_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPXARBASEOBJ pThis = (PRTZIPXARBASEOBJ)pvThis;
    return rtZipXarFssBaseObj_QueryInfo(pThis, pObjInfo, enmAddAttr);
}

/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtZipXarFssSym_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    NOREF(pvThis); NOREF(fMode); NOREF(fMask);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtZipXarFssSym_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                                 PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    NOREF(pvThis); NOREF(pAccessTime); NOREF(pModificationTime); NOREF(pChangeTime); NOREF(pBirthTime);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtZipXarFssSym_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    NOREF(pvThis); NOREF(uid); NOREF(gid);
    return VERR_ACCESS_DENIED;
}


/**
 * @interface_method_impl{RTVFSSYMLINKOPS,pfnRead}
 */
static DECLCALLBACK(int) rtZipXarFssSym_Read(void *pvThis, char *pszTarget, size_t cbXarget)
{
    PRTZIPXARBASEOBJ pThis = (PRTZIPXARBASEOBJ)pvThis;
#if 0
    return RTStrCopy(pszTarget, cbXarget, pThis->pXarReader->szTarget);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * Xar symbolic (and hardlink) operations.
 */
static const RTVFSSYMLINKOPS g_rtZipXarFssSymOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_SYMLINK,
        "XarFsStream::Symlink",
        rtZipXarFssSym_Close,
        rtZipXarFssSym_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSSYMLINKOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSSYMLINKOPS, Obj) - RT_OFFSETOF(RTVFSSYMLINKOPS, ObjSet),
        rtZipXarFssSym_SetMode,
        rtZipXarFssSym_SetTimes,
        rtZipXarFssSym_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtZipXarFssSym_Read,
    RTVFSSYMLINKOPS_VERSION
};


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipXarFss_Close(void *pvThis)
{
    PRTZIPXARFSSTREAM pThis = (PRTZIPXARFSSTREAM)pvThis;

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
static DECLCALLBACK(int) rtZipXarFss_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPXARFSSTREAM pThis = (PRTZIPXARFSSTREAM)pvThis;
    /* Take the lazy approach here, with the sideffect of providing some info
       that is actually kind of useful. */
    return RTVfsIoStrmQueryInfo(pThis->hVfsIos, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSFSSTREAMOPS,pfnNext}
 */
static DECLCALLBACK(int) rtZipXarFss_Next(void *pvThis, char **ppszName, RTVFSOBJTYPE *penmType, PRTVFSOBJ phVfsObj)
{
    PRTZIPXARFSSTREAM pThis = (PRTZIPXARFSSTREAM)pvThis;

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

#if 0
    /*
     * Make sure the input stream is in the right place.
     */
    RTFOFF offHdr = RTVfsIoStrmTell(pThis->hVfsIos);
    while (   offHdr >= 0
           && offHdr < pThis->offNextHdr)
    {
        int rc = RTVfsIoStrmSkip(pThis->hVfsIos, pThis->offNextHdr - offHdr);
        if (RT_FAILURE(rc))
        {
            /** @todo Ignore if we're at the end of the stream? */
            return pThis->rcFatal = rc;
        }

        offHdr = RTVfsIoStrmTell(pThis->hVfsIos);
    }

    if (offHdr < 0)
        return pThis->rcFatal = (int)offHdr;
    if (offHdr > pThis->offNextHdr)
        return pThis->rcFatal = VERR_INTERNAL_ERROR_3;

    /*
     * Consume XAR headers.
     */
    size_t cbHdrs = 0;
    int rc;
    do
    {
        /*
         * Read the next header.
         */
        RTZIPXARHDR Hdr;
        size_t cbRead;
        rc = RTVfsIoStrmRead(pThis->hVfsIos, &Hdr, sizeof(Hdr), true /*fBlocking*/, &cbRead);
        if (RT_FAILURE(rc))
            return pThis->rcFatal = rc;
        if (rc == VINF_EOF && cbRead == 0)
        {
            pThis->fEndOfStream = true;
            return rtZipXarReaderIsAtEnd(&pThis->XarReader) ? VERR_EOF : VERR_XAR_UNEXPECTED_EOS;
        }
        if (cbRead != sizeof(Hdr))
            return pThis->rcFatal = VERR_XAR_UNEXPECTED_EOS;

        cbHdrs += sizeof(Hdr);

        /*
         * Parse the it.
         */
        rc = rtZipXarReaderParseHeader(&pThis->XarReader, &Hdr);
        if (RT_FAILURE(rc))
            return pThis->rcFatal = rc;
    } while (rtZipXarReaderExpectingMoreHeaders(&pThis->XarReader));

    pThis->offNextHdr = offHdr + cbHdrs;

    /*
     * Fill an object info structure from the current XAR state.
     */
    RTFSOBJINFO Info;
    rc = rtZipXarReaderGetFsObjInfo(&pThis->XarReader, &Info);
    if (RT_FAILURE(rc))
        return pThis->rcFatal = rc;

    /*
     * Create an object of the appropriate type.
     */
    RTVFSOBJTYPE    enmType;
    RTVFSOBJ        hVfsObj;
    RTFMODE         fType = Info.Attr.fMode & RTFS_TYPE_MASK;
    if (rtZipXarReaderIsHardlink(&pThis->XarReader))
        fType = RTFS_TYPE_SYMLINK;
    switch (fType)
    {
        /*
         * Files are represented by a VFS I/O stream.
         */
        case RTFS_TYPE_FILE:
        {
            RTVFSIOSTREAM       hVfsIos;
            PRTZIPXARIOSTREAM   pIosData;
            rc = RTVfsNewIoStream(&g_rtZipXarFssIosOps,
                                  sizeof(*pIosData),
                                  RTFILE_O_READ | RTFILE_O_DENY_NONE | RTFILE_O_OPEN,
                                  NIL_RTVFS,
                                  NIL_RTVFSLOCK,
                                  &hVfsIos,
                                  (void **)&pIosData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pIosData->BaseObj.offHdr    = offHdr;
            pIosData->BaseObj.pXarReader= &pThis->XarReader;
            pIosData->BaseObj.ObjInfo   = Info;
            pIosData->cbFile            = Info.cbObject;
            pIosData->offFile           = 0;
            pIosData->cbPadding         = (uint32_t)(Info.cbAllocated - Info.cbObject);
            pIosData->fEndOfStream      = false;
            pIosData->hVfsIos           = pThis->hVfsIos;
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
         * best with the way XAR stores it and there is currently no better
         * fitting VFS type alternative.
         */
        case RTFS_TYPE_SYMLINK:
        {
            RTVFSSYMLINK        hVfsSym;
            PRTZIPXARBASEOBJ    pBaseObjData;
            rc = RTVfsNewSymlink(&g_rtZipXarFssSymOps,
                                 sizeof(*pBaseObjData),
                                 NIL_RTVFS,
                                 NIL_RTVFSLOCK,
                                 &hVfsSym,
                                 (void **)&pBaseObjData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pBaseObjData->offHdr    = offHdr;
            pBaseObjData->pXarReader= &pThis->XarReader;
            pBaseObjData->ObjInfo   = Info;

            enmType = RTVFSOBJTYPE_SYMLINK;
            hVfsObj = RTVfsObjFromSymlink(hVfsSym);
            RTVfsSymlinkRelease(hVfsSym);
            break;
        }

        /*
         * All other objects are repesented using a VFS base object since they
         * carry no data streams (unless some XAR extension implements extended
         * attributes / alternative streams).
         */
        case RTFS_TYPE_DEV_BLOCK:
        case RTFS_TYPE_DEV_CHAR:
        case RTFS_TYPE_DIRECTORY:
        case RTFS_TYPE_FIFO:
        {
            PRTZIPXARBASEOBJ pBaseObjData;
            rc = RTVfsNewBaseObj(&g_rtZipXarFssBaseObjOps,
                                 sizeof(*pBaseObjData),
                                 NIL_RTVFS,
                                 NIL_RTVFSLOCK,
                                 &hVfsObj,
                                 (void **)&pBaseObjData);
            if (RT_FAILURE(rc))
                return pThis->rcFatal = rc;

            pBaseObjData->offHdr    = offHdr;
            pBaseObjData->pXarReader= &pThis->XarReader;
            pBaseObjData->ObjInfo   = Info;

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
        rc = RTStrDupEx(ppszName, pThis->XarReader.szName);
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
#endif

    return VINF_SUCCESS;
}



/**
 * Xar filesystem stream operations.
 */
static const RTVFSFSSTREAMOPS rtZipXarFssOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_FS_STREAM,
        "XarFsStream",
        rtZipXarFss_Close,
        rtZipXarFss_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSFSSTREAMOPS_VERSION,
    0,
    rtZipXarFss_Next,
    RTVFSFSSTREAMOPS_VERSION
};



/**
 * TOC validation part 2.
 *
 * Will advance the input stream past the TOC hash and signature data.
 *
 * @returns IPRT status code.
 * @param   pThis       The FS stream instance being created.
 * @param   pXarHdr     The XAR header.
 * @param   pTocDigest  The TOC input data digest.
 */
static int rtZipXarValidateTocPart2(PRTZIPXARFSSTREAM pThis, PCXARHEADER pXarHdr, PCRTZIPXARHASHDIGEST pTocDigest)
{
    int rc;

    /*
     * Check that the hash function in the TOC matches the one in the XAR header.
     */
    const xml::ElementNode *pChecksumElem = pThis->XarReader.pToc->findChildElement("checksum");
    if (pChecksumElem)
    {
        const xml::AttributeNode *pAttr = pChecksumElem->findAttribute("style");
        if (!pAttr)
            return VERR_XAR_BAD_CHECKSUM_ELEMENT;

        const char *pszStyle = pAttr->getValue();
        if (!pszStyle)
            return VERR_XAR_BAD_CHECKSUM_ELEMENT;

        uint8_t uHashFunction;
        if (!strcmp(pszStyle, "sha1"))
            uHashFunction = XAR_HASH_SHA1;
        else if (!strcmp(pszStyle, "md5"))
            uHashFunction = XAR_HASH_MD5;
        else if (!strcmp(pszStyle, "none"))
            uHashFunction = XAR_HASH_NONE;
        else
            return VERR_XAR_BAD_CHECKSUM_ELEMENT;
        if (uHashFunction != pThis->uHashFunction)
            return VERR_XAR_HASH_FUNCTION_MISMATCH;

        /*
         * Verify the checksum if we got one.
         */
        if (pThis->uHashFunction != XAR_HASH_NONE)
        {
            RTFOFF   offChecksum;
            uint64_t cbChecksum;
            rc = rtZipXarGetOffsetSizeFromElem(pChecksumElem, &offChecksum, &cbChecksum);
            if (RT_FAILURE(rc))
                return rc;
            if (cbChecksum != pThis->cbHashDigest)
                return VERR_XAR_BAD_DIGEST_LENGTH;
            if (offChecksum != 0 && pThis->hVfsFile == NIL_RTVFSFILE)
                return VERR_XAR_NOT_STREAMBLE_ELEMENT_ORDER;

            RTZIPXARHASHDIGEST StoredDigest;
            rc = RTVfsIoStrmReadAt(pThis->hVfsIos, pThis->offZero + offChecksum, &StoredDigest, cbChecksum,
                                   true /*fBlocking*/, NULL /*pcbRead*/);
            if (RT_FAILURE(rc))
                return rc;
            if (memcmp(&StoredDigest, pTocDigest, pThis->cbHashDigest))
                return VERR_XAR_TOC_DIGEST_MISMATCH;
        }
    }
    else if (pThis->uHashFunction != XAR_HASH_NONE)
        return VERR_XAR_BAD_CHECKSUM_ELEMENT;

    /*
     * Check the signature, if we got one.
     */
    /** @todo signing. */

    return VINF_SUCCESS;
}


/**
 * Reads and validates the table of content.
 *
 * @returns IPRT status code.
 * @param   hVfsIosIn   The input stream.
 * @param   pXarHdr     The XAR header.
 * @param   pDoc        The TOC XML document.
 * @param   ppTocElem   Where to return the pointer to the TOC element on
 *                      success.
 * @param   pTocDigest  Where to return the TOC digest on success.
 */
static int rtZipXarReadAndValidateToc(RTVFSIOSTREAM hVfsIosIn, PCXARHEADER pXarHdr,
                                      xml::Document *pDoc, xml::ElementNode const **ppTocElem, PRTZIPXARHASHDIGEST pTocDigest)
{
    /*
     * Decompress it, calculating the hash while doing so.
     */
    char *pszOutput = (char *)RTMemTmpAlloc(pXarHdr->cbTocUncompressed + 1);
    if (!pszOutput)
        return VERR_NO_TMP_MEMORY;
    int rc = VERR_NO_TMP_MEMORY;
    void *pvInput = RTMemTmpAlloc(pXarHdr->cbTocCompressed);
    if (pvInput)
    {
        rc = RTVfsIoStrmRead(hVfsIosIn, pvInput, pXarHdr->cbTocCompressed, true /*fBlocking*/,  NULL);
        if (RT_SUCCESS(rc))
        {
            rtZipXarCalcHash(pXarHdr->uHashFunction, pvInput, pXarHdr->cbTocCompressed, pTocDigest);

            size_t cbActual;
            rc = RTZipBlockDecompress(RTZIPTYPE_ZLIB, 0 /*fFlags*/,
                                      pvInput, pXarHdr->cbTocCompressed, NULL,
                                      pszOutput, pXarHdr->cbTocUncompressed, &cbActual);
            if (RT_SUCCESS(rc) && cbActual != pXarHdr->cbTocUncompressed)
                rc = VERR_XAR_TOC_UNCOMP_SIZE_MISMATCH;
        }
        RTMemTmpFree(pvInput);
    }
    if (RT_SUCCESS(rc))
    {
        pszOutput[pXarHdr->cbTocUncompressed] = '\0';

        /*
         * Parse the TOC (XML document) and do some basic validations.
         */
        size_t cchToc = strlen(pszOutput);
        if (   cchToc     == pXarHdr->cbTocUncompressed
            || cchToc + 1 == pXarHdr->cbTocUncompressed)
        {
            rc = RTStrValidateEncoding(pszOutput);
            if (RT_SUCCESS(rc))
            {
                xml::XmlMemParser Parser;
                try
                {
                    Parser.read(pszOutput, cchToc, RTCString("xar-toc.xml"), *pDoc);
                }
                catch (xml::XmlError Err)
                {
                    rc = VERR_XAR_TOC_XML_PARSE_ERROR;
                }
                catch (...)
                {
                    rc = VERR_NO_MEMORY;
                }
                if (RT_SUCCESS(rc))
                {
                    xml::ElementNode const *pRootElem = pDoc->getRootElement();
                    xml::ElementNode const *pTocElem  = NULL;
                    if (pRootElem && pRootElem->nameEquals("xar"))
                        pTocElem = pRootElem ? pRootElem->findChildElement(NULL, "toc") : NULL;
                    if (pTocElem)
                    {
#ifndef USE_STD_LIST_FOR_CHILDREN
                        Assert(pRootElem->getParent() == NULL);
                        Assert(pTocElem->getParent() == pRootElem);
                        if (   !pTocElem->getNextSibiling()
                            && !pTocElem->getPrevSibiling())
#endif
                        {
                            /*
                             * Further parsing and validation is done after the
                             * caller has created an file system stream instance.
                             */
                            *ppTocElem = pTocElem;

                            RTMemTmpFree(pszOutput);
                            return VINF_SUCCESS;
                        }

                        rc = VERR_XML_TOC_ELEMENT_HAS_SIBLINGS;
                    }
                    else
                        rc = VERR_XML_TOC_ELEMENT_MISSING;
                }
            }
            else
                rc = VERR_XAR_TOC_UTF8_ENCODING;
        }
        else
            rc = VERR_XAR_TOC_STRLEN_MISMATCH;
    }

    RTMemTmpFree(pszOutput);
    return rc;
}


/**
 * Reads and validates the XAR header.
 *
 * @returns IPRT status code.
 * @param   hVfsIosIn   The input stream.
 * @param   pXarHdr     Where to return the XAR header in host byte order.
 */
static int rtZipXarReadAndValidateHeader(RTVFSIOSTREAM hVfsIosIn, PXARHEADER pXarHdr)
{
    /*
     * Read it and check the signature.
     */
    int rc = RTVfsIoStrmRead(hVfsIosIn, pXarHdr, sizeof(*pXarHdr), true /*fBlocking*/,  NULL);
    if (RT_FAILURE(rc))
        return rc;
    if (pXarHdr->u32Magic != XAR_HEADER_MAGIC)
        return VERR_XAR_WRONG_MAGIC;

    /*
     * Correct the byte order.
     */
    pXarHdr->cbHeader             = RT_BE2H_U16(pXarHdr->cbHeader);
    pXarHdr->uVersion             = RT_BE2H_U16(pXarHdr->uVersion);
    pXarHdr->cbTocCompressed      = RT_BE2H_U64(pXarHdr->cbTocCompressed);
    pXarHdr->cbTocUncompressed    = RT_BE2H_U64(pXarHdr->cbTocUncompressed);
    pXarHdr->uHashFunction        = RT_BE2H_U32(pXarHdr->uHashFunction);

    /*
     * Validate the header.
     */
    if (pXarHdr->uVersion > XAR_HEADER_VERSION)
        return VERR_XAR_UNSUPPORTED_VERSION;
    if (pXarHdr->cbHeader < sizeof(XARHEADER))
        return VERR_XAR_BAD_HDR_SIZE;
    if (pXarHdr->uHashFunction > XAR_HASH_MAX)
        return VERR_XAR_UNSUPPORTED_HASH_FUNCTION;
    if (pXarHdr->cbTocUncompressed < 16)
        return VERR_XAR_TOC_TOO_SMALL;
    if (pXarHdr->cbTocUncompressed > _4M)
        return VERR_XAR_TOC_TOO_BIG;
    if (pXarHdr->cbTocCompressed > _4M)
        return VERR_XAR_TOC_TOO_BIG_COMPRESSED;

    /*
     * Skip over bytes we don't understand (could be padding).
     */
    if (pXarHdr->cbHeader > sizeof(XARHEADER))
    {
        rc = RTVfsIoStrmSkip(hVfsIosIn, pXarHdr->cbHeader - sizeof(XARHEADER));
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTZipXarFsStreamFromIoStream(RTVFSIOSTREAM hVfsIosIn, uint32_t fFlags, PRTVFSFSSTREAM phVfsFss)
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
     * Read and validate the header, then uncompress the TOC.
     */
    XARHEADER XarHdr;
    int rc = rtZipXarReadAndValidateHeader(hVfsIosIn, &XarHdr);
    if (RT_SUCCESS(rc))
    {
        xml::Document *pDoc = NULL;
        try         { pDoc = new xml::Document(); }
        catch (...) { }
        if (pDoc)
        {
            RTZIPXARHASHDIGEST      TocDigest;
            xml::ElementNode const *pTocElem = NULL;
            rc = rtZipXarReadAndValidateToc(hVfsIosIn, &XarHdr, pDoc, &pTocElem, &TocDigest);
            if (RT_SUCCESS(rc))
            {
                size_t offZero = RTVfsIoStrmTell(hVfsIosIn);
                if (offZero > 0)
                {
                    /*
                     * Create a file system stream before we continue the parsing.
                     */
                    PRTZIPXARFSSTREAM pThis;
                    RTVFSFSSTREAM     hVfsFss;
                    rc = RTVfsNewFsStream(&rtZipXarFssOps, sizeof(*pThis), NIL_RTVFS, NIL_RTVFSLOCK, &hVfsFss, (void **)&pThis);
                    if (RT_SUCCESS(rc))
                    {
                        pThis->hVfsIos              = hVfsIosIn;
                        pThis->hVfsFile             = RTVfsIoStrmToFile(hVfsIosIn);
                        pThis->hVfsCurObj           = NIL_RTVFSOBJ;
                        pThis->pCurIosData          = NULL;
                        pThis->offStart             = offStart;
                        pThis->offZero              = offZero;
                        pThis->uHashFunction        = (uint8_t)XarHdr.uHashFunction;
                        switch (pThis->uHashFunction)
                        {
                            case XAR_HASH_MD5:  pThis->cbHashDigest = sizeof(TocDigest.abMd5); break;
                            case XAR_HASH_SHA1: pThis->cbHashDigest = sizeof(TocDigest.abSha1); break;
                            default:            pThis->cbHashDigest = 0; break;
                        }
                        pThis->fEndOfStream         = false;
                        pThis->rcFatal              = VINF_SUCCESS;
                        pThis->XarReader.pDoc       = pDoc;
                        pThis->XarReader.pToc       = pTocElem;
                        pThis->XarReader.idCurFile  = 0; /* Assuming the ID numbering is from 1. */

                        /*
                         * Next validation step.
                         */
                        rc = rtZipXarValidateTocPart2(pThis, &XarHdr, &TocDigest);
                        if (RT_SUCCESS(rc))
                        {
                            *phVfsFss = hVfsFss;
                            return VINF_SUCCESS;
                        }

                        RTVfsFsStrmRelease(hVfsFss);
                        return rc;
                    }
                }
                else
                    rc = (int)offZero;
            }
            delete pDoc;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    RTVfsIoStrmRelease(hVfsIosIn);
    return rc;
}

