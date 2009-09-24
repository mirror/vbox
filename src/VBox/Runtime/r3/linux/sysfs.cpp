/* $Id$ */
/** @file
 * IPRT - Linux sysfs access.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#define LOG_GROUP RTLOGGROUP_SYSTEM
#include <iprt/linux/sysfs.h>
#include <iprt/assert.h>
#include <iprt/dir.h>
#include <iprt/err.h>
#include <iprt/fs.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>

#include <unistd.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <errno.h>


/**
 * Constructs the path of a sysfs file from the format paramaters passed,
 * prepending a prefix if the path is relative.
 *
 * @returns The number of characters returned, or -1 and errno set to ERANGE on
 *        failure.
 *
 * @param   pszPrefix  The prefix to prepend if the path is relative.  Must end
 *                     in '/'.
 * @param   pszBuf     Where to write the path.  Must be at least
 *                     sizeof(@a pszPrefix) characters long
 * @param   cchBuf     The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat  The name format, either absolute or relative to "/sys/".
 * @param   va         The format args.
 */
static ssize_t rtLinuxConstructPathV(char *pszBuf, size_t cchBuf,
                                     const char *pszPrefix,
                                     const char *pszFormat, va_list va)
{
    size_t cchPrefix = strlen(pszPrefix);
    AssertReturnStmt(pszPrefix[cchPrefix - 1] == '/', errno = ERANGE, -1);
    AssertReturnStmt(cchBuf > cchPrefix + 1, errno = ERANGE, -1);

    size_t cch = RTStrPrintfV(pszBuf, cchBuf, pszFormat, va);
    if (*pszBuf != '/')
    {
        AssertReturnStmt(cchBuf >= cch + cchPrefix + 1, errno = ERANGE, -1);
        memmove(pszBuf + cchPrefix, pszBuf, cch + 1);
        memcpy(pszBuf, pszPrefix, cchPrefix);
        cch += cchPrefix;
    }
    return cch;
}


/**
 * Constructs the path of a sysfs file from the format paramaters passed,
 * prepending a prefix if the path is relative.
 *
 * @returns The number of characters returned, or -1 and errno set to ERANGE on
 *        failure.
 *
 * @param   pszPrefix  The prefix to prepend if the path is relative.  Must end
 *                     in '/'.
 * @param   pszBuf     Where to write the path.  Must be at least
 *                     sizeof(@a pszPrefix) characters long
 * @param   cchBuf     The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat  The name format, either absolute or relative to "/sys/".
 * @param   ...        The format args.
 */
static ssize_t rtLinuxConstructPath(char *pszBuf, size_t cchBuf,
                                    const char *pszPrefix,
                                    const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = rtLinuxConstructPathV(pszBuf, cchBuf, pszPrefix, pszFormat, va);
    va_end(va);
    return rc;
}


/**
 * Constructs the path of a sysfs file from the format paramaters passed,
 * prepending "/sys/" if the path is relative.
 *
 * @returns The number of characters returned, or -1 and errno set to ERANGE on
 *        failure.
 *
 * @param   pszBuf     Where to write the path.  Must be at least
 *                     sizeof("/sys/") characters long
 * @param   cchBuf     The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat  The name format, either absolute or relative to "/sys/".
 * @param   va         The format args.
 */
static ssize_t rtLinuxSysFsConstructPath(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va)
{
    return rtLinuxConstructPathV(pszBuf, cchBuf, "/sys/", pszFormat, va);
}


RTDECL(bool) RTLinuxSysFsExistsV(const char *pszFormat, va_list va)
{
    int iSavedErrno = errno;

    /*
     * Construct the filename and call stat.
     */
    bool fRet = false;
    char szFilename[RTPATH_MAX];
    ssize_t rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename), pszFormat, va);
    if (rc != -1)
    {
        struct stat st;
        fRet = stat(szFilename, &st) == 0;
    }

    errno = iSavedErrno;
    return fRet;
}


RTDECL(bool) RTLinuxSysFsExists(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    bool fRet = RTLinuxSysFsExistsV(pszFormat, va);
    va_end(va);
    return fRet;
}


RTDECL(int) RTLinuxSysFsOpenV(const char *pszFormat, va_list va)
{
    /*
     * Construct the filename and call open.
     */
    char szFilename[RTPATH_MAX];
    ssize_t rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename), pszFormat, va);
    if (rc != -1)
        rc = open(szFilename, O_RDONLY, 0);
    return rc;
}


RTDECL(int) RTLinuxSysFsOpen(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    va_end(va);
    return fd;
}


RTDECL(void) RTLinuxSysFsClose(int fd)
{
    int iSavedErrno = errno;
    close(fd);
    errno = iSavedErrno;
}


RTDECL(ssize_t) RTLinuxSysFsReadStr(int fd, char *pszBuf, size_t cchBuf)
{
    Assert(cchBuf > 1);
    ssize_t cchRead = read(fd, pszBuf, cchBuf - 1);
    pszBuf[cchRead >= 0 ? cchRead : 0] = '\0';
    return cchRead;
}


