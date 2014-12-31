/* $Id$ */
/** @file
 * IPRT - X509 functions.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
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
#include <iprt/x509-branch-collision.h>

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include <iprt/sha.h>
#include <iprt/manifest.h>
#include <iprt/string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/pem.h>
#include <openssl/x509.h>
#include <openssl/x509v3.h>


/**
 * Preparation before start to work with openssl
 *
 * @returns IPRT status code.
 */
RTDECL(int) RTX509PrepareOpenSSL(void)
{
    OpenSSL_add_all_digests();
    ERR_load_BIO_strings();
    ERR_load_crypto_strings();
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTX509PrepareOpenSSL);


/**
 * Read X509 certificate from the given memory buffer into the internal
 * structure.
 *
 * @returns IPRT status code.
 *
 * @param   pvBuf           String representation containing X509
 *                          certificate in PEM format.
 * @param   cbBuf           The amount of data @a pvBuf points to.
 * @param   ppOutCert       Where to store the pointer to the structure where
 *                          the info about X509 certificate will be stored.
 */
static int rtX509ReadCertificateFromPEM(void const *pvPem, unsigned int cbPem, X509 **ppOutCert)
{
    BIO *pBio = BIO_new(BIO_s_mem());
    if (!pBio)
        return VERR_NO_MEMORY;

    int cb = BIO_write(pBio, pvPem, cbPem);
    *ppOutCert = PEM_read_bio_X509(pBio, NULL, 0, NULL);
    BIO_free(pBio);

    return *ppOutCert ? VINF_SUCCESS : VERR_X509_READING_CERT_FROM_BIO;
}


/**
 * Convert X509 certificate from string to binary representation
 *
 * @returns iprt status code.
 *
 * @param   pvBuf                 string representation
 *                                containing X509 certificate
 *                                in PEM format
 * @param   pOutSignature         memory buffer where the binary
 *                                representation will be stored
 * @param   lengthOfSignature     length of X509 certificate in
 *                                binary representation
 */
static int RTX509ConvertCertificateToBinary(void *pvBuf, unsigned char** pOutSignature, unsigned int* lengthOfSignature)
{
    int rc = VINF_SUCCESS;

    char* beginSignatureStr = RTStrStr((char*)pvBuf, "=");
    beginSignatureStr+=2;
    char* endSignatureStr = RTStrStr((char*)pvBuf, "-----BEGIN CERTIFICATE-----");
    --endSignatureStr;
    *lengthOfSignature = (endSignatureStr - beginSignatureStr)/2;//two hex digits in one byte
    *pOutSignature = (unsigned char *)RTMemAlloc(*lengthOfSignature);

    rc = RTStrConvertHexBytes(beginSignatureStr, *pOutSignature, *lengthOfSignature, 0);

    if (RT_FAILURE(rc))
    {
        RTMemFree(*pOutSignature);
        *pOutSignature = NULL;
    }
    return rc;
}

/**
 * Convert digest from string to binary representation
 *
 * @returns iprt status code.
 *
 * @param   digest                string representation
 * @param   pOutDigest            memory buffer where the binary
 *                                representation will be stored
 * @param   digestType            Type of digest
 * @param   lengthOfDigest        length of digest in binary
 *                                representation
 */
static int RTConvertDigestToBinary(const char* digest,
                                    unsigned char** pOutDigest,
                                    RTDIGESTTYPE digestType,
                                    unsigned int* lengthOfDigest)
{
    int rc = VINF_SUCCESS;

    if(digestType == RTDIGESTTYPE_SHA1)
    {
        *lengthOfDigest = RTSHA1_HASH_SIZE;
        *pOutDigest = (unsigned char *)RTMemAlloc(RTSHA1_HASH_SIZE);
        rc = RTStrConvertHexBytes(digest, *pOutDigest, RTSHA1_HASH_SIZE, 0);
    }
    else if(digestType == RTDIGESTTYPE_SHA256)
    {
        *lengthOfDigest = RTSHA256_HASH_SIZE;
        *pOutDigest = (unsigned char *)RTMemAlloc(RTSHA256_HASH_SIZE);
        rc = RTStrConvertHexBytes(digest, *pOutDigest, RTSHA256_HASH_SIZE, 0);
    }
    else
    {
        rc = VERR_MANIFEST_UNSUPPORTED_DIGEST_TYPE;
    }

    if (RT_FAILURE(rc))
    {
        if(*pOutDigest != NULL)
            RTMemFree(*pOutDigest);
        *pOutDigest = NULL;
    }

    return rc;
}

