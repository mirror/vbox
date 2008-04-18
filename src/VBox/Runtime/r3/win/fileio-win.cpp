/* $Id$ */
/** @file
 * Incredibly Portable Runtime - File I/O, native implementation for the Windows host platform.
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
#define LOG_GROUP RTLOGGROUP_DIR
#include <Windows.h>

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
# define RT_DONT_CONVERT_FILENAMES 1
#endif


/**
 * This is wrapper around the ugly SetFilePointer api.
 *
 * It's equivalent to SetFilePointerEx which we so unfortunately cannot use because of
 * it not being present in NT4 GA.
 *
 * @returns Success indicator. Extended error information obtainable using GetLastError().
 * @param   File        Filehandle.
 * @param   offSeek     Offset to seek.
 * @param   poffNew     Where to store the new file offset. NULL allowed.
 * @param   uMethod     Seek method. (The windows one!)
 */
inline bool MySetFilePointer(RTFILE File, uint64_t offSeek, uint64_t *poffNew, unsigned uMethod)
{
    bool            fRc;
    LARGE_INTEGER   off;

    off.QuadPart = offSeek;
#if 1
    if (off.LowPart != INVALID_SET_FILE_POINTER)
    {
        off.LowPart = SetFilePointer((HANDLE)File, off.LowPart, &off.HighPart, uMethod);
        fRc = off.LowPart != INVALID_SET_FILE_POINTER;
    }
    else
    {
        SetLastError(NO_ERROR);
        off.LowPart = SetFilePointer((HANDLE)File, off.LowPart, &off.HighPart, uMethod);
        fRc = GetLastError() == NO_ERROR;
    }
#else
    fRc = SetFilePointerEx((HANDLE)File, off, &off, uMethod);
#endif
    if (fRc && poffNew)
        *poffNew = off.QuadPart;
    return fRc;
}


/**
 * This is a helper to check if an attempt was made to grow a file beyond the
 * limit of the filesystem.
 *
 * @returns true for file size limit exceeded.
 * @param   File        Filehandle.
 * @param   offSeek     Offset to seek.
 * @param   uMethod     The seek method.
 */
DECLINLINE(bool) IsBeyondLimit(RTFILE File, uint64_t offSeek, unsigned uMethod)
{
    bool fIsBeyondLimit = false;

    /*
     * Get the current file position and try set the new one.
     * If it fails with a seek error it's because we hit the file system limit.
     */
/** @todo r=bird: I'd be very interested to know on which versions of windows and on which file systems
 * this supposedly works. The fastfat sources in the latest WDK makes no limit checks during
 * file seeking, only at the time of writing (and some other odd ones we cannot make use of). */
    uint64_t offCurrent;
    if (MySetFilePointer(File, 0, &offCurrent, FILE_CURRENT))
    {
        if (!MySetFilePointer(File, offSeek, NULL, uMethod))
            fIsBeyondLimit = GetLastError() == ERROR_SEEK;
        else /* Restore file pointer on success. */
            MySetFilePointer(File, offCurrent, NULL, FILE_BEGIN);
    }

    return fIsBeyondLimit;
}


