/* $Id$ */
/** @file
 * IPRT - TAR Virtual Filesystem, Writer.
 */

/*
 * Copyright (C) 2010-2017 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/zip.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include "tar.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * Tar filesystem stream private data.
 */
typedef struct RTZIPTARFSSTREAMWRITER
{
    /** The output I/O stream. */
    RTVFSIOSTREAM           hVfsIos;

    /** Set if we've encountered a fatal error. */
    int                     rcFatal;
    /** Flags. */
    uint32_t                fFlags;

    /** Number of bytes written. */
    uint64_t                cbWritten;

    /** Number of headers returned by rtZipTarFssWriter_ObjInfoToHdr. */
    uint32_t                cHdrs;
    /** Header buffers returned by rtZipTarFssWriter_ObjInfoToHdr. */
    RTZIPTARHDR             aHdrs[3];
} RTZIPTARFSSTREAMWRITER;
/** Pointer to a the private data of a TAR filesystem stream. */
typedef RTZIPTARFSSTREAMWRITER *PRTZIPTARFSSTREAMWRITER;


/**
 * Calculates the header checksum and stores it in the chksum field.
 *
 * @returns IPRT status code.
 * @param   pHdr                The header.
 */
static int rtZipTarFssWriter_ChecksumHdr(PRTZIPTARHDR pHdr)
{
    int32_t iUnsignedChksum;
    rtZipTarCalcChkSum(pHdr, &iUnsignedChksum, NULL);

    int rc = RTStrFormatU32(pHdr->Common.chksum, sizeof(pHdr->Common.chksum), iUnsignedChksum,
                            8 /*uBase*/, -1 /*cchWidth*/, sizeof(pHdr->Common.chksum) - 1, RTSTR_F_ZEROPAD);
    AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);
    return VINF_SUCCESS;
}


/**
 * Creates one or more tar headers for the object.
 *
 * Returns RTZIPTARFSSTREAMWRITER::aHdrs and RTZIPTARFSSTREAMWRITER::cHdrs.
 *
 * @returns IPRT status code.
 * @param   pThis           The TAR writer instance.
 * @param   pszPath         The path to the file.
 * @param   hVfsIos         The I/O stream of the file.
 * @param   fFlags          The RTVFSFSSTREAMOPS::pfnAdd flags.
 * @param   pObjInfo        The object information.
 * @param   pszOwnerNm      The owner name.
 * @param   pszGroupNm      The group name.
 * @param   chType          The tar record type, UINT8_MAX for default.
 */
