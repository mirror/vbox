/* $Id$ */
/** @file
 * IPRT - Virtual File System, Standard File Implementation.
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
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/vfs.h>
#include <iprt/vfslowlevel.h>

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/poll.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Private data of a standard file.
 */
typedef struct RTVFSSTDFILE
{
    /** The file handle. */
    RTFILE          hFile;
    /** Whether to leave the handle open when the VFS handle is closed. */
    bool            fLeaveOpen;
} RTVFSSTDFILE;
/** Pointer to the private data of a standard file. */
typedef RTVFSSTDFILE *PRTVFSSTDFILE;


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnClose}
 */
static DECLCALLBACK(int) rtVfsStdFile_Close(void *pvThis)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;

    int rc;
    if (!pThis->fLeaveOpen)
        rc = RTFileClose(pThis->hFile);
    else
        rc = VINF_SUCCESS;
    pThis->hFile = NIL_RTFILE;

    return rc;
}


/**
 * @interface_method_impl{RTVFSOBJOPS,pfnQueryInfo}
 */
static DECLCALLBACK(int) rtVfsStdFile_QueryInfo(void *pvThis, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAddAttr)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileQueryInfo(pThis->hFile, pObjInfo, enmAddAttr);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnRead}
 */
static DECLCALLBACK(int) rtVfsStdFile_Read(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbRead)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int           rc;

    NOREF(fBlocking);
    if (pSgBuf->cSegs == 1)
    {
        if (off < 0)
            rc = RTFileRead(pThis->hFile, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
        else
            rc = RTFileReadAt(pThis->hFile, off, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbRead);
    }
    else
    {
        size_t  cbRead     = 0;
        size_t  cbReadSeg;
        size_t *pcbReadSeg = pcbRead ? &cbReadSeg : NULL;
        rc = VINF_SUCCESS;

        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            void   *pvSeg  = pSgBuf->paSegs[iSeg].pvSeg;
            size_t  cbSeg  = pSgBuf->paSegs[iSeg].cbSeg;

            cbReadSeg = 0;
            if (off < 0)
                rc = RTFileRead(pThis->hFile, pvSeg, cbSeg, pcbReadSeg);
            else
            {
                rc = RTFileReadAt(pThis->hFile, off, pvSeg, cbSeg, pcbReadSeg);
                off += cbSeg;
            }
            if (RT_FAILURE(rc))
                break;
            if (pcbRead)
            {
                cbRead += cbReadSeg;
                if (cbReadSeg != cbSeg)
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
static DECLCALLBACK(int) rtVfsStdFile_Write(void *pvThis, RTFOFF off, PCRTSGBUF pSgBuf, bool fBlocking, size_t *pcbWritten)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int           rc;

    NOREF(fBlocking);
    if (pSgBuf->cSegs == 1)
    {
        if (off < 0)
            rc = RTFileWrite(pThis->hFile, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
        else
            rc = RTFileWriteAt(pThis->hFile, off, pSgBuf->paSegs[0].pvSeg, pSgBuf->paSegs[0].cbSeg, pcbWritten);
    }
    else
    {
        size_t  cbWritten     = 0;
        size_t  cbWrittenSeg;
        size_t *pcbWrittenSeg = pcbWritten ? &cbWrittenSeg : NULL;
        rc = VINF_SUCCESS;

        for (uint32_t iSeg = 0; iSeg < pSgBuf->cSegs; iSeg++)
        {
            void   *pvSeg  = pSgBuf->paSegs[iSeg].pvSeg;
            size_t  cbSeg  = pSgBuf->paSegs[iSeg].cbSeg;

            cbWrittenSeg = 0;
            if (off < 0)
                rc = RTFileWrite(pThis->hFile, pvSeg, cbSeg, pcbWrittenSeg);
            else
            {
                rc = RTFileWriteAt(pThis->hFile, off, pvSeg, cbSeg, pcbWrittenSeg);
                off += cbSeg;
            }
            if (RT_FAILURE(rc))
                break;
            if (pcbWritten)
            {
                cbWritten += cbWrittenSeg;
                if (cbWrittenSeg != cbSeg)
                    break;
            }
        }

        if (pcbWritten)
            *pcbWritten = cbWritten;
    }

    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnFlush}
 */
static DECLCALLBACK(int) rtVfsStdFile_Flush(void *pvThis)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileFlush(pThis->hFile);
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnPollOne}
 */
static DECLCALLBACK(int) rtVfsStdFile_PollOne(void *pvThis, uint32_t fEvents, RTMSINTERVAL cMillies, bool fIntr,
                                              uint32_t *pfRetEvents)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    int           rc;

    if (fEvents != RTPOLL_EVT_ERROR)
    {
        *pfRetEvents = fEvents & ~RTPOLL_EVT_ERROR;
        rc = VINF_SUCCESS;
    }
    else if (fIntr)
        rc = RTThreadSleep(cMillies);
    else
    {
        uint64_t uMsStart = RTTimeMilliTS();
        do
            rc = RTThreadSleep(cMillies);
        while (   rc == VERR_INTERRUPTED
               && !fIntr
               && RTTimeMilliTS() - uMsStart < cMillies);
        if (rc == VERR_INTERRUPTED)
            rc = VERR_TIMEOUT;
    }
    return rc;
}


/**
 * @interface_method_impl{RTVFSIOSTREAMOPS,pfnTell}
 */
static DECLCALLBACK(int) rtVfsStdFile_Tell(void *pvThis, PRTFOFF poffActual)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileTell(pThis->hFile);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnMode}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetMode(void *pvThis, RTFMODE fMode, RTFMODE fMask)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    if (fMask != ~RTFS_TYPE_MASK)
    {
#if 0
        RTFMODE fCurMode;
        int rc = RTFileGetMode(pThis->hFile, &fCurMode);
        if (RT_FAILURE(rc))
            return rc;
        fMode |= ~fMask & fCurMode;
#else
        RTFSOBJINFO ObjInfo;
        int rc = RTFileQueryInfo(pThis->hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
        if (RT_FAILURE(rc))
            return rc;
        fMode |= ~fMask & ObjInfo.Attr.fMode;
#endif
    }
    return RTFileSetMode(pThis->hFile, fMode);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetTimes}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetTimes(void *pvThis, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                                               PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileSetTimes(pThis->hFile, pAccessTime, pModificationTime, pChangeTime, pBirthTime);
}


