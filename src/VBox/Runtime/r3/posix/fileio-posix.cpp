/* $Id$ */
/** @file
 * IPRT - File I/O, POSIX.
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
#define LOG_GROUP RTLOGGROUP_FILE

#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include <fcntl.h>
#ifdef _MSC_VER
# include <io.h>
# include <stdio.h>
#else
# include <unistd.h>
# include <sys/time.h>
#endif
#if defined(RT_OS_OS2) && (!defined(__INNOTEK_LIBC__) || __INNOTEK_LIBC__ < 0x006)
# include <io.h>
#endif
#ifdef RT_OS_L4
/* This is currently ifdef'ed out in the relevant L4 header file */
/* Same as `utimes', but takes an open file descriptor instead of a name.  */
extern int futimes(int __fd, __const struct timeval __tvp[2]) __THROW;
#endif

#ifdef RT_OS_SOLARIS
# define futimes(filedes, timeval)   futimesat(filedes, NULL, timeval)
#endif

#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include "internal/file.h"
#include "internal/fs.h"
#include "internal/path.h"



/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @def RT_DONT_CONVERT_FILENAMES
 * Define this to pass UTF-8 unconverted to the kernel. */
#ifdef __DOXYGEN__
#define RT_DONT_CONVERT_FILENAMES 1
#endif

/** Default file permissions for newly created files. */
#if defined(S_IRUSR) && defined(S_IWUSR)
# define RT_FILE_PERMISSION  (S_IRUSR | S_IWUSR)
#else
# define RT_FILE_PERMISSION  (00600)
#endif


