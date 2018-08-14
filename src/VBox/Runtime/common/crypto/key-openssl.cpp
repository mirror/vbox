/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Keys, OpenSSL glue.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/crypto/digest.h>


#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt-openssl.h"
# include "internal/magics.h"
# include "openssl/evp.h"
# ifndef OPENSSL_VERSION_NUMBER
#  error "Missing OPENSSL_VERSION_NUMBER!"
# endif

# include "key-internal.h"


/**
 * Creates an OpenSSL key for the given IPRT one, returning the message digest
 * algorithm if desired.
 *
 * @returns IRPT status code.
 * @param   hKey            The key to convert to an OpenSSL key.
 * @param   fNeedPublic     Set if we need the public side of the key.
 * @param   pszAlgoObjId    Alogrithm stuff we currently need.
 * @param   ppEvpKey        Where to return the pointer to the key structure.
 * @param   ppEvpMdType     Where to optionally return the message digest type.
 * @param   pErrInfo        Where to optionally return more error details.
 */
DECLHIDDEN(int) rtCrKeyToOpenSslKey(RTCRKEY hKey, bool fNeedPublic, const char *pszAlgoObjId,
                                    EVP_PKEY **ppEvpKey, const EVP_MD **ppEvpMdType, PRTERRINFO pErrInfo)
{
    *ppEvpKey = NULL;
    if (ppEvpMdType)
        *ppEvpMdType = NULL;
    AssertReturn(hKey->u32Magic == RTCRKEYINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(fNeedPublic == !(hKey->fFlags & RTCRKEYINT_F_PRIVATE), VERR_WRONG_TYPE);

    rtCrOpenSslInit();

    /*
     * Translate algorithm object ID into stuff that OpenSSL wants.
     */
    int iAlgoNid = OBJ_txt2nid(pszAlgoObjId);
    if (iAlgoNid == NID_undef)
        return RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN,
                             "Unknown public key algorithm [OpenSSL]: %s", pszAlgoObjId);
    const char *pszAlgoSn = OBJ_nid2sn(iAlgoNid);

# if OPENSSL_VERSION_NUMBER >= 0x10001000 && !defined(LIBRESSL_VERSION_NUMBER)
    int idAlgoPkey = 0;
    int idAlgoMd = 0;
    if (!OBJ_find_sigid_algs(iAlgoNid, &idAlgoMd, &idAlgoPkey))
        return RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN_EVP,
                             "OBJ_find_sigid_algs failed on %u (%s, %s)", iAlgoNid, pszAlgoSn, pszAlgoObjId);
    if (ppEvpMdType)
    {
        const EVP_MD *pEvpMdType = EVP_get_digestbynid(idAlgoMd);
        if (!pEvpMdType)
            return RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN_EVP,
                                 "EVP_get_digestbynid failed on %d (%s, %s)", idAlgoMd, pszAlgoSn, pszAlgoObjId);
        *ppEvpMdType = pEvpMdType;
    }
# else
    const EVP_MD *pEvpMdType = EVP_get_digestbyname(pszAlgoSn);
    if (!pEvpMdType)
        return RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_CIPHER_ALGO_NOT_KNOWN_EVP,
                             "EVP_get_digestbyname failed on %s (%s)", pszAlgoSn, pszAlgoObjId);
    if (ppEvpMdType)
        *ppEvpMdType = pEvpMdType;
# endif

    /*
     * Allocate a new key structure and set its type.
     */
    EVP_PKEY *pEvpNewKey = EVP_PKEY_new();
    if (!pEvpNewKey)
        return RTErrInfoSetF(pErrInfo, VERR_NO_MEMORY, "EVP_PKEY_new(%d) failed", iAlgoNid);

    int rc;
# if OPENSSL_VERSION_NUMBER >= 0x10001000 && !defined(LIBRESSL_VERSION_NUMBER)
    if (EVP_PKEY_set_type(pEvpNewKey, idAlgoPkey))
    {
        int idKeyType = EVP_PKEY_base_id(pEvpNewKey);
# else
        int idKeyType = pEvpNewKey->type = EVP_PKEY_type(pEvpMdType->required_pkey_type[0]);
# endif
        if (idKeyType != NID_undef)

        {
            /*
             * Load the key into the structure.
             */
            const unsigned char *puchPublicKey = hKey->pbEncoded;
            EVP_PKEY *pRet;
            if (fNeedPublic)
                *ppEvpKey = pRet = d2i_PublicKey(idKeyType, &pEvpNewKey, &puchPublicKey, hKey->cbEncoded);
            else
                *ppEvpKey = pRet = d2i_PrivateKey(idKeyType, &pEvpNewKey, &puchPublicKey, hKey->cbEncoded);
            if (pRet)
                return VINF_SUCCESS;

            /* Bail out: */
            rc = RTErrInfoSet(pErrInfo, VERR_CR_PKIX_OSSL_D2I_PUBLIC_KEY_FAILED,
                              fNeedPublic ? "d2i_PublicKey failed" : "d2i_PrivateKey failed");
        }
        else
# if OPENSSL_VERSION_NUMBER < 0x10001000 || defined(LIBRESSL_VERSION_NUMBER)
            rc = RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_EVP_PKEY_TYPE_ERROR, "EVP_PKEY_type() failed");
# else
            rc = RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_EVP_PKEY_TYPE_ERROR, "EVP_PKEY_base_id() failed");
    }
    else
        rc = RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_OSSL_EVP_PKEY_TYPE_ERROR,
                           "EVP_PKEY_set_type(%u) failed (sig algo %s)", idAlgoPkey, pszAlgoSn);
# endif

    EVP_PKEY_free(pEvpNewKey);
    return rc;
}

#endif /* IPRT_WITH_OPENSSL */

