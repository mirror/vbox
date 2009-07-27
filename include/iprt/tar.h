/** @file
 * IPRT - Tar archive I/O.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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

#ifndef ___iprt_tar_h
#define ___iprt_tar_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_tar    RTTar - Tar archive I/O
 * @ingroup grp_rt
 * @{
 */

/**
 * Check if the specified file exists in the Tar archive.
 *
 * (The matching is case sensitive.)
 *
 * @note    Currently only regular files are supported.
 *
 * @returns iprt status code.
 * @retval  VINF_SUCCESS when the file exists in the Tar archive.
 * @retval  VERR_FILE_NOT_FOUND when the file not exists in the Tar archive.
 *
 * @param   pszTarFile      Tar file to check.
 * @param   pszFile         Filename to check for.
 */
RTR3DECL(int) RTTarQueryFileExists(const char *pszTarFile, const char *pszFile);

/**
 * Create a file list from a Tar archive.
 *
 * @note    Currently only regular files are supported.
 *
 * @returns iprt status code.
 *
 * @param   pszTarFile      Tar file to list files from.
 * @param   ppapszFiles     On success an array with array with the filenames is
 *                          returned. The names must be freed with RTStrFree and
 *                          the array with RTMemFree.
 * @param   pcFiles         On success the number of entries in ppapszFiles.
 */
RTR3DECL(int) RTTarList(const char *pszTarFile, char ***ppapszFiles, size_t *pcFiles);

/**
 * Extract a set of files from a Tar archive.
 *
 * Also note that this function is atomic. If an error occurs all previously
 * extracted files will be deleted.
 *
 * (The matching is case sensitive.)
 *
 * @note    Currently only regular files are supported. Also some of the heade
 *          fields are not used (uid, gid, uname, gname, mtime).
 *
 * @returns iprt status code.
 *
 * @param   pszTarFile      Tar file to extract files from.
 * @param   pszOutputDir    Where to store the extracted files. Must exist.
 * @param   papszFiles      Which files should be extracted.
 * @param   cFiles          The number of files in papszFiles.
 */
RTR3DECL(int) RTTarExtractFiles(const char *pszTarFile, const char *pszOutputDir, const char * const *papszFiles, size_t cFiles);

/**
 * Extract a file by index from a Tar archive.
 *
 * @note    Currently only regular files are supported. Also some of the header
 *          fields are not used (uid, gid, uname, gname, mtime).
 *
 * @returns iprt status code.
 * @retval  VERR_FILE_NOT_FOUND when the index isn't valid.
 *
 * @param   pszTarFile      Tar file to extract the file from.
 * @param   pszOutputDir    Where to store the extracted file. Must exist.
 * @param   iIndex          Which file should be extracted, 0 based.
 * @param   ppszFileName    On success the filename of the extracted file. Must
 *                          be freed with RTStrFree.
 */
RTR3DECL(int) RTTarExtractByIndex(const char *pszTarFile, const char *pszOutputDir, size_t iIndex, char **ppszFileName);

/**
 * Create a Tar archive out of the given files.
 *
 * @note Currently only regular files are supported.
 *
 * @returns iprt status code.
 *
 * @param   pszTarFile      Where to create the Tar archive.
 * @param   papszFiles      Which files should be included.
 * @param   cFiles          The number of files in papszFiles.
 */
RTR3DECL(int) RTTarCreate(const char *pszTarFile, const char * const *papszFiles, size_t cFiles);

/** @} */

RT_C_DECLS_END

#endif /* ___iprt_tar_h */

