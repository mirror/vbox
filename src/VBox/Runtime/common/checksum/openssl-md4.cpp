/* $Id$ */
/** @file
 * IPRT - Message-Digest Algorithm 4.
 */

/*
 * Copyright (C) 2009-2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"

#include <openssl/opensslconf.h>
#if 0 //ndef OPENSSL_NO_MD4
# include <openssl/md4.h>

# define RT_MD4_PRIVATE_CONTEXT
# include <iprt/md4.h>

# include <iprt/assert.h>

AssertCompile(RT_SIZEOFMEMB(RTMD4CONTEXT, abPadding) >= RT_SIZEOFMEMB(RTMD4CONTEXT, Private));


RTDECL(void) RTMd4(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTMD4_HASH_SIZE])
{
    RTMD4CONTEXT Ctx;
    RTMd4Init(&Ctx);
    RTMd4Update(&Ctx, pvBuf, cbBuf);
    RTMd4Final(&Ctx, pabDigest);
}
RT_EXPORT_SYMBOL(RTMd4);


RTDECL(void) RTMd4Init(PRTMD4CONTEXT pCtx)
{
    MD4_Init(&pCtx->Private);
}
RT_EXPORT_SYMBOL(RTMd4Init);


RTDECL(void) RTMd4Update(PRTMD4CONTEXT pCtx, const void *pvBuf, size_t cbBuf)
{
    MD4_Update(&pCtx->Private, (const unsigned char *)pvBuf, cbBuf);
}
RT_EXPORT_SYMBOL(RTMd4Update);


RTDECL(void) RTMd4Final(PRTMD4CONTEXT pCtx, uint8_t pabDigest[RTMD4_HASH_SIZE])
{
    MD4_Final((unsigned char *)&pabDigest[0], &pCtx->Private);
}
RT_EXPORT_SYMBOL(RTMd4Final);


#else /* OPENSSL_NO_MD4 */
/*
 * If the OpenSSL build doesn't have MD4, use the IPRT implementation.
 */
# include "alt-md4.cpp"
#endif /* OPENSSL_NO_MD4 */

