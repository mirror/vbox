/** @file
 * IPRT - Crypto - Secure Socket Layer (SSL) / Transport Security Layer (TLS).
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

#ifdef IPRT_WITH_OPENSSL

/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/ssl.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/file.h>
#include <iprt/mem.h>

#include "internal/magics.h"

#include "internal/iprt-openssl.h"
#include <openssl/ssl.h>


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
/**
 * SSL instance data for OpenSSL.
 */
typedef struct RTCRSSLINT
{
    /** Magic value (RTCRSSLINT_MAGIC). */
    uint32_t            u32Magic;
    /** Reference count. */
    uint32_t volatile   cRefs;
    /** The SSL context. */
    SSL_CTX            *pCtx;
} RTCRSSLINT;

/**
 * SSL session instance data for OpenSSL.
 */
typedef struct RTCRSSLSESSIONINT
{
    /** Magic value (RTCRSSLSESSIONINT_MAGIC). */
    uint32_t            u32Magic;
    /** Reference count. */
    uint32_t volatile   cRefs;
    /** RTCRSSLSESSION_F_XXX. */
    uint32_t            fFlags;

    /** The SSL instance. */
    SSL                *pSsl;
    /** The socket BIO instance. */
    BIO                *pBio;
} RTCRSSLSESSIONINT;



RTDECL(int) RTCrSslCreate(PRTCRSSL phSsl, uint32_t fFlags)
{
    AssertPtr(phSsl);
    *phSsl = NIL_RTCRSSL;
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    const SSL_METHOD *pSslMethod = TLS_server_method();
    if (pSslMethod)
    {
        RTCRSSLINT *pThis = (RTCRSSLINT *)RTMemAllocZ(sizeof(*pThis));
        if (pThis)
        {
            pThis->pCtx = SSL_CTX_new(pSslMethod);
            if (pThis->pCtx)
            {
                pThis->u32Magic = RTCRSSLINT_MAGIC;
                pThis->cRefs    = 1;

                *phSsl = pThis;
                return VINF_SUCCESS;
            }
        }
        return VERR_NO_MEMORY;
    }
    return VERR_NOT_SUPPORTED;
}


RTDECL(uint32_t) RTCrSslRetain(RTCRSSL hSsl)
{
    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < 1024);
    return cRefs;
}


/**
 * Worker for RTCrSslRelease.
 */
static int rtCrSslDestroy(RTCRSSLINT *pThis)
{
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTCRSSLINT_MAGIC);
    SSL_CTX_free(pThis->pCtx);
    pThis->pCtx = NULL;
    RTMemFree(pThis);
    return 0;
}


RTDECL(uint32_t) RTCrSslRelease(RTCRSSL hSsl)
{
    RTCRSSLINT *pThis = hSsl;
    if (pThis == NIL_RTCRSSL)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 1024);
    if (cRefs == 0)
        return rtCrSslDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTCrSslSetCertificateFile(RTCRSSL hSsl, const char *pszFile, uint32_t fFlags)
{
    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fFlags & ~RTCRSSL_FILE_F_ASN1), VERR_INVALID_FLAGS);

    int rcOssl = SSL_CTX_use_certificate_file(pThis->pCtx, pszFile,
                                              RTCRSSL_FILE_F_ASN1 & fFlags ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1);
    if (rcOssl != 0)
        return VINF_SUCCESS;
    return !RTFileExists(pszFile) ? VERR_FILE_NOT_FOUND : VERR_OPEN_FAILED; /** @todo Better status codes */
}


RTDECL(int) RTCrSslSetPrivateKeyFile(RTCRSSL hSsl, const char *pszFile, uint32_t fFlags)
{
    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fFlags & ~RTCRSSL_FILE_F_ASN1), VERR_INVALID_FLAGS);

    int rcOssl = SSL_CTX_use_PrivateKey_file(pThis->pCtx, pszFile,
                                             RTCRSSL_FILE_F_ASN1 & fFlags ? SSL_FILETYPE_PEM : SSL_FILETYPE_ASN1);
    if (rcOssl != 0)
        return VINF_SUCCESS;
    return !RTFileExists(pszFile) ? VERR_FILE_NOT_FOUND : VERR_OPEN_FAILED; /** @todo Better status codes */
}