RTDECL(int64_t) RTLinuxSysFsReadIntFileV(unsigned uBase, const char *pszFormat, va_list va)
{
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    if (fd == -1)
        return -1;

    int64_t i64Ret = -1;
    char szNum[128];
    ssize_t cchNum = RTLinuxSysFsReadStr(fd, szNum, sizeof(szNum));
    if (cchNum > 0)
    {
        int rc = RTStrToInt64Ex(szNum, NULL, uBase, &i64Ret);
        if (RT_FAILURE(rc))
        {
            i64Ret = -1;
            errno = -ETXTBSY; /* just something that won't happen at read / open. */
        }
    }
    else if (cchNum == 0)
        errno = -ETXTBSY; /* just something that won't happen at read / open. */

    RTLinuxSysFsClose(fd);
    return i64Ret;
}


RTDECL(int64_t) RTLinuxSysFsReadIntFile(unsigned uBase, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int64_t i64Ret = RTLinuxSysFsReadIntFileV(uBase, pszFormat, va);
    va_end(va);
    return i64Ret;
}


RTDECL(dev_t) RTLinuxSysFsReadDevNumFileV(const char *pszFormat, va_list va)
{
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    if (fd == -1)
        return 0;

    dev_t devNum = 0;
    char szNum[128];
    ssize_t cchNum = RTLinuxSysFsReadStr(fd, szNum, sizeof(szNum));
    if (cchNum > 0)
    {
        uint32_t u32Maj = 0;
        uint32_t u32Min = 0;
        char *pszNext = NULL;
        int rc = RTStrToUInt32Ex(szNum, &pszNext, 10, &u32Maj);
        if (RT_FAILURE(rc) || (rc != VWRN_TRAILING_CHARS) || (*pszNext != ':'))
            errno = EINVAL;
        else
        {
            rc = RTStrToUInt32Ex(pszNext + 1, NULL, 10, &u32Min);
            if (   rc != VINF_SUCCESS
                && rc != VWRN_TRAILING_CHARS
                && rc != VWRN_TRAILING_SPACES)
                errno = EINVAL;
            else
            {
                errno = 0;
                devNum = makedev(u32Maj, u32Min);
            }
        }
    }
    else if (cchNum == 0)
        errno = EINVAL;

    RTLinuxSysFsClose(fd);
    return devNum;
}


RTDECL(dev_t) RTLinuxSysFsReadDevNumFile(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    dev_t devNum = RTLinuxSysFsReadDevNumFileV(pszFormat, va);
    va_end(va);
    return devNum;
}


RTDECL(ssize_t) RTLinuxSysFsReadStrFileV(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va)
{
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    if (fd == -1)
        return -1;

    ssize_t cchRet = RTLinuxSysFsReadStr(fd, pszBuf, cchBuf);
    RTLinuxSysFsClose(fd);
    if (cchRet > 0)
    {
        char *pchNewLine = (char *)memchr(pszBuf, '\n', cchRet);
        if (pchNewLine)
            *pchNewLine = '\0';
    }
    return cchRet;
}


RTDECL(ssize_t) RTLinuxSysFsReadStrFile(char *pszBuf, size_t cchBuf, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    ssize_t cchRet = RTLinuxSysFsReadStrFileV(pszBuf, cchBuf, pszFormat, va);
    va_end(va);
    return cchRet;
}


RTDECL(ssize_t) RTLinuxSysFsGetLinkDestV(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va)
{
    AssertReturnStmt(cchBuf >= 2, errno = EINVAL, -1);

    /*
     * Construct the filename and read the link.
     */
    char szFilename[RTPATH_MAX];
    int rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename), pszFormat, va);
    if (rc == -1)
        return -1;

    char szLink[RTPATH_MAX];
    rc = readlink(szFilename, szLink, sizeof(szLink));
    if (rc == -1)
        return -1;
    if ((size_t)rc > sizeof(szLink) - 1)
    {
        errno = ERANGE;
        return -1;
    }
    szLink[rc] = '\0'; /* readlink fun. */

    /*
     * Extract the file name component and copy it into the return buffer.
     */
    size_t cchName;
    const char *pszName = RTPathFilename(szLink);
    if (pszName)
    {
        cchName = strlen(pszName);
        if (cchName >= cchBuf)
        {
            errno = ERANGE;
            return -1;
        }
        memcpy(pszBuf, pszName, cchName);
    }
    else
    {
        *pszBuf = '\0';
        cchName = 0;
    }
    return cchName;
}


RTDECL(ssize_t) RTLinuxSysFsGetLinkDest(char *pszBuf, size_t cchBuf, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int rc = RTLinuxSysFsGetLinkDestV(pszBuf, cchBuf, pszFormat, va);
    va_end(va);
    return rc;
}


