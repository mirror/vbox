/* $Id$ */
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/manifest.h>

#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <iprt/sha1.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Internal per file structure used by RTManifestVerify
 */
typedef struct RTMANIFESTFILEENTRY
{
    char *pszManifestFile;
    char *pszManifestDigest;
    PRTMANIFESTTEST pTestPattern;
} RTMANIFESTFILEENTRY;
typedef RTMANIFESTFILEENTRY* PRTMANIFESTFILEENTRY;



RTR3DECL(int) RTManifestVerify(const char *pszManifestFile, PRTMANIFESTTEST paTests, size_t cTests, size_t *piFailed)
{
    /* Validate input */
    AssertPtrReturn(pszManifestFile, VERR_INVALID_POINTER);
    AssertPtrReturn(paTests, VERR_INVALID_POINTER);
    AssertReturn(cTests > 0, VERR_INVALID_PARAMETER);

    /* Open the manifest file */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszManifestFile, "r", &pStream);
    if (RT_FAILURE(rc))
        return rc;

    PRTMANIFESTFILEENTRY paFiles = (PRTMANIFESTFILEENTRY)RTMemTmpAllocZ(sizeof(RTMANIFESTFILEENTRY) * cTests);
    if (!paFiles)
    {
        RTStrmClose(pStream);
        return VERR_NO_MEMORY;
    }

    /* Fill our compare list */
    for (size_t i = 0; i < cTests; ++i)
        paFiles[i].pTestPattern = &paTests[i];

    /* Parse the manifest file line by line */
    char szLine[1024];
    for (;;)
    {
        rc = RTStrmGetLine(pStream, szLine, sizeof(szLine));
        if (RT_FAILURE(rc))
            break;
        size_t cch = strlen(szLine);

        /* Skip empty lines */
        if (cch == 0)
            continue;

        /** @todo r=bird:
         *  -# The SHA1 test should probably include a blank space check.
         *  -# If there is a specific order to the elements in the string, it would be
         *     good if the delimiter searching checked for it.
         *  -# Deal with filenames containing delimiter characters.
         */

        /* Check for the digest algorithm */
        if (   cch < 4
            || !(  szLine[0] == 'S'
                && szLine[1] == 'H'
                && szLine[2] == 'A'
                && szLine[3] == '1'))
        {
            /* Digest unsupported */
            rc = VERR_MANIFEST_UNSUPPORTED_DIGEST_TYPE;
            break;
        }

        /* Try to find the filename */
        char *pszNameStart = strchr(szLine, '(');
        if (!pszNameStart)
        {
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }
        char *pszNameEnd = strchr(szLine, ')');
        if (!pszNameEnd)
        {
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }

        /* Copy the filename part */
        size_t cchName = pszNameEnd - pszNameStart - 1;
        char *pszName = (char *)RTMemTmpAlloc(cchName + 1);
        if (!pszName)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        memcpy(pszName, pszNameStart + 1, cchName);
        pszName[cchName] = '\0';

        /* Try to find the digest sum */
        char *pszDigestStart = strchr(szLine, '=');
        if (!pszDigestStart)
        {
            RTMemTmpFree(pszName);
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }
        char *pszDigest = ++pszDigestStart;

        /* Check our file list against the extracted data */
        bool fFound = false;
        for (size_t i = 0; i < cTests; ++i)
        {
            if (!RTStrCmp(RTPathFilename(paFiles[i].pTestPattern->pszTestFile), RTStrStrip(pszName)))
            {
                /* Add the data of the manifest file to the file list */
                paFiles[i].pszManifestFile = RTStrDup(RTStrStrip(pszName));
                paFiles[i].pszManifestDigest = RTStrDup(RTStrStrip(pszDigest));
                fFound = true;
                break;
            }
        }
        RTMemTmpFree(pszName);
        if (!fFound)
        {
            /* There have to be an entry in the file list */
            rc = VERR_MANIFEST_FILE_MISMATCH;
            break;
        }
    }
    RTStrmClose(pStream);

    if (   rc == VINF_SUCCESS
        || rc == VERR_EOF)
    {
        rc = VINF_SUCCESS;
        for (size_t i = 0; i < cTests; ++i)
        {
            /* If there is an entry in the file list, which hasn't an
             * equivalent in the manifest file, its an error. */
            if (   !paFiles[i].pszManifestFile
                || !paFiles[i].pszManifestDigest)
            {
                rc = VERR_MANIFEST_FILE_MISMATCH;
                break;
            }

            /* Do the manifest SHA1 digest match against the actual digest? */
            if (RTStrICmp(paFiles[i].pszManifestDigest, paFiles[i].pTestPattern->pszTestDigest))
            {
                if (piFailed)
                    *piFailed = i;
                rc = VERR_MANIFEST_DIGEST_MISMATCH;
                break;
            }
        }
    }

    /* Cleanup */
    for (size_t i = 0; i < cTests; ++i)
    {
        if (paFiles[i].pszManifestFile)
            RTStrFree(paFiles[i].pszManifestFile);
        if (paFiles[i].pszManifestDigest)
            RTStrFree(paFiles[i].pszManifestDigest);
    }
    RTMemTmpFree(paFiles);

    return rc;
}


RTR3DECL(int) RTManifestVerifyFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles, size_t *piFailed)
{
    /* Validate input */
    AssertPtrReturn(pszManifestFile, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);

    /* Create our compare list */
    PRTMANIFESTTEST paFiles = (PRTMANIFESTTEST)RTMemTmpAllocZ(sizeof(RTMANIFESTTEST) * cFiles);
    if (!paFiles)
        return VERR_NO_MEMORY;

    /* Fill our compare list */
    int rc = VINF_SUCCESS;
    for (size_t i = 0; i < cFiles; ++i)
    {
        char *pszDigest;
        rc = RTSha1Digest(papszFiles[i], &pszDigest);
        if (RT_FAILURE(rc))
            break;
        paFiles[i].pszTestFile = (char*)papszFiles[i];
        paFiles[i].pszTestDigest = pszDigest;
    }

    /* Do the verification */
    if (RT_SUCCESS(rc))
        rc = RTManifestVerify(pszManifestFile, paFiles, cFiles, piFailed);

    /* Cleanup */
    for (size_t i = 0; i < cFiles; ++i)
    {
        if (paFiles[i].pszTestDigest)
            RTStrFree(paFiles[i].pszTestDigest);
    }
    RTMemTmpFree(paFiles);

    return rc;
}


RTR3DECL(int) RTManifestWriteFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    AssertPtrReturn(pszManifestFile, VERR_INVALID_POINTER);
    AssertPtrReturn(papszFiles, VERR_INVALID_POINTER);

    /* Open a file to stream in */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszManifestFile, "w", &pStream);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i = 0; i < cFiles; ++i)
    {
        /* Calculate the SHA1 digest of every file */
        char *pszDigest;
        rc = RTSha1Digest(papszFiles[i], &pszDigest);
        if (RT_FAILURE(rc))
            break;

        /* Add the entry to the manifest file */
        int cch = RTStrmPrintf(pStream, "SHA1 (%s)= %s\n", RTPathFilename(papszFiles[i]), pszDigest);
        RTStrFree(pszDigest);
        if (RT_UNLIKELY(cch < 0))
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }
    int rc2 = RTStrmClose(pStream);
    if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
        rc2 = rc;

    /* Delete the manifest file on failure */
    if (RT_FAILURE(rc))
        RTFileDelete(pszManifestFile);

    return rc;
}

