/* $Id$ */
/** @file
 * IPRT - Crypto - OpenSSL Helpers.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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

#ifdef IPRT_WITH_OPENSSL    /* Whole file. */
# include <iprt/err.h>
# include <iprt/string.h>
# include <iprt/mem.h>
# include <iprt/asn1.h>
# include <iprt/crypto/digest.h>

# include "internal/iprt-openssl.h"
# include "internal/openssl-pre.h"
# include <openssl/x509.h>
# include <openssl/err.h>
# include "internal/openssl-post.h"


DECLHIDDEN(void) rtCrOpenSslInit(void)
{
    static bool s_fOssInitalized;
    if (!s_fOssInitalized)
    {
        OpenSSL_add_all_algorithms();
        ERR_load_ERR_strings();
        ERR_load_crypto_strings();

        s_fOssInitalized = true;
    }
}


DECLHIDDEN(int) rtCrOpenSslErrInfoCallback(const char *pach, size_t cch, void *pvUser)
{
    PRTERRINFO pErrInfo = (PRTERRINFO)pvUser;
    size_t cchAlready = pErrInfo->fFlags & RTERRINFO_FLAGS_SET ? strlen(pErrInfo->pszMsg) : 0;
    if (cchAlready + 1 < pErrInfo->cbMsg)
        RTStrCopyEx(pErrInfo->pszMsg + cchAlready, pErrInfo->cbMsg - cchAlready, pach, cch);
    return -1;
}


DECLHIDDEN(int) rtCrOpenSslConvertX509Cert(void **ppvOsslCert, PCRTCRX509CERTIFICATE pCert, PRTERRINFO pErrInfo)
{
    const unsigned char *pabEncoded;
    uint32_t             cbEncoded;
    void                *pvFree;
    int rc = RTAsn1EncodeQueryRawBits(RTCrX509Certificate_GetAsn1Core(pCert),
                                      (const uint8_t **)&pabEncoded, &cbEncoded, &pvFree, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        X509 *pOsslCert = NULL;
        X509 *pOsslCertRet = d2i_X509(&pOsslCert, &pabEncoded, cbEncoded);
        RTMemTmpFree(pvFree);
        if (pOsslCertRet == pOsslCert)
        {
            *ppvOsslCert = pOsslCert;
            return VINF_SUCCESS;
        }
        rc = RTErrInfoSet(pErrInfo, VERR_CR_X509_OSSL_D2I_FAILED, "d2i_X509");

    }
    *ppvOsslCert = NULL;
    return rc;
}


DECLHIDDEN(void) rtCrOpenSslFreeConvertedX509Cert(void *pvOsslCert)
{
    X509_free((X509 *)pvOsslCert);
}


DECLHIDDEN(int) rtCrOpenSslAddX509CertToStack(void *pvOsslStack, PCRTCRX509CERTIFICATE pCert, PRTERRINFO pErrInfo)
{
    X509 *pOsslCert = NULL;
    int rc = rtCrOpenSslConvertX509Cert((void **)&pOsslCert, pCert, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (sk_X509_push((STACK_OF(X509) *)pvOsslStack, pOsslCert))
            rc = VINF_SUCCESS;
        else
        {
            rtCrOpenSslFreeConvertedX509Cert(pOsslCert);
            rc = RTErrInfoSet(pErrInfo, VERR_NO_MEMORY, "sk_X509_push");
        }
    }
    return rc;
}


DECLHIDDEN(const void /*EVP_MD*/ *) rtCrOpenSslConvertDigestType(RTDIGESTTYPE enmDigestType, PRTERRINFO pErrInfo)
{
    const char *pszAlgoObjId = RTCrDigestTypeToAlgorithmOid(enmDigestType);
    AssertReturnStmt(pszAlgoObjId, RTErrInfoSetF(pErrInfo, VERR_INVALID_PARAMETER, "Invalid type: %d", enmDigestType), NULL);

    int iAlgoNid = OBJ_txt2nid(pszAlgoObjId);
    AssertReturnStmt(iAlgoNid != NID_undef,
                     RTErrInfoSetF(pErrInfo, VERR_CR_DIGEST_OSSL_DIGEST_INIT_ERROR,
                                   "OpenSSL does not know: %s (%s)", pszAlgoObjId, RTCrDigestTypeToName(enmDigestType)),
                     NULL);

    const char   *pszAlgoSn  = OBJ_nid2sn(iAlgoNid);
    const EVP_MD *pEvpMdType = EVP_get_digestbyname(pszAlgoSn);
    AssertReturnStmt(pEvpMdType,
                     RTErrInfoSetF(pErrInfo, VERR_CR_DIGEST_OSSL_DIGEST_INIT_ERROR, "OpenSSL/EVP does not know: %d (%s; %s; %s)",
                                   iAlgoNid, pszAlgoSn, pszAlgoSn, RTCrDigestTypeToName(enmDigestType)),
                     NULL);

    return pEvpMdType;
}

#endif /* IPRT_WITH_OPENSSL */

