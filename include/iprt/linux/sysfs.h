/* $Id$ */
/** @file
 * IPRT - Linux sysfs access.
 */

/*
 * Copyright (C) 2008-2010 Oracle Corporation
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

#ifndef ___iprt_linux_sysfs_h
#define ___iprt_linux_sysfs_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>

#include <sys/types.h> /* for dev_t */

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_linux_sysfs    RTLinuxSysfs - Linux sysfs
 * @ingroup grp_rt
 * @{
 */

/**
 * Checks if a sysfs file (or directory, device, symlink, whatever) exists.
 *
 * @returns true / false, errno is preserved.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   va          The format args.
 */
RTDECL(bool) RTLinuxSysFsExistsV(const char *pszFormat, va_list va);

/**
 * Checks if a sysfs file (or directory, device, symlink, whatever) exists.
 *
 * @returns true / false, errno is preserved.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   ...         The format args.
 */
RTDECL(bool) RTLinuxSysFsExists(const char *pszFormat, ...);

/**
 * Opens a sysfs file.
 *
 * @returns The file descriptor. -1 and errno on failure.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   va          The format args.
 */
RTDECL(int) RTLinuxSysFsOpenV(const char *pszFormat, va_list va);

/**
 * Opens a sysfs file.
 *
 * @returns The file descriptor. -1 and errno on failure.
 * @param   pszFormat   The name format, either absolute or relative to "/sys/".
 * @param   ...         The format args.
 */
RTDECL(int) RTLinuxSysFsOpen(const char *pszFormat, ...);

/**
 * Closes a file opened with RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 *
 * @param   fd          File descriptor returned by RTLinuxSysFsOpen or
 *                      RTLinuxSysFsOpenV.
 */
RTDECL(void) RTLinuxSysFsClose(int fd);

/**
 * Reads a string from a file opened with RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 *
 * @returns The number of bytes read. -1 and errno on failure.
 * @param   fd          The file descriptor returned by RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 * @param   pszBuf      Where to store the string.
 * @param   cchBuf      The size of the buffer. Must be at least 2 bytes.
 */
RTDECL(ssize_t) RTLinuxSysFsReadStr(int fd, char *pszBuf, size_t cchBuf);

/**
 * Reads the remainder of a file opened with RTLinuxSysFsOpen or
 * RTLinuxSysFsOpenV.
 *
 * @returns IPRT status code.
 * @param   fd          The file descriptor returned by RTLinuxSysFsOpen or RTLinuxSysFsOpenV.
 * @param   pvBuf       Where to store the bits from the file.
 * @param   cbBuf       The size of the buffer.
 * @param   pcbRead     Where to return the number of bytes read.  Optional.
 */
RTDECL(int) RTLinuxSysFsReadFile(int fd, void *pvBuf, size_t cbBuf, size_t *pcbRead);

/**
 * Reads a number from a sysfs file.
 *
 * @returns 64-bit signed value on success, -1 and errno on failure.
 * @param   uBase       The number base, 0 for autodetect.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   va          Format args.
 */
RTDECL(int64_t) RTLinuxSysFsReadIntFileV(unsigned uBase, const char *pszFormat, va_list va);

/**
 * Reads a number from a sysfs file.
 *
 * @returns 64-bit signed value on success, -1 and errno on failure.
 * @param   uBase       The number base, 0 for autodetect.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
RTDECL(int64_t) RTLinuxSysFsReadIntFile(unsigned uBase, const char *pszFormat, ...);

/**
 * Reads a device number from a sysfs file.
 *
 * @returns device number on success, 0 and errno on failure.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   va          Format args.
 */
RTDECL(dev_t) RTLinuxSysFsReadDevNumFileV(const char *pszFormat, va_list va);

/**
 * Reads a device number from a sysfs file.
 *
 * @returns device number on success, 0 and errno on failure.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
RTDECL(dev_t) RTLinuxSysFsReadDevNumFile(const char *pszFormat, ...);

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
RTDECL(ssize_t) RTLinuxSysFsReadStrFileV(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va);

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
RTDECL(ssize_t) RTLinuxSysFsReadStrFile(char *pszBuf, size_t cchBuf, const char *pszFormat, ...);

/**
 * Reads the last element of the path of the file pointed to by the symbolic
 * link specified.
 *
 * This is needed at least to get the name of the driver associated with a
 * device, where pszFormat should be the "driver" link in the devices sysfs
 * directory.
 *
 * @returns The length of the returned string on success, -1 and errno on
 *          failure.
 * @param   pszBuf      Where to store the path element.  Must be at least two
 *                      characters, but a longer buffer would be advisable.
 * @param   cchBuf      The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   va           Format args.
 */
RTDECL(ssize_t) RTLinuxSysFsGetLinkDestV(char *pszBuf, size_t cchBuf, const char *pszFormat, va_list va);

/**
 * Reads the last element of the path of the file pointed to by the symbolic
 * link specified.
 *
 * This is needed at least to get the name of the driver associated with a
 * device, where pszFormat should be the "driver" link in the devices sysfs
 * directory.
 *
 * @returns The length of the returned string on success, -1 and errno on
 *          failure.
 * @param   pszBuf      Where to store the path element.  Must be at least two
 *                      characters, but a longer buffer would be advisable.
 * @param   cchBuf      The size of the buffer pointed to by @a pszBuf.
 * @param   pszFormat   The filename format, either absolute or relative to "/sys/".
 * @param   ...         Format args.
 */
RTDECL(ssize_t) RTLinuxSysFsGetLinkDest(char *pszBuf, size_t cchBuf, const char *pszFormat, ...);

/**
 * Find the path of a device node under /dev, given then device number.
 *
 * This function will recursively search under /dev until it finds a device node
 * matching @a devnum, and store the path into @a pszBuf.  The caller may
 * provide an expected path in pszSuggestion, which will be tried before
 * searching, but due to the variance in Linux systems it can be hard to always
 * correctly predict the path.
 *
 * @returns The length of the returned string on success, -1 and errno on
 *          failure.
 * @returns -1 and ENOENT if no matching device node could be found.
 * @param   DevNum         The device number to search for.
 * @param   fMode          The type of device - only RTFS_TYPE_DEV_CHAR and
 *                         RTFS_TYPE_DEV_BLOCK are valid values.
 * @param   pszBuf         Where to store the path.
 * @param   cchBuf         The size of the buffer.
 * @param   pszSuggestion  The expected path format of the device node, either
 *                         absolute or relative to "/dev". (Optional)
 * @param   va             Format args.
 */
RTDECL(ssize_t) RTLinuxFindDevicePathV(dev_t DevNum, RTFMODE fMode, char *pszBuf, size_t cchBuf,
                                       const char *pszSuggestion, va_list va);

/**
 * Find the path of a device node under /dev, given the device number.
 *
 * This function will recursively search under /dev until it finds a device node
 * matching @a devnum, and store the path into @a pszBuf.  The caller may
 * provide an expected path in pszSuggestion, which will be tried before
 * searching, but due to the variance in Linux systems it can be hard to always
 * correctly predict the path.
 *
 * @returns The length of the returned string on success, -1 and errno on
 *          failure.
 * @returns -1 and ENOENT if no matching device node could be found.
 * @param   DevNum          The device number to search for
 * @param   fMode           The type of device - only RTFS_TYPE_DEV_CHAR and
 *                          RTFS_TYPE_DEV_BLOCK are valid values
 * @param   pszBuf          Where to store the path.
 * @param   cchBuf          The size of the buffer.
 * @param   pszSuggestion   The expected path format of the device node, either
 *                          absolute or relative to "/dev". (Optional)
 * @param   ...             Format args.
 */
RTDECL(ssize_t) RTLinuxFindDevicePath(dev_t DevNum, RTFMODE fMode, char *pszBuf, size_t cchBuf,
                                      const char *pszSuggestion, ...);

/** @} */

RT_C_DECLS_END

#endif

