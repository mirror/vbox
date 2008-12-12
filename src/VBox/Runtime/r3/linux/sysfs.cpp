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
#include <unistd.h>
#include <stdio.h>
#include <sys/sysctl.h>
#include <sys/stat.h>
#include <sys/fcntl.h>
#include <errno.h>

#include <iprt/linux/sysfs.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>


/**
 * Constructs the path of a sysfs file from the format paramaters passed,
 * prepending "/sys/" if the path is relative.
 *
 * @returns The number of characters written, or -1 and errno on failure.
 * @param   pszBuf     Where to write the path.  Must be at least
 *                     sizeof("/sys/") characters long
 * @param   cchBuf     The size of the buffer pointed to by @a pszBuf
 * @param   pszFormat  The name format, either absolute or relative to "/sys/".
 * @param   va         The format args.
 */
static ssize_t rtLinuxSysFsConstructPath(char *pszBuf, size_t cchBuf,
                                         const char *pszFormat, va_list va)
{
    char szFilename[RTPATH_MAX];
    const char szPrefix[] = "/sys/";
    if (cchBuf < sizeof(szPrefix))
        AssertFailedReturnStmt(errno = EINVAL, -1);
    size_t cch = RTStrPrintfV(szFilename, sizeof(szFilename), pszFormat, va);
    if (szFilename[0] == '/')
        *pszBuf = '\0';
    else
        strcpy(pszBuf, szPrefix);
    strncat(pszBuf, szFilename, cchBuf);
    return strlen(pszBuf);
}

/**
 * Checks if a sysfs file (or directory, device, symlink, whatever) exists.
 *
 * @returns true / false, errno is preserved.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   va          The format args.
 */
bool RTLinuxSysFsExistsV(const char *pszFormat, va_list va)
{
    int iSavedErrno = errno;

    /*
     * Construct the filename and call stat.
     */
    char szFilename[RTPATH_MAX];
    bool fRet = false;
    ssize_t rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename),
                                           pszFormat, va);
    if (rc > 0 && static_cast<size_t>(rc) < sizeof(szFilename))
    {
        struct stat st;
        fRet = stat(szFilename, &st) == 0;
    }

    errno = iSavedErrno;
    return fRet;
}

/**
 * Checks if a sysfs file (or directory, device, symlink, whatever) exists.
 *
 * @returns true / false, errno is preserved.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   ...         The format args.
 */
bool RTLinuxSysFsExists(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    bool fRet = RTLinuxSysFsExistsV(pszFormat, va);
    va_end(va);
    return fRet;
}


/**
 * Opens a sysfs file.
 *
 * @returns The file descriptor. -1 and errno on failure.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   va          The format args.
 */
int RTLinuxSysFsOpenV(const char *pszFormat, va_list va)
{
    /*
     * Construct the filename and call open.
     */
    char szFilename[RTPATH_MAX];
    ssize_t rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename),
                                           pszFormat, va);
    if (rc > 0 && static_cast<size_t>(rc) < sizeof(szFilename))
        rc = open(szFilename, O_RDONLY, 0);
    return rc;
}


/**
 * Opens a sysfs file.
 *
 * @returns The file descriptor. -1 and errno on failure.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   ...         The format args.
 */
int RTLinuxSysFsOpen(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    va_end(va);
    return fd;
}


/**
 * Closes a file opened with RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 *
 * @param   fd
 */
void RTLinuxSysFsClose(int fd)
{
    int iSavedErrno = errno;
    close(fd);
    errno = iSavedErrno;
}


/**
 * Reads a string from a file opened with RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 *
 * @returns The number of bytes read. -1 and errno on failure.
 * @param   fd          The file descriptor returned by RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 * @param   pszBuf      Where to store the string.
 * @param   cchBuf      The size of the buffer. Must be at least 2 bytes.
 */
ssize_t RTLinuxSysFsReadStr(int fd, char *pszBuf, size_t cchBuf)
{
    Assert(cchBuf > 1);
    ssize_t cchRead = read(fd, pszBuf, cchBuf - 1);
    pszBuf[cchRead >= 0 ? cchRead : 0] = '\0';
    return cchRead;
}


