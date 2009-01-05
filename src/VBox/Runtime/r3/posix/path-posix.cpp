/* $Id$ */
/** @file
 * IPRT - Path Manipulation, POSIX.
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
    char *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath);
    if (RT_SUCCESS(rc))
    {
        /*
         * On POSIX platforms the API doesn't take a length parameter, which makes it
         * a little bit more work.
         */
        char szTmpPath[PATH_MAX + 1];
        const char *psz = realpath(pszNativePath, szTmpPath);
        if (psz)
        {
            /*
             * Convert result and copy it to the return buffer.
             */
            char *pszUtf8RealPath;
            rc = rtPathFromNative(&pszUtf8RealPath, szTmpPath);
            if (RT_SUCCESS(rc))
            {
                size_t cch = strlen(pszUtf8RealPath) + 1;
                if (cch <= cchRealPath)
                    memcpy(pszRealPath, pszUtf8RealPath, cch);
                else
                    rc = VERR_BUFFER_OVERFLOW;
                RTStrFree(pszUtf8RealPath);
            }
        }
        else
            rc = RTErrConvertFromErrno(errno);
        RTStrFree(pszNativePath);
    }

    LogFlow(("RTPathReal(%p:{%s}, %p:{%s}, %u): returns %Rrc\n", pszPath, pszPath,
             pszRealPath, RT_SUCCESS(rc) ? pszRealPath : "<failed>",  cchRealPath));
    return rc;
}


/**
 * Cleans up a path specifier a little bit.
 * This includes removing duplicate slashes, uncessary single dots, and
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

    /* fsCleanPath will leave the single dot alone, we don't need it here. */
    if (szTmpPath[0] == '.' && !szTmpPath[1])
        szTmpPath[0] = '\0';

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
        /** @todo use RTPathGetCurrent */
        char szNativeCurDir[PATH_MAX + 1];
        if (getcwd(szNativeCurDir, sizeof(szNativeCurDir)) == NULL)
        {
            rc = RTErrConvertFromErrno(errno);
            AssertMsgFailedReturn(("Couldn't get cwd! rc=%Rrc errno=%d\n", rc, errno), rc);
        }

        char *pszCurDir;
        rc = rtPathFromNative(&pszCurDir, szNativeCurDir);
        if (RT_FAILURE(rc))
        {
            LogFlow(("RTPathAbs(%p:{%s}, %p, %d): returns %Rrc\n", pszPath, pszPath, pszAbsPath, cchAbsPath, rc));
            return rc;
        }

        size_t cchCurDir = fsCleanPath(pszCurDir); /* paranoia */
        if (cchCurDir + cchTmpPath + 1 > PATH_MAX)
        {
            RTStrFree(pszCurDir);
            LogFlow(("RTPathAbs(%p:{%s}, %p, %d): returns %Rrc\n", pszPath, pszPath, pszAbsPath, cchAbsPath, VERR_FILENAME_TOO_LONG));
            return VERR_FILENAME_TOO_LONG;
        }

        memmove(szTmpPath + cchCurDir + 1, szTmpPath, cchTmpPath + 1);
        memcpy(szTmpPath, pszCurDir, cchCurDir);
        szTmpPath[cchCurDir] = '/';

        RTStrFree(pszCurDir);

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


#ifndef RT_OS_L4
/**
 * Worker for RTPathUserHome that looks up the home directory
 * using the getpwuid_r api.
 *
 * @returns IPRT status code.
 * @param   pszPath     The path buffer.
 * @param   cchPath     The size of the buffer.
 * @param   uid         The User ID to query the home directory of.
 */
