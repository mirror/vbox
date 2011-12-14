/* $Id$ */
/** @file
 * IPRT - Path Manipulation, POSIX, Part 1.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include <stdlib.h>
#include <limits.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdio.h>
#include <sys/types.h>
#include <pwd.h>

#include <iprt/path.h>
#include <iprt/env.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include "internal/path.h"
#include "internal/process.h"
#include "internal/fs.h"

#ifdef RT_OS_L4
# include <l4/vboxserver/vboxserver.h>
#endif




RTDECL(int) RTPathReal(const char *pszPath, char *pszRealPath, size_t cchRealPath)
{
    /*
     * Convert input.
     */
    char const *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath, NULL);
    if (RT_SUCCESS(rc))
    {
        /*
         * On POSIX platforms the API doesn't take a length parameter, which makes it
         * a little bit more work.
         */
        char szTmpPath[PATH_MAX + 1];
        const char *psz = realpath(pszNativePath, szTmpPath);
        if (psz)
            rc = rtPathFromNativeCopy(pszRealPath, cchRealPath, szTmpPath, NULL);
        else
            rc = RTErrConvertFromErrno(errno);
        rtPathFreeNative(pszNativePath, pszPath);
    }

    LogFlow(("RTPathReal(%p:{%s}, %p:{%s}, %u): returns %Rrc\n", pszPath, pszPath,
             pszRealPath, RT_SUCCESS(rc) ? pszRealPath : "<failed>",  cchRealPath));
    return rc;
}


/**
 * Cleans up a path specifier a little bit.
 * This includes removing duplicate slashes, unnecessary single dots, and
 * trailing slashes. Also, replaces all RTPATH_SLASH characters with '/'.
 *
 * @returns Number of bytes in the clean path.
 * @param   pszPath     The path to cleanup.
 */
static int fsCleanPath(char *pszPath)
{
    /*
     * Change to '/' and remove duplicates.
     */
    char   *pszSrc = pszPath;
    char   *pszTrg = pszPath;
#ifdef HAVE_UNC
    int     fUnc = 0;
    if (    RTPATH_IS_SLASH(pszPath[0])
        &&  RTPATH_IS_SLASH(pszPath[1]))
    {   /* Skip first slash in a unc path. */
        pszSrc++;
        *pszTrg++ = '/';
        fUnc = 1;
    }
#endif

    for (;;)
    {
        char ch = *pszSrc++;
        if (RTPATH_IS_SLASH(ch))
        {
            *pszTrg++ = '/';
            for (;;)
            {
                do  ch = *pszSrc++;
                while (RTPATH_IS_SLASH(ch));

                /* Remove '/./' and '/.'. */
                if (ch != '.' || (*pszSrc && !RTPATH_IS_SLASH(*pszSrc)))
                    break;
            }
        }
        *pszTrg = ch;
        if (!ch)
            break;
        pszTrg++;
    }

    /*
     * Remove trailing slash if the path may be pointing to a directory.
     */
    int cch = pszTrg - pszPath;
    if (    cch > 1
        &&  RTPATH_IS_SLASH(pszTrg[-1])
#ifdef HAVE_DRIVE
        &&  !RTPATH_IS_VOLSEP(pszTrg[-2])
#endif
        &&  !RTPATH_IS_SLASH(pszTrg[-2]))
        pszPath[--cch] = '\0';

    return cch;
}


