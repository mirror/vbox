/* $Id$ */
/** @file
 * innotek Portable Runtime - Path manipulation.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP RTLOGGROUP_PATH
#include <Windows.h>

#include <iprt/path.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/time.h>
#include <iprt/param.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include "internal/path.h"
#include "internal/fs.h"


/**
 * Get the real (no symlinks, no . or .. components) path, must exist.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszRealPath     Where to store the real path.
 * @param   cchRealPath     Size of the buffer.
 */
RTDECL(int) RTPathReal(const char *pszPath, char *pszRealPath, unsigned cchRealPath)
{
    /*
     * Convert to UCS2, call Win32 APIs, convert back.
     */
    PRTUTF16 pwszPath;
    int rc = RTStrUtf8ToUcs2(&pwszPath, pszPath);
    if (!RT_SUCCESS(rc))
        return (rc);

    LPWSTR lpFile;
    WCHAR  wsz[RTPATH_MAX];
    rc = GetFullPathNameW((LPCWSTR)pwszPath, ELEMENTS(wsz), &wsz[0], &lpFile);
    if (rc > 0 && rc < ELEMENTS(wsz))
    {
        /* Check that it exists. (Use RTPathAbs() to just resolve the name.) */
        DWORD dwAttr = GetFileAttributesW(wsz);
        if (dwAttr != INVALID_FILE_ATTRIBUTES)
            rc = RTStrUcs2ToUtf8Ex(&pszRealPath, cchRealPath, (PRTUTF16)&wsz[0]);
        else
            rc = RTErrConvertFromWin32(GetLastError());
    }
    else if (rc <= 0)
        rc = RTErrConvertFromWin32(GetLastError());
    else
        rc = VERR_FILENAME_TOO_LONG;

    RTStrUcs2Free(pwszPath);

    return rc;
}


/**
 * Get the absolute path (no symlinks, no . or .. components), doesn't have to exit.
 *
 * @returns iprt status code.
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbs(const char *pszPath, char *pszAbsPath, unsigned cchAbsPath)
{
    /*
     * Convert to UCS2, call Win32 API, convert back.
     */
    LPWSTR lpwPath;
    int rc = RTStrUtf8ToUcs2(&lpwPath, pszPath);
    if (!RT_SUCCESS(rc))
        return (rc);

    LPWSTR lpFile;
    RTUCS2 ucsz[RTPATH_MAX];
    rc = GetFullPathNameW(lpwPath, ELEMENTS(ucsz), &ucsz[0], &lpFile);
    if (rc > 0 && rc < ELEMENTS(ucsz))
        rc = RTStrUcs2ToUtf8Ex(&pszAbsPath, cchAbsPath, &ucsz[0]);
    else if (rc <= 0)
        rc = RTErrConvertFromWin32(GetLastError());
    else
        rc = VERR_FILENAME_TOO_LONG;

    RTStrUcs2Free(lpwPath);

    return rc;
}


/**
 * Gets the program path.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathProgram(char *pszPath, unsigned cchPath)
{
    /*
     * First time only.
     */
    if (!g_szrtProgramPath[0])
    {
        HMODULE hExe = GetModuleHandle(NULL);
        if (!GetModuleFileName(hExe, &g_szrtProgramPath[0], sizeof(g_szrtProgramPath)))
        {
            AssertMsgFailed(("Couldn't get exe module name. lasterr=%d\n", GetLastError()));
            return RTErrConvertFromWin32(GetLastError());
        }
        RTPathStripFilename(g_szrtProgramPath);
    }

    /*
     * Calc the length and check if there is space before copying.
     */
    unsigned cch = strlen(g_szrtProgramPath) + 1;
    if (cch <= cchPath)
    {
        memcpy(pszPath, g_szrtProgramPath, cch + 1);
        return VINF_SUCCESS;
    }

    AssertMsgFailed(("Buffer too small (%d < %d)\n", cchPath, cch));
    return VERR_BUFFER_OVERFLOW;
}