static int rtPathUserHomeByPasswd(char *pszPath, size_t cchPath, uid_t uid)
{
    /*
     * The getpwuid_r function uses the passed in buffer to "allocate" any
     * extra memory it needs. On some systems we should probably use the
     * sysconf function to find the appropriate buffer size, but since it won't
     * work everywhere we'll settle with a 5KB buffer and ASSUME that it'll
     * suffice for even the lengthiest user descriptions...
     */
    char            achBuffer[5120];
    struct passwd   Passwd;
    struct passwd  *pPasswd;
    memset(&Passwd, 0, sizeof(Passwd));
    int rc = getpwuid_r(uid, &Passwd, &achBuffer[0], sizeof(achBuffer), &pPasswd);
    if (rc != 0)
        return RTErrConvertFromErrno(rc);
    if (!pPasswd) /* uid not found in /etc/passwd */
        return VERR_PATH_NOT_FOUND;

    /*
     * Check that it isn't empty and that it exists.
     */
    struct stat st;
    if (    !pPasswd->pw_dir
        ||  !*pPasswd->pw_dir
        ||  stat(pPasswd->pw_dir, &st)
        ||  !S_ISDIR(st.st_mode))
        return VERR_PATH_NOT_FOUND;

    /*
     * Convert it to UTF-8 and copy it to the return buffer.
     */
    char *pszUtf8Path;
    rc = rtPathFromNative(&pszUtf8Path, pPasswd->pw_dir);
    if (RT_SUCCESS(rc))
    {
        size_t cchHome = strlen(pszUtf8Path);
        if (cchHome < cchPath)
            memcpy(pszPath, pszUtf8Path, cchHome + 1);
        else
            rc = VERR_BUFFER_OVERFLOW;
        RTStrFree(pszUtf8Path);
    }
    return rc;
}
#endif


/**
 * Worker for RTPathUserHome that looks up the home directory
 * using the HOME environment variable.
 *
 * @returns IPRT status code.
 * @param   pszPath     The path buffer.
 * @param   cchPath     The size of the buffer.
 */
static int rtPathUserHomeByEnv(char *pszPath, size_t cchPath)
{
    /*
     * Get HOME env. var it and validate it's existance.
     */
    int rc = VERR_PATH_NOT_FOUND;
    const char *pszHome = getenv("HOME");
    if (pszHome)

    {
        struct stat st;
        if (    !stat(pszHome, &st)
            &&  S_ISDIR(st.st_mode))
        {
            /*
             * Convert it to UTF-8 and copy it to the return buffer.
             */
            char *pszUtf8Path;
            rc = rtPathFromNative(&pszUtf8Path, pszHome);
            if (RT_SUCCESS(rc))
            {
                size_t cchHome = strlen(pszUtf8Path);
                if (cchHome < cchPath)
                    memcpy(pszPath, pszUtf8Path, cchHome + 1);
                else
                    rc = VERR_BUFFER_OVERFLOW;
                RTStrFree(pszUtf8Path);
            }
        }
    }
    return rc;
}


RTDECL(int) RTPathUserHome(char *pszPath, size_t cchPath)
{
    int rc;
#ifndef RT_OS_L4
    /*
     * We make an exception for the root user and use the system call
     * getpwuid_r to determine their initial home path instead of
     * reading it from the $HOME variable.  This is because the $HOME
     * variable does not get changed by sudo (and possibly su and others)
     * which can cause root-owned files to appear in user's home folders.
     */
     uid_t uid = geteuid();
     if (!uid)
         rc = rtPathUserHomeByPasswd(pszPath, cchPath, uid);
     else
         rc = rtPathUserHomeByEnv(pszPath, cchPath);

     /*
      * On failure, retry using the alternative method.
      * (Should perhaps restrict the retry cases a bit more here...)
      */
     if (   RT_FAILURE(rc)
         && rc != VERR_BUFFER_OVERFLOW)
     {
         if (!uid)
             rc = rtPathUserHomeByEnv(pszPath, cchPath);
         else
             rc = rtPathUserHomeByPasswd(pszPath, cchPath, uid);
     }
#else  /* RT_OS_L4 */
    rc = rtPathUserHomeByEnv(pszPath, cchPath);
#endif /* RT_OS_L4 */

    LogFlow(("RTPathUserHome(%p:{%s}, %u): returns %Rrc\n", pszPath,
             RT_SUCCESS(rc) ? pszPath : "<failed>",  cchPath, rc));
    return rc;
}


