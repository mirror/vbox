/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Path Manipulation.
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
#include <iprt/path.h>
#include <iprt/dir.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/uni.h>
#include "internal/fs.h"
#include "internal/path.h"


/**
 * Strips the filename from a path.
 *
 * @param   pszPath     Path which filename should be extracted from.
 *
 */
RTDECL(void) RTPathStripFilename(char *pszPath)
{
    char *psz = pszPath;
    char *pszLastSep = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastSep = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastSep = psz;
                break;

            /* the end */
            case '\0':
                if (pszLastSep == pszPath)
                    *pszLastSep++ = '.';
                *pszLastSep = '\0';
                return;
        }
    }
    /* will never get here */
}


/**
 * Strips the extension from a path.
 *
 * @param   pszPath     Path which extension should be stripped.
 */
RTDECL(void) RTPathStripExt(char *pszPath)
{
    char *pszDot = NULL;
    for (;; pszPath++)
    {
        switch (*pszPath)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
            case '\\':
#endif
            case '/':
                pszDot = NULL;
                break;
            case '.':
                pszDot = pszPath;
                break;

            /* the end */
            case '\0':
                if (pszDot)
                    *pszDot = '\0';
                return;
        }
    }
    /* will never get here */
}


/**
 * Finds the filename in a path.
 *
 * @returns Pointer to filename within pszPath.
 * @returns NULL if no filename (i.e. empty string or ends with a slash).
 * @param   pszPath     Path to find filename in.
 */
RTDECL(char *) RTPathFilename(const char *pszPath)
{
    const char *psz = pszPath;
    const char *pszLastComp = pszPath;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszLastComp = psz + 1;
                break;

            case '\\':
#endif
            case '/':
                pszLastComp = psz + 1;
                break;

            /* the end */
            case '\0':
                if (*pszLastComp)
                    return (char *)(void *)pszLastComp;
                return NULL;
        }
    }

    /* will never get here */
    return NULL;
}


/**
 * Strips the trailing slashes of a path name.
 *
 * @param   pszPath     Path to strip.
 */
RTDECL(void) RTPathStripTrailingSlash(char *pszPath)
{
    char *pszEnd = strchr(pszPath, '\0');
    while (pszEnd-- > pszPath)
    {
        switch (*pszEnd)
        {
            case '/':
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case '\\':
#endif
                *pszEnd = '\0';
                break;
            default:
                return;
        }
    }
    return;
}


/**
 * Finds the extension part of in a path.
 *
 * @returns Pointer to extension within pszPath.
 * @returns NULL if no extension.
 * @param   pszPath     Path to find extension in.
 */
RTDECL(char *) RTPathExt(const char *pszPath)
{
    const char *psz = pszPath;
    const char *pszExt = NULL;

    for (;; psz++)
    {
        switch (*psz)
        {
            /* handle separators. */
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            case ':':
                pszExt = NULL;
                break;

            case '\\':
#endif
            case '/':
                pszExt = NULL;
                break;
            case '.':
                pszExt = psz;
                break;

            /* the end */
            case '\0':
                if (pszExt)
                    return (char *)(void *)pszExt;
                return NULL;
        }
    }

    /* will never get here */
    return NULL;
}


/**
 * Checks if a path have an extension.
 *
 * @returns true if extension present.
 * @returns false if no extension.
 * @param   pszPath     Path to check.
 */
RTDECL(bool) RTPathHaveExt(const char *pszPath)
{
    return RTPathExt(pszPath) != NULL;
}


/**
 * Checks if a path includes more than a filename.
 *
 * @returns true if path present.
 * @returns false if no path.
 * @param   pszPath     Path to check.
 */
RTDECL(bool) RTPathHavePath(const char *pszPath)
{
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    return strpbrk(pszPath, "/\\:") != NULL;
#else
    return strpbrk(pszPath, "/") != NULL;
#endif
}


/**
 * Helper for RTPathCompare() and RTPathStartsWith().
 *
 * @returns similar to strcmp.
 * @param   pszPath1        Path to compare.
 * @param   pszPath2        Path to compare.
 * @param   fLimit          Limit the comparison to the length of \a pszPath2
 * @internal
 */
