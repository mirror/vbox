/* $Id$ */
/** @file
 * IPRT - SHA1 digest creation
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/sha.h>

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/file.h>

#include <openssl/sha.h>

RTR3DECL(int) RTSha1Digest(const char *pszFile, char **ppszDigest, PFNRTSHAPROGRESS pfnProgressCallback, void *pvUser)
{
    /* Validate input */
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszDigest, VERR_INVALID_POINTER);
    AssertPtrNullReturn(pfnProgressCallback, VERR_INVALID_PARAMETER);

    *ppszDigest = NULL;
    int rc = VINF_SUCCESS;

    /* Initialize OpenSSL */
    SHA_CTX ctx;
    if (!SHA1_Init(&ctx))
        return VERR_INTERNAL_ERROR;

    /* Fetch the file size. Only needed if there is a progress callback. */
    float multi = 0;
    if (pfnProgressCallback)
    {
        uint64_t cbFile;
        rc = RTFileQuerySize(pszFile, &cbFile);
        if (RT_FAILURE(rc))
            return rc;
        multi = 100.0 / cbFile;
    }

    /* Open the file to calculate a SHA1 sum of */
    RTFILE file;
    rc = RTFileOpen(&file, pszFile, RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
    if (RT_FAILURE(rc))
        return rc;

    /* Read that file in blocks */
    void *pvBuf = RTMemTmpAlloc(_1M);
    if (!pvBuf)
    {
        RTFileClose(file);
        rc = VERR_NO_MEMORY;
    }
    size_t cbRead;
    size_t cbReadFull = 0;
    for (;;)
    {
        cbRead = 0;
        rc = RTFileRead(file, pvBuf, _1M, &cbRead);
        if (RT_FAILURE(rc) || !cbRead)
            break;
        if(!SHA1_Update(&ctx, pvBuf, cbRead))
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
        cbReadFull += cbRead;
        /* Call progress callback if some is defined */
        if (   pfnProgressCallback
            && RT_FAILURE(pfnProgressCallback((unsigned)(cbReadFull * multi), pvUser)))
        {
            /* Cancel support */
            rc = VERR_CANCELLED;
            break;
        }
    }
    RTMemTmpFree(pvBuf);
    RTFileClose(file);

    if (RT_FAILURE(rc))
        return rc;

    /* Finally calculate & format the SHA1 sum */
    unsigned char auchDig[RTSHA1_HASH_SIZE];
    if (!SHA1_Final(auchDig, &ctx))
        return VERR_INTERNAL_ERROR;

    char *pszDigest;
    rc = RTStrAllocEx(&pszDigest, RTSHA1_DIGEST_LEN + 1);
    if (RT_SUCCESS(rc))
    {
        rc = RTSha1ToString(auchDig, pszDigest, RTSHA1_DIGEST_LEN + 1);
        if (RT_SUCCESS(rc))
            *ppszDigest = pszDigest;
        else
            RTStrFree(pszDigest);
    }

    return rc;
}