RTR3DECL(int)  RTFileOpen(PRTFILE pFile, const char *pszFilename, unsigned fOpen)
{
    /*
     * Validate input.
     */
    if (!pFile)
    {
        AssertMsgFailed(("Invalid pFile\n"));
        return VERR_INVALID_PARAMETER;
    }
    *pFile = NIL_RTFILE;
    if (!pszFilename)
    {
        AssertMsgFailed(("Invalid pszFilename\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Merge forced open flags and validate them.
     */
    int rc = rtFileRecalcAndValidateFlags(&fOpen);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Determine disposition, access, share mode, creation flags, and security attributes
     * for the CreateFile API call.
     */
    DWORD dwCreationDisposition;
    switch (fOpen & RTFILE_O_ACTION_MASK)
    {
        case RTFILE_O_OPEN:
            dwCreationDisposition = fOpen & RTFILE_O_TRUNCATE ? TRUNCATE_EXISTING : OPEN_EXISTING;
            break;
        case RTFILE_O_OPEN_CREATE:
            dwCreationDisposition = OPEN_ALWAYS;
            break;
        case RTFILE_O_CREATE:
            dwCreationDisposition = CREATE_NEW;
            break;
        case RTFILE_O_CREATE_REPLACE:
            dwCreationDisposition = CREATE_ALWAYS;
            break;
        default:
            AssertMsgFailed(("Impossible fOpen=%#x\n", fOpen));
            return VERR_INVALID_PARAMETER;
    }

    DWORD dwDesiredAccess;
    switch (fOpen & RTFILE_O_ACCESS_MASK)
    {
        case RTFILE_O_READ:        dwDesiredAccess = GENERIC_READ; break;
        case RTFILE_O_WRITE:       dwDesiredAccess = GENERIC_WRITE; break;
        case RTFILE_O_READWRITE:   dwDesiredAccess = GENERIC_READ | GENERIC_WRITE; break;
        default:
            AssertMsgFailed(("Impossible fOpen=%#x\n", fOpen));
            return VERR_INVALID_PARAMETER;
    }

    DWORD dwShareMode;
    Assert(RTFILE_O_DENY_READWRITE == RTFILE_O_DENY_ALL && !RTFILE_O_DENY_NONE);
    switch (fOpen & RTFILE_O_DENY_MASK)
    {
        case RTFILE_O_DENY_NONE:                                dwShareMode = FILE_SHARE_READ | FILE_SHARE_WRITE; break;
        case RTFILE_O_DENY_READ:                                dwShareMode = FILE_SHARE_WRITE; break;
        case RTFILE_O_DENY_WRITE:                               dwShareMode = FILE_SHARE_READ; break;
        case RTFILE_O_DENY_READWRITE:                           dwShareMode = 0; break;

        case RTFILE_O_DENY_NOT_DELETE | RTFILE_O_DENY_NONE:     dwShareMode = FILE_SHARE_DELETE | FILE_SHARE_READ | FILE_SHARE_WRITE; break;
        case RTFILE_O_DENY_NOT_DELETE | RTFILE_O_DENY_READ:     dwShareMode = FILE_SHARE_DELETE | FILE_SHARE_WRITE; break;
        case RTFILE_O_DENY_NOT_DELETE | RTFILE_O_DENY_WRITE:    dwShareMode = FILE_SHARE_DELETE | FILE_SHARE_READ; break;
        case RTFILE_O_DENY_NOT_DELETE | RTFILE_O_DENY_READWRITE:dwShareMode = FILE_SHARE_DELETE; break;
        default:
            AssertMsgFailed(("Impossible fOpen=%#x\n", fOpen));
            return VERR_INVALID_PARAMETER;
    }

    SECURITY_ATTRIBUTES  SecurityAttributes;
    PSECURITY_ATTRIBUTES pSecurityAttributes = NULL;
    if (fOpen & RTFILE_O_INHERIT)
    {
        SecurityAttributes.nLength              = sizeof(SecurityAttributes);
        SecurityAttributes.lpSecurityDescriptor = NULL;
        SecurityAttributes.bInheritHandle       = TRUE;
        pSecurityAttributes = &SecurityAttributes;
    }

    DWORD dwFlagsAndAttributes;
    dwFlagsAndAttributes = FILE_ATTRIBUTE_NORMAL;
    if (fOpen & RTFILE_O_WRITE_THROUGH)
        dwFlagsAndAttributes |= FILE_FLAG_WRITE_THROUGH;

    /*
     * Open/Create the file.
     */
#ifdef RT_DONT_CONVERT_FILENAMES
    HANDLE hFile = CreateFile(pszFilename,
                              dwDesiredAccess,
                              dwShareMode,
                              pSecurityAttributes,
                              dwCreationDisposition,
                              dwFlagsAndAttributes,
                              NULL);
#else
    PRTUTF16 pwszFilename;
    rc = RTStrToUtf16(pszFilename, &pwszFilename);
    if (RT_FAILURE(rc))
        return rc;

    HANDLE hFile = CreateFileW(pwszFilename,
                               dwDesiredAccess,
                               dwShareMode,
                               pSecurityAttributes,
                               dwCreationDisposition,
                               dwFlagsAndAttributes,
                               NULL);
#endif
    if (hFile != INVALID_HANDLE_VALUE)
    {
        bool fCreated = dwCreationDisposition == CREATE_ALWAYS
                     || dwCreationDisposition == CREATE_NEW
                     || (dwCreationDisposition == OPEN_ALWAYS && GetLastError() == 0);

        /*
         * Turn off indexing of directory through Windows Indexing Service.
         */
        if (    fCreated
            &&  (fOpen & RTFILE_O_NOT_CONTENT_INDEXED))
        {
#ifdef RT_DONT_CONVERT_FILENAMES
            if (!SetFileAttributes(pszFilename, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED))
#else
            if (!SetFileAttributesW(pwszFilename, FILE_ATTRIBUTE_NOT_CONTENT_INDEXED))
#endif
                rc = RTErrConvertFromWin32(GetLastError());
        }
        /*
         * Do we need to truncate the file?
         */
        else if (    !fCreated
                 &&     (fOpen & (RTFILE_O_TRUNCATE | RTFILE_O_ACTION_MASK))
                     == (RTFILE_O_TRUNCATE | RTFILE_O_OPEN_CREATE))
        {
            if (!SetEndOfFile(hFile))
                rc = RTErrConvertFromWin32(GetLastError());
        }
        if (RT_SUCCESS(rc))
        {
            *pFile = (RTFILE)hFile;
            Assert((HANDLE)*pFile == hFile);
#ifndef RT_DONT_CONVERT_FILENAMES
            RTUtf16Free(pwszFilename);
#endif
            return VINF_SUCCESS;
        }

        CloseHandle(hFile);
    }
    else
        rc = RTErrConvertFromWin32(GetLastError());
#ifndef RT_DONT_CONVERT_FILENAMES
    RTUtf16Free(pwszFilename);
#endif
    return rc;
}


RTR3DECL(int)  RTFileClose(RTFILE File)
{
    if (CloseHandle((HANDLE)File))
        return VINF_SUCCESS;
    return RTErrConvertFromWin32(GetLastError());
}


RTR3DECL(int)  RTFileSeek(RTFILE File, int64_t offSeek, unsigned uMethod, uint64_t *poffActual)
{
    static ULONG aulSeekRecode[] =
    {
        FILE_BEGIN,
        FILE_CURRENT,
        FILE_END,
    };

    /*
     * Validate input.
     */
    if (uMethod > RTFILE_SEEK_END)
    {
        AssertMsgFailed(("Invalid uMethod=%d\n", uMethod));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Execute the seek.
     */
    if (MySetFilePointer(File, offSeek, poffActual, aulSeekRecode[uMethod]))
        return VINF_SUCCESS;
    return RTErrConvertFromWin32(GetLastError());
}


RTR3DECL(int)  RTFileRead(RTFILE File, void *pvBuf, size_t cbToRead, size_t *pcbRead)
{
    if (cbToRead <= 0)
        return VINF_SUCCESS;

    ULONG cbRead = 0;
    if (ReadFile((HANDLE)File, pvBuf, cbToRead, &cbRead, NULL))
    {
        if (pcbRead)
            /* Caller can handle partial reads. */
            *pcbRead = cbRead;
        else
        {
            /* Caller expects everything to be read. */
            while (cbToRead > cbRead)
            {
                ULONG cbReadPart = 0;
                if (!ReadFile((HANDLE)File, (char*)pvBuf + cbRead, cbToRead - cbRead, &cbReadPart, NULL))
                    return RTErrConvertFromWin32(GetLastError());
                if (cbReadPart == 0)
                    return VERR_EOF;
                cbRead += cbReadPart;
            }
        }
        return VINF_SUCCESS;
    }
    return RTErrConvertFromWin32(GetLastError());
}


RTR3DECL(int)  RTFileWrite(RTFILE File, const void *pvBuf, size_t cbToWrite, size_t *pcbWritten)
{
    if (cbToWrite <= 0)
        return VINF_SUCCESS;

    ULONG cbWritten = 0;
    if (WriteFile((HANDLE)File, pvBuf, cbToWrite, &cbWritten, NULL))
    {
        if (pcbWritten)
            /* Caller can handle partial writes. */
            *pcbWritten = cbWritten;
        else
        {
            /* Caller expects everything to be written. */
            while (cbToWrite > cbWritten)
            {
                ULONG cbWrittenPart = 0;
                if (!WriteFile((HANDLE)File, (char*)pvBuf + cbWritten, cbToWrite - cbWritten, &cbWrittenPart, NULL))
                {
                    int rc = RTErrConvertFromWin32(GetLastError());
                    if (   rc == VERR_DISK_FULL
                        && IsBeyondLimit(File, cbToWrite - cbWritten, FILE_CURRENT)
                       )
                        rc = VERR_FILE_TOO_BIG;
                    return rc;
                }
                if (cbWrittenPart == 0)
                    return VERR_WRITE_ERROR;
                cbWritten += cbWrittenPart;
            }
        }
        return VINF_SUCCESS;
    }
    int rc = RTErrConvertFromWin32(GetLastError());
    if (   rc == VERR_DISK_FULL
        && IsBeyondLimit(File, cbToWrite - cbWritten, FILE_CURRENT)
       )
        rc = VERR_FILE_TOO_BIG;
    return rc;
}


RTR3DECL(int)  RTFileFlush(RTFILE File)
{
    int rc;

    if (FlushFileBuffers((HANDLE)File) == FALSE)
    {
        rc = GetLastError();
        Log(("FlushFileBuffers failed with %d\n", rc));
        return RTErrConvertFromWin32(rc);
    }
    return VINF_SUCCESS;
}


RTR3DECL(int)  RTFileSetSize(RTFILE File, uint64_t cbSize)
{
    /*
     * Get current file pointer.
     */
    int         rc;
    uint64_t    offCurrent;
    if (MySetFilePointer(File, 0, &offCurrent, FILE_CURRENT))
    {
        /*
         * Set new file pointer.
         */
        if (MySetFilePointer(File, cbSize, NULL, FILE_BEGIN))
        {
            /* file pointer setted */
            if (SetEndOfFile((HANDLE)File))
            {
                /*
                 * Restore file pointer and return.
                 * If the old pointer was beyond the new file end, ignore failure.
                 */
                if (    MySetFilePointer(File, offCurrent, NULL, FILE_BEGIN)
                    ||  offCurrent > cbSize)
                    return VINF_SUCCESS;
            }

            /*
             * Failed, try restore file pointer.
             */
            rc = GetLastError();
            MySetFilePointer(File, offCurrent, NULL, FILE_BEGIN);
        }
        else
            rc = GetLastError();
    }
    else
        rc = GetLastError();

    return RTErrConvertFromWin32(rc);
}


RTR3DECL(int)  RTFileGetSize(RTFILE File, uint64_t *pcbSize)
{
    ULARGE_INTEGER  Size;
    Size.LowPart = GetFileSize((HANDLE)File, &Size.HighPart);
    if (Size.LowPart != INVALID_FILE_SIZE)
    {
        *pcbSize = Size.QuadPart;
        return VINF_SUCCESS;
    }

    /* error exit */
    return RTErrConvertFromWin32(GetLastError());
}


RTR3DECL(bool) RTFileIsValid(RTFILE File)
{
    if (File != NIL_RTFILE)
    {
        DWORD dwType = GetFileType((HANDLE)File);
        switch (dwType)
        {
            case FILE_TYPE_CHAR:
            case FILE_TYPE_DISK:
            case FILE_TYPE_PIPE:
            case FILE_TYPE_REMOTE:
                return true;

            case FILE_TYPE_UNKNOWN:
                if (GetLastError() == NO_ERROR)
                    return true;
                break;
        }
    }
    return false;
}


#define LOW_DWORD(u64) ((DWORD)u64)
#define HIGH_DWORD(u64) (((DWORD *)&u64)[1])

RTR3DECL(int)  RTFileLock(RTFILE File, unsigned fLock, int64_t offLock, uint64_t cbLock)
{
    Assert(offLock >= 0);

    /* Check arguments. */
    if (fLock & ~RTFILE_LOCK_MASK)
    {
        AssertMsgFailed(("Invalid fLock=%08X\n", fLock));
        return VERR_INVALID_PARAMETER;
    }

    /* Prepare flags. */
    Assert(RTFILE_LOCK_WRITE);
    DWORD dwFlags = (fLock & RTFILE_LOCK_WRITE) ? LOCKFILE_EXCLUSIVE_LOCK : 0;
    Assert(RTFILE_LOCK_WAIT);
    if (!(fLock & RTFILE_LOCK_WAIT))
        dwFlags |= LOCKFILE_FAIL_IMMEDIATELY;

    /* Windows structure. */
    OVERLAPPED Overlapped;
    memset(&Overlapped, 0, sizeof(Overlapped));
    Overlapped.Offset = LOW_DWORD(offLock);
    Overlapped.OffsetHigh = HIGH_DWORD(offLock);

    /* Note: according to Microsoft, LockFileEx API call is available starting from NT 3.5 */
    if (LockFileEx((HANDLE)File, dwFlags, 0, LOW_DWORD(cbLock), HIGH_DWORD(cbLock), &Overlapped))
        return VINF_SUCCESS;

    return RTErrConvertFromWin32(GetLastError());
}


RTR3DECL(int)  RTFileChangeLock(RTFILE File, unsigned fLock, int64_t offLock, uint64_t cbLock)
{
    Assert(offLock >= 0);

    /* Check arguments. */
    if (fLock & ~RTFILE_LOCK_MASK)
    {
        AssertMsgFailed(("Invalid fLock=%08X\n", fLock));
        return VERR_INVALID_PARAMETER;
    }

    /* Remove old lock. */
    int rc = RTFileUnlock(File, offLock, cbLock);
    if (RT_FAILURE(rc))
        return rc;

    /* Set new lock. */
    rc = RTFileLock(File, fLock, offLock, cbLock);
    if (RT_SUCCESS(rc))
        return rc;

    /* Try to restore old lock. */
    unsigned fLockOld = (fLock & RTFILE_LOCK_WRITE) ? fLock & ~RTFILE_LOCK_WRITE : fLock | RTFILE_LOCK_WRITE;
    rc = RTFileLock(File, fLockOld, offLock, cbLock);
    if (RT_SUCCESS(rc))
        return VERR_FILE_LOCK_VIOLATION;
    else
        return VERR_FILE_LOCK_LOST;
}


RTR3DECL(int)  RTFileUnlock(RTFILE File, int64_t offLock, uint64_t cbLock)
{
    Assert(offLock >= 0);

    if (UnlockFile((HANDLE)File, LOW_DWORD(offLock), HIGH_DWORD(offLock), LOW_DWORD(cbLock), HIGH_DWORD(cbLock)))
        return VINF_SUCCESS;

    return RTErrConvertFromWin32(GetLastError());
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
    BY_HANDLE_FILE_INFORMATION Data;
    if (!GetFileInformationByHandle((HANDLE)File, &Data))
        return RTErrConvertFromWin32(GetLastError());

    /*
     * Setup the returned data.
     */
    pObjInfo->cbObject    = ((uint64_t)Data.nFileSizeHigh << 32)
                          |  (uint64_t)Data.nFileSizeLow;
    pObjInfo->cbAllocated = pObjInfo->cbObject;

    Assert(sizeof(uint64_t) == sizeof(Data.ftCreationTime));
    RTTimeSpecSetNtTime(&pObjInfo->BirthTime,         *(uint64_t *)&Data.ftCreationTime);
    RTTimeSpecSetNtTime(&pObjInfo->AccessTime,        *(uint64_t *)&Data.ftLastAccessTime);
    RTTimeSpecSetNtTime(&pObjInfo->ModificationTime,  *(uint64_t *)&Data.ftLastWriteTime);
    pObjInfo->ChangeTime  = pObjInfo->ModificationTime;

    pObjInfo->Attr.fMode  = rtFsModeFromDos((Data.dwFileAttributes << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_NT, "", 0);

    /*
     * Requested attributes (we cannot provide anything actually).
     */
    switch (enmAdditionalAttribs)
    {
        case RTFSOBJATTRADD_EASIZE:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_EASIZE;
            pObjInfo->Attr.u.EASize.cb            = 0;
            break;

        case RTFSOBJATTRADD_UNIX:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_UNIX;
            pObjInfo->Attr.u.Unix.uid             = ~0U;
            pObjInfo->Attr.u.Unix.gid             = ~0U;
            pObjInfo->Attr.u.Unix.cHardlinks      = Data.nNumberOfLinks ? Data.nNumberOfLinks : 1;
            pObjInfo->Attr.u.Unix.INodeIdDevice   = 0;
            pObjInfo->Attr.u.Unix.INodeId         = 0;
            pObjInfo->Attr.u.Unix.fFlags          = 0;
            pObjInfo->Attr.u.Unix.GenerationId    = 0;
            pObjInfo->Attr.u.Unix.Device          = 0;
            break;

        case RTFSOBJATTRADD_NOTHING:
            pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_NOTHING;
            break;

        default:
            AssertMsgFailed(("Impossible!\n"));
            return VERR_INTERNAL_ERROR;
    }

    return VINF_SUCCESS;
}


RTR3DECL(int) RTFileSetTimes(RTFILE File, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    if (!pAccessTime && !pModificationTime && !pBirthTime)
        return VINF_SUCCESS;    /* NOP */

    FILETIME    CreationTimeFT;
    PFILETIME   pCreationTimeFT = NULL;
    if (pBirthTime)
        pCreationTimeFT = RTTimeSpecGetNtFileTime(pBirthTime, &CreationTimeFT);

    FILETIME    LastAccessTimeFT;
    PFILETIME   pLastAccessTimeFT = NULL;
    if (pAccessTime)
        pLastAccessTimeFT = RTTimeSpecGetNtFileTime(pAccessTime, &LastAccessTimeFT);

    FILETIME    LastWriteTimeFT;
    PFILETIME   pLastWriteTimeFT = NULL;
    if (pModificationTime)
        pLastWriteTimeFT = RTTimeSpecGetNtFileTime(pModificationTime, &LastWriteTimeFT);

    int rc = VINF_SUCCESS;
    if (!SetFileTime((HANDLE)File, pCreationTimeFT, pLastAccessTimeFT, pLastWriteTimeFT))
    {
        DWORD Err = GetLastError();
        rc = RTErrConvertFromWin32(Err);
        Log(("RTFileSetTimes(%RTfile, %p, %p, %p, %p): SetFileTime failed with lasterr %d (%Vrc)\n",
             File, pAccessTime, pModificationTime, pChangeTime, pBirthTime, Err, rc));
    }
    return rc;
}


RTR3DECL(int) RTFileSetMode(RTFILE File, RTFMODE fMode)
{
    /** @todo darn. this needs a full path; probably must be done if the file is closed
     * It's quite possible that there is an NT API for this. NtSetInformationFile() for instance. */
    return VINF_SUCCESS;
}


RTR3DECL(int)  RTFileDelete(const char *pszFilename)
{
#ifdef RT_DONT_CONVERT_FILENAMES
    if (DeleteFile(pszFilename))
        return VINF_SUCCESS;
    return RTErrConvertFromWin32(GetLastError());

#else
    PRTUTF16 pwszFilename;
    int rc = RTStrToUtf16(pszFilename, &pwszFilename);
    if (RT_SUCCESS(rc))
    {
        if (!DeleteFileW(pwszFilename))
            rc = RTErrConvertFromWin32(GetLastError());
        RTUtf16Free(pwszFilename);
    }

    return rc;
#endif
}


RTDECL(int) RTFileRename(const char *pszSrc, const char *pszDst, unsigned fRename)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("%p\n", pszSrc), VERR_INVALID_POINTER);
    AssertMsgReturn(VALID_PTR(pszDst), ("%p\n", pszDst), VERR_INVALID_POINTER);
    AssertMsgReturn(!(fRename & ~RTPATHRENAME_FLAGS_REPLACE), ("%#x\n", fRename), VERR_INVALID_PARAMETER);

    /*
     * Hand it on to the worker.
     */
    int rc = rtPathWin32MoveRename(pszSrc, pszDst,
                                   fRename & RTPATHRENAME_FLAGS_REPLACE ? MOVEFILE_REPLACE_EXISTING : 0,
                                   RTFS_TYPE_FILE);

    LogFlow(("RTFileMove(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n",
             pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;

}


RTDECL(int) RTFileMove(const char *pszSrc, const char *pszDst, unsigned fMove)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszSrc), ("%p\n", pszSrc), VERR_INVALID_POINTER);
    AssertMsgReturn(VALID_PTR(pszDst), ("%p\n", pszDst), VERR_INVALID_POINTER);
    AssertMsgReturn(!(fMove & ~RTFILEMOVE_FLAGS_REPLACE), ("%#x\n", fMove), VERR_INVALID_PARAMETER);

    /*
     * Hand it on to the worker.
     */
    int rc = rtPathWin32MoveRename(pszSrc, pszDst,
                                   fMove & RTFILEMOVE_FLAGS_REPLACE
                                   ? MOVEFILE_COPY_ALLOWED | MOVEFILE_REPLACE_EXISTING
                                   : MOVEFILE_COPY_ALLOWED,
                                   RTFS_TYPE_FILE);

    LogFlow(("RTFileMove(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n",
             pszSrc, pszSrc, pszDst, pszDst, fMove, rc));
    return rc;
}