static int rtZipTarFssWriter_ObjInfoToHdr(PRTZIPTARFSSTREAMWRITER pThis, const char *pszPath, PCRTFSOBJINFO pObjInfo,
                                          const char *pszOwnerNm, const char *pszGroupNm, uint8_t chType)
{
    pThis->cHdrs = 0;
    RT_ZERO(pThis->aHdrs[0]);

    /*
     * The path name first.  Make sure to flip DOS slashes.
     */
    size_t cchPath = strlen(pszPath);
    if (cchPath < sizeof(pThis->aHdrs[0].Common.name))
    {
        memcpy(pThis->aHdrs[0].Common.name, pszPath, cchPath + 1);
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_UNIX
        char *pszDosSlash = strchr(pThis->aHdrs[0].Common.name, '\\');
        while (pszDosSlash)
        {
            *pszDosSlash = '/';
            pszDosSlash = strchr(pszDosSlash + 1, '\\');
        }
#endif
    }
    else
    {
        /** @todo implement gnu and pax long name extensions. */
        return VERR_TAR_NAME_TOO_LONG;
    }

    /*
     * File mode.  ASSUME that the unix part of the IPRT mode mask is
     * compatible with the TAR/Unix world.
     */
    int rc;
    rc = RTStrFormatU32(pThis->aHdrs[0].Common.mode, sizeof(pThis->aHdrs[0].Common.mode), pObjInfo->Attr.fMode & RTFS_UNIX_MASK,
                        8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.mode) - 1, RTSTR_F_ZEROPAD);
    AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);


    /*
     * uid & gid.  Just guard against NIL values as they won't fit.
     */
    uint32_t uValue = pObjInfo->Attr.u.Unix.uid != NIL_RTUID ? pObjInfo->Attr.u.Unix.uid : 0;
    rc = RTStrFormatU32(pThis->aHdrs[0].Common.uid, sizeof(pThis->aHdrs[0].Common.uid), uValue,
                        8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.uid) - 1, RTSTR_F_ZEROPAD);
    AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);

    uValue = pObjInfo->Attr.u.Unix.gid != NIL_RTUID ? pObjInfo->Attr.u.Unix.gid : 0;
    rc = RTStrFormatU32(pThis->aHdrs[0].Common.gid, sizeof(pThis->aHdrs[0].Common.gid), uValue,
                        8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.gid) - 1, RTSTR_F_ZEROPAD);
    AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);

    /*
     * Is the size small enough for the standard octal string encoding?
     *
     * Note! We could actually use the terminator character as well if we liked,
     *       but let not do that as it's easier to test this way.
     */
    uint64_t cbObject = pObjInfo->cbObject;
    if (cbObject < _4G * 2U)
    {
        rc = RTStrFormatU64(pThis->aHdrs[0].Common.size, sizeof(pThis->aHdrs[0].Common.size), cbObject,
                            8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.size) - 1, RTSTR_F_ZEROPAD);
        AssertRCReturn(rc, rc);
    }
    /*
     * No, use the base 256 extension. Set the highest bit of the left most
     * character.  We don't deal with negatives here, cause the size have to
     * be greater than zero.
     *
     * Note! The base-256 extension are never used by gtar or libarchive
     *       with the "ustar  \0" format version, only the later
     *       "ustar\000" version.  However, this shouldn't cause much
     *       trouble as they are not picky about what they read.
     */
    else
    {
        size_t         cchField  = sizeof(pThis->aHdrs[0].Common.size) - 1;
        unsigned char *puchField = (unsigned char*)pThis->aHdrs[0].Common.size;
        puchField[0] = 0x80;
        do
        {
            puchField[cchField--] = cbObject & 0xff;
            cbObject >>= 8;
        } while (cchField);
    }

    /*
     * Modification time relative to unix epoc.
     */
    rc = RTStrFormatU64(pThis->aHdrs[0].Common.mtime, sizeof(pThis->aHdrs[0].Common.mtime),
                        RTTimeSpecGetSeconds(&pObjInfo->ModificationTime),
                        8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.mtime) - 1, RTSTR_F_ZEROPAD);
    AssertRCReturn(rc, rc);

    /* Skipping checksum for now */

    /*
     * The type flag.
     */
    if (chType == UINT8_MAX)
        switch (pObjInfo->Attr.fMode & RTFS_TYPE_MASK)
        {
            case RTFS_TYPE_FIFO:        chType = RTZIPTAR_TF_FIFO; break;
            case RTFS_TYPE_DEV_CHAR:    chType = RTZIPTAR_TF_CHR; break;
            case RTFS_TYPE_DIRECTORY:   chType = RTZIPTAR_TF_DIR; break;
            case RTFS_TYPE_DEV_BLOCK:   chType = RTZIPTAR_TF_BLK; break;
            case RTFS_TYPE_FILE:        chType = RTZIPTAR_TF_NORMAL; break;
            case RTFS_TYPE_SYMLINK:     chType = RTZIPTAR_TF_SYMLINK; break;
            case RTFS_TYPE_SOCKET:      chType = RTZIPTAR_TF_FIFO; break;
            case RTFS_TYPE_WHITEOUT:    AssertFailedReturn(VERR_WRONG_TYPE);
        }
    pThis->aHdrs[0].Common.typeflag = chType;

    /* No link name, at least not for now.  Caller might set it. */

    /*
     * Set TAR record magic and version.
     */
    memcpy(pThis->aHdrs[0].Common.magic, RT_STR_TUPLE("ustar "));
    pThis->aHdrs[0].Common.version[0] = ' ';
    pThis->aHdrs[0].Common.version[1] = ' ';

    /*
     * Owner and group names.  Silently truncate them for now.
     */
    RTStrCopy(pThis->aHdrs[0].Common.uname, sizeof(pThis->aHdrs[0].Common.uname), pszOwnerNm);
    RTStrCopy(pThis->aHdrs[0].Common.gname, sizeof(pThis->aHdrs[0].Common.uname), pszGroupNm);

    /*
     * Char/block device numbers.
     */
    if (   RTFS_IS_DEV_BLOCK(pObjInfo->Attr.fMode)
        || RTFS_IS_DEV_CHAR(pObjInfo->Attr.fMode) )
    {
        rc = RTStrFormatU32(pThis->aHdrs[0].Common.devmajor, sizeof(pThis->aHdrs[0].Common.devmajor),
                            RTDEV_MAJOR(pObjInfo->Attr.u.Unix.Device),
                            8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.devmajor) - 1, RTSTR_F_ZEROPAD);
        AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);

        rc = RTStrFormatU32(pThis->aHdrs[0].Common.devminor, sizeof(pThis->aHdrs[0].Common.devmajor),
                            RTDEV_MINOR(pObjInfo->Attr.u.Unix.Device),
                            8 /*uBase*/, -1 /*cchWidth*/, sizeof(pThis->aHdrs[0].Common.devmajor) - 1, RTSTR_F_ZEROPAD);
        AssertRCReturn(rc, VERR_TAR_NUM_VALUE_TOO_LARGE);
    }

    /*
     * Finally the checksum.
     */
    return rtZipTarFssWriter_ChecksumHdr(&pThis->aHdrs[0]);
}