/**
 * Gets the user home directory.
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathUserHome(char *pszPath, unsigned cchPath)
{
    RTUCS2  ucszPath[RTPATH_MAX];
    DWORD   dwAttr;

    /*
     * There are multiple definitions for what WE think of as user home...
     */
    if (    !GetEnvironmentVariableW(L"HOME", &ucszPath[0], RTPATH_MAX)
        ||  (dwAttr = GetFileAttributesW(&ucszPath[0])) == INVALID_FILE_ATTRIBUTES
        ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
    {
        if (    !GetEnvironmentVariableW(L"USERPROFILE", &ucszPath[0], RTPATH_MAX)
            ||  (dwAttr = GetFileAttributesW(&ucszPath[0])) == INVALID_FILE_ATTRIBUTES
            ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
        {
            if (!GetEnvironmentVariableW(L"HOMEDRIVE", &ucszPath[0], RTPATH_MAX))
                return VERR_PATH_NOT_FOUND;
            PRTUCS2 pucsz = &ucszPath[RTStrUcs2Len(&ucszPath[0])];
            if (    !GetEnvironmentVariableW(L"HOMEPATH", &ucszPath[0], RTPATH_MAX)
                ||  (dwAttr = GetFileAttributesW(&ucszPath[0])) == INVALID_FILE_ATTRIBUTES
                ||  !(dwAttr & FILE_ATTRIBUTE_DIRECTORY))
                return VERR_PATH_NOT_FOUND;
        }
    }

    /*
     * Convert and return.
     */
    return RTStrUcs2ToUtf8Ex(&pszPath, cchPath, &ucszPath[0]);
}


RTR3DECL(int) RTPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    /*
     * Validate input.
     */
    if (!pszPath)
    {
        AssertMsgFailed(("Invalid pszPath=%p\n", pszPath));
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
    WIN32_FILE_ATTRIBUTE_DATA Data;
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszPath;
    int rc = RTStrUtf8ToUcs2(&puszPath, pszPath);
    if (RT_FAILURE(rc))
        return rc;
    if (!GetFileAttributesExW(puszPath, GetFileExInfoStandard, &Data))
    {
        rc = RTErrConvertFromWin32(GetLastError());
        RTStrUcs2Free(puszPath);
        return rc;
    }
    RTStrUcs2Free(puszPath);
#else
    if (!GetFileAttributesExA(pszPath, GetFileExInfoStandard, &Data))
        return RTErrConvertFromWin32(GetLastError());
#endif

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

    pObjInfo->Attr.fMode  = rtFsModeFromDos((Data.dwFileAttributes << RTFS_DOS_SHIFT) & RTFS_DOS_MASK_NT,
                                            pszPath, strlen(pszPath));

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
            pObjInfo->Attr.u.Unix.cHardlinks      = 1;
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


RTR3DECL(int) RTPathSetTimes(const char *pszPath, PCRTTIMESPEC pAccessTime, PCRTTIMESPEC pModificationTime,
                             PCRTTIMESPEC pChangeTime, PCRTTIMESPEC pBirthTime)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszPath), ("%p\n", pszPath), VERR_INVALID_POINTER);
    AssertMsgReturn(*pszPath, ("%p\n", pszPath), VERR_INVALID_PARAMETER);
    AssertMsgReturn(!pAccessTime || VALID_PTR(pAccessTime), ("%p\n", pAccessTime), VERR_INVALID_POINTER);
    AssertMsgReturn(!pModificationTime || VALID_PTR(pModificationTime), ("%p\n", pModificationTime), VERR_INVALID_POINTER);
    AssertMsgReturn(!pChangeTime || VALID_PTR(pChangeTime), ("%p\n", pChangeTime), VERR_INVALID_POINTER);
    AssertMsgReturn(!pBirthTime || VALID_PTR(pBirthTime), ("%p\n", pBirthTime), VERR_INVALID_POINTER);

    /*
     * Convert the path.
     */
    PRTUTF16 pwszPath;
    int rc = RTStrToUtf16(pszPath, &pwszPath);
    if (RT_SUCCESS(rc))
    {
        HANDLE hFile = CreateFileW(pwszPath,
                                   FILE_WRITE_ATTRIBUTES,   /* dwDesiredAccess */
                                   FILE_SHARE_WRITE | FILE_SHARE_READ | FILE_SHARE_DELETE, /* dwShareMode */
                                   NULL,                    /* security attribs */
                                   OPEN_EXISTING,           /* dwCreationDisposition */
                                   FILE_FLAG_BACKUP_SEMANTICS | FILE_ATTRIBUTE_NORMAL,
                                   NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            /*
             * Check if it's a no-op.
             */
            if (!pAccessTime && !pModificationTime && !pBirthTime)
                rc = VINF_SUCCESS;    /* NOP */
            else
            {
                /*
                 * Convert the input and call the API.
                 */
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

                if (SetFileTime(hFile, pCreationTimeFT, pLastAccessTimeFT, pLastWriteTimeFT))
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD Err = GetLastError();
                    rc = RTErrConvertFromWin32(Err);
                    Log(("RTPathSetTimes('%s', %p, %p, %p, %p): SetFileTime failed with lasterr %d (%Vrc)\n",
                         pszPath, pAccessTime, pModificationTime, pChangeTime, pBirthTime, Err, rc));
                }
            }
            BOOL fRc = CloseHandle(hFile); Assert(fRc); NOREF(fRc);
        }
        else
        {
            DWORD Err = GetLastError();
            rc = RTErrConvertFromWin32(Err);
            Log(("RTPathSetTimes('%s',,,,): failed with %Rrc and lasterr=%u\n", pszPath, rc, Err));
        }

        RTUtf16Free(pwszPath);
    }

    LogFlow(("RTPathSetTimes(%p:{%s}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}): return %Rrc\n",
             pszPath, pszPath, pAccessTime, pAccessTime, pModificationTime, pModificationTime,
             pChangeTime, pChangeTime, pBirthTime, pBirthTime));
    return rc;
}




