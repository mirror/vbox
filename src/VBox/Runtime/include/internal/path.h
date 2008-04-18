/* $Id$ */
/** @file
 * innotek Portable Runtime - RTPath Internal header.
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

#ifndef ___internal_path_h
#define ___internal_path_h

#include <iprt/cdefs.h>
#include <iprt/param.h>

__BEGIN_DECLS
extern char g_szrtProgramPath[RTPATH_MAX];

#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define HAVE_UNC 1
# define HAVE_DRIVE 1
#endif


int rtPathPosixRename(const char *pszSrc, const char *pszDst, unsigned fRename, RTFMODE fFileType);
int rtPathWin32MoveRename(const char *pszSrc, const char *pszDst, uint32_t fFlags, RTFMODE fFileType);


/**
 * Converts a path from IPRT to native representation.
 *
 * This may involve querying filesystems what codeset they
 * speak and so forth.
 *
 * @returns IPRT status code.
 * @param   ppszNativePath  Where to store the pointer to the native path.
 *                          Free by calling rtPathFreeHost(). NULL on failure.
 * @param   pszPath         The path to convert.
 * @remark  This function is not available on hosts using something else than byte seqences as names. (eg win32)
 */
int rtPathToNative(char **ppszNativePath, const char *pszPath);

/**
 * Converts a path from IPRT to native representation.
 *
 * This may involve querying filesystems what codeset they
 * speak and so forth.
 *
 * @returns IPRT status code.
 * @param   ppszNativePath  Where to store the pointer to the native path.
 *                          Free by calling rtPathFreeHost(). NULL on failure.
 * @param   pszPath         The path to convert.
 * @param   pszBasePath     What pszPath is relative to. If NULL the function behaves like rtPathToNative().
 * @remark  This function is not available on hosts using something else than byte seqences as names. (eg win32)
 */
int rtPathToNativeEx(char **ppszNativePath, const char *pszPath, const char *pszBasePath);

/**
 * Frees a native path returned by rtPathToNative() or rtPathToNativeEx().
 *
 * @param   pszNativePath   The host path to free. NULL allowed.
 * @remark  This function is not available on hosts using something else than byte seqences as names. (eg win32)
 */
void rtPathFreeNative(char *pszNativePath);

/**
 * Converts a path from the native to the IPRT representation.
 *
 * @returns IPRT status code.
 * @param   ppszPath        Where to store the pointer to the IPRT path.
 *                          Free by calling RTStrFree(). NULL on failure.
 * @param   pszNativePath   The native path to convert.
 * @remark  This function is not available on hosts using something else than byte seqences as names. (eg win32)
 */
int rtPathFromNative(char **ppszPath, const char *pszNativePath);

/**
 * Converts a path from the native to the IPRT representation.
 *
 * @returns IPRT status code.
 * @param   ppszPath        Where to store the pointer to the IPRT path.
 *                          Free by calling RTStrFree(). NULL on failure.
 * @param   pszNativePath   The native path to convert.
 * @param   pszBasePath     What pszHostPath is relative to - in IPRT representation.
 *                          If NULL the function behaves like rtPathFromNative().
 * @remark  This function is not available on hosts using something else than byte seqences as names. (eg win32)
 */
int rtPathFromNativeEx(char **ppszPath, const char *pszNativePath, const char *pszBasePath);


__END_DECLS

#endif