RTDECL(int) RTRSAVerify(void *pvBuf, unsigned int cbSize, const char* pManifestDigestIn, RTDIGESTTYPE digestType)
{
    int rc = VINF_SUCCESS;
    unsigned char* pSignatureRSA = NULL;
    unsigned char* pManifestDigestOut = NULL;
    X509 *certificate = NULL;
    EVP_PKEY * evp_key = NULL;
    RSA * rsa_key = NULL;

    while(1)
    {
        unsigned int siglen = 0;
        rc = RTX509ConvertCertificateToBinary(pvBuf, &pSignatureRSA, &siglen);
        if (RT_FAILURE(rc))
        {
            /*pSignatureRSA isn't allocated in this case, thus there is no need to free it*/
            break;
        }

        unsigned int diglen = 0;
        rc = RTConvertDigestToBinary(pManifestDigestIn,&pManifestDigestOut, digestType, &diglen);
        if (RT_FAILURE(rc))
        {
            /*pManifestDigestOut isn't allocated in this case, thus there is no need to free it*/
            break;
        }

        rc = rtX509ReadCertificateFromPEM(pvBuf, cbSize, &certificate);
        if (RT_FAILURE(rc))
        {
            /*memory for certificate isn't allocated in this case, thus there is no need to free it*/
            break;
        }

        evp_key = X509_get_pubkey(certificate);
        if (evp_key == NULL)
        {
            rc = VERR_X509_EXTRACT_PUBKEY_FROM_CERT;
            break;
        }

        rsa_key = EVP_PKEY_get1_RSA(evp_key);
        if (rsa_key == NULL)
        {
            rc = VERR_X509_EXTRACT_RSA_FROM_PUBLIC_KEY;
            break;
        }

        rc = RSA_verify(NID_sha1,
                        pManifestDigestOut,
                        diglen,
                        pSignatureRSA,
                        siglen,
                        rsa_key);

        if (rc != 1)
        {
            rc = VERR_X509_RSA_VERIFICATION_FUILURE;
        }

        break;
    }//end while(1)

    if(rsa_key)
        RSA_free(rsa_key);
    if(evp_key)
        EVP_PKEY_free(evp_key);
    if(certificate)
        X509_free(certificate);
    if (pManifestDigestOut)
        RTMemFree(pManifestDigestOut);
    if (pSignatureRSA)
        RTMemFree(pSignatureRSA);

    return rc;
}
RT_EXPORT_SYMBOL(RTRSAVerify);

/**
 * Get X509 certificate basic constraints
 *
 * @returns iprt status code.
 *
 * @param   pvBuf                 string representation
 *                                containing X509 certificate
 *                                in PEM format
 * @param   cbSize                The amount of data (in bytes)
 * @param   pBasicConstraintsOut  memory buffer where the
 *                                extracted basic constraints
 *                                will be stored in string
 *                                representation
 */