static int rtPathCompare(const char *pszPath1, const char *pszPath2, bool fLimit)
{
    if (pszPath1 == pszPath2)
        return 0;
    if (!pszPath1)
        return -1;
    if (!pszPath2)
        return 1;

#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
    PRTUNICP puszPath1;
    int rc = RTStrToUni(pszPath1, &puszPath1);
    if (RT_FAILURE(rc))
        return -1;
    PRTUNICP puszPath2;
    rc = RTStrToUni(pszPath2, &puszPath2);
    if (RT_FAILURE(rc))
    {
        RTUniFree(puszPath1);
        return 1;
    }

    int iDiff = 0;
    PRTUNICP puszTmpPath1 = puszPath1;
    PRTUNICP puszTmpPath2 = puszPath2;
    for (;;)
    {
        register RTUNICP uc1 = *puszTmpPath1;
        register RTUNICP uc2 = *puszTmpPath2;
        if (uc1 != uc2)
        {
            if (uc1 == '\\')
                uc1 = '/';
            else
                uc1 = RTUniCpToUpper(uc1);
            if (uc2 == '\\')
                uc2 = '/';
            else
                uc2 = RTUniCpToUpper(uc2);
            if (uc1 != uc2)
            {
                iDiff = uc1 > uc2 ? 1 : -1; /* (overflow/underflow paranoia) */
                if (fLimit && uc2 == '\0')
                    iDiff = 0;
                break;
            }
        }
        if (!uc1)
            break;
        puszTmpPath1++;
        puszTmpPath2++;

    }

    RTUniFree(puszPath2);
    RTUniFree(puszPath1);
    return iDiff;

#else
    if (!fLimit)
        return strcmp(pszPath1, pszPath2);
    return strncmp(pszPath1, pszPath2, strlen(pszPath2));
#endif
}


/**
 * Compares two paths.
 *
 * The comparison takes platform-dependent details into account,
 * such as:
 * <ul>
 * <li>On DOS-like platforms, both |\| and |/| separator chars are considered
 *     to be equal.
 * <li>On platforms with case-insensitive file systems, mismatching characters
 *     are uppercased and compared again.
 * </ul>
 *
 * @remark
 *
 * File system details are currently ignored. This means that you won't get
 * case-insentive compares on unix systems when a path goes into a case-insensitive
 * filesystem like FAT, HPFS, HFS, NTFS, JFS, or similar. For NT, OS/2 and similar
 * you'll won't get case-sensitve compares on a case-sensitive file system.
 *
 * @param   pszPath1    Path to compare (must be an absolute path).
 * @param   pszPath2    Path to compare (must be an absolute path).
 *
 * @returns @< 0 if the first path less than the second path.
 * @returns 0 if the first path identical to the second path.
 * @returns @> 0 if the first path greater than the second path.
 */
RTDECL(int) RTPathCompare(const char *pszPath1, const char *pszPath2)
{
    return rtPathCompare(pszPath1, pszPath2, false /* full path lengths */);
}


/**
 * Checks if a path starts with the given parent path.
 *
 * This means that either the path and the parent path matches completely, or that
 * the path is to some file or directory residing in the tree given by the parent
 * directory.
 *
 * The path comparison takes platform-dependent details into account,
 * see RTPathCompare() for details.
 *
 * @param   pszPath         Path to check, must be an absolute path.
 * @param   pszParentPath   Parent path, must be an absolute path.
 *                          No trailing directory slash!
 *
 * @returns |true| when \a pszPath starts with \a pszParentPath (or when they
 *          are identical), or |false| otherwise.
 *
 * @remark  This API doesn't currently handle root directory compares in a manner
 *          consistant with the other APIs. RTPathStartsWith(pszSomePath, "/") will
 *          not work if pszSomePath isn't "/".
 */
RTDECL(bool) RTPathStartsWith(const char *pszPath, const char *pszParentPath)
{
    if (pszPath == pszParentPath)
        return true;
    if (!pszPath || !pszParentPath)
        return false;

    if (rtPathCompare(pszPath, pszParentPath, true /* limited by path 2 */) != 0)
        return false;

    const size_t cchParentPath = strlen(pszParentPath);
    return RTPATH_IS_SLASH(pszPath[cchParentPath])
        || pszPath[cchParentPath] == '\0';
}


/**
 * Same as RTPathReal only the result is RTStrDup()'ed.
 *
 * @returns Pointer to real path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathReal() or RTStrDup() fails.
 * @param   pszPath
 */
RTDECL(char *) RTPathRealDup(const char *pszPath)
{
    char szPath[RTPATH_MAX];
    int rc = RTPathReal(pszPath, szPath, sizeof(szPath));
    if (RT_SUCCESS(rc))
        return RTStrDup(szPath);
    return NULL;
}


