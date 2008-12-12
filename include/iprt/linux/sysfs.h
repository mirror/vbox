/* $Id$ */
/** @file
 * IPRT - Linux sysfs access.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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

#ifndef ___iprt_linux_sysfs_h
#define ___iprt_linux_sysfs_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#include <stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_rt_mp RTLinuxSysfs - Linux sysfs
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
 * @param   fd
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
RTDECL(int) RTLinuxSysFsGetLinkDest(char *pszBuf, size_t cchBuf, const char *pszFormat, ...);

/** @} */

__END_DECLS

#endif

