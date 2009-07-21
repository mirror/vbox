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


#include "iprt/manifest.h"

#include <iprt/sha1.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/path.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/err.h>

#include <stdlib.h>

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTFILELISTINT
{
    char *pszManifestFile;
    char *pszManifestDigest;
    PRTMANIFESTTEST pTestPattern;
} RTFILELISTINT;
typedef RTFILELISTINT* PRTFILELISTINT;

/*******************************************************************************
*   Public RTManifest interface                                                *
*******************************************************************************/

RTR3DECL(int) RTManifestVerify(const char *pszManifestFile, PRTMANIFESTTEST paTests, size_t cTests, size_t *piFailed)
{
    /* Validate input */
    if (!pszManifestFile || !paTests)
    {
        AssertMsgFailed(("Must supply pszManifestFile and paTests!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open the manifest file */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszManifestFile, "r", &pStream);
    if (RT_FAILURE(rc))
        return rc;

    PRTFILELISTINT pFileList = (PRTFILELISTINT)RTMemAllocZ(sizeof(RTFILELISTINT)*cTests);
    if (!pFileList)
        return VERR_NO_MEMORY;
    /* Fill our compare list */
    for (size_t i=0; i < cTests; ++i)
        pFileList[i].pTestPattern = &paTests[i];

    /* Parse the manifest file line by line */
    char pszLine[1024];
    do
    {
        rc = RTStrmGetLine(pStream, pszLine, 1024);
        if (RT_FAILURE(rc))
            break;
        size_t cbCount = strlen(pszLine);
        /* Skip empty lines */
        if (cbCount == 0)
            continue;
        /* Check for the digest algorithm */
        if (cbCount < 4 ||
            !(pszLine[0] == 'S' &&
              pszLine[1] == 'H' &&
              pszLine[2] == 'A' &&
              pszLine[3] == '1'))
        {
            /* Digest unsupported */
            rc = VERR_MANIFEST_UNSUPPORTED_DIGEST_TYPE;
            break;
        }
        /* Try to find the filename */
        char *pszNameStart = RTStrStr(pszLine, "(");
        if (!pszNameStart)
        {
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }
        char *pszNameEnd = RTStrStr(pszLine, ")");
        if (!pszNameEnd)
        {
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }
        /* Copy the filename part */
        size_t len = pszNameEnd-pszNameStart - 1;
        char* pszName = (char*)RTMemAlloc(len+1);
        if (!pszName)
        {
            rc = VERR_NO_MEMORY;
            break;
        }
        memcpy(pszName, pszNameStart + 1, len);
        pszName[len] = '\0';
        /* Try to find the digest sum */
        char *pszDigestStart = RTStrStr(pszLine, "=") + 1;
        if (!pszDigestStart)
        {
            rc = VERR_MANIFEST_WRONG_FILE_FORMAT;
            break;
        }
        char *pszDigest = pszDigestStart;
        /* Check our file list against the extracted data */
        bool fFound = false;
        for (size_t i=0; i < cTests; ++i)
        {
            if (!RTStrCmp(RTPathFilename(pFileList[i].pTestPattern->pszTestFile), RTStrStrip(pszName)))
            {
                /* Add the data of the manifest file to the file list */
                fFound = true;
                pFileList[i].pszManifestFile = RTStrDup(RTStrStrip(pszName));
                pFileList[i].pszManifestDigest = RTStrDup(RTStrStrip(pszDigest));
                break;
            }
        }
        RTStrFree(pszName);
        if (!fFound)
        {
            /* There have to be an entry in the file list */
            rc = VERR_MANIFEST_FILE_MISMATCH;
            break;
        }
    }
    while (1);
    RTStrmClose(pStream);

    if (rc == VINF_SUCCESS ||
        rc == VERR_EOF)
    {
        rc = VINF_SUCCESS;
        for (size_t i=0; i < cTests; ++i)
        {
            /* If there is an entry in the file list, which hasn't an
             * equivalent in the manifest file, its an error. */
            if (!pFileList[i].pszManifestFile ||
                !pFileList[i].pszManifestDigest)
            {
                rc = VERR_MANIFEST_FILE_MISMATCH;
                break;
            }
            /* Do the manifest SHA1 digest match against the actual digest? */
            if (RTStrICmp(pFileList[i].pszManifestDigest, pFileList[i].pTestPattern->pszTestDigest))
            {
                if (piFailed)
                    *piFailed = i;
                rc = VERR_MANIFEST_DIGEST_MISMATCH;
                break;
            }
        }
    }

    /* Cleanup */
    for (size_t i=0; i < cTests; ++i)
    {
        if (pFileList[i].pszManifestFile)
            RTStrFree (pFileList[i].pszManifestFile);
        if (pFileList[i].pszManifestDigest)
            RTStrFree (pFileList[i].pszManifestDigest);
    }
    RTMemFree(pFileList);

    return rc;
}

RTR3DECL(int) RTManifestVerifyFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles, size_t *piFailed)
{
    /* Validate input */
    if (!pszManifestFile || !papszFiles)
    {
        AssertMsgFailed(("Must supply pszManifestFile and papszFiles!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Create our compare list */
    PRTMANIFESTTEST pFileList = (PRTMANIFESTTEST)RTMemAllocZ(sizeof(RTMANIFESTTEST)*cFiles);
    if (!pFileList)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;
    /* Fill our compare list */
    for (size_t i=0; i < cFiles; ++i)
    {
        char *pszDigest;
        rc = RTSha1Digest(papszFiles[i], &pszDigest);
        if (RT_FAILURE(rc))
            break;
        pFileList[i].pszTestFile = (char*)papszFiles[i];
        pFileList[i].pszTestDigest = pszDigest;
    }
    /* Do the verify */
    if (RT_SUCCESS(rc))
        rc = RTManifestVerify(pszManifestFile, pFileList, cFiles, piFailed);

    /* Cleanup */
    for (size_t i=0; i < cFiles; ++i)
    {
        if (pFileList[i].pszTestDigest)
            RTStrFree(pFileList[i].pszTestDigest);
    }
    RTMemFree(pFileList);

    return rc;
}

RTR3DECL(int) RTManifestWriteFiles(const char *pszManifestFile, const char * const *papszFiles, size_t cFiles)
{
    /* Validate input */
    if (!pszManifestFile || !papszFiles)
    {
        AssertMsgFailed(("Must supply pszManifestFile and papszFiles!\n"));
        return VERR_INVALID_PARAMETER;
    }

    /* Open a file to stream in */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszManifestFile, "w", &pStream);
    if (RT_FAILURE(rc))
        return rc;

    for (size_t i=0; i < cFiles; ++i)
    {
        /* Calculate the SHA1 digest of every file */
        char *pszDigest;
        rc = RTSha1Digest(papszFiles[i], &pszDigest);
        if (RT_FAILURE(rc))
            break;
        /* Add the entry to the manifest file */
        int cbRet = RTStrmPrintf(pStream, "SHA1 (%s)= %s\n", RTPathFilename(papszFiles[i]), pszDigest);
        RTStrFree(pszDigest);
        if (RT_UNLIKELY(cbRet < 0))
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    }
    RTStrmClose(pStream);
    /* Delete the manifest file on failure */
    if (RT_FAILURE(rc))
        RTFileDelete(pszManifestFile);

    return rc;
}

