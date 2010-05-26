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
#include <iprt/err.h>
#include <iprt/stream.h>
#include <iprt/string.h>

#include <openssl/sha.h>



RTR3DECL(int) RTSha1Digest(const char *pszFile, char **ppszDigest)
{
    /* Validate input */
    AssertPtrReturn(pszFile, VERR_INVALID_POINTER);
    AssertPtrReturn(ppszDigest, VERR_INVALID_POINTER);

    *ppszDigest = NULL;

    /* Initialize OpenSSL */
    SHA_CTX ctx;
    if (!SHA1_Init(&ctx))
        return VERR_INTERNAL_ERROR;

    /** @todo r=bird: Using a stream here doesn't really serve much purpose as
     *        few stream implementations uses a buffer much larger than 4KB. (The
     *        only I'm aware of is libc on OS/2, which uses 8KB.) */

    /* Open the file to calculate a SHA1 sum of */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszFile, "rb", &pStream);
    if (RT_FAILURE(rc))
        return rc;

    /* Read that file in blocks */
    void *pvBuf[4096];
    size_t cbRead;
    do
    {
        cbRead = 0;
        rc = RTStrmReadEx(pStream, pvBuf, 4096, &cbRead);
        if (RT_FAILURE(rc))
            break;
        if(!SHA1_Update(&ctx, pvBuf, cbRead))
        {
            rc = VERR_INTERNAL_ERROR;
            break;
        }
    } while (cbRead > 0);
    RTStrmClose(pStream);

    if (RT_FAILURE(rc))
        return rc;

    /* Finally calculate & format the SHA1 sum */
    unsigned char auchDig[20];
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