/**
 * Reads a number from a sysfs file.
 *
 * @returns 64-bit signed value on success, -1 and errno on failure.
 * @param   uBase       The number base, 0 for autodetect.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   va          Format args.
 */
int64_t RTLinuxSysFsReadIntFileV(unsigned uBase, const char *pszFormat, va_list va)
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


/**
 * Reads a number from a sysfs file.
 *
 * @returns 64-bit signed value on success, -1 and errno on failure.
 * @param   uBase       The number base, 0 for autodetect.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
int64_t RTLinuxSysFsReadIntFile(unsigned uBase, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int64_t i64Ret = RTLinuxSysFsReadIntFileV(uBase, pszFormat, va);
    va_end(va);
    return i64Ret;
}


/**
 * Reads a string from a sysfs file.  If the file contains a newline, we only
 * return the text up until there.
 *
 * @returns number of characters read on success, -1 and errno on failure.
 * @param   pszBuf      Where to store the path element.  Must be at least two
 *                      characters, but a longer buffer would be advisable.
 * @param   cchBuf      The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   va          Format args.
 */
ssize_t RTLinuxSysFsReadStrFileV(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va)
{
    int fd = RTLinuxSysFsOpenV(pszFormat, va);
    if (fd == -1)
        return -1;

    ssize_t cchRet = RTLinuxSysFsReadStr(fd, pszBuf, cchBuf);
    RTLinuxSysFsClose(fd);
    char *pchNewLine = NULL;
    if (cchRet > 0)
        pchNewLine = reinterpret_cast<char *>(memchr(pszBuf, '\n', cchRet));
    if (pchNewLine != NULL)
        *pchNewLine = '\0';
    return cchRet;
}


/**
 * Reads a string from a sysfs file.  If the file contains a newline, we only
 * return the text up until there.
 *
 * @returns number of characters read on success, -1 and errno on failure.
 * @param   pszBuf      Where to store the path element.  Must be at least two
 *                      characters, but a longer buffer would be advisable.
 * @param   cchBuf      The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
ssize_t RTLinuxSysFsReadStrFile(char *pszBuf, size_t cchBuf, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    ssize_t cchRet = RTLinuxSysFsReadStrFileV(pszBuf, cchBuf, pszFormat, va);
    va_end(va);
    return cchRet;
}


/**
 * Reads the last element of the path of the file pointed to by the symbolic
 * link specified.  This is needed at least to get the name of the driver
 * associated with a device, where pszFormat should be the "driver" link in the
 * devices sysfs directory.
 *
 * @returns The number of characters written on success, -1 and errno on failure.
 * @param   pszBuf      Where to store the path element.  Must be at least two
 *                      characters, but a longer buffer would be advisable.
 * @param   cchBuf      The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
int RTLinuxSysFsGetLinkDest(char *pszBuf, size_t cchBuf, const char *pszFormat, ...)
{
    if (cchBuf < 2)
        AssertFailedReturnStmt(errno = EINVAL, -1);
    va_list va;
    va_start(va, pszFormat);
    char szFilename[RTPATH_MAX], szLink[RTPATH_MAX];
    int rc = rtLinuxSysFsConstructPath(szFilename, sizeof(szFilename), pszFormat, va);
    va_end(va);
    if (rc == sizeof(szFilename))
    {
        rc = -1;
        errno = EINVAL;  /* Bad format arguements */
    }
    if (rc > 0)
        rc = readlink(szFilename, szLink, sizeof(szLink));
    if (rc > 0 && static_cast<size_t>(rc) > sizeof(szLink) - 1)
    {
        rc = -1;
        errno = EIO;  /* The path name can't be this big. */
    }
    if (rc >= 0)
    {
        szLink[rc] = '\0';
        char *lastPart = strrchr(szLink, '/');
        Assert(lastPart != NULL);  /* rtLinuxSysFsConstructPath guarantees a path
                                    * starting with '/'. */
        *pszBuf = '\0';
        strncat(pszBuf, lastPart + 1, cchBuf);
        rc = strlen(pszBuf);
    }
    return rc;
}

