/* $Id$ */
/** @file
 * IPRT - Crypto - PKCS \#7, Signing
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
#include <iprt/crypto/pkcs7.h>

#include <iprt/errcore.h>
#include <iprt/string.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/key.h>
#include <iprt/crypto/pkix.h>
#include <iprt/crypto/store.h>
#include <iprt/crypto/x509.h>

#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt-openssl.h"
# include <openssl/pkcs7.h>
# include <openssl/cms.h>
# include <openssl/x509.h>
# include <openssl/err.h>
#endif


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * PKCS\#7 / CMS signing operation instance.
 */
typedef struct RTCRPKCS7SIGNINGJOBINT
{
    /** Magic value (RTCRPKCS7SIGNINGJOBINT).  */
    uint32_t                u32Magic;
    /** Reference counter. */
    uint32_t volatile       cRefs;
    /** RTCRPKCS7SIGN_F_XXX. */
    uint64_t                fFlags;
    /** Set if finalized. */
    bool                    fFinallized;

    //....
} RTCRPKCS7SIGNINGJOBINT;

/** Magic value for RTCRPKCS7SIGNINGJOBINT (Jonathan Lethem). */
#define RTCRPKCS7SIGNINGJOBINT_MAGIC    UINT32_C(0x19640219)

/** Handle to PKCS\#7/CMS signing operation. */
typedef struct RTCRPKCS7SIGNINGJOBINT *RTCRPKCS7SIGNINGJOB;
/** Pointer to a PKCS\#7/CMS signing operation handle. */
typedef RTCRPKCS7SIGNINGJOB *PRTCRPKCS7SIGNINGJOB;

//// CMS_sign
//RTDECL(int) RTCrPkcs7Sign(PRTCRPKCS7SIGNINGJOB *phJob, uint64_t fFlags, PCRTCRX509CERTIFICATE pSigner, RTCRKEY hPrivateKey,
//                          RTCRSTORE hAdditionalCerts,
//



RTDECL(int) RTCrPkcs7SimpleSignSignedData(uint32_t fFlags, PCRTCRX509CERTIFICATE pSigner, RTCRKEY hPrivateKey,
                                          void const *pvData, size_t cbData, RTDIGESTTYPE enmDigestType,
                                          RTCRSTORE hAdditionalCerts, void *pvResult, size_t *pcbResult, PRTERRINFO pErrInfo)
{
    size_t const cbResultBuf = *pcbResult;
    *pcbResult = 0;
    AssertReturn(!(fFlags & ~RTCRPKCS7SIGN_SD_F_VALID_MASK), VERR_INVALID_FLAGS);
#ifdef IPRT_WITH_OPENSSL
    AssertReturn((int)cbData >= 0 && (unsigned)cbData == cbData, VERR_TOO_MUCH_DATA);

    /*
     * Resolve the digest type.
     */
    const EVP_MD *pEvpMd = NULL;
    if (enmDigestType != RTDIGESTTYPE_UNKNOWN)
    {
        pEvpMd = (const EVP_MD *)rtCrOpenSslConvertDigestType(enmDigestType, pErrInfo);
        AssertReturn(pEvpMd, pErrInfo ? pErrInfo->rc : VERR_INVALID_PARAMETER);
    }

    /*
     * Convert the private key.
     */
    EVP_PKEY *pEvpPrivateKey = NULL;
    int rc = rtCrKeyToOpenSslKey(hPrivateKey, false /*fNeedPublic*/, (void **)&pEvpPrivateKey, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        /*
         * Convert the signing certificate.
         */
        X509 *pOsslSigner = NULL;
        rc = rtCrOpenSslConvertX509Cert((void **)&pOsslSigner, pSigner, pErrInfo);
        if (RT_SUCCESS(rc))
        {
            /*
             * Convert any additional certificates.
             */
            STACK_OF(X509) *pOsslAdditionalCerts = NULL;
            if (hAdditionalCerts != NIL_RTCRSTORE)
                rc = RTCrStoreConvertToOpenSslCertStack(hAdditionalCerts, 0 /*fFlags*/, (void **)&pOsslAdditionalCerts, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Create a BIO for the data buffer.
                 */
                BIO *pOsslData = BIO_new_mem_buf((void *)pvData, (int)cbData);
                if (pOsslData)
                {
                    /*
                     * Do the signing.
                     */
                    unsigned int fOsslSign = CMS_BINARY | CMS_PARTIAL;
                    if (fFlags & RTCRPKCS7SIGN_SD_F_DEATCHED)
                        fOsslSign |= CMS_DETACHED;
                    if (fFlags & RTCRPKCS7SIGN_SD_F_NO_SMIME_CAP)
                        fOsslSign |= CMS_NOSMIMECAP;
                    CMS_ContentInfo *pCms = CMS_sign(NULL, NULL, pOsslAdditionalCerts, NULL, fOsslSign);
                    if (pCms != NULL)
                    {
                        if (CMS_add1_signer(pCms, pOsslSigner, pEvpPrivateKey, pEvpMd, fOsslSign) != NULL)
                        {
                            rc = CMS_final(pCms, pOsslData, NULL /*dcont*/, fOsslSign);
                            if (rc > 0)
                            {
                                /*
                                 * Get the output and copy it into the result buffer.
                                 */
                                BIO *pOsslResult = BIO_new(BIO_s_mem());
                                if (pOsslResult)
                                {
                                    rc = i2d_CMS_bio(pOsslResult, pCms);
                                    if (rc > 0)
                                    {
                                        BUF_MEM *pBuf = NULL;
                                        rc = (int)BIO_get_mem_ptr(pOsslResult, &pBuf);
                                        if (rc > 0)
                                        {
                                            AssertPtr(pBuf);
                                            size_t const cbResult = pBuf->length;
                                            if (   cbResultBuf >= cbResult
                                                && pvResult != NULL)
                                            {
                                                memcpy(pvResult, pBuf->data, cbResult);
                                                rc = VINF_SUCCESS;
                                            }
                                            else
                                                rc = VERR_BUFFER_OVERFLOW;
                                            *pcbResult = cbResult;
                                        }
                                        else
                                            rc = RTErrInfoSet(pErrInfo, VERR_GENERAL_FAILURE, "BIO_get_mem_ptr");
                                    }
                                    else
                                        rc = RTErrInfoSet(pErrInfo, VERR_GENERAL_FAILURE, "i2d_CMS_bio");
                                    BIO_free(pOsslResult);
                                }
                                else
                                    rc = RTErrInfoSet(pErrInfo, VERR_NO_MEMORY, "BIO_new/BIO_s_mem");
                            }
                            else
                                rc = RTErrInfoSet(pErrInfo, VERR_GENERAL_FAILURE, "CMS_final");
                        }
                        else
                            rc = RTErrInfoSet(pErrInfo, VERR_GENERAL_FAILURE, "CMS_add1_signer");
                        CMS_ContentInfo_free(pCms);
                    }
                    else
                        rc = RTErrInfoSet(pErrInfo, VERR_GENERAL_FAILURE, "CMS_sign");
                    BIO_free(pOsslData);
                }
            }
            rtCrOpenSslFreeConvertedX509Cert(pOsslSigner);
        }
        EVP_PKEY_free(pEvpPrivateKey);
    }
    return rc;
#else
    RT_NOREF(fFlags, pSigner, hPrivateKey, pvData, cbData, enmDigestType, hAdditionalCerts, pvResult, pErrInfo, cbResultBuf);
    *pcbResult = 0;
    return VERR_NOT_IMPLEMENTED;
#endif
}