RTR3DECL(int)  RTFileOpen(PRTFILE pFile, const char *pszFilename, unsigned fOpen)
{
    /*
     * Validate input.
     */
    if (!VALID_PTR(pFile))
    {
        AssertMsgFailed(("Invalid pFile %p\n", pFile));
        return VERR_INVALID_PARAMETER;
    }
    *pFile = NIL_RTFILE;
    if (!VALID_PTR(pszFilename))
    {
        AssertMsgFailed(("Invalid pszFilename %p\n", pszFilename));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Merge forced open flags and validate them.
     */
    int rc = rtFileRecalcAndValidateFlags(&fOpen);
    if (RT_FAILURE(rc))
        return rc;
#ifndef O_NONBLOCK
    if (fOpen & RTFILE_O_NON_BLOCK)
    {
        AssertMsgFailed(("Invalid parameters! fOpen=%#x\n", fOpen));
        return VERR_INVALID_PARAMETER;
    }
#endif

    /*
     * Calculate open mode flags.
     */
    int fOpenMode = 0;
#ifdef O_BINARY
    fOpenMode |= O_BINARY;              /* (pc) */
#endif
#ifdef O_LARGEFILE
    fOpenMode |= O_LARGEFILE;           /* (linux) */
#endif
#ifdef O_NOINHERIT
    if (!(fOpen & RTFILE_O_INHERIT))
        fOpenMode |= O_NOINHERIT;
#endif
#ifdef O_NONBLOCK
    if (fOpen & RTFILE_O_NON_BLOCK)
        fOpenMode |= O_NONBLOCK;
#endif
#ifdef O_SYNC
    if (fOpen & RTFILE_O_WRITE_THROUGH)
        fOpenMode |= O_SYNC;
#endif

    /* create/truncate file */
    switch (fOpen & RTFILE_O_ACTION_MASK)
    {
        case RTFILE_O_OPEN:             break;
        case RTFILE_O_OPEN_CREATE:      fOpenMode |= O_CREAT; break;
        case RTFILE_O_CREATE:           fOpenMode |= O_CREAT | O_EXCL; break;
        case RTFILE_O_CREATE_REPLACE:   fOpenMode |= O_CREAT | O_TRUNC; break; /** @todo replacing needs fixing, this is *not* a 1:1 mapping! */
    }
    if (fOpen & RTFILE_O_TRUNCATE)
        fOpenMode |= O_TRUNC;

    switch (fOpen & RTFILE_O_ACCESS_MASK)
    {
        case RTFILE_O_READ:             fOpenMode |= O_RDONLY; break;
        case RTFILE_O_WRITE:            fOpenMode |= O_WRONLY; break;
        case RTFILE_O_READWRITE:        fOpenMode |= O_RDWR; break;
        default:
            AssertMsgFailed(("RTFileOpen received an invalid RW value, fOpen=%#x\n", fOpen));
            return VERR_INVALID_PARAMETER;
    }

    /** @todo sharing! */

    /*
     * Open/create the file.
     */
#ifdef RT_DONT_CONVERT_FILENAMES
    int fh = open(pszFilename, fOpenMode, RT_FILE_PERMISSION);
    int iErr = errno;
#else
    char *pszNativeFilename;
    rc = rtPathToNative(&pszNativeFilename, pszFilename);
    if (RT_FAILURE(rc))
        return (rc);

    int fh = open(pszNativeFilename, fOpenMode, RT_FILE_PERMISSION);
    int iErr = errno;
    rtPathFreeNative(pszNativeFilename);
#endif
    if (fh >= 0)
    {
        /*
         * Mark the file handle close on exec, unless inherit is specified.
         */
        if (    !(fOpen & RTFILE_O_INHERIT)
#ifdef O_NOINHERIT
            ||  (fOpenMode & O_NOINHERIT) /* careful since it could be a dummy. */
#endif
            ||  fcntl(fh, F_SETFD, FD_CLOEXEC) >= 0)
        {
            *pFile = (RTFILE)fh;
            Assert((int)*pFile == fh);
            LogFlow(("RTFileOpen(%p:{%RTfile}, %p:{%s}, %#x): returns %Rrc\n",
                     pFile, *pFile, pszFilename, pszFilename, fOpen, rc));
            return VINF_SUCCESS;
        }
        iErr = errno;
        close(fh);
    }
    return RTErrConvertFromErrno(iErr);
}


RTR3DECL(int)  RTFileClose(RTFILE File)
{
    if (close((int)File) == 0)
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int) RTFileFromNative(PRTFILE pFile, RTHCINTPTR uNative)
{
    if (    uNative < 0
        ||  (RTFILE)uNative != (RTUINTPTR)uNative)
    {
        AssertMsgFailed(("%p\n", uNative));
        *pFile = NIL_RTFILE;
        return VERR_INVALID_HANDLE;
    }
    *pFile = (RTFILE)uNative;
    return VINF_SUCCESS;
}


RTR3DECL(RTHCINTPTR) RTFileToNative(RTFILE File)
{
    AssertReturn(File != NIL_RTFILE, -1);
    return (RTHCINTPTR)File;
}


RTR3DECL(int)  RTFileDelete(const char *pszFilename)
{
    char *pszNativeFilename;
    int rc = rtPathToNative(&pszNativeFilename, pszFilename);
    if (RT_SUCCESS(rc))
    {
        if (unlink(pszNativeFilename) != 0)
            rc = RTErrConvertFromErrno(errno);
        rtPathFreeNative(pszNativeFilename);
    }
    return rc;
}


RTR3DECL(int)  RTFileSeek(RTFILE File, int64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    static const unsigned aSeekRecode[] =
    {
        SEEK_SET,
        SEEK_CUR,
        SEEK_END,
    };

    /*
     * Validate input.
     */
    if (uMethod > RTFILE_SEEK_END)
    {
        AssertMsgFailed(("Invalid uMethod=%d\n", uMethod));
        return VERR_INVALID_PARAMETER;
    }

    /* check that within off_t range. */
    if (    sizeof(off_t) < sizeof(offSeek)
        && (    (offSeek > 0 && (unsigned)(offSeek >> 32) != 0)
            ||  (offSeek < 0 && (unsigned)(-offSeek >> 32) != 0)))
    {
        AssertMsgFailed(("64-bit search not supported\n"));
        return VERR_NOT_SUPPORTED;
    }

    off_t offCurrent = lseek((int)File, (off_t)offSeek, aSeekRecode[uMethod]);
    if (offCurrent != ~0)
    {
        if (poffActual)
            *poffActual = (uint64_t)offCurrent;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int)  RTFileRead(RTFILE File, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    if (cbToRead <= 0)
        return VINF_SUCCESS;

    /*
     * Attempt read.
     */
    ssize_t cbRead = read((int)File, pvBuf, cbToRead);
    if (cbRead >= 0)
    {
        if (pcbRead)
            /* caller can handle partial read. */
            *pcbRead = cbRead;
        else
        {
            /* Caller expects all to be read. */
            while ((ssize_t)cbToRead > cbRead)
            {
                ssize_t cbReadPart = read((int)File, (char*)pvBuf + cbRead, cbToRead - cbRead);
                if (cbReadPart <= 0)
                {
                    if (cbReadPart == 0)
                        return VERR_EOF;
                    return RTErrConvertFromErrno(errno);
                }
                cbRead += cbReadPart;
            }
        }
        return VINF_SUCCESS;
    }

    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int)  RTFileWrite(RTFILE File, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    if (cbToWrite <= 0)
        return VINF_SUCCESS;

    /*
     * Attempt write.
     */
    ssize_t cbWritten = write((int)File, pvBuf, cbToWrite);
    if (cbWritten >= 0)
    {
        if (pcbWritten)
            /* caller can handle partial write. */
            *pcbWritten = cbWritten;
        else
        {
            /* Caller expects all to be write. */
            while ((ssize_t)cbToWrite > cbWritten)
            {
                ssize_t cbWrittenPart = write((int)File, (const char *)pvBuf + cbWritten, cbToWrite - cbWritten);
                if (cbWrittenPart <= 0)
                    return RTErrConvertFromErrno(errno);
                cbWritten += cbWrittenPart;
            }
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int)  RTFileSetSize(RTFILE File, uint64_t cbSize)
{
    /*
     * Validate offset.
     */
    if (    sizeof(off_t) < sizeof(cbSize)
        &&  (cbSize >> 32) != 0)
    {
        AssertMsgFailed(("64-bit filesize not supported! cbSize=%lld\n", cbSize));
        return VERR_NOT_SUPPORTED;
    }

#if defined(_MSC_VER) || (defined(RT_OS_OS2) && (!defined(__INNOTEK_LIBC__) || __INNOTEK_LIBC__ < 0x006))
    if (chsize((int)File, (off_t)cbSize) == 0)
#else
    /* This relies on a non-standard feature of FreeBSD, Linux, and OS/2
     * LIBC v0.6 and higher. (SuS doesn't define ftruncate() and size bigger
     * than the file.)
     */
    if (ftruncate((int)File, (off_t)cbSize) == 0)
#endif
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(int)   RTFileGetSize(RTFILE File, uint64_t *pcbSize)
{
    struct stat st;
    if (!fstat((int)File, &st))
    {
        *pcbSize = st.st_size;
        return VINF_SUCCESS;
    }
    return RTErrConvertFromErrno(errno);
}


RTR3DECL(bool) RTFileIsValid(RTFILE File)
{
    if (File != NIL_RTFILE)
    {
        int fFlags = fcntl(File, F_GETFD);
        if (fFlags >= 0)
            return true;
    }
    return false;
}


RTR3DECL(int)  RTFileFlush(RTFILE File)
{
    if (fsync((int)File))
        return RTErrConvertFromErrno(errno);
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFileIoCtl(RTFILE File, int iRequest, void *pvData, unsigned cbData, int *piRet)
{
    int rc = ioctl((int)File, iRequest, pvData);
    if (piRet)
        *piRet = rc;
    return rc >= 0 ? VINF_SUCCESS : RTErrConvertFromErrno(errno);
}


RTR3DECL(int) RTFileQueryInfo(RTFILE File, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    /*
     * Validate input.
     */
    if (File == NIL_RTFILE)
    {
        AssertMsgFailed(("Invalid File=%RTfile\n", File));
        return VERR_INVALID_PARAMETER;
    }
    if (!pObjInfo)
    {
        AssertMsgFailed(("Invalid pObjInfo=%p\n", pObjInfo));
        return VERR_INVALID_PARAMETER;
    }
    if (    enmAdditionalAttribs < RTFSOBJATTRADD_NOTHING
        ||  enmAdditionalAttribs > RTFSOBJATTRADD_LAST)
    {
        AssertMsgFailed(("Invalid enmAdditionalAttribs=%p\n", enmAdditionalAttribs));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Query file info.
     */
    struct stat Stat;
    if (fstat((int)File, &Stat))
    {
        int rc = RTErrConvertFromErrno(errno);
        Log(("RTFileQueryInfo(%RTfile,,%d): returns %Rrc\n", File, enmAdditionalAttribs, rc));
        return rc;
    }

    /*
     * Setup the returned data.
     */
    rtFsConvertStatToObjInfo(pObjInfo, &Stat, NULL, 0);

    /*
     * Requested attributes (we cannot provide anything actually).
     */
    switch (enmAdditionalAttribs)
    {
        case RTFSOBJATTRADD_EASIZE:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_EASIZE;
            pObjInfo->Attr.u.EASize.cb            = 0;
            break;

        case RTFSOBJATTRADD_NOTHING:
        case RTFSOBJATTRADD_UNIX:
            /* done */
            break;

        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_INTERNAL_ERROR;
    }

    LogFlow(("RTFileQueryInfo(%RTfile,,%d): returns VINF_SUCCESS\n", File, enmAdditionalAttribs));
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFileSetTimes(RTFILE File, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    /*
     * We can only set AccessTime and ModificationTime, so if neither
     * are specified we can return immediately.
     */
    if (!pAccessTime && !pModificationTime)
        return VINF_SUCCESS;

    /*
     * Convert the input to timeval, getting the missing one if necessary,
     * and call the API which does the change.
     */
    struct timeval aTimevals[2];
    if (pAccessTime && pModificationTime)
    {
        RTTimeSpecGetTimeval(pAccessTime,       &aTimevals[0]);
        RTTimeSpecGetTimeval(pModificationTime, &aTimevals[1]);
    }
    else
    {
        RTFSOBJINFO ObjInfo;
        int rc = RTFileQueryInfo(File, &ObjInfo, RTFSOBJATTRADD_UNIX);
        if (RT_FAILURE(rc))
            return rc;
        RTTimeSpecGetTimeval(pAccessTime        ? pAccessTime       : &ObjInfo.AccessTime,       &aTimevals[0]);
        RTTimeSpecGetTimeval(pModificationTime  ? pModificationTime : &ObjInfo.ModificationTime, &aTimevals[1]);
    }

    if (futimes((int)File, aTimevals))
    {
        int rc = RTErrConvertFromErrno(errno);
        Log(("RTFileSetTimes(%RTfile,%p,%p,,): returns %Rrc\n", File, pAccessTime, pModificationTime, rc));
        return rc;
    }
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFileSetMode(RTFILE File, RTFMODE fMode)
{
    /*
     * Normalize the mode and call the API.
     */
    fMode = rtFsModeNormalize(fMode, NULL, 0);
    if (!rtFsModeIsValid(fMode))
        return VERR_INVALID_PARAMETER;

    if (fchmod((int)File, fMode & RTFS_UNIX_MASK))
    {
        int rc = RTErrConvertFromErrno(errno);
        Log(("RTFileSetMode(%RTfile,%RTfmode): returns %Rrc\n", File, fMode));
        return rc;
    }
    return VINF_SUCCESS;
}


RTR3DECL(int) RTFileRename(const char *pszSrc, const char *pszDst, unsigned fRename)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("%p\n", pszSrc), VERR_INVALID_POINTER);
    AssertMsgReturn(VALID_PTR(pszDst), ("%p\n", pszDst), VERR_INVALID_POINTER);
    AssertMsgReturn(*pszSrc, ("%p\n", pszSrc), VERR_INVALID_PARAMETER);
    AssertMsgReturn(*pszDst, ("%p\n", pszDst), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!(fRename & ~RTPATHRENAME_FLAGS_REPLACE), ("%#x\n", fRename), VERR_INVALID_PARAMETER);

    /*
     * Take common cause with RTPathRename.
     */
    int rc = rtPathPosixRename(pszSrc, pszDst, fRename, RTFS_TYPE_FILE);

    LogFlow(("RTDirRename(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n",
             pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;
}