/**
 * Same as RTPathAbs only the result is RTStrDup()'ed.
 *
 * @returns Pointer to real path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathAbs() or RTStrDup() fails.
 * @param   pszPath         The path to resolve.
 */
RTDECL(char *) RTPathAbsDup(const char *pszPath)
{
    char szPath[RTPATH_MAX];
    int rc = RTPathAbs(pszPath, szPath, sizeof(szPath));
    if (RT_SUCCESS(rc))
        return RTStrDup(szPath);
    return NULL;
}


/**
 * Returns the length of the volume name specifier of the given path.
 * If no such specifier zero is returned.
 */
size_t rtPathVolumeSpecLen(const char *pszPath)
{
#if defined (RT_OS_OS2) || defined (RT_OS_WINDOWS)
    if (pszPath && *pszPath)
    {
        /* UTC path. */
        if (    (pszPath[0] == '\\' || pszPath[0] == '/')
            &&  (pszPath[1] == '\\' || pszPath[1] == '/'))
            return strcspn(pszPath + 2, "\\/") + 2;

        /* Drive letter. */
        if (    pszPath[1] == ':'
            &&  toupper(pszPath[0]) >= 'A' && toupper(pszPath[0]) <= 'Z')
            return 2;
    }
    return 0;

#else
    /* This isn't quite right when looking at the above stuff, but it works assuming that '//' does not mean UNC. */
    /// @todo (dmik) well, it's better to consider there's no volume name
    //  at all on *nix systems
    return 0;
//    return pszPath && pszPath[0] == '/';
#endif
}


/**
 * Get the absolute path (no symlinks, no . or .. components), assuming the
 * given base path as the current directory. The resulting path doesn't have
 * to exist.
 *
 * @returns iprt status code.
 * @param   pszBase         The base path to act like a current directory.
 *                          When NULL, the actual cwd is used (i.e. the call
 *                          is equivalent to RTPathAbs(pszPath, ...).
 * @param   pszPath         The path to resolve.
 * @param   pszAbsPath      Where to store the absolute path.
 * @param   cchAbsPath      Size of the buffer.
 */
RTDECL(int) RTPathAbsEx(const char *pszBase, const char *pszPath, char *pszAbsPath, unsigned cchAbsPath)
{
    if (pszBase && pszPath && !rtPathVolumeSpecLen(pszPath))
    {
#if defined(RT_OS_WINDOWS)
        /* The format for very long paths is not supported. */
        if (    (pszBase[0] == '/' || pszBase[0] == '\\')
            &&  (pszBase[1] == '/' || pszBase[1] == '\\')
            &&   pszBase[2] == '?'
            &&  (pszBase[3] == '/' || pszBase[3] == '\\'))
            return VERR_INVALID_NAME;
#endif

        /** @todo there are a couple of things which isn't 100% correct, although the
         * current code will have to work for now - I don't have time to fix it right now.
         *
         * 1) On Windows & OS/2 we confuse '/' with an abspath spec and will
         *    not necessarily resolve it on the right drive.
         * 2) A trailing slash in the base might cause UNC names to be created.
         * 3) The lengths total doesn't have to be less than max length
         *    if the pszPath starts with a slash.
         */
        size_t cchBase = strlen(pszBase);
        size_t cchPath = strlen(pszPath);
        if (cchBase + cchPath >= RTPATH_MAX)
            return VERR_FILENAME_TOO_LONG;

        bool fRootSpec = pszPath[0] == '/'
#if defined(RT_OS_WINDOWS) || defined(RT_OS_OS2)
            || pszPath[0] == '\\'
#endif
            ;
        size_t cchVolSpec = rtPathVolumeSpecLen(pszBase);
        char szPath[RTPATH_MAX];
        if (fRootSpec)
        {
            /* join the disk name from base and the path */
            memcpy(szPath, pszBase, cchVolSpec);
            strcpy(&szPath[cchVolSpec], pszPath);
        }
        else
        {
            /* join the base path and the path */
            strcpy(szPath, pszBase);
            szPath[cchBase] = RTPATH_DELIMITER;
            strcpy(&szPath[cchBase + 1], pszPath);
        }
        return RTPathAbs(szPath, pszAbsPath, cchAbsPath);
    }

    /* Fallback to the non *Ex version */
    return RTPathAbs(pszPath, pszAbsPath, cchAbsPath);
}


/**
 * Same as RTPathAbsEx only the result is RTStrDup()'ed.
 *
 * @returns Pointer to the absolute path. Use RTStrFree() to free this string.
 * @returns NULL if RTPathAbsEx() or RTStrDup() fails.
 * @param   pszBase         The base path to act like a current directory.
 *                          When NULL, the actual cwd is used (i.e. the call
 *                          is equivalent to RTPathAbs(pszPath, ...).
 * @param   pszPath         The path to resolve.
 */
