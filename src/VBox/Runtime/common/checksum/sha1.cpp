/* $Id$ */
/** @file
 * IPRT - SHA1 digest creation
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

#include <iprt/sha1.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/assert.h>
#include <iprt/err.h>

#include <openssl/sha.h>

/*******************************************************************************
*   Public RTSha1Digest interface                                              *
*******************************************************************************/

RTR3DECL(int) RTSha1Digest(const char *pszFile, char **ppszDigest)
{
    /* Validate input */
    if (!pszFile || !ppszDigest)
    {
        AssertMsgFailed(("Must supply pszFile and ppszDigest!\n"));
        return VERR_INVALID_PARAMETER;
    }

    *ppszDigest = NULL;

    /* Initialize OpenSSL */
    SHA_CTX ctx;
    if (!SHA1_Init(&ctx))
        return VERR_INTERNAL_ERROR;

    /* Open the file to calculate a SHA1 sum of */
    PRTSTREAM pStream;
    int rc = RTStrmOpen(pszFile, "r+b", &pStream);
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
    }
    while (cbRead > 0);
    RTStrmClose(pStream);

    if (RT_FAILURE(rc))
        return rc;

    /* Finally calculate & format the SHA1 sum */
    unsigned char pucDig[20];
    if (!SHA1_Final(pucDig, &ctx))
        return VERR_INTERNAL_ERROR;

    int cbRet = RTStrAPrintf(ppszDigest, "%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x%.2x",
                             pucDig[0] , pucDig[1] , pucDig[2] , pucDig[3] , pucDig[4],
                             pucDig[5] , pucDig[6] , pucDig[7] , pucDig[8] , pucDig[9],
                             pucDig[10], pucDig[11], pucDig[12], pucDig[13], pucDig[14],
                             pucDig[15], pucDig[16], pucDig[17], pucDig[18], pucDig[19]);
    if (RT_UNLIKELY(cbRet == -1))
        rc = VERR_INTERNAL_ERROR;

    return rc;
}

