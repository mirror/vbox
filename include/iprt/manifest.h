/** @file
 * IPRT - Manifest file handling.
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

#ifndef ___iprt_manifest_h
#define ___iprt_manifest_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_manifest    RTManifest - Manifest file creation and checking
 * @ingroup grp_rt
 * @{
 */

/**
 * Input structure for RTManifestVerify() which contains the filename & the
 * SHA1 digest.
 */
typedef struct RTMANIFESTTEST
{
    /** The filename. */
    char *pszTestFile;
    /** The SHA1 digest of the file. */
    char *pszTestDigest;
} RTMANIFESTTEST;
/** Pointer to the input structure. */
typedef RTMANIFESTTEST* PRTMANIFESTTEST;

/**
 * Verify the given SHA1 digests against the entries in the manifest file.
 *
 * Please note that not only the various digest have to match, but the
 * filenames as well. If there are more or even less files listed in the
 * manifest file than provided by paTests, VERR_MANIFEST_FILE_MISMATCH will be
 * returned.
 *
 * @returns VBox status code.
 *
 * @param   pszManifestFile      Filename of the manifest file to verify.
 * @param   paTests              Array of files & SHA1 sums.
 * @param   cTests               Number of entries in paTests.
 * @param   piFailed             A index to paTests in the
 *                               VERR_MANIFEST_DIGEST_MISMATCH error case
 *                               (optional).
 */
RTR3DECL(int) RTManifestVerify(const char *pszManifestFile, PRTMANIFESTTEST paTests, size_t cTests, size_t *piFailed);

/**
 * This is analogous to function RTManifestVerify(), but calculates the SHA1
 * sums of the given files itself.
 *
 * @returns VBox status code.
 *
 * @param   pszManifestFile      Filename of the manifest file to verify.
 * @param   papszFiles           Array of files to check SHA1 sums.
 * @param   cFiles               Number of entries in papszFiles.
 * @param   piFailed             A index to papszFiles in the
 *                               VERR_MANIFEST_DIGEST_MISMATCH error case
 *                               (optional).
 */
RTR3DECL(int) RTManifestVerifyFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles, size_t *piFailed);

/**
 * Creates a manifest file for a set of files. The manifest file contains SHA1
 * sums of every provided file and could be used to verify the data integrity
 * of them.
 *
 * @returns VBox status code.
 *
 * @param   pszManifestFile      Filename of the manifest file to create.
 * @param   papszFiles           Array of files to create SHA1 sums for.
 * @param   cFiles               Number of entries in papszFiles.
 */
RTR3DECL(int) RTManifestWriteFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles);

/** @} */

RT_C_DECLS_END

#endif /* ___iprt_manifest_h */