/**
 * @interface_method_impl{RTVFSOBJSETOPS,pfnSetOwner}
 */
static DECLCALLBACK(int) rtVfsStdFile_SetOwner(void *pvThis, RTUID uid, RTGID gid)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
#if 0
    return RTFileSetOwner(pThis->hFile, uid, gid);
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnSeek}
 */
static DECLCALLBACK(int) rtVfsStdFile_Seek(void *pvThis, RTFOFF offSeek, unsigned uMethod, PRTFOFF poffActual)
{
    PRTVFSSTDFILE pThis     = (PRTVFSSTDFILE)pvThis;
    uint64_t      offActual = 0;
    int rc = RTFileSeek(pThis->hFile, offSeek, uMethod, &offActual);
    if (RT_SUCCESS(rc))
        *poffActual = offActual;
    return rc;
}


/**
 * @interface_method_impl{RTVFSFILEOPS,pfnQuerySize}
 */
static DECLCALLBACK(int) rtVfsStdFile_QuerySize(void *pvThis, uint64_t *pcbFile)
{
    PRTVFSSTDFILE pThis = (PRTVFSSTDFILE)pvThis;
    return RTFileGetSize(pThis->hFile, pcbFile);
}


/**
 * Standard file operations.
 */
DECLHIDDEN(const RTVFSFILEOPS) g_rtVfsStdFileOps =
{
    { /* Stream */
        /** The basic object operation.  */
        { /* Obj */
            RTVFSOBJOPS_VERSION,
            RTVFSOBJTYPE_FILE,
            "StdFile",
            rtVfsStdFile_Close,
            rtVfsStdFile_QueryInfo,
            RTVFSOBJOPS_VERSION
        },
        RTVFSIOSTREAMOPS_VERSION,
        0,
        rtVfsStdFile_Read,
        rtVfsStdFile_Write,
        rtVfsStdFile_Flush,
        rtVfsStdFile_PollOne,
        rtVfsStdFile_Tell,
        RTVFSIOSTREAMOPS_VERSION,
    },
    RTVFSFILEOPS_VERSION,
    0,
    { /* ObjSet */
        RTVFSOBJSETOPS_VERSION,
        RT_OFFSETOF(RTVFSFILEOPS, Stream.Obj) - RT_OFFSETOF(RTVFSFILEOPS, ObjSet),
        rtVfsStdFile_SetMode,
        rtVfsStdFile_SetTimes,
        rtVfsStdFile_SetOwner,
        RTVFSOBJSETOPS_VERSION
    },
    rtVfsStdFile_Seek,
    rtVfsStdFile_QuerySize,
    RTVFSFILEOPS_VERSION
};


RTDECL(int) RTVfsFileFromRTFile(RTFILE hFile, uint32_t fOpen, bool fLeaveOpen, PRTVFSFILE phVfsFile)
{
    /*
     * Check the handle validity.
     */
    RTFSOBJINFO ObjInfo;
    int rc = RTFileQueryInfo(hFile, &ObjInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Set up some fake fOpen flags.
     */
    if (!fOpen)
        fOpen = RTFILE_O_READWRITE | RTFILE_O_DENY_NONE | RTFILE_O_OPEN_CREATE;

    /*
     * Create the handle.
     */
    PRTVFSSTDFILE   pThis;
    RTVFSFILE       hVfsFile;
    rc = RTVfsNewFile(&g_rtVfsStdFileOps, sizeof(RTVFSSTDFILE), fOpen, NIL_RTVFS, &hVfsFile, (void **)&pThis);
    if (RT_FAILURE(rc))
        return rc;

    pThis->hFile        = hFile;
    pThis->fLeaveOpen   = fLeaveOpen;
    *phVfsFile = hVfsFile;
    return VINF_SUCCESS;
}

