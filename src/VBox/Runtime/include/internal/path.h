/* $Id$ */
/** @file
 * innotek Portable Runtime - RTPath Internal header.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __path_h__
#define __path_h__

#include <iprt/cdefs.h>
#include <iprt/param.h>

__BEGIN_DECLS
extern char g_szrtProgramPath[RTPATH_MAX];

#if defined(__OS2__) || defined(__WIN__)
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