RTDECL(int) RTPathAbs(const char *pszPath, char *pszAbsPath, size_t cchAbsPath)
{
    int rc;

    /*
     * Validation.
     */
    AssertPtr(pszAbsPath);
    AssertPtr(pszPath);
    if (RT_UNLIKELY(!*pszPath))
        return VERR_INVALID_PARAMETER;

    /*
     * Make a clean working copy of the input.
     */
    size_t cchPath = strlen(pszPath);
    if (cchPath > PATH_MAX)
    {
        LogFlow(("RTPathAbs(%p:{%s}, %p, %d): returns %Rrc\n", pszPath, pszPath, pszAbsPath, cchAbsPath, VERR_FILENAME_TOO_LONG));
        return VERR_FILENAME_TOO_LONG;
    }

    char szTmpPath[PATH_MAX + 1];
    memcpy(szTmpPath, pszPath, cchPath + 1);
    size_t cchTmpPath = fsCleanPath(szTmpPath);

    /*
     * Handle "." specially (fsCleanPath does).
     */
    if (szTmpPath[0] == '.' && !szTmpPath[1])
        return RTPathGetCurrent(pszAbsPath, cchAbsPath);

    /*
     * Do we have a root slash?
     */
    char *pszCur = szTmpPath;
#ifdef HAVE_DRIVE
    if (pszCur[0] && RTPATH_IS_VOLSEP(pszCur[1]) && pszCur[2] == '/')
        pszCur += 3;
# ifdef HAVE_UNC
    else if (pszCur[0] == '/' && pszCur[1] == '/')
        pszCur += 2;
# endif
#else  /* !HAVE_DRIVE */
    if (pszCur[0] == '/')
        pszCur += 1;
#endif /* !HAVE_DRIVE */
    else
    {
        /*
         * No, prepend the current directory to the relative path.
         */
        char szCurDir[RTPATH_MAX];
        rc = RTPathGetCurrent(szCurDir, sizeof(szCurDir));
        AssertRCReturn(rc, rc);

        size_t cchCurDir = fsCleanPath(szCurDir); /* paranoia */
        if (cchCurDir + cchTmpPath + 1 > PATH_MAX)
        {
            LogFlow(("RTPathAbs(%p:{%s}, %p, %d): returns %Rrc\n", pszPath, pszPath, pszAbsPath, cchAbsPath, VERR_FILENAME_TOO_LONG));
            return VERR_FILENAME_TOO_LONG;
        }

        memmove(szTmpPath + cchCurDir + 1, szTmpPath, cchTmpPath + 1);
        memcpy(szTmpPath, szCurDir, cchCurDir);
        szTmpPath[cchCurDir] = '/';


#ifdef HAVE_DRIVE
        if (pszCur[0] && RTPATH_IS_VOLSEP(pszCur[1]) && pszCur[2] == '/')
            pszCur += 3;
# ifdef HAVE_UNC
        else if (pszCur[0] == '/' && pszCur[1] == '/')
            pszCur += 2;
# endif
#else
        if (pszCur[0] == '/')
            pszCur += 1;
#endif
        else
            AssertMsgFailedReturn(("pszCur=%s\n", pszCur), VERR_INTERNAL_ERROR);
    }

    char *pszTop = pszCur;

    /*
     * Get rid of double dot path components by evaluating them.
     */
    for (;;)
    {
        if (   pszCur[0] == '.'
            && pszCur[1] == '.'
            && (!pszCur[2] || pszCur[2] == '/'))
        {
            /* rewind to the previous component if any */
            char *pszPrev = pszCur - 1;
            if (pszPrev > pszTop)
                while (*--pszPrev != '/')
                    ;

            AssertMsg(*pszPrev == '/', ("szTmpPath={%s}, pszPrev=+%u\n", szTmpPath, pszPrev - szTmpPath));
            memmove(pszPrev, pszCur + 2, strlen(pszCur + 2) + 1);

            pszCur = pszPrev;
        }
        else
        {
            /* advance to end of component. */
            while (*pszCur && *pszCur != '/')
                pszCur++;
        }

        if (!*pszCur)
            break;

        /* skip the slash */
        ++pszCur;
    }

    if (pszCur < pszTop)
    {
        /*
         * We overwrote the root slash with '\0', restore it.
         */
        *pszCur++ = '/';
        *pszCur = '\0';
    }
    else if (pszCur > pszTop && pszCur[-1] == '/')
    {
        /*
         * Extra trailing slash in a non-root path, remove it.
         * (A bit questionable...)
         */
        *--pszCur = '\0';
    }

    /*
     * Copy the result to the user buffer.
     */
    cchTmpPath = pszCur - szTmpPath;
    if (cchTmpPath < cchAbsPath)
    {
        memcpy(pszAbsPath, szTmpPath, cchTmpPath + 1);
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_BUFFER_OVERFLOW;

    LogFlow(("RTPathAbs(%p:{%s}, %p:{%s}, %d): returns %Rrc\n", pszPath, pszPath, pszAbsPath,
             RT_SUCCESS(rc) ? pszAbsPath : "<failed>", cchAbsPath, rc));
    return rc;
}


RTR3DECL(int) RTPathSetMode(const char *pszPath, RTFMODE fMode)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);

    int rc;
    fMode = rtFsModeNormalize(fMode, pszPath, 0);
    if (rtFsModeIsValidPermissions(fMode))
    {
        char const *pszNativePath;
        rc = rtPathToNative(&pszNativePath, pszPath, NULL);
        if (RT_SUCCESS(rc))
        {
            if (chmod(pszNativePath, fMode & RTFS_UNIX_MASK) != 0)
                rc = RTErrConvertFromErrno(errno);
            rtPathFreeNative(pszNativePath, pszPath);
        }
    }
    else
    {
        AssertMsgFailed(("Invalid file mode! %RTfmode\n", fMode));
        rc = VERR_INVALID_FMODE;
    }
    return rc;
}