/**
 * Adds a file to the stream.
 *
 * @returns IPRT status code.
 * @param   pThis           The TAR writer instance.
 * @param   pszPath         The path to the file.
 * @param   hVfsIos         The I/O stream of the file.
 * @param   fFlags          The RTVFSFSSTREAMOPS::pfnAdd flags.
 * @param   pObjInfo        The object information.
 * @param   pszOwnerNm      The owner name.
 * @param   pszGroupNm      The group name.
 */
static int rtZipTarFssWriter_AddFile(PRTZIPTARFSSTREAMWRITER pThis, const char *pszPath, RTVFSIOSTREAM hVfsIos, uint32_t fFlags,
                                     PCRTFSOBJINFO pObjInfo, const char *pszOwnerNm, const char *pszGroupNm)
{
    RT_NOREF(fFlags);

    /*
     * Append the header.
     */
    int rc = rtZipTarFssWriter_ObjInfoToHdr(pThis, pszPath, pObjInfo, pszOwnerNm, pszGroupNm, UINT8_MAX);
    if (RT_SUCCESS(rc))
    {
        rc = RTVfsIoStrmWrite(pThis->hVfsIos, pThis->aHdrs, pThis->cHdrs * sizeof(pThis->aHdrs[0]), true /*fBlocking*/, NULL);
        if (RT_SUCCESS(rc))
        {
            pThis->cbWritten += pThis->cHdrs * sizeof(pThis->aHdrs[0]);

            /*
             * Allocate a tmporary buffer
             */
            uint8_t *pbFree;
            size_t   cbBuf = _512K;
            uint8_t *pbBuf = pbFree = (uint8_t *)RTMemTmpAlloc(cbBuf);
            if (!pbBuf)
            {
                cbBuf = _32K;
                pbBuf = pbFree = (uint8_t *)RTMemTmpAlloc(cbBuf);
                if (!pbBuf)
                {
                    cbBuf = sizeof(pThis->aHdrs);
                    pbBuf = (uint8_t *)&pThis->aHdrs[0];
                }
            }

            /*
             * Copy the bytes.  Padding the last buffer to a multiple of 512.
             */
            uint64_t cbLeft = pObjInfo->cbObject;
            while (cbLeft > 0 && RT_SUCCESS(rc))
            {
                size_t cbRead = cbLeft > cbBuf ? cbBuf : (size_t)cbBuf;
                rc = RTVfsIoStrmRead(hVfsIos, pbBuf, cbRead, true /*fBlocking*/, NULL);
                if (RT_FAILURE(rc))
                    break;

                size_t cbToWrite = cbRead;
                if (cbRead & (sizeof(RTZIPTARHDR) - 1))
                {
                    size_t cbToZero = sizeof(RTZIPTARHDR) - (cbRead & (sizeof(RTZIPTARHDR) - 1));
                    memset(&pbBuf[cbRead], 0, cbToZero);
                    cbToWrite += cbToZero;
                }

                rc = RTVfsIoStrmWrite(pThis->hVfsIos, pbBuf, cbToWrite, true /*fBlocking*/, NULL);
                if (RT_FAILURE(rc))
                    break;
                pThis->cbWritten += cbToWrite;
                cbLeft -= cbRead;
            }

            RTMemTmpFree(pbFree);
            if (RT_SUCCESS(rc))
                return VINF_SUCCESS;
        }
        pThis->rcFatal = rc;
    }
    return rc;
}


/**
 * Adds a symbolic link to the stream.
 *
 * @returns IPRT status code.
 * @param   pThis           The TAR writer instance.
 * @param   pszPath         The path to the object.
 * @param   hVfsSymlink     The symbolic link object to add.
 * @param   pObjInfo        The object information.
 * @param   pszOwnerNm      The owner name.
 * @param   pszGroupNm      The group name.
 */
