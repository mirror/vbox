/* $Id$ */
/** @file
 * IPRT - File I/O.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/file.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/alloca.h>
#include <iprt/err.h>
#include "internal/file.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Set of forced set open flags for files opened read-only. */
static unsigned g_fOpenReadSet = 0;

/** Set of forced cleared open flags for files opened read-only. */
static unsigned g_fOpenReadMask = 0;

/** Set of forced set open flags for files opened write-only. */
static unsigned g_fOpenWriteSet = 0;

/** Set of forced cleared open flags for files opened write-only. */
static unsigned g_fOpenWriteMask = 0;

/** Set of forced set open flags for files opened read-write. */
static unsigned g_fOpenReadWriteSet = 0;

/** Set of forced cleared open flags for files opened read-write. */
static unsigned g_fOpenReadWriteMask = 0;


/**
 * Force the use of open flags for all files opened after the setting is
 * changed. The caller is responsible for not causing races with RTFileOpen().
 *
 * @returns iprt status code.
 * @param   fOpenForAccess  Access mode to which the set/mask settings apply.
 * @param   fSet            Open flags to be forced set.
 * @param   fMask           Open flags to be masked out.
 */
RTR3DECL(int)  RTFileSetForceFlags(unsigned fOpenForAccess, unsigned fSet, unsigned fMask)
{
    /*
     * For now allow only RTFILE_O_WRITE_THROUGH. The other flags either
     * make no sense in this context or are not useful to apply to all files.
     */
    if ((fSet | fMask) & ~RTFILE_O_WRITE_THROUGH)
        return VERR_INVALID_PARAMETER;
    switch (fOpenForAccess)
    {
        case RTFILE_O_READ:
            g_fOpenReadSet = fSet;
            g_fOpenReadMask = fMask;
            break;
        case RTFILE_O_WRITE:
            g_fOpenWriteSet = fSet;
            g_fOpenWriteMask = fMask;
            break;
        case RTFILE_O_READWRITE:
            g_fOpenReadWriteSet = fSet;
            g_fOpenReadWriteMask = fMask;
            break;
        default:
            AssertMsgFailed(("Invalid access mode %d\n", fOpenForAccess));
            return VERR_INVALID_PARAMETER;
    }
    return VINF_SUCCESS;
}


/**
 * Adjusts and validates the flags.
 *
 * The adjustments are made according to the wishes specified using the RTFileSetForceFlags API.
 *
 * @returns IPRT status code.
 * @param   pfOpen      Pointer to the user specified flags on input.
 *                      Updated on successful return.
 * @internal
 */