static int RTX509GetBasicConstraints(void *pvBuf, unsigned int cbSize, unsigned char** pBasicConstraintsOut)
{
    int rc = VINF_SUCCESS;
    BUF_MEM *bptr = NULL;
    X509 *certificate = NULL;
    char *errDesc = NULL;
    BIO *bio_memory = NULL;

    for (;;)
    {
        rc = rtX509ReadCertificateFromPEM(pvBuf, cbSize, &certificate);
        int loc = X509_get_ext_by_NID(certificate, NID_basic_constraints,-1);

        if(loc == -1)
        {
            rc = VERR_X509_NO_BASIC_CONSTARAINTS;
            break;
        }

        X509_EXTENSION *ext = X509_get_ext(certificate, loc);
        if(!ext)
        {
            rc = VERR_X509_GETTING_EXTENSION_FROM_CERT;
            break;
        }

        ASN1_OCTET_STRING *extdata = X509_EXTENSION_get_data(ext);
        if(!extdata)
        {
            rc = VERR_X509_GETTING_DATA_FROM_EXTENSION;
            break;
        }

        bio_memory = BIO_new(BIO_s_mem());
        if(!X509V3_EXT_print(bio_memory, ext, 0, 0))
        {
            rc = VERR_X509_PRINT_EXTENSION_TO_BIO;
            break;
        }
        BIO_ctrl(bio_memory,BIO_CTRL_FLUSH,0,NULL);
        BIO_ctrl(bio_memory,BIO_C_GET_BUF_MEM_PTR,0,(void *)&bptr);

        // now bptr contains the strings of the key_usage
        unsigned char *buf = (unsigned char *)RTMemAlloc((bptr->length + 1)*sizeof(char));
        memcpy(buf, bptr->data, bptr->length);
        // take care that bptr->data is NOT NULL terminated, so add '\0'
        buf[bptr->length] = '\0';

        *pBasicConstraintsOut = buf;

        break;
    }

    if(certificate)
        X509_free(certificate);

    BIO_free(bio_memory);

    return rc;
}

RTDECL(int) RTX509CertificateVerify(void *pvBuf, unsigned int cbSize)
{
    int rc = VINF_SUCCESS;

    X509 *certificate = NULL;
    X509_NAME * subject = NULL;
    X509_NAME * issuer = NULL;
    EVP_PKEY * evp_key = NULL;
    unsigned char* strBasicConstraints =  NULL;

    while(1)
    {
        rc = rtX509ReadCertificateFromPEM(pvBuf, cbSize, &certificate);
        if (RT_FAILURE(rc))
        {
            break;
        }

        rc = RTX509GetBasicConstraints(pvBuf, cbSize, &strBasicConstraints);
        if (RT_FAILURE(rc))
        {
            break;
        }

        issuer = X509_get_issuer_name(certificate);

        if(strcmp("CA:TRUE", (const char*)strBasicConstraints) == 0)
        {
            subject = X509_get_subject_name(certificate);
            int ki=0;

            if(X509_name_cmp(issuer, subject) == 0)
            {
                evp_key = X509_get_pubkey(certificate);
                ki=X509_verify(certificate,evp_key);
                if(ki>0)
                {
                    /* if it's needed will do something with the verified certificate */
                }
                else
                    rc = VERR_X509_CERTIFICATE_VERIFICATION_FAILURE;
            }
            else
            {
                rc = VINF_X509_NOT_SELFSIGNED_CERTIFICATE;
            }
        }
        else
        {
            rc = VINF_X509_NOT_SELFSIGNED_CERTIFICATE;
        }

        break;
    }

    if(certificate)
        X509_free(certificate);
    if(evp_key)
        EVP_PKEY_free(evp_key);

    RTMemFree(strBasicConstraints);

    return rc;
}
RT_EXPORT_SYMBOL(RTX509CertificateVerify);


RTDECL(unsigned long) RTX509GetErrorDescription(char** pErrorDesc)
{
    char *errorDescription = (char *)RTMemAlloc(256);// buffer must be at least 120 bytes long.
                                             // see http://www.openssl.org/docs/crypto/ERR_error_string.html
    unsigned long err = ERR_get_error();
    ERR_error_string(err, errorDescription);

    *pErrorDesc = errorDescription;

    return err;
}
RT_EXPORT_SYMBOL(RTX509GetError);