/**
 * Checks if two files are the one and same file.
 */
static bool rtPathSame(const char *pszNativeSrc, const char *pszNativeDst)
{
    struct stat SrcStat;
    if (lstat(pszNativeSrc, &SrcStat))
        return false;
    struct stat DstStat;
    if (lstat(pszNativeDst, &DstStat))
        return false;
    Assert(SrcStat.st_dev && DstStat.st_dev);
    Assert(SrcStat.st_ino && DstStat.st_ino);
    if (    SrcStat.st_dev == DstStat.st_dev
        &&  SrcStat.st_ino == DstStat.st_ino
        &&  (SrcStat.st_mode & S_IFMT) == (DstStat.st_mode & S_IFMT))
        return true;
    return false;
}


/**
 * Worker for RTPathRename, RTDirRename, RTFileRename.
 *
 * @returns IPRT status code.
 * @param   pszSrc      The source path.
 * @param   pszDst      The destination path.
 * @param   fRename     The rename flags.
 * @param   fFileType   The filetype. We use the RTFMODE filetypes here. If it's 0,
 *                      anything goes. If it's RTFS_TYPE_DIRECTORY we'll check that the
 *                      source is a directory. If Its RTFS_TYPE_FILE we'll check that it's
 *                      not a directory (we are NOT checking whether it's a file).
 */
DECLHIDDEN(int) rtPathPosixRename(const char *pszSrc, const char *pszDst, unsigned fRename, RTFMODE fFileType)
{
    /*
     * Convert the paths.
     */
    char const *pszNativeSrc;
    int rc = rtPathToNative(&pszNativeSrc, pszSrc, NULL);
    if (RT_SUCCESS(rc))
    {
        char const *pszNativeDst;
        rc = rtPathToNative(&pszNativeDst, pszDst, NULL);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check that the source exists and that any types that's specified matches.
             * We have to check this first to avoid getting errnous VERR_ALREADY_EXISTS
             * errors from the next step.
             *
             * There are race conditions here (perhaps unlikely ones, but still), but I'm
             * afraid there is little with can do to fix that.
             */
            struct stat SrcStat;
            if (lstat(pszNativeSrc, &SrcStat))
                rc = RTErrConvertFromErrno(errno);
            else if (!fFileType)
                rc = VINF_SUCCESS;
            else if (RTFS_IS_DIRECTORY(fFileType))
                rc = S_ISDIR(SrcStat.st_mode) ? VINF_SUCCESS : VERR_NOT_A_DIRECTORY;
            else
                rc = S_ISDIR(SrcStat.st_mode) ? VERR_IS_A_DIRECTORY : VINF_SUCCESS;
            if (RT_SUCCESS(rc))
            {
                bool fSameFile = false;

                /*
                 * Check if the target exists, rename is rather destructive.
                 * We'll have to make sure we don't overwrite the source!
                 * Another race condition btw.
                 */
                struct stat DstStat;
                if (lstat(pszNativeDst, &DstStat))
                    rc = errno == ENOENT ? VINF_SUCCESS : RTErrConvertFromErrno(errno);
                else
                {
                    Assert(SrcStat.st_dev && DstStat.st_dev);
                    Assert(SrcStat.st_ino && DstStat.st_ino);
                    if (    SrcStat.st_dev == DstStat.st_dev
                        &&  SrcStat.st_ino == DstStat.st_ino
                        &&  (SrcStat.st_mode & S_IFMT) == (DstStat.st_mode & S_IFMT))
                    {
                        /*
                         * It's likely that we're talking about the same file here.
                         * We should probably check paths or whatever, but for now this'll have to be enough.
                         */
                        fSameFile = true;
                    }
                    if (fSameFile)
                        rc = VINF_SUCCESS;
                    else if (S_ISDIR(DstStat.st_mode) || !(fRename & RTPATHRENAME_FLAGS_REPLACE))
                        rc = VERR_ALREADY_EXISTS;
                    else
                        rc = VINF_SUCCESS;

                }
                if (RT_SUCCESS(rc))
                {
                    if (!rename(pszNativeSrc, pszNativeDst))
                        rc = VINF_SUCCESS;
                    else if (   (fRename & RTPATHRENAME_FLAGS_REPLACE)
                             && (errno == ENOTDIR || errno == EEXIST))
                    {
                        /*
                         * Check that the destination isn't a directory.
                         * Yet another race condition.
                         */
                        if (rtPathSame(pszNativeSrc, pszNativeDst))
                        {
                            rc = VINF_SUCCESS;
                            Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): appears to be the same file... (errno=%d)\n",
                                 pszSrc, pszDst, fRename, fFileType, errno));
                        }
                        else
                        {
                            if (lstat(pszNativeDst, &DstStat))
                                rc = errno != ENOENT ? RTErrConvertFromErrno(errno) : VINF_SUCCESS;
                            else if (S_ISDIR(DstStat.st_mode))
                                rc = VERR_ALREADY_EXISTS;
                            else
                                rc = VINF_SUCCESS;
                            if (RT_SUCCESS(rc))
                            {
                                if (!unlink(pszNativeDst))
                                {
                                    if (!rename(pszNativeSrc, pszNativeDst))
                                        rc = VINF_SUCCESS;
                                    else
                                    {
                                        rc = RTErrConvertFromErrno(errno);
                                        Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): rename failed rc=%Rrc errno=%d\n",
                                             pszSrc, pszDst, fRename, fFileType, rc, errno));
                                    }
                                }
                                else
                                {
                                    rc = RTErrConvertFromErrno(errno);
                                    Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): failed to unlink dst rc=%Rrc errno=%d\n",
                                         pszSrc, pszDst, fRename, fFileType, rc, errno));
                                }
                            }
                            else
                                Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): dst !dir check failed rc=%Rrc\n",
                                     pszSrc, pszDst, fRename, fFileType, rc));
                        }
                    }
                    else
                    {
                        rc = RTErrConvertFromErrno(errno);
                        if (errno == ENOTDIR)
                            rc = VERR_ALREADY_EXISTS; /* unless somebody is racing us, this is the right interpretation */
                        Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): rename failed rc=%Rrc errno=%d\n",
                             pszSrc, pszDst, fRename, fFileType, rc, errno));
                    }
                }
                else
                    Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): destination check failed rc=%Rrc errno=%d\n",
                         pszSrc, pszDst, fRename, fFileType, rc, errno));
            }
            else
                Log(("rtPathRename('%s', '%s', %#x ,%RTfmode): source type check failed rc=%Rrc errno=%d\n",
                     pszSrc, pszDst, fRename, fFileType, rc, errno));

            rtPathFreeNative(pszNativeDst, pszDst);
        }
        rtPathFreeNative(pszNativeSrc, pszSrc);
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
     * Hand it to the worker.
     */
    int rc = rtPathPosixRename(pszSrc, pszDst, fRename, 0);

    Log(("RTPathRename(%p:{%s}, %p:{%s}, %#x): returns %Rrc\n", pszSrc, pszSrc, pszDst, pszDst, fRename, rc));
    return rc;
}