RTDECL(int) RTCrSslLoadTrustedRootCerts(RTCRSSL hSsl, const char *pszFile, const char *pszDir)
{
    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, VERR_INVALID_HANDLE);

    int rcOssl = SSL_CTX_load_verify_locations(pThis->pCtx, pszFile, pszDir);
    if (rcOssl != 0)
        return VINF_SUCCESS;

    if (pszFile && !RTFileExists(pszFile))
        return VERR_FILE_NOT_FOUND;
    return VERR_OPEN_FAILED; /** @todo Better status codes */
}


RTDECL(int) RTCrSslSetNoPeerVerify(RTCRSSL hSsl)
{
    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, VERR_INVALID_HANDLE);

    SSL_CTX_set_verify(pThis->pCtx, SSL_VERIFY_NONE, NULL);
    return VINF_SUCCESS;
}



//RTDECL(int) RTCrSslCreateSession(RTCRSSL hSsl, RTSOCKET hSocket, uint32_t fFlags, PRTCRSSLSESSION phSslConn);

RTDECL(int) RTCrSslCreateSessionForNativeSocket(RTCRSSL hSsl, RTHCINTPTR hNativeSocket, uint32_t fFlags,
                                                PRTCRSSLSESSION phSslSession)
{
    /*
     * Validate input.
     */
    *phSslSession = NIL_RTCRSSLSESSION;

    RTCRSSLINT *pThis = hSsl;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!(fFlags & ~RTCRSSLSESSION_F_NON_BLOCKING), VERR_INVALID_FLAGS);

    /*
     * Create a new session.
     */
    int                rc       = VERR_NO_MEMORY;
    RTCRSSLSESSIONINT *pSession = (RTCRSSLSESSIONINT *)RTMemAllocZ(sizeof(*pSession));
    if (pSession)
    {
        pSession->pSsl = SSL_new(pThis->pCtx);
        if (pSession->pSsl)
        {
            /* Disable read-ahead if non-blocking socket relying on select/poll. */
            if (fFlags & RTCRSSLSESSION_F_NON_BLOCKING)
                SSL_set_read_ahead(pSession->pSsl, 0);

            /* Create a wrapper for the socket handle. */
            pSession->pBio = BIO_new_socket(hNativeSocket, BIO_NOCLOSE);
            if (pSession->pBio)
            {
                BIO_up_ref(pSession->pBio); /* our reference. */
                SSL_set_bio(pSession->pSsl, pSession->pBio, pSession->pBio);

                /*
                 * Done.
                 */
                pSession->cRefs   = 1;
                pSession->u32Magic = RTCRSSLSESSIONINT_MAGIC;
                *phSslSession = pSession;
                return VINF_SUCCESS;
            }

            SSL_free(pSession->pSsl);
            pSession->pSsl = NULL;
        }
        RTMemFree(pThis);
    }
    return rc;
}


/*********************************************************************************************************************************
*   Session implementation.                                                                                                      *
*********************************************************************************************************************************/

RTDECL(uint32_t) RTCrSslSessionRetain(RTCRSSLSESSION hSslSession)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1);
    Assert(cRefs < 1024);
    return cRefs;
}


/**
 * Worker for RTCrSslRelease.
 */
static int rtCrSslSessionDestroy(RTCRSSLSESSIONINT *pThis)
{
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTCRSSLSESSIONINT_MAGIC);
    SSL_free(pThis->pSsl);
    pThis->pSsl = NULL;
    BIO_free(pThis->pBio);
    pThis->pBio = NULL;
    RTMemFree(pThis);
    return 0;
}


RTDECL(uint32_t) RTCrSslSessionRelease(RTCRSSLSESSION hSslSession)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    if (pThis == NIL_RTCRSSLSESSION)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 1024);
    if (cRefs == 0)
        return rtCrSslSessionDestroy(pThis);
    return cRefs;
}