static ssize_t rtLinuxFindDevicePathRecursive(dev_t devNum, RTFMODE fMode,
                                              const char *pszBasePath,
                                              char *pszBuf, size_t cchBuf)
{
    PRTDIR pDir = NULL;
    RTDIRENTRYEX entry = { {0} };
    int vrc = VINF_SUCCESS;
    ssize_t rc;

    vrc = RTDirOpen(&pDir, pszBasePath);
    if (RT_SUCCESS(vrc))
        while (true)
        {
            vrc = RTDirReadEx(pDir, &entry, NULL, RTFSOBJATTRADD_UNIX);
            if (RT_FAILURE(vrc))
            {
                errno =   (vrc == VERR_NO_MORE_FILES) ? ENOENT
                        : (vrc == VERR_BUFFER_OVERFLOW) ? EOVERFLOW
                        : EIO;
                rc = -1;
                break;
            }
            if (RTFS_IS_SYMLINK(entry.Info.Attr.fMode))
                continue;
            if (   (entry.Info.Attr.u.Unix.Device == devNum)
                && (   (   (fMode == RTFS_TYPE_DEV_CHAR)
                        && RTFS_IS_DEV_CHAR(entry.Info.Attr.fMode))
                    || (   (fMode == RTFS_TYPE_DEV_BLOCK)
                        && RTFS_IS_DEV_BLOCK(entry.Info.Attr.fMode))))
            {
                rc = rtLinuxConstructPath(pszBuf, cchBuf, pszBasePath, "%s",
                                          entry.szName);
                break;
            }
            if (!RTFS_IS_DIRECTORY(entry.Info.Attr.fMode))
                continue;
            if (entry.szName[0] == '.')
                continue;
            char szPath[RTPATH_MAX];
            /** @todo this is a temporary hack, as RTDirReadEx currently
             * doesn't know about symbolic links */
            rc = rtLinuxConstructPath(szPath, sizeof(szPath), pszBasePath,
                                      "%s", entry.szName);
            if (rc < 0)
                break;
            struct stat Stat = { 0 };
            if (   lstat(szPath, &Stat) < 0
                || S_ISLNK(Stat.st_mode))
                continue;
            /* @todo ends here */
            rc = rtLinuxConstructPath(szPath, sizeof(szPath), pszBasePath,
                                      "%s/", entry.szName);
            if (rc < 0)
                break;
            rc = rtLinuxFindDevicePathRecursive(devNum, fMode, szPath,
                                                pszBuf, cchBuf);
            if ((rc >= 0) || errno != ENOENT)
                break;
        }
    else
    {
        rc = -1;
        errno = RTErrConvertToErrno(vrc);
    }
    RTDirClose(pDir);
    return rc;
}


RTDECL(ssize_t) RTLinuxFindDevicePathV(dev_t devNum, RTFMODE fMode,
                                       char *pszBuf, size_t cchBuf,
                                       const char *pszSuggestion, va_list va)
{
    AssertReturnStmt(cchBuf >= 2, errno = EINVAL, -1);
    AssertReturnStmt(   (fMode == RTFS_TYPE_DEV_CHAR)
                     || (fMode == RTFS_TYPE_DEV_BLOCK),
                     errno = EINVAL, -1);

    if (!pszSuggestion)
        return rtLinuxFindDevicePathRecursive(devNum, fMode, "/dev/",
                                              pszBuf, cchBuf);

    /*
     * Construct the filename and read the link.
     */
    char szFilename[RTPATH_MAX];
    int rc = rtLinuxConstructPathV(szFilename, sizeof(szFilename), "/dev/",
                                   pszSuggestion, va);
    if (rc == -1)
        return -1;

    /*
     * Check whether the caller's suggestion was right.
     */
    /** @todo Should we just be using POSIX stat here? */
    RTFSOBJINFO info = {0};
    int vrc = RTPathQueryInfo(szFilename, &info, RTFSOBJATTRADD_UNIX);
    if (RT_FAILURE(vrc))
    {
        errno =   (vrc == VERR_PATH_NOT_FOUND) ? ENOENT
                : (vrc == VERR_FILE_NOT_FOUND) ? ENOENT
                : EIO;
        return -1;
    }
    if (   (info.Attr.u.Unix.Device == devNum)
        && (   (   (fMode == RTFS_TYPE_DEV_CHAR)
                && RTFS_IS_DEV_CHAR(info.Attr.fMode))
            || (   (fMode == RTFS_TYPE_DEV_BLOCK)
                && RTFS_IS_DEV_BLOCK(info.Attr.fMode))))
    {
        size_t cchPath = strlen(szFilename) + 1;
        if (cchPath > cchBuf)
        {
            errno = EOVERFLOW;
            return -1;
        }
        strcpy(pszBuf, szFilename);
        return cchPath;
    }

    /* If the suggestion was wrong, try the brute force method */
    return rtLinuxFindDevicePathRecursive(devNum, fMode, "/dev/",
                                          pszBuf, cchBuf);
}


RTDECL(ssize_t) RTLinuxFindDevicePath(dev_t devNum, RTFMODE fMode,
                                      char *pszBuf, size_t cchBuf,
                                      const char *pszSuggestion, ...)
{
    va_list va;
    va_start(va, pszSuggestion);
    int rc = RTLinuxFindDevicePathV(devNum, fMode, pszBuf, cchBuf,
                                    pszSuggestion, va);
    va_end(va);
    return rc;
}
