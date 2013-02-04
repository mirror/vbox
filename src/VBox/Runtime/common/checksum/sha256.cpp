/* $Id$ */
/** @file
 * IPRT - SHA-256 hash functions.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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

#include <openssl/sha.h>

#define RT_SHA256_PRIVATE_CONTEXT
#include <iprt/sha.h>

#include <iprt/assert.h>

AssertCompile(RT_SIZEOFMEMB(RTSHA256CONTEXT, abPadding) >= RT_SIZEOFMEMB(RTSHA256CONTEXT, Private));


RTDECL(void) RTSha256(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTSHA256_HASH_SIZE])
{
    RTSHA256CONTEXT Ctx;
    RTSha256Init(&Ctx);
    RTSha256Update(&Ctx, pvBuf, cbBuf);
    RTSha256Final(&Ctx, pabDigest);
}
RT_EXPORT_SYMBOL(RTSha256);


RTDECL(void) RTSha256Init(PRTSHA256CONTEXT pCtx)
{
    SHA256_Init(&pCtx->Private);
}
RT_EXPORT_SYMBOL(RTSha256Init);


RTDECL(void) RTSha256Update(PRTSHA256CONTEXT pCtx, const void *pvBuf, size_t cbBuf)
{
    SHA256_Update(&pCtx->Private, pvBuf, cbBuf);
}
RT_EXPORT_SYMBOL(RTSha256Update);


RTDECL(void) RTSha256Final(PRTSHA256CONTEXT pCtx, uint8_t pabDigest[32])
{
    SHA256_Final((unsigned char *)&pabDigest[0], &pCtx->Private);
}
RT_EXPORT_SYMBOL(RTSha256Final);

