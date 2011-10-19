/* $Id$ */
/** @file
 * IPRT - GZIP Compressor and Decompressor I/O Stream.
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


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/zip.h>

#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/err.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/vfslowlevel.h>

#include <zlib.h>

#if defined(RT_OS_OS2) || defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS)
/**
 * Drag in the missing zlib symbols.
 */
PFNRT g_apfnRTZlibDeps[] =
{
    (PFNRT)gzrewind,
    (PFNRT)gzread,
    (PFNRT)gzopen,
    (PFNRT)gzwrite,
    (PFNRT)gzclose,
    (PFNRT)gzdopen,
    NULL
};
#endif /* RT_OS_OS2 || RT_OS_SOLARIS || RT_OS_WINDOWS */

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#pragma pack(1)
typedef struct RTZIPGZIPHDR
{
    /** RTZIPGZIPHDR_ID1. */
    uint8_t     bId1;
    /** RTZIPGZIPHDR_ID2. */
    uint8_t     bId2;
    /** CM - The compression method. */
    uint8_t     bCompressionMethod;
    /** FLG - Flags. */
    uint8_t     fFlags;
    /** Modification time of the source file or the timestamp at the time the
     * compression took place. Can also be zero.  Is the number of seconds since
     * unix epoch. */
    uint32_t    u32ModTime;
    /** Flags specific to the compression method. */
    uint8_t     bXtraFlags;
    /** An ID indicating which OS or FS gzip ran on. */
    uint8_t     bOS;
} RTZIPGZIPHDR;
#pragma pack()
AssertCompileSize(RTZIPGZIPHDR, 10);
/** Pointer to a const gzip header. */
typedef RTZIPGZIPHDR const *PCRTZIPGZIPHDR;

/** gzip header identification no 1. */
#define RTZIPGZIPHDR_ID1            0x1f
/** gzip header identification no 2. */
#define RTZIPGZIPHDR_ID2            0x8b
/** gzip deflate compression method. */
#define RTZIPGZIPHDR_CM_DEFLATE     8

/** @name gzip header flags
 * @{ */
/** Probably a text file */
#define RTZIPGZIPHDR_FLG_TEXT       UINT8_C(0x01)
/** Header CRC present (crc32 of header cast to uint16_t). */
#define RTZIPGZIPHDR_FLG_HDR_CRC    UINT8_C(0x02)
/** Length prefixed xtra field is present. */
#define RTZIPGZIPHDR_FLG_EXTRA      UINT8_C(0x04)
/** A name field is present (latin-1). */
#define RTZIPGZIPHDR_FLG_NAME       UINT8_C(0x08)
/** A comment field is present (latin-1). */
#define RTZIPGZIPHDR_FLG_COMMENT    UINT8_C(0x10)
/** Mask of valid flags. */
#define RTZIPGZIPHDR_FLG_VALID_MASK UINT8_C(0x1f)
/** @}  */

/** @name gzip default xtra flag values
 * @{ */
#define RTZIPGZIPHDR_XFL_DEFLATE_MAX        UINT8_C(0x02)
#define RTZIPGZIPHDR_XFL_DEFLATE_FASTEST    UINT8_C(0x04)
/** @} */

/** @name Operating system / Filesystem IDs
 * @{ */
#define RTZIPGZIPHDR_OS_FAT             UINT8_C(0x00)
#define RTZIPGZIPHDR_OS_AMIGA           UINT8_C(0x01)
#define RTZIPGZIPHDR_OS_VMS             UINT8_C(0x02)
#define RTZIPGZIPHDR_OS_UNIX            UINT8_C(0x03)
#define RTZIPGZIPHDR_OS_VM_CMS          UINT8_C(0x04)
#define RTZIPGZIPHDR_OS_ATARIS_TOS      UINT8_C(0x05)
#define RTZIPGZIPHDR_OS_HPFS            UINT8_C(0x06)
#define RTZIPGZIPHDR_OS_MACINTOSH       UINT8_C(0x07)
#define RTZIPGZIPHDR_OS_Z_SYSTEM        UINT8_C(0x08)
#define RTZIPGZIPHDR_OS_CPM             UINT8_C(0x09)
#define RTZIPGZIPHDR_OS_TOPS_20         UINT8_C(0x0a)
#define RTZIPGZIPHDR_OS_NTFS            UINT8_C(0x0b)
#define RTZIPGZIPHDR_OS_QDOS            UINT8_C(0x0c)
#define RTZIPGZIPHDR_OS_ACORN_RISCOS    UINT8_C(0x0d)
#define RTZIPGZIPHDR_OS_UNKNOWN         UINT8_C(0xff)
/** @}  */


/**
 * The internal data of a GZIP I/O stream.
 */