RTR3DECL(int) RTPathQueryInfo(const char *pszPath, PRTFSOBJINFO pObjInfo, RTFSOBJATTRADD enmAdditionalAttribs)
{
    /*
     * Validate input.
     */
    AssertMsgReturn(VALID_PTR(pszPath), ("%p\n", pszPath), VERR_INVALID_POINTER);
    AssertReturn(*pszPath, VERR_INVALID_PARAMETER);
    AssertMsgReturn(VALID_PTR(pObjInfo), ("%p\n", pszPath), VERR_INVALID_POINTER);
    AssertMsgReturn(    enmAdditionalAttribs >= RTFSOBJATTRADD_NOTHING
                    &&  enmAdditionalAttribs <= RTFSOBJATTRADD_LAST,
                    ("Invalid enmAdditionalAttribs=%p\n", enmAdditionalAttribs),
                    VERR_INVALID_PARAMETER);

    /*
     * Convert the filename.
     */
    char *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath);
    if (RT_SUCCESS(rc))
    {
        struct stat Stat;
        if (!stat(pszNativePath, &Stat))
        {
            rtFsConvertStatToObjInfo(pObjInfo, &Stat, pszPath, 0);
            switch (enmAdditionalAttribs)
            {
                case RTFSOBJATTRADD_EASIZE:
                    /** @todo Use SGI extended attribute interface to query EA info. */
                    pObjInfo->Attr.enmAdditional          = RTFSOBJATTRADD_EASIZE;
                    pObjInfo->Attr.u.EASize.cb            = 0;
                    break;

                case RTFSOBJATTRADD_NOTHING:
                case RTFSOBJATTRADD_UNIX:
                    Assert(pObjInfo->Attr.enmAdditional == RTFSOBJATTRADD_UNIX);
                    break;

                default:
                    AssertMsgFailed(("Impossible!\n"));
                    return VERR_INTERNAL_ERROR;
            }
        }
        else
            rc = RTErrConvertFromErrno(errno);
        rtPathFreeNative(pszNativePath);
    }

    LogFlow(("RTPathQueryInfo(%p:{%s}, pObjInfo=%p, %d): returns %Rrc\n",
             pszPath, pszPath, pObjInfo, enmAdditionalAttribs, rc));
    return rc;
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
     * Convert the paths.
     */
    char *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath);
    if (RT_SUCCESS(rc))
    {
        /*
         * If it's a no-op, we'll only verify the existance of the file.
         */
        if (!pAccessTime && !pModificationTime)
        {
            struct stat Stat;
            if (!stat(pszNativePath, &Stat))
                rc = VINF_SUCCESS;
            else
            {
                rc = RTErrConvertFromErrno(errno);
                Log(("RTPathSetTimes('%s',,,,): failed with %Rrc and errno=%d\n", pszPath, rc, errno));
            }
        }
        else
        {
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
                int rc = RTPathQueryInfo(pszPath, &ObjInfo, RTFSOBJATTRADD_UNIX);
                if (RT_SUCCESS(rc))
                {
                    RTTimeSpecGetTimeval(pAccessTime        ? pAccessTime       : &ObjInfo.AccessTime,       &aTimevals[0]);
                    RTTimeSpecGetTimeval(pModificationTime  ? pModificationTime : &ObjInfo.ModificationTime, &aTimevals[1]);
                }
                else
                    Log(("RTPathSetTimes('%s',%p,%p,,): RTPathQueryInfo failed with %Rrc\n",
                         pszPath, pAccessTime, pModificationTime, rc));
            }
            if (RT_SUCCESS(rc))
            {
                if (utimes(pszNativePath, aTimevals))
                {
                    rc = RTErrConvertFromErrno(errno);
                    Log(("RTPathSetTimes('%s',%p,%p,,): failed with %Rrc and errno=%d\n",
                         pszPath, pAccessTime, pModificationTime, rc, errno));
                }
            }
        }
        rtPathFreeNative(pszNativePath);
    }

    LogFlow(("RTPathSetTimes(%p:{%s}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}, %p:{%RDtimespec}): return %Rrc\n",
             pszPath, pszPath, pAccessTime, pAccessTime, pModificationTime, pModificationTime,
             pChangeTime, pChangeTime, pBirthTime, pBirthTime));
    return rc;
}