int rtFileRecalcAndValidateFlags(unsigned *pfOpen)
{
    /*
     * Recalc.
     */
    unsigned fOpen = *pfOpen;
    switch (fOpen & RTFILE_O_ACCESS_MASK)
    {
        case RTFILE_O_READ:
            fOpen |= g_fOpenReadSet;
            fOpen &= ~g_fOpenReadMask;
            break;
        case RTFILE_O_WRITE:
            fOpen |= g_fOpenWriteSet;
            fOpen &= ~g_fOpenWriteMask;
            break;
        case RTFILE_O_READWRITE:
            fOpen |= g_fOpenReadWriteSet;
            fOpen &= ~g_fOpenReadWriteMask;
            break;
        default:
            AssertMsgFailed(("RTFileOpen received an invalid RW value, fOpen=%#x\n", fOpen));
            return VERR_INVALID_PARAMETER;
    }

    /*
     * Validate                                                                                                                                       .
     */
    if (    !(fOpen & RTFILE_O_ACCESS_MASK)
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
        ||  (fOpen & (~RTFILE_O_VALID_MASK | RTFILE_O_NON_BLOCK))
#else
        ||  (fOpen & ~RTFILE_O_VALID_MASK)
#endif
        ||  (fOpen & (RTFILE_O_TRUNCATE | RTFILE_O_WRITE)) == RTFILE_O_TRUNCATE
        ||  (   fOpen & RTFILE_O_NOT_CONTENT_INDEXED
             && !(   (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_OPEN_CREATE
                  || (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE
                  || (fOpen & RTFILE_O_ACTION_MASK) == RTFILE_O_CREATE_REPLACE))
       )
    {
        AssertMsgFailed(("Invalid parameters! fOpen=%#x\n", fOpen));
        return VERR_INVALID_PARAMETER;
    }

    /* done */
    *pfOpen = fOpen;
    return VINF_SUCCESS;
}



/**
 * Read bytes from a file at a given offset.
 * This function may modify the file position.
 *
 * @returns iprt status code.
 * @param   File        Handle to the file.
 * @param   off         Where to read.
 * @param   pvBuf       Where to put the bytes we read.
 * @param   cbToRead    How much to read.
 * @param   *pcbRead    How much we actually read.
 *                      If NULL an error will be returned for a partial read.
 */
RTR3DECL(int)  RTFileReadAt(RTFILE File, RTFOFF off, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    int rc = RTFileSeek(File, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
        rc = RTFileRead(File, pvBuf, cbToRead, pcbRead);
    return rc;
}


/**
 * Write bytes to a file at a given offset.
 * This function may modify the file position.
 *
 * @returns iprt status code.
 * @param   File        Handle to the file.
 * @param   off         Where to write.
 * @param   pvBuf       What to write.
 * @param   cbToWrite   How much to write.
 * @param   *pcbWritten How much we actually wrote.
 *                      If NULL an error will be returned for a partial write.
 */
RTR3DECL(int)  RTFileWriteAt(RTFILE File, RTFOFF off, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    int rc = RTFileSeek(File, off, RTFILE_SEEK_BEGIN, NULL);
    if (RT_SUCCESS(rc))
        rc = RTFileWrite(File, pvBuf, cbToWrite, pcbWritten);
    return rc;
}


/**
 * Gets the current file position.
 *
 * @returns File offset.
 * @returns ~0UUL on failure.
 * @param   File        File handle.
 */
RTR3DECL(uint64_t)  RTFileTell(RTFILE File)
{
    /*
     * Call the seek api to query the stuff.
     */
    uint64_t off = 0;
    int rc = RTFileSeek(File, 0, RTFILE_SEEK_CURRENT, &off);
    if (RT_SUCCESS(rc))
        return off;
    AssertMsgFailed(("RTFileSeek(%d) -> %d\n", File, rc));
    return ~0ULL;
}


/**
 * Determine the maximum file size.
 *
 * @returns The max size of the file.
 *          -1 on failure, the file position is undefined.
 * @param   File        Handle to the file.
 * @see     RTFileGetMaxSizeEx.
 */
RTR3DECL(RTFOFF) RTFileGetMaxSize(RTFILE File)
{
    RTFOFF cbMax;
    int rc = RTFileGetMaxSizeEx(File, &cbMax);
    return RT_SUCCESS(rc) ? cbMax : -1;
}


/**
 * Copies a file given the handles to both files.
 *
 * @returns VBox Status code.
 *
 * @param   FileSrc     The source file. The file position is unaltered.
 * @param   FileDst     The destination file.
 *                      On successful returns the file position is at the end of the file.
 *                      On failures the file position and size is undefined.
 */
RTDECL(int) RTFileCopyByHandles(RTFILE FileSrc, RTFILE FileDst)
{
    return RTFileCopyByHandlesEx(FileSrc, FileDst, NULL, NULL);
}


/**
 * Copies a file.
 *
 * @returns VERR_ALREADY_EXISTS if the destination file exists.
 * @returns VBox Status code.
 *
 * @param   pszSrc      The path to the source file.
 * @param   pszDst      The path to the destination file.
 *                      This file will be created.
 * @param   fFlags      Flags, any of the RTFILECOPY_FLAGS_ \#defines.
 * @param   pfnProgress Pointer to callback function for reporting progress.
 * @param   pvUser      User argument to pass to pfnProgress along with the completion precentage.
 */
RTDECL(int) RTFileCopyEx(const char *pszSrc, const char *pszDst, uint32_t fFlags, PFNRTPROGRESS pfnProgress, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("pszSrc=%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszSrc, ("pszSrc=%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pszDst), ("pszDst=%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszDst, ("pszDst=%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!pfnProgress || VALID_PTR(pfnProgress), ("pfnProgress=%p\n", pfnProgress), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fFlags & ~RTFILECOPY_FLAGS_MASK), ("%#x\n", fFlags), VERR_INVALID_PARAMETER);

    /*
     * Open the files.
     */
    RTFILE FileSrc;
    int rc = RTFileOpen(&FileSrc, pszSrc,
                        RTFILE_O_READ | (fFlags & RTFILECOPY_FLAGS_NO_SRC_DENY_WRITE ? 0 : RTFILE_O_DENY_WRITE) | RTFILE_O_OPEN);
    if (RT_SUCCESS(rc))
    {
        RTFILE FileDst;
        rc = RTFileOpen(&FileDst, pszDst,
                        RTFILE_O_WRITE | (fFlags & RTFILECOPY_FLAGS_NO_DST_DENY_WRITE ? 0 : RTFILE_O_DENY_WRITE) | RTFILE_O_CREATE);
        if (RT_SUCCESS(rc))
        {
            /*
             * Call the ByHandles version and let it do the job.
             */
            rc = RTFileCopyByHandlesEx(FileSrc, FileDst, pfnProgress, pvUser);

            /*
             * Close the files regardless of the result.
             * Don't bother cleaning up or anything like that.
             */
            int rc2 = RTFileClose(FileDst);
            AssertRC(rc2);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }

        int rc2 = RTFileClose(FileSrc);
        AssertRC(rc2);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


/**
 * Copies a file given the handles to both files and
 * provide progress callbacks.
 *
 * @returns VBox Status code.
 *
 * @param   FileSrc     The source file. The file position is unaltered.
 * @param   FileDst     The destination file.
 *                      On successful returns the file position is at the end of the file.
 *                      On failures the file position and size is undefined.
 * @param   pfnProgress Pointer to callback function for reporting progress.
 * @param   pvUser      User argument to pass to pfnProgress along with the completion precentage.
 */
RTDECL(int) RTFileCopyByHandlesEx(RTFILE FileSrc, RTFILE FileDst, PFNRTPROGRESS pfnProgress, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(RTFileIsValid(FileSrc), ("FileSrc=%RTfile\n", FileSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(RTFileIsValid(FileDst), ("FileDst=%RTfile\n", FileDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!pfnProgress || VALID_PTR(pfnProgress), ("pfnProgress=%p\n", pfnProgress), VERR_INVALID_PARAMETER);

    /*
     * Save file offset.
     */
    RTFOFF offSrcSaved;
    int rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_CURRENT, (uint64_t *)&offSrcSaved);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Get the file size.
     */
    RTFOFF cbSrc;
    rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_END, (uint64_t *)&cbSrc);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Allocate buffer.
     */
    size_t      cbBuf;
    uint8_t    *pbBufFree = NULL;
    uint8_t    *pbBuf;
    if (cbSrc < _512K)
    {
        cbBuf = 8*_1K;
        pbBuf = (uint8_t *)alloca(cbBuf);
    }
    else
    {
        cbBuf = _128K;
        pbBuf = pbBufFree = (uint8_t *)RTMemTmpAlloc(cbBuf);
    }
    if (pbBuf)
    {
        /*
         * Seek to the start of each file
         * and set the size of the destination file.
         */
        rc = RTFileSeek(FileSrc, 0, RTFILE_SEEK_BEGIN, NULL);
        if (RT_SUCCESS(rc))
        {
            rc = RTFileSeek(FileDst, 0, RTFILE_SEEK_BEGIN, NULL);
            if (RT_SUCCESS(rc))
                rc = RTFileSetSize(FileDst, cbSrc);
            if (RT_SUCCESS(rc) && pfnProgress)
                rc = pfnProgress(0, pvUser);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Copy loop.
                 */
                unsigned    uPercentage = 0;
                RTFOFF      off = 0;
                RTFOFF      cbPercent = cbSrc / 100;
                RTFOFF      offNextPercent = cbPercent;
                while (off < cbSrc)
                {
                    /* copy block */
                    RTFOFF cbLeft = cbSrc - off;
                    size_t cbBlock = cbLeft >= (RTFOFF)cbBuf ? cbBuf : (size_t)cbLeft;
                    rc = RTFileRead(FileSrc, pbBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;
                    rc = RTFileWrite(FileDst, pbBuf, cbBlock, NULL);
                    if (RT_FAILURE(rc))
                        break;

                    /* advance */
                    off += cbBlock;
                    if (pfnProgress && offNextPercent < off)
                    {
                        while (offNextPercent < off)
                        {
                            uPercentage++;
                            offNextPercent += cbPercent;
                        }
                        rc = pfnProgress(uPercentage, pvUser);
                        if (RT_FAILURE(rc))
                            break;
                    }
                }

#if 0
                /*
                 * Copy OS specific data (EAs and stuff).
                 */
                rtFileCopyOSStuff(FileSrc, FileDst);
#endif

                /* 100% */
                if (pfnProgress && uPercentage < 100 && RT_SUCCESS(rc))
                    rc = pfnProgress(100, pvUser);
            }
        }
        RTMemTmpFree(pbBufFree);
    }
    else
        rc = VERR_NO_MEMORY;

    /*
     * Restore source position.
     */
    RTFileSeek(FileSrc, offSrcSaved, RTFILE_SEEK_BEGIN, NULL);

    return rc;
}

