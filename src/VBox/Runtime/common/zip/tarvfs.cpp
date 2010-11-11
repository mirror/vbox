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

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/poll.h>
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



DECLINLINE(bool) rtZipTarHdrHasPosixUserName(PCRTZIPTARHDR pTar)
{
    return true;
}


DECLINLINE(bool) rtZipTarHdrHasPosixGroupName(PCRTZIPTARHDR pTar)
{
    return true;
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
                RTStrCopy(pObjInfo->Attr.u.UnixOwner.szName, sizeof(pObjInfo->Attr.u.UnixOwner.szName), pThis->Hdr.Hdr.uname);
            break;

        case RTFSOBJATTRADD_UNIX_GROUP:
            *pObjInfo = pThis->ObjInfo;
            pObjInfo->Attr.enmAdditional         = RTFSOBJATTRADD_UNIX_GROUP;
            pObjInfo->Attr.u.UnixGroup.gid       = pThis->ObjInfo.Attr.u.Unix.gid;
            pObjInfo->Attr.u.UnixGroup.szName[0] = '\0';
            if (rtZipTarHdrHasPosixGroupName(&pThis->Hdr))
                RTStrCopy(pObjInfo->Attr.u.UnixGroup.szName, sizeof(pObjInfo->Attr.u.UnixGroup.szName), pThis->Hdr.Hdr.gname);
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
 * Tar filesystem stream operations.
 */
static const RTVFSOBJOPS g_rtZipTarFsStrmOps =
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
 * Standard file operations.
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
        int rc = RTVfsIoStrmSkip(pThis->hVfsIos, off - pThis->offNextHdr);
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

    /*
     * Validate the header.
     */
    /** @todo continue here... */

    /*
     * Create an object of the appropriate type.
     */


    return VERR_NOT_IMPLEMENTED;
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


RTDECL(int) RTZipTarFsStreamFromIoStream(RTVFSIOSTREAM hVfsIosIn, PRTVFSFSSTREAM phVfsFss)
{
    /*
     * Input validation.
     */
    AssertPtrReturn(phVfsFss, VERR_INVALID_HANDLE);
    *phVfsFss = NIL_RTVFSFSSTREAM;
    AssertPtrReturn(hVfsIosIn, VERR_INVALID_HANDLE);

    RTFOFF const offStart = RTVfsIoStrmTell(hVfsIosIn);
    AssertReturn(offStart >= 0, (int)offStart);

    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Retain the input stream and create a new filesystem stream handle.
     */
    PRTZIPTARFSSTREAM pThis;
    RTVFSFSSTREAM     hVfsFss;
    int rc = RTVfsNewFsStream(&rtZipTarFssOps, sizeof(*pThis), NIL_RTVFS, NIL_RTSEMRW, &hVfsFss, (void **)&pThis);
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

