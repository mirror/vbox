/* $Id$ */
/** @file
 * IPRT - Crypto - RTCrPkcs5Pbkdf2Hmac implementation using OpenSSL.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt.h"
# include <iprt/crypto/misc.h>

# include <iprt/rand.h>
# include <iprt/assert.h>

# include "internal/iprt-openssl.h"
# include "openssl/evp.h"


RTDECL(int) RTCrPkcs5Pbkdf2Hmac(void const *pvInput, size_t cbInput, void const *pvSalt, size_t cbSalt, uint32_t cIterations,
                                RTDIGESTTYPE enmDigestType, size_t cbKeyLen, void *pvOutput)
{
    const EVP_MD *pDigest;
    switch (enmDigestType)
    {
        case RTDIGESTTYPE_SHA1:
            pDigest = EVP_sha1();
            break;
        case RTDIGESTTYPE_SHA256:
            pDigest = EVP_sha256();
            break;
        case RTDIGESTTYPE_SHA512:
            pDigest = EVP_sha512();
            break;
        default:
            AssertFailedReturn(VERR_CR_DIGEST_NOT_SUPPORTED);
    }

    /* Note! This requires OpenSSL 1.0.0 or higher. */
    Assert((size_t)(int)cbInput == cbInput);
    Assert((size_t)(int)cbSalt == cbSalt);
    Assert((size_t)(int)cbKeyLen == cbKeyLen);
    int rcOssl = PKCS5_PBKDF2_HMAC((const char *)pvInput, (int)cbInput, (const unsigned char *)pvSalt, (int)cbSalt,
                                   (int)cIterations, pDigest, (int)cbKeyLen, (unsigned char *)pvOutput);
    if (rcOssl)
        return VINF_SUCCESS;
    return VERR_CR_PASSWORD_2_KEY_DERIVIATION_FAILED;
}

#endif /* IPRT_WITH_OPENSSL */