typedef struct RTZIPGZIPSTREAM
{
    /** The stream we're reading or writing the compressed data from or to. */
    RTVFSIOSTREAM       hVfsIos;
    /** Set if it's a decompressor, clear if it's a compressor. */
    bool                fDecompress;
    /** Set if zlib reported a fatal error. */
    bool                fFatalError;
    /** Set if we've reached the end of the zlib stream. */
    bool                fEndOfStream;
    /** The stream offset for pfnTell. */
    RTFOFF              offStream;
    /** The zlib stream.  */
    z_stream            Zlib;
    /** The data buffer.  */
    uint8_t             abBuffer[_64K];
    /** Scatter gather segment describing abBuffer. */
    RTSGSEG             SgSeg;
    /** Scatter gather buffer describing abBuffer. */
    RTSGBUF             SgBuf;
    /** The original file name (decompressor only). */
    char               *pszOrgName;
    /** The comment (decompressor only). */
    char               *pszComment;
    /** The gzip header. */
    RTZIPGZIPHDR        Hdr;
} RTZIPGZIPSTREAM;
/** Pointer to a the internal data of a GZIP I/O stream. */
typedef RTZIPGZIPSTREAM *PRTZIPGZIPSTREAM;


/**
 * Convert from zlib to IPRT status codes.
 *
 * This will also set the fFatalError flag when appropriate.
 *
 * @returns IPRT status code.
 * @param   pThis           The gzip I/O stream instance data.
 * @param   rc              Zlib error code.
 */
static int rtZipGzipConvertErrFromZlib(PRTZIPGZIPSTREAM pThis, int rc)
{
    switch (rc)
    {
        case Z_OK:
            return VINF_SUCCESS;

        case Z_BUF_ERROR:
            /* This isn't fatal. */
            return VINF_SUCCESS;

        case Z_ERRNO:
        case Z_STREAM_ERROR:
        case Z_DATA_ERROR:
        case Z_MEM_ERROR:
        case Z_VERSION_ERROR:
            pThis->fFatalError = true;
            switch (rc)
            {
                case Z_ERRNO:           return VERR_INTERNAL_ERROR_5;
                case Z_STREAM_ERROR:    return VERR_INTERNAL_ERROR_3;
                case Z_DATA_ERROR:      return VERR_ZIP_ERROR;
                case Z_MEM_ERROR:       return VERR_ZIP_NO_MEMORY;
                case Z_VERSION_ERROR:   return VERR_ZIP_UNSUPPORTED_VERSION;
            }
            /* not reached */

        default:
            if (rc >= 0)
                return VINF_SUCCESS;
            pThis->fFatalError = true;
            return VERR_ZIP_ERROR;
    }
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtZipGzip_Close(void *pvThis)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;

    int rc;
    if (pThis->fDecompress)
        rc = inflateEnd(&pThis->Zlib);
    else
        rc = deflateEnd(&pThis->Zlib);
    if (rc != Z_OK)
        rc = rtZipGzipConvertErrFromZlib(pThis, rc);

    RTVfsIoStrmRelease(pThis->hVfsIos);
    pThis->hVfsIos = NIL_RTVFSIOSTREAM;
    RTStrFree(pThis->pszOrgName);
    pThis->pszOrgName = NULL;
    RTStrFree(pThis->pszComment);
    pThis->pszComment = NULL;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtZipGzip_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;
    return RTVfsIoStrmQueryInfo(pThis->hVfsIos, pObjInfo, enmAddAttr);
}


/**
 * Reads one segment.
 *
 * @returns IPRT status code.
 * @param   pThis           The gzip I/O stream instance data.
 * @param   pvBuf           Where to put the read bytes.
 * @param   cbToRead        The number of bytes to read.
 * @param   fBlocking       Whether to block or not.
 * @param   pcbRead         Where to store the number of bytes actually read.
 */