static int rtZipTarFssWriter_AddSymlink(PRTZIPTARFSSTREAMWRITER pThis, const char *pszPath, RTVFSSYMLINK hVfsSymlink,
                                        PCRTFSOBJINFO pObjInfo,  const char *pszOwnerNm, const char *pszGroupNm)
{
    /*
     * Read the symlink target first and check that it's not too long.
     * Flip DOS slashes.
     */
    char szTarget[RTPATH_MAX];
    int rc = RTVfsSymlinkRead(hVfsSymlink, szTarget,  sizeof(szTarget));
    if (RT_SUCCESS(rc))
    {
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_UNIX
        char *pszDosSlash = strchr(szTarget, '\\');
        while (pszDosSlash)
        {
            *pszDosSlash = '/';
            pszDosSlash = strchr(pszDosSlash + 1, '\\');
        }
#endif
        size_t cchTarget = strlen(szTarget);
        if (cchTarget < sizeof(pThis->aHdrs[0].Common.linkname))
        {
            /*
             * Create a header, add the link target and push it out.
             */
            rc = rtZipTarFssWriter_ObjInfoToHdr(pThis, pszPath, pObjInfo, pszOwnerNm, pszGroupNm, UINT8_MAX);
            if (RT_SUCCESS(rc))
            {
                memcpy(pThis->aHdrs[0].Common.linkname, szTarget, cchTarget + 1);
                rc = rtZipTarFssWriter_ChecksumHdr(&pThis->aHdrs[0]);
                if (RT_SUCCESS(rc))
                {
                    rc = RTVfsIoStrmWrite(pThis->hVfsIos, pThis->aHdrs, pThis->cHdrs * sizeof(pThis->aHdrs[0]),
                                          true /*fBlocking*/, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        pThis->cbWritten += pThis->cHdrs * sizeof(pThis->aHdrs[0]);
                        return VINF_SUCCESS;
                    }
                    pThis->rcFatal = rc;
                }
            }
        }
        else
        {
            /** @todo implement gnu and pax long name extensions. */
            rc = VERR_TAR_NAME_TOO_LONG;
        }
    }
    return rc;
}


/**
 * Adds a simple object to the stream.
 *
 * Simple objects only contains metadata, no actual data bits.  Directories,
 * devices, fifos, sockets and such.
 *
 * @returns IPRT status code.
 * @param   pThis           The TAR writer instance.
 * @param   pszPath         The path to the object.
 * @param   pObjInfo        The object information.
 * @param   pszOwnerNm      The owner name.
 * @param   pszGroupNm      The group name.
 */
