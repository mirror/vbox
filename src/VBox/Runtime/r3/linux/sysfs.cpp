/* $Id$ */
/** @file
 * IPRT - Linux sysfs access.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

/** @todo r=bird: This whole API should be rewritten to use IPRT status codes.
 *        using errno was a mistake and only results in horrible code. */


/**
 * Constructs the path of a sysfs file from the format parameters passed,
 * prepending a prefix if the path is relative.
 *
 * @returns The number of characters returned, or an iprt error code on failure.
 *
 * @param   pszPrefix  The prefix to prepend if the path is relative.  Must end
 *                     in '/'.
 * @param   pszBuf     Where to write the path.  Must be at least
 *                     sizeof(@a pszPrefix) characters long
 * @param   cchBuf     The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat  The name format, either absolute or relative to the
 *                     prefix specified by @a pszPrefix.
 * @param   va         The format args.
 */
static ssize_t rtLinuxConstructPathV(char *pszBuf, size_t cchBuf,
                                     const char *pszPrefix,
                                     const char *pszFormat, va_list va)
{
    size_t cchPrefix = strlen(pszPrefix);
    AssertReturn(pszPrefix[cchPrefix - 1] == '/', VERR_INVALID_PARAMETER);
    AssertReturn(cchBuf > cchPrefix + 1, VERR_INVALID_PARAMETER);

    /** @todo While RTStrPrintfV prevents overflows, it doesn't make it easy to
     *        check for truncations. RTPath should provide some formatters and
     *        joiners which can take over this rather common task that is
     *        performed here. */
    size_t cch = RTStrPrintfV(pszBuf, cchBuf, pszFormat, va);
    if (*pszBuf != '/')
    {
        AssertReturn(cchBuf >= cch + cchPrefix + 1, VERR_BUFFER_OVERFLOW);
        memmove(pszBuf + cchPrefix, pszBuf, cch + 1);
        memcpy(pszBuf, pszPrefix, cchPrefix);
        cch += cchPrefix;
    }
    return cch;
}


/**
 * Constructs the path of a sysfs file from the format parameters passed,
 * prepending a prefix if the path is relative.
 *
 * @returns The number of characters returned, or an iprt error code on failure.
 * @note  Unused.
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
 * Constructs the path of a sysfs file from the format parameters passed,
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
    ssize_t rc = rtLinuxConstructPathV(pszBuf, cchBuf, "/sys/", pszFormat, va);
    if (rc >= 0)
        return rc;
    errno = ERANGE;
    return -1;
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


RTDECL(int) RTLinuxSysFsReadFile(int fd, void *pvBuf, size_t cbBuf, size_t *pcbRead)
{
    int     rc;
    ssize_t cbRead = read(fd, pvBuf, cbBuf);
    if (cbRead >= 0)
    {
        if (pcbRead)
            *pcbRead = cbRead;
        if ((size_t)cbRead < cbBuf)
            rc = VINF_SUCCESS;
        else
        {
            /* Check for EOF */
            char    ch;
            off_t   off     = lseek(fd, 0, SEEK_CUR);
            ssize_t cbRead2 = read(fd, &ch, 1);
            if (cbRead2 == 0)
                rc = VINF_SUCCESS;
            else if (cbRead2 > 0)
            {
                lseek(fd, off, SEEK_SET);
                rc = VERR_BUFFER_OVERFLOW;
            }
            else
                rc = RTErrConvertFromErrno(errno);
        }
    }
    else
        rc = RTErrConvertFromErrno(errno);
    return rc;
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

    dev_t DevNum = 0;
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
                DevNum = makedev(u32Maj, u32Min);
            }
        }
    }
    else if (cchNum == 0)
        errno = EINVAL;

    RTLinuxSysFsClose(fd);
    return DevNum;
}