RTDECL(int) RTCrSslSessionAccept(RTCRSSLSESSION hSslSession, uint32_t fFlags)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    int rcOssl = SSL_accept(pThis->pSsl);
    if (rcOssl > 0)
        return VINF_SUCCESS;

    /** @todo better status codes. */
    if (BIO_should_retry(pThis->pBio))
        return VERR_TRY_AGAIN;
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTCrSslSessionConnect(RTCRSSLSESSION hSslSession, uint32_t fFlags)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    int rcOssl = SSL_connect(pThis->pSsl);
    if (rcOssl > 0)
        return VINF_SUCCESS;

    /** @todo better status codes. */
    if (BIO_should_retry(pThis->pBio))
        return VERR_TRY_AGAIN;
    return VERR_NOT_SUPPORTED;
}


RTDECL(const char *) RTCrSslSessionGetVersion(RTCRSSLSESSION hSslSession)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, NULL);

    return SSL_get_version(pThis->pSsl);
}


RTDECL(int) RTCrSslSessionGetCertIssuerNameAsString(RTCRSSLSESSION hSslSession, char *pszBuf, size_t cbBuf, size_t *pcbActual)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrNull(pszBuf);
    AssertPtrNull(pcbActual);
    if (*pcbActual)
        pcbActual = 0;

    /*
     * Get and format the certificate issuer name.
     */
    int rc = VERR_NOT_AVAILABLE;
    X509 *pCert = SSL_get_certificate(pThis->pSsl);
    if (pCert)
    {
        X509_NAME *pIssuer = X509_get_issuer_name(pCert);
        if (pIssuer)
        {
            char *pszSrc = X509_NAME_oneline(pIssuer, NULL, 0);
            if (pszSrc)
            {
                /*
                 * Copy out the result and free it.
                 */
                size_t cbNeeded = strlen(pszSrc) + 1;
                if (*pcbActual)
                    *pcbActual = cbNeeded;

                if (pszBuf != NULL && cbBuf > 0)
                {
                    if (cbBuf >= cbNeeded)
                    {
                        memcpy(pszBuf, pszSrc, cbNeeded);
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        memcpy(pszBuf, pszSrc, cbBuf - 1);
                        pszBuf[cbBuf - 1] = '\0';
                        rc = VERR_BUFFER_OVERFLOW;
                    }
                }
                else
                    rc = VERR_BUFFER_OVERFLOW;
                OPENSSL_free(pszSrc);
            }
        }
    }
    return rc;
}


RTDECL(bool) RTCrSslSessionPending(RTCRSSLSESSION hSslSession)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, true);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, true);

    return SSL_pending(pThis->pSsl) != 0;
}


RTDECL(ssize_t) RTCrSslSessionRead(RTCRSSLSESSION hSslSession, void *pvBuf, size_t cbToRead)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, VERR_INVALID_HANDLE);

    Assert((size_t)(int)cbToRead == cbToRead);

    int cbActual = SSL_read(pThis->pSsl, pvBuf, (int)cbToRead);
    if (cbActual > 0)
        return cbActual;
    if (BIO_should_retry(pThis->pBio))
        return VERR_TRY_AGAIN;
    return VERR_READ_ERROR; /** @todo better status codes. */
}


RTDECL(ssize_t) RTCrSslSessionWrite(RTCRSSLSESSION hSslSession, void const *pvBuf, size_t cbToWrite)
{
    RTCRSSLSESSIONINT *pThis = hSslSession;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRSSLSESSIONINT_MAGIC, VERR_INVALID_HANDLE);

    Assert((size_t)(int)cbToWrite == cbToWrite);
    Assert(cbToWrite != 0 /* undefined behavior if zero */);

    int cbActual = SSL_write(pThis->pSsl, pvBuf, (int)cbToWrite);
    if (cbActual > 0)
        return cbActual;
    if (BIO_should_retry(pThis->pBio))
        return VERR_TRY_AGAIN;
    return VERR_WRITE_ERROR; /** @todo better status codes. */
}

#endif /* IPRT_WITH_OPENSSL */