static int rtZipTarFssWriter_AddSimpleObject(PRTZIPTARFSSTREAMWRITER pThis, const char *pszPath, PCRTFSOBJINFO pObjInfo,
                                             const char *pszOwnerNm, const char *pszGroupNm)
{
    int rc = rtZipTarFssWriter_ObjInfoToHdr(pThis, pszPath, pObjInfo, pszOwnerNm, pszGroupNm, UINT8_MAX);
    if (RT_SUCCESS(rc))
    {
        rc = RTVfsIoStrmWrite(pThis->hVfsIos, pThis->aHdrs, pThis->cHdrs * sizeof(pThis->aHdrs[0]), true /*fBlocking*/, NULL);
        if (RT_SUCCESS(rc))
        {
            pThis->cbWritten += pThis->cHdrs * sizeof(pThis->aHdrs[0]);
            return VINF_SUCCESS;
        }
        pThis->rcFatal = rc;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipTarFssWriter_Close(void *pvThis)
{
    PRTZIPTARFSSTREAMWRITER pThis = (PRTZIPTARFSSTREAMWRITER)pvThis;

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;

    /** @todo investigate zero end records.  */

    return VINF_SUCCESS;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipTarFssWriter_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPTARFSSTREAMWRITER pThis = (PRTZIPTARFSSTREAMWRITER)pvThis;
    /* Take the lazy approach here, with the sideffect of providing some info
       that is actually kind of useful. */
    return RTVfsIoStrmQueryInfo(pThis->hVfsIos, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSFSSTREAMOPS,pfnAdd}
 */
static DECLCALLBACK(int) rtZipTarFssWriter_Add(void *pvThis, const char *pszPath, RTVFSOBJ hVfsObj, uint32_t fFlags)
{
    PRTZIPTARFSSTREAMWRITER pThis = (PRTZIPTARFSSTREAMWRITER)pvThis;
    RT_NOREF(pThis, pszPath, hVfsObj, fFlags);

    /*
     * Refuse to do anything if we've encountered a fatal error.
     * Assert this because the caller should know better than calling us again.
     */
    AssertRCReturn(pThis->rcFatal, pThis->rcFatal);

    /*
     * Query information about the object.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTVfsObjQueryInfo(hVfsObj, &ObjInfo, RTFSOBJATTRADD_UNIX);
    AssertRCReturn(rc, rc);

    RTFSOBJINFO ObjOwnerName;
    rc = RTVfsObjQueryInfo(hVfsObj, &ObjOwnerName, RTFSOBJATTRADD_UNIX_OWNER);
    if (RT_FAILURE(rc) || ObjOwnerName.Attr.u.UnixOwner.szName[0] == '\0')
        strcpy(ObjOwnerName.Attr.u.UnixOwner.szName, "someone");

    RTFSOBJINFO ObjGrpName;
    rc = RTVfsObjQueryInfo(hVfsObj, &ObjGrpName, RTFSOBJATTRADD_UNIX_GROUP);
    if (RT_FAILURE(rc) || ObjGrpName.Attr.u.UnixGroup.szName[0] == '\0')
        strcpy(ObjGrpName.Attr.u.UnixGroup.szName, "somegroup");

    /*
     * Do type specific handling.
     */
    if (RTFS_IS_FILE(ObjInfo.Attr.fMode))
    {
        RTVFSIOSTREAM hVfsIos = RTVfsObjToIoStream(hVfsObj);
        AssertReturn(hVfsIos != NIL_RTVFSIOSTREAM, VERR_WRONG_TYPE);
        rc = rtZipTarFssWriter_AddFile(pThis, pszPath, hVfsIos, fFlags, &ObjInfo,
                                       ObjOwnerName.Attr.u.UnixOwner.szName, ObjGrpName.Attr.u.UnixOwner.szName);
        RTVfsIoStrmRelease(hVfsIos);
    }
    else if (RTFS_IS_SYMLINK(ObjInfo.Attr.fMode))
    {
        RTVFSSYMLINK hVfsSymlink = RTVfsObjToSymlink(hVfsObj);
        AssertReturn(hVfsSymlink != NIL_RTVFSSYMLINK, VERR_WRONG_TYPE);
        rc = rtZipTarFssWriter_AddSymlink(pThis, pszPath, hVfsSymlink, &ObjInfo,
                                          ObjOwnerName.Attr.u.UnixOwner.szName, ObjGrpName.Attr.u.UnixOwner.szName);
        RTVfsSymlinkRelease(hVfsSymlink);
    }
    else
        rc = rtZipTarFssWriter_AddSimpleObject(pThis, pszPath, &ObjInfo,
                                               ObjOwnerName.Attr.u.UnixOwner.szName, ObjGrpName.Attr.u.UnixOwner.szName);

    return rc;
}



/**
 * Tar filesystem stream operations.
 */
static const RTVFSFSSTREAMOPS rtZipTarFssOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_FS_STREAM,
        "TarFsStreamWriter",
        rtZipTarFssWriter_Close,
        rtZipTarFssWriter_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSFSSTREAMOPS_VERSION,
    0,
    NULL,
    rtZipTarFssWriter_Add,
    RTVFSFSSTREAMOPS_VERSION
};


RTDECL(int) RTZipTarFsStreamToIoStream(RTVFSIOSTREAM hVfsIosOut, uint32_t fFlags, PRTVFSFSSTREAM phVfsFss)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(phVfsFss, VERR_INVALID_HANDLE);
    *phVfsFss = NIL_RTVFSFSSTREAM;
    AssertPtrReturn(hVfsIosOut, VERR_INVALID_HANDLE);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);

    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosOut);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Retain the input stream and create a new filesystem stream handle.
     */
    PRTZIPTARFSSTREAMWRITER pThis;
    RTVFSFSSTREAM           hVfsFss;
    int rc = RTVfsNewFsStream(&rtZipTarFssOps, sizeof(*pThis), NIL_RTVFS, NIL_RTVFSLOCK, false /*fReadOnly*/,
                              &hVfsFss, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsIos = hVfsIosOut;
        pThis->rcFatal = VINF_SUCCESS;

        *phVfsFss = hVfsFss;
        return VINF_SUCCESS;
    }

    RTVfsIoStrmRelease(hVfsIosOut);
    return rc;
}