RTDECL(dev_t) RTLinuxSysFsReadDevNumFile(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    dev_t DevNum = RTLinuxSysFsReadDevNumFileV(pszFormat, va);
    va_end(va);
    return DevNum;
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
        cchName = strlen(pszName); /* = &szLink[rc] - pszName; */
        if (cchName >= cchBuf)
        {
            errno = ERANGE;
            return -1;
        }
        memcpy(pszBuf, pszName, cchName + 1);
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


/** Search for a device node with the number @a DevNum and the type (character
 * or block) @a fMode below the path @a pszPath.  @a pszPath MUST point to a
 * buffer of size at least RTPATH_MAX which will be modified during the function
 * execution.  On successful return it will contain the path to the device node
 * found. */
/** @note This function previously used a local stack buffer of size RTPATH_MAX
 *    to construct the path passed to the next recursive call, which used up 4K
 *    of stack space per iteration and caused a stack overflow on a path with
 *    too many components. */
static int rtLinuxFindDevicePathRecursive(dev_t DevNum, RTFMODE fMode,
                                          char *pszPath)
{
    int rc;
    PRTDIR  pDir;
    size_t const cchPath = strlen(pszPath);

    /*
     * Check assumptions made by the code below.
     */
    AssertReturn(cchPath < RTPATH_MAX - 10U, VERR_BUFFER_OVERFLOW);
    rc = RTDirOpen(&pDir, pszPath);
    if (RT_SUCCESS(rc))
    {
        for (;;)
        {
            RTDIRENTRYEX Entry;
            rc = RTDirReadEx(pDir, &Entry, NULL, RTFSOBJATTRADD_UNIX,
                             RTPATH_F_ON_LINK);
            if (RT_FAILURE(rc))
                break;
            if (RTFS_IS_SYMLINK(Entry.Info.Attr.fMode))
                continue;
            pszPath[cchPath] = '\0';
            rc = RTPathAppend(pszPath, RTPATH_MAX, Entry.szName);
            if (RT_FAILURE(rc))
                break;
            /* Do the matching. */
            if (   Entry.Info.Attr.u.Unix.Device == DevNum
                && (Entry.Info.Attr.fMode & RTFS_TYPE_MASK) == fMode)
                break;
            /* Recurse into subdirectories. */
            if (!RTFS_IS_DIRECTORY(Entry.Info.Attr.fMode))
                continue;
            if (Entry.szName[0] == '.')
                continue;
            rc = rtLinuxFindDevicePathRecursive(DevNum, fMode, pszPath);
            if (RT_SUCCESS(rc) || rc != VERR_NO_MORE_FILES)
                break;
        }
        RTDirClose(pDir);
    }
    return rc;
}


RTDECL(ssize_t) RTLinuxFindDevicePathV(dev_t DevNum, RTFMODE fMode, char *pszBuf,
                                       size_t cchBuf, const char *pszSuggestion,
                                       va_list va)
{
    char szFilename[RTPATH_MAX];
    int rc = VINF_TRY_AGAIN;

    AssertReturn(cchBuf >= 2, VERR_INVALID_PARAMETER);
    AssertReturn(   fMode == RTFS_TYPE_DEV_CHAR
                 || fMode == RTFS_TYPE_DEV_BLOCK,
                 VERR_INVALID_PARAMETER);
    if (pszSuggestion)
    {
        /*
         * Construct the filename and read the link.
         */
        rc = rtLinuxConstructPathV(szFilename, sizeof(szFilename), "/dev/",
                                   pszSuggestion, va);
        if (rc > 0)
        {
            /*
             * Check whether the caller's suggestion was right.
             */
            RTFSOBJINFO Info;
            rc = RTPathQueryInfo(szFilename, &Info, RTFSOBJATTRADD_UNIX);
            if (   rc == VERR_PATH_NOT_FOUND 
                || rc == VERR_FILE_NOT_FOUND
                || (   RT_SUCCESS(rc)
                    && (   Info.Attr.u.Unix.Device != DevNum
                        || (Info.Attr.fMode & RTFS_TYPE_MASK) != fMode)))
            /* The suggestion was wrong, fall back on the brute force attack. */
                rc = VINF_TRY_AGAIN;
        }
    }

    if (rc == VINF_TRY_AGAIN)
    {
        RTStrCopy(szFilename, sizeof(szFilename), "/dev/");
        rc = rtLinuxFindDevicePathRecursive(DevNum, fMode, szFilename);
    }
    if (RT_SUCCESS(rc))
    {
        size_t cchPath = strlen(szFilename);
        if (cchPath >= cchBuf)
            return VERR_BUFFER_OVERFLOW;
        memcpy(pszBuf, szFilename, cchPath + 1);
        return cchPath;
    }
    return rc;
}


/** @todo Do we really need to return the string length?  If the caller is
 * interested (the current ones aren't) they can check themselves. */
RTDECL(ssize_t) RTLinuxFindDevicePath(dev_t DevNum, RTFMODE fMode, char *pszBuf,
                                      size_t cchBuf, const char *pszSuggestion,
                                      ...)
{
    va_list va;
    va_start(va, pszSuggestion);
    int rc = RTLinuxFindDevicePathV(DevNum, fMode, pszBuf, cchBuf, pszSuggestion, va);
    va_end(va);
    return rc;
}

