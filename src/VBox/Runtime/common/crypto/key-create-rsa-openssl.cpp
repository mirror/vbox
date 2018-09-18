/* $Id$ */
/** @file
 * IPRT - Crypto - RSA Key Creation using OpenSSL.
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
#include "internal/iprt.h"
#include <iprt/crypto/key.h>

#ifdef IPRT_WITH_OPENSSL    /* Whole file. */
# include <iprt/err.h>
# include <iprt/string.h>

# include "internal/iprt-openssl.h"
# include <openssl/rsa.h>
# include <openssl/err.h>

# include "key-internal.h"



RTDECL(int) RTCrKeyCreateNewRsa(PRTCRKEY phKey, uint32_t cBits, uint32_t uPubExp, uint32_t fFlags)
{
    AssertPtrReturn(phKey, VERR_INVALID_POINTER);
    AssertMsgReturn(cBits >= 128 && cBits <= _64K, ("cBits=%u\n", cBits), VERR_OUT_OF_RANGE);
    AssertReturn(uPubExp > 0, VERR_OUT_OF_RANGE);
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    rtCrOpenSslInit();

    /*
     * Do the key generation first.
     */
    int rc = VERR_NO_MEMORY;
    RSA *pRsa = RSA_new();
    if (pRsa)
    {
        BIGNUM *pPubExp = BN_new();
        if (pPubExp)
        {
            if (BN_set_word(pPubExp, uPubExp) != 0)
            {
                if (RSA_generate_key_ex(pRsa, cBits, pPubExp, NULL))
                {
                    /*
                     * Create a IPRT key for it by encoding it as a private key.
                     */
                    unsigned char *pbRsaPrivateKey = NULL;
                    int cbRsaPrivateKey = i2d_RSAPrivateKey(pRsa, &pbRsaPrivateKey);
                    if (cbRsaPrivateKey)
                    {
                        rc = rtCrKeyCreateRsaPrivate(phKey, pbRsaPrivateKey, cbRsaPrivateKey, NULL, NULL);
                        OPENSSL_free(pbRsaPrivateKey);
                    }
                    /* else: VERR_NO_MEMORY */
                }
                else
                    rc = VERR_CR_KEY_GEN_FAILED_RSA;
            }
            BN_free(pPubExp);
        }
        RSA_free(pRsa);
    }
    return rc;
}

#endif /* IPRT_WITH_OPENSSL */