/**
 * Checks if two files are the one and same file.
 */
static bool rtPathSame(const char *pszNativeSrc, const char *pszNativeDst)
{
    struct stat SrcStat;
    if (stat(pszNativeSrc, &SrcStat))
        return false;
    struct stat DstStat;
    if (stat(pszNativeDst, &DstStat))
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
 * @param   pszDst      The destintation path.
 * @param   fRename     The rename flags.
 * @param   fFileType   The filetype. We use the RTFMODE filetypes here. If it's 0,
 *                      anything goes. If it's RTFS_TYPE_DIRECTORY we'll check that the
 *                      source is a directory. If Its RTFS_TYPE_FILE we'll check that it's
 *                      not a directory (we are NOT checking whether it's a file).
 */
int rtPathPosixRename(const char *pszSrc, const char *pszDst, unsigned fRename, RTFMODE fFileType)
{
    /*
     * Convert the paths.
     */
    char *pszNativeSrc;
    int rc = rtPathToNative(&pszNativeSrc, pszSrc);
    if (RT_SUCCESS(rc))
    {
        char *pszNativeDst;
        rc = rtPathToNative(&pszNativeDst, pszDst);
        if (RT_SUCCESS(rc))
        {
            /*
             * Check that the source exists and that any types that's specified matches.
             * We have to check this first to avoid getting errnous VERR_ALREADY_EXISTS
             * errors from the next step.
             *
             * There are race conditions here (perhaps unlikly ones but still), but I'm
             * afraid there is little with can do to fix that.
             */
            struct stat SrcStat;
            if (stat(pszNativeSrc, &SrcStat))
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
                if (stat(pszNativeDst, &DstStat))
                    rc = errno == ENOENT ? VINF_SUCCESS : RTErrConvertFromErrno(errno);
                else
                {
                    Assert(SrcStat.st_dev && DstStat.st_dev);
                    Assert(SrcStat.st_ino && DstStat.st_ino);
                    if (    SrcStat.st_dev == DstStat.st_dev
                        &&  SrcStat.st_ino == DstStat.st_ino
                        &&  (SrcStat.st_mode & S_IFMT) == (SrcStat.st_mode & S_IFMT))
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
                            if (stat(pszNativeDst, &DstStat))
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

            rtPathFreeNative(pszNativeDst);
        }
        rtPathFreeNative(pszNativeSrc);
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


RTDECL(bool) RTPathExists(const char *pszPath)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, false);
    AssertReturn(*pszPath, false);

    /*
     * Convert the path and check if it exists using stat().
     */
    char *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath);
    if (RT_SUCCESS(rc))
    {
        struct stat Stat;
        if (!stat(pszNativePath, &Stat))
            rc = VINF_SUCCESS;
        else
            rc = VERR_GENERAL_FAILURE;
        RTStrFree(pszNativePath);
    }
    return RT_SUCCESS(rc);
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
    char *pszNativePath;
    int rc = rtPathToNative(&pszNativePath, pszPath);
    if (RT_SUCCESS(rc))
    {
        if (chdir(pszNativePath))
            rc = RTErrConvertFromErrno(errno);
        RTStrFree(pszNativePath);
    }
    return rc;
}