/**
 * Internal worker for RTFileRename and RTFileMove.
 *
 * @returns iprt status code.
 * @param   pszSrc      The source filename.
 * @param   pszDst      The destination filename.
 * @param   fFlags      The windows MoveFileEx flags.
 * @param   fFileType   The filetype. We use the RTFMODE filetypes here. If it's 0,
 *                      anything goes. If it's RTFS_TYPE_DIRECTORY we'll check that the
 *                      source is a directory. If Its RTFS_TYPE_FILE we'll check that it's
 *                      not a directory (we are NOT checking whether it's a file).
 */
int rtPathWin32MoveRename(const char *pszSrc, const char *pszDst, uint32_t fFlags, RTFMODE fFileType)
{
    /*
     * Convert the strings.
     */
    PRTUTF16 pwszSrc;
    int rc = RTStrToUtf16(pszSrc, &pwszSrc);
    if (RT_SUCCESS(rc))
    {
        PRTUTF16 pwszDst;
        rc = RTStrToUtf16(pszDst, &pwszDst);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check object type if requested.
             * This is open to race conditions.
             */
            if (fFileType)
            {
                DWORD dwAttr = GetFileAttributesW(pwszSrc);
                if (dwAttr == INVALID_FILE_ATTRIBUTES)
                    rc = RTErrConvertFromWin32(GetLastError());
                else if (RTFS_IS_DIRECTORY(fFileType))
                    rc = dwAttr & FILE_ATTRIBUTE_DIRECTORY ? VINF_SUCCESS : VERR_NOT_A_DIRECTORY;
                else
                    rc = dwAttr & FILE_ATTRIBUTE_DIRECTORY ? VERR_IS_A_DIRECTORY : VINF_SUCCESS;
            }
            if (RT_SUCCESS(rc))
            {
                if (MoveFileExW(pwszSrc, pwszDst, fFlags))
                    rc = VINF_SUCCESS;
                else
                {
                    DWORD Err = GetLastError();
                    rc = RTErrConvertFromWin32(Err);
                    Log(("MoveFileExW('%s', '%s', %#x, %RTfmode): fails with rc=%Rrc & lasterr=%d\n",
                         pszSrc, pszDst, fFlags, fFileType, rc, Err));
                }
            }
            RTUtf16Free(pwszDst);
        }
        RTUtf16Free(pwszSrc);
    }
    return rc;
}


RTR3DECL(int) RTPathRename(const char *pszSrc, const char *pszDst, unsigned fRename)
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
     * Call the worker.
     */
    int rc = rtPathWin32MoveRename(pszSrc, pszDst, fRename & RTPATHRENAME_FLAGS_REPLACE ? MOVEFILE_REPLACE_EXISTING : 0, 0);

    LogFlow(("RTPathRename(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n", pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;
}


/**
 * Checks if the path exists.
 * 
 * Symbolic links will all be attempted resolved.
 * 
 * @returns true if it exists and false if it doesn't
 * @param   pszPath     The path to check.
 */
RTDECL(bool) RTPathExists(const char *pszPath)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, false);
    AssertReturn(*pszPath, false);

    /*
     * Try query file info.
     */
#ifndef RT_DONT_CONVERT_FILENAMES
    PRTUCS2 puszPath;
    int rc = RTStrUtf8ToUcs2(&puszPath, pszPath);
    if (RT_SUCCESS(rc))
    {
        if (GetFileAttributesW(puszPath) == INVALID_FILE_ATTRIBUTES)
            rc = VERR_GENERAL_FAILURE;
        RTStrUcs2Free(puszPath);
    }
#else
    int rc = VINF_SUCCESS;
    if (GetFileAttributesExA(pszPath) == INVALID_FILE_ATTRIBUTES)
        rc = VERR_GENERAL_FAILURE;
#endif

    return RT_SUCCESS(rc);
}