static int rtZipGzip_ReadOneSeg(PRTZIPGZIPSTREAM pThis, void *pvBuf, size_t cbToRead, bool fBlocking, size_t *pcbRead)
{
    /*
     * This simplifies life a wee bit below.
     */
    if (pThis->fEndOfStream)
        return pcbRead ? VINF_EOF : VERR_EOF;

    /*
     * Set up the output buffer.
     */
    pThis->Zlib.next_out  = (Bytef *)pvBuf;
    pThis->Zlib.avail_out = (uInt)cbToRead;
    AssertReturn(pThis->Zlib.avail_out == cbToRead, VERR_OUT_OF_RANGE);

    /*
     * Be greedy reading input, even if no output buffer is left. It's possible
     * that it's just the end of stream marker which needs to be read. Happens
     * for incompressible blocks just larger than the input buffer size.
     */
    int rc = VINF_SUCCESS;
    while (   pThis->Zlib.avail_out > 0
           || pThis->Zlib.avail_in == 0 /* greedy */)
    {
        /*
         * Read more input?
         *
         * N.B. The assertions here validate the RTVfsIoStrmSgRead behavior
         *      since the API is new and untested.  They could be removed later
         *      but, better leaving them in.
         */
        if (pThis->Zlib.avail_in == 0)
        {
            size_t cbReadIn = ~(size_t)0;
            rc = RTVfsIoStrmSgRead(pThis->hVfsIos, &pThis->SgBuf, fBlocking, &cbReadIn);
            if (rc != VINF_SUCCESS)
            {
                AssertMsg(RT_FAILURE(rc) || rc == VINF_TRY_AGAIN || rc == VINF_EOF, ("%Rrc\n", rc));
                if (rc == VERR_INTERRUPTED)
                {
                    Assert(cbReadIn == 0);
                    continue;
                }
                if (RT_FAILURE(rc) || rc == VINF_TRY_AGAIN || cbReadIn == 0)
                {
                    Assert(cbReadIn == 0);
                    break;
                }
                AssertMsg(rc == VINF_EOF, ("%Rrc\n", rc));
            }
            AssertMsgBreakStmt(cbReadIn > 0 && cbReadIn <= sizeof(pThis->abBuffer), ("%zu %Rrc\n", cbReadIn, rc),
                               rc = VERR_INTERNAL_ERROR_4);

            pThis->Zlib.avail_in = (uInt)cbReadIn;
            pThis->Zlib.next_in  = &pThis->abBuffer[0];
        }

        /*
         * Pass it on to zlib.
         */
        rc = inflate(&pThis->Zlib, Z_NO_FLUSH);
        if (rc != Z_OK && rc != Z_BUF_ERROR)
        {
            if (rc == Z_STREAM_END)
            {
                pThis->fEndOfStream = true;
                if (pThis->Zlib.avail_out == 0)
                    rc = VINF_SUCCESS;
                else
                    rc = pcbRead ? VINF_EOF : VERR_EOF;
            }
            else
                rc = rtZipGzipConvertErrFromZlib(pThis, rc);
            break;
        }
        rc = VINF_SUCCESS;
    }

    /*
     * Update the read counters before returning.
     */
    size_t const cbRead = cbToRead - pThis->Zlib.avail_out;
    pThis->offStream += cbRead;
    if (pcbRead)
        *pcbRead      = cbRead;

    return rc;
}

/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtZipGzip_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;
    int              rc;

    if (!pThis->fDecompress)
        return VERR_ACCESS_DENIED;

    if (pSgBuf->cSegs == 1)
        rc = rtZipGzip_ReadOneSeg(pThis, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, fBlocking, pcbRead);
    else
    {
        rc = VINF_SUCCESS;
        size_t  cbRead = 0;
        size_t  cbReadSeg;
        size_t *pcbReadSeg = pcbRead ? &cbReadSeg : NULL;
        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            cbReadSeg = 0;
            rc = rtZipGzip_ReadOneSeg(pThis, pSgBuf->paSegs[iSeg].pvSeg, pSgBuf->paSegs[iSeg].cbSeg, fBlocking, pcbReadSeg);
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
static DECLCALLBACK(int) rtZipGzip_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;
    //int              rc;

    NOREF(fBlocking);
    if (pThis->fDecompress)
        return VERR_ACCESS_DENIED;

    /** @todo implement compression. */
    return VERR_NOT_IMPLEMENTED;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtZipGzip_Flush(void *pvThis)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;
    return RTVfsIoStrmFlush(pThis->hVfsIos);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtZipGzip_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                           uint32_t *pfRetEvents)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;

    /*
     * Collect our own events first and see if that satisfies the request.  If
     * not forward the call to the compressed stream.
     */
    uint32_t fRetEvents = 0;
    if (pThis->fFatalError)
        fRetEvents |= RTPOLL_EVT_ERROR;
    if (pThis->fDecompress)
    {
        fEvents &= ~RTPOLL_EVT_WRITE;
        if (pThis->Zlib.avail_in > 0)
            fRetEvents = RTPOLL_EVT_READ;
    }
    else
    {
        fEvents &= ~RTPOLL_EVT_READ;
        if (pThis->Zlib.avail_out > 0)
            fRetEvents = RTPOLL_EVT_WRITE;
    }

    int rc = VINF_SUCCESS;
    fRetEvents &= fEvents;
    if (!fRetEvents)
        rc = RTVfsIoStrmPoll(pThis->hVfsIos, fEvents, cMillies, fIntr, pfRetEvents);
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtZipGzip_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTZIPGZIPSTREAM pThis = (PRTZIPGZIPSTREAM)pvThis;
    *poffActual = pThis->offStream;
    return VINF_SUCCESS;
}