RTDECL(char *) RTPathAbsExDup(const char *pszBase, const char *pszPath)
{
    char szPath[RTPATH_MAX];
    int rc = RTPathAbsEx(pszBase, pszPath, szPath, sizeof(szPath));
    if (RT_SUCCESS(rc))
        return RTStrDup(szPath);
    return NULL;
}


#ifndef RT_MINI

/**
 * Gets the directory for architecture-independent application data, for
 * example NLS files, module sources, ...
 *
 * Linux:    /usr/shared/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathProgram()
 *
 */
RTDECL(int) RTPathAppPrivateNoArch(char *pszPath, unsigned cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE)
    char *pszUtf8Path;
    int rc;
    rc = rtPathFromNative(&pszUtf8Path, RTPATH_APP_PRIVATE);
    if (RT_SUCCESS(rc))
    {
        size_t cchPathPrivateNoArch = strlen(pszUtf8Path);
        if (cchPathPrivateNoArch < cchPath)
            memcpy(pszPath, pszUtf8Path, cchPathPrivateNoArch + 1);
        else
            rc = VERR_BUFFER_OVERFLOW;
        RTStrFree(pszUtf8Path);
    }
    return rc;
#else
    return RTPathProgram(pszPath, cchPath);
#endif
}


/**
 * Gets the directory for architecture-dependent application data, for
 * example modules which can be loaded at runtime.
 *
 * Linux:    /usr/lib/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathProgram()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppPrivateArch(char *pszPath, unsigned cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_PRIVATE_ARCH)
    char *pszUtf8Path;
    int rc;
    rc = rtPathFromNative(&pszUtf8Path, RTPATH_APP_PRIVATE_ARCH);
    if (RT_SUCCESS(rc))
    {
        size_t cchPathPrivateArch = strlen(pszUtf8Path);
        if (cchPathPrivateArch < cchPath)
            memcpy(pszPath, pszUtf8Path, cchPathPrivateArch + 1);
        else
            rc = VERR_BUFFER_OVERFLOW;
        RTStrFree(pszUtf8Path);
    }
    return rc;
#else
    return RTPathProgram(pszPath, cchPath);
#endif
}


/**
 * Gets the directory of shared libraries. This is not the same as
 * RTPathAppPrivateArch() as Linux depends all shared libraries in
 * a common global directory where ld.so can found them.
 *
 * Linux:    /usr/lib
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathProgram()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathSharedLibs(char *pszPath, unsigned cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_SHARED_LIBS)
    char *pszUtf8Path;
    int rc;
    rc = rtPathFromNative(&pszUtf8Path, RTPATH_SHARED_LIBS);
    if (RT_SUCCESS(rc))
    {
        size_t cchPathSharedLibs = strlen(pszUtf8Path);
        if (cchPathSharedLibs < cchPath)
            memcpy(pszPath, pszUtf8Path, cchPathSharedLibs + 1);
        else
            rc = VERR_BUFFER_OVERFLOW;
        RTStrFree(pszUtf8Path);
    }
    return rc;
#else
    return RTPathProgram(pszPath, cchPath);
#endif
}


/**
 * Gets the directory for documentation.
 *
 * Linux:    /usr/share/doc/@<application@>
 * Windows:  @<program files directory@>/@<application@>
 * Old path: same as RTPathProgram()
 *
 * @returns iprt status code.
 * @param   pszPath     Buffer where to store the path.
 * @param   cchPath     Buffer size in bytes.
 */
RTDECL(int) RTPathAppDocs(char *pszPath, unsigned cchPath)
{
#if !defined(RT_OS_WINDOWS) && defined(RTPATH_APP_DOCS)
    char *pszUtf8Path;
    int rc;
    rc = rtPathFromNative(&pszUtf8Path, RTPATH_APP_DOCS);
    if (RT_SUCCESS(rc))
    {
        size_t cchPathAppDocs = strlen(pszUtf8Path);
        if (cchPathAppDocs < cchPath)
            memcpy(pszPath, pszUtf8Path, cchPathAppDocs + 1);
        else
            rc = VERR_BUFFER_OVERFLOW;
        RTStrFree(pszUtf8Path);
    }
    return rc;
#else
    return RTPathProgram(pszPath, cchPath);
#endif
}

#endif /* !RT_MINI */