RTR3DECL(int) RTPathUnlink(const char *pszPath, uint32_t fUnlink)
{
    return VERR_NOT_IMPLEMENTED;
}


RTDECL(bool) RTPathExists(const char *pszPath)
{
    return RTPathExistsEx(pszPath, RTPATH_F_FOLLOW_LINK);
}


RTDECL(bool) RTPathExistsEx(const char *pszPath, uint32_t fFlags)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, false);
    AssertReturn(*pszPath, false);
    Assert(RTPATH_F_IS_VALID(fFlags, 0));

    /*
     * Convert the path and check if it exists using stat().
     */
    char const *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath, NULL);
    if (RT_SUCCESS(rc))
    {
        struct stat Stat;
        if (fFlags & RTPATH_F_FOLLOW_LINK)
            rc = stat(pszNativePath, &Stat);
        else
            rc = lstat(pszNativePath, &Stat);
        if (!rc)
            rc = VINF_SUCCESS;
        else
            rc = VERR_GENERAL_FAILURE;
        rtPathFreeNative(pszNativePath, pszPath);
    }
    return RT_SUCCESS(rc);
}


RTDECL(int)  RTPathGetCurrent(char *pszPath, size_t cchPath)
{
    int rc;
    char szNativeCurDir[RTPATH_MAX];
    if (getcwd(szNativeCurDir, sizeof(szNativeCurDir)) != NULL)
        rc = rtPathFromNativeCopy(pszPath, cchPath, szNativeCurDir, NULL);
    else
        rc = RTErrConvertFromErrno(errno);
    return rc;
}


RTDECL(int) RTPathSetCurrent(const char *pszPath)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);

    /*
     * Change the directory.
     */
    char const *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath, NULL);
    if (RT_SUCCESS(rc))
    {
        if (chdir(pszNativePath))
            rc = RTErrConvertFromErrno(errno);
        rtPathFreeNative(pszNativePath, pszPath);
    }
    return rc;
}