/**
 * The GZIP I/O stream vtable.
 */
static RTVFSIOSTREAMOPS g_rtZipGzipOps =
{
    { /* Obj */
        RTVFSOBJOPS_VERSION,
        RTVFSOBJTYPE_IO_STREAM,
        "gzip",
        rtZipGzip_Close,
        rtZipGzip_QueryInfo,
        RTVFSOBJOPS_VERSION
    },
    RTVFSIOSTREAMOPS_VERSION,
    0,
    rtZipGzip_Read,
    rtZipGzip_Write,
    rtZipGzip_Flush,
    rtZipGzip_PollOne,
    rtZipGzip_Tell,
    NULL /* Skip */,
    NULL /*ZeroFill*/,
    RTVFSIOSTREAMOPS_VERSION,
};


RTDECL(int) RTZipGzipDecompressIoStream(RTVFSIOSTREAM hVfsIosIn, uint32_t fFlags, PRTVFSIOSTREAM phVfsIosOut)
{
    AssertPtrReturn(hVfsIosIn, VERR_INVALID_HANDLE);
    AssertReturn(!fFlags, VERR_INVALID_PARAMETER);
    AssertPtrReturn(phVfsIosOut, VERR_INVALID_POINTER);

    uint32_t cRefs = RTVfsIoStrmRetain(hVfsIosIn);
    AssertReturn(cRefs != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Create the decompression I/O stream.
     */
    RTVFSIOSTREAM    hVfsIos;
    PRTZIPGZIPSTREAM pThis;
    int rc = RTVfsNewIoStream(&g_rtZipGzipOps, sizeof(RTZIPGZIPSTREAM), RTFILE_O_READ, NIL_RTVFS, NIL_RTVFSLOCK,
                              &hVfsIos, (void **)&pThis);
    if (RT_SUCCESS(rc))
    {
        pThis->hVfsIos      = hVfsIosIn;
        pThis->offStream    = 0;
        pThis->fDecompress  = true;
        pThis->SgSeg.pvSeg  = &pThis->abBuffer[0];
        pThis->SgSeg.cbSeg  = sizeof(pThis->abBuffer);
        RTSgBufInit(&pThis->SgBuf, &pThis->SgSeg, 1);

        memset(&pThis->Zlib, 0, sizeof(pThis->Zlib));
        pThis->Zlib.opaque  = pThis;
        rc = inflateInit2(&pThis->Zlib, MAX_WBITS + 16 /* autodetect gzip header */);
        if (rc >= 0)
        {
            /*
             * Read the gzip header from the input stream to check that it's
             * a gzip stream.
             *
             * Note!. Since we've told zlib to check for the gzip header, we
             *        prebuffer what we read in the input buffer so it can
             *        be handed on to zlib later on.
             */
            rc = RTVfsIoStrmRead(pThis->hVfsIos, pThis->abBuffer, sizeof(RTZIPGZIPHDR), true /*fBlocking*/, NULL /*pcbRead*/);
            if (RT_SUCCESS(rc))
            {
                /* Validate the header and make a copy of it. */
                PCRTZIPGZIPHDR pHdr = (PCRTZIPGZIPHDR)pThis->abBuffer;
                if (   pHdr->bId1 != RTZIPGZIPHDR_ID1
                    || pHdr->bId2 != RTZIPGZIPHDR_ID2
                    || pHdr->fFlags & ~RTZIPGZIPHDR_FLG_VALID_MASK)
                    rc = VERR_ZIP_BAD_HEADER;
                else if (pHdr->bCompressionMethod != RTZIPGZIPHDR_CM_DEFLATE)
                    rc = VERR_ZIP_UNSUPPORTED_METHOD;
                else
                {
                    pThis->Hdr = *pHdr;
                    pThis->Zlib.avail_in = sizeof(RTZIPGZIPHDR);
                    pThis->Zlib.next_in  = &pThis->abBuffer[0];

                    /* Parse on if there are names or comments. */
                    if (pHdr->fFlags & (RTZIPGZIPHDR_FLG_NAME | RTZIPGZIPHDR_FLG_COMMENT))
                    {
                        /** @todo Can implement this when someone needs the
                         *        name or comment for something useful. */
                    }
                    if (RT_SUCCESS(rc))
                    {
                        *phVfsIosOut = hVfsIos;
                        return VINF_SUCCESS;
                    }
                }
            }
        }
        else
            rc = rtZipGzipConvertErrFromZlib(pThis, rc); /** @todo cleaning up in this situation is going to go wrong. */
        RTVfsIoStrmRelease(hVfsIos);
    }
    else
        RTVfsIoStrmRelease(hVfsIosIn);
    return rc;
}

