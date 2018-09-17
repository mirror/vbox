/* $Id$ */
/** @file
 * IPRT - Crypto - Symmetric Cipher using OpenSSL.
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
# include <iprt/crypto/cipher.h>

# include <iprt/asm.h>
# include <iprt/assert.h>
# include <iprt/err.h>
# include <iprt/mem.h>
# include <iprt/string.h>

# include "internal/iprt-openssl.h"
# include "openssl/evp.h"

# include "internal/magics.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * OpenSSL cipher instance data.
 */
typedef struct RTCRCIPHERINT
{
    /** Magic value (RTCRCIPHERINT_MAGIC). */
    uint32_t            u32Magic;
    /** Reference count. */
    uint32_t volatile   cRefs;
    /** The cihper. */
    const EVP_CIPHER   *pCipher;
    /** The IPRT cipher type, if we know it. */
    RTCRCIPHERTYPE      enmType;
} RTCRCIPHERINT;


RTDECL(int) RTCrCipherOpenByType(PRTCRCIPHER phCipher, RTCRCIPHERTYPE enmType, uint32_t fFlags)
{
    AssertPtrReturn(phCipher, VERR_INVALID_POINTER);
    *phCipher = NIL_RTCRCIPHER;
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);

    /*
     * Translate the IPRT cipher type to EVP cipher.
     */
    const EVP_CIPHER *pCipher = NULL;
    switch (enmType)
    {
        case RTCRCIPHERTYPE_XTS_AES_128:
            pCipher = EVP_aes_128_xts();
            break;
        case RTCRCIPHERTYPE_XTS_AES_256:
            pCipher = EVP_aes_256_xts();
            break;

        /* no default! */
        case RTCRCIPHERTYPE_INVALID:
        case RTCRCIPHERTYPE_END:
        case RTCRCIPHERTYPE_32BIT_HACK:
            AssertFailedReturn(VERR_INVALID_PARAMETER);
    }
    AssertReturn(pCipher, VERR_CR_CIPHER_NOT_SUPPORTED);

    /*
     * Create the instance.
     */
    RTCRCIPHERINT *pThis = (RTCRCIPHERINT *)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTCRCIPHERINT_MAGIC;
        pThis->cRefs    = 1;
        pThis->pCipher  = pCipher;
        pThis->enmType  = enmType;
        *phCipher = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(uint32_t) RTCrCipherRetain(RTCRCIPHER hCipher)
{
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs > 1 && cRefs < 1024);
    return cRefs;
}


/**
 * Destroys the cipher instance.
 */
static uint32_t rtCrCipherDestroy(RTCRCIPHER pThis)
{
    pThis->u32Magic= ~RTCRCIPHERINT_MAGIC;
    pThis->pCipher = NULL;
    RTMemFree(pThis);
    return 0;
}


RTDECL(uint32_t) RTCrCipherRelease(RTCRCIPHER hCipher)
{
    RTCRCIPHERINT *pThis = hCipher;
    if (pThis == NIL_RTCRCIPHER)
        return 0;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pThis->cRefs);
    Assert(cRefs < 1024);
    if (cRefs == 0)
        return rtCrCipherDestroy(pThis);
    return cRefs;
}


RTDECL(uint32_t) RTCrCipherGetKeyLength(RTCRCIPHER hCipher)
{
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, 0);

    return EVP_CIPHER_key_length(pThis->pCipher);
}


RTDECL(uint32_t) RTCrCipherGetInitialVectorLength(RTCRCIPHER hCipher)
{
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, 0);

    return EVP_CIPHER_iv_length(pThis->pCipher);
}


RTDECL(uint32_t) RTCrCipherGetBlockSize(RTCRCIPHER hCipher)
{
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, 0);

    return EVP_CIPHER_block_size(pThis->pCipher);
}


RTDECL(int) RTCrCipherEncrypt(RTCRCIPHER hCipher, void const *pvKey, size_t cbKey,
                              void const *pvInitVector, size_t cbInitVector,
                              void const *pvPlainText, size_t cbPlainText,
                              void *pvEncrypted, size_t cbEncrypted, size_t *pcbEncrypted)
{
    /*
     * Validate input.
     */
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, VERR_INVALID_HANDLE);
    AssertMsgReturn((ssize_t)cbKey != EVP_CIPHER_key_length(pThis->pCipher),
                    ("%zu, expected %d\n", cbKey, EVP_CIPHER_key_length(pThis->pCipher)),
                    VERR_CR_CIPHER_INVALID_KEY_LENGTH);
    AssertMsgReturn((ssize_t)cbInitVector != EVP_CIPHER_iv_length(pThis->pCipher),
                    ("%zu, expected %d\n", cbInitVector, EVP_CIPHER_iv_length(pThis->pCipher)),
                    VERR_CR_CIPHER_INVALID_INITIALIZATION_VECTOR_LENGTH);
    AssertReturn(cbPlainText > 0, VERR_NO_DATA);

    Assert(EVP_CIPHER_block_size(pThis->pCipher) <= 1); /** @todo more complicated ciphers later */
    size_t const cbNeeded = cbPlainText;
    if (pcbEncrypted)
    {
        *pcbEncrypted = cbNeeded;
        AssertReturn(cbEncrypted >= cbNeeded, VERR_BUFFER_OVERFLOW);
    }
    else
        AssertReturn(cbEncrypted == cbNeeded, VERR_INVALID_PARAMETER);
    AssertReturn((size_t)(int)cbPlainText == cbPlainText && (int)cbPlainText > 0, VERR_OUT_OF_RANGE);

    /*
     * Allocate and initialize the cipher context.
     */
    int rc = VERR_NO_MEMORY;
# if OPENSSL_VERSION_NUMBER >= 0x10100000 && !defined(LIBRESSL_VERSION_NUMBER)
    EVP_CIPHER_CTX *pCipherCtx = EVP_CIPHER_CTX_new();
    if (pCipherCtx)
# else
    EVP_CIPHER_CTX  CipherCtx;
    EVP_CIPHER_CTX *pCipherCtx = &CipherCtx;
    RT_ZERO(CipherCtx);
# endif
    {
        int rcOssl = EVP_EncryptInit(pCipherCtx, pThis->pCipher, (unsigned char const *)pvKey,
                                     (unsigned char const *)pvInitVector);
        if (rcOssl > 0)
        {
            /*
             * Do the encryption.
             */
            int cbEncrypted1 = 0;
            rcOssl = EVP_EncryptUpdate(pCipherCtx, (unsigned char *)pvEncrypted, &cbEncrypted1,
                                       (unsigned char const *)pvPlainText, (int)cbPlainText);
            if (rcOssl > 0)
            {
                Assert(cbEncrypted1 <= (ssize_t)cbNeeded);
                int cbEncrypted2 = 0;
                rcOssl = EVP_DecryptFinal(pCipherCtx, (unsigned char *)pvEncrypted + cbEncrypted1, &cbEncrypted1);
                if (rcOssl > 0)
                {
                    Assert(cbEncrypted1 + cbEncrypted2 == (ssize_t)cbNeeded);
                    if (pcbEncrypted)
                        *pcbEncrypted = cbEncrypted1 + cbEncrypted2;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_CR_CIPHER_OSSL_ENCRYPT_FINAL_FAILED;
            }
            else
                rc = VERR_CR_CIPHER_OSSL_ENCRYPT_UPDATE_FAILED;
        }
        else
            rc = VERR_CR_CIPHER_OSSL_ENCRYPT_INIT_FAILED;

# if OPENSSL_VERSION_NUMBER >= 0x10100000 && !defined(LIBRESSL_VERSION_NUMBER)
        EVP_CIPHER_CTX_free(pCipherCtx);
# else
        EVP_CIPHER_CTX_cleanup(&CipherCtx);
# endif
    }
    return rc;
}


RTDECL(int) RTCrCipherDecrypt(RTCRCIPHER hCipher, void const *pvKey, size_t cbKey,
                              void const *pvInitVector, size_t cbInitVector,
                              void const *pvEncrypted, size_t cbEncrypted,
                              void *pvPlainText, size_t cbPlainText, size_t *pcbPlainText)
{
    /*
     * Validate input.
     */
    RTCRCIPHERINT *pThis = hCipher;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTCRCIPHERINT_MAGIC, VERR_INVALID_HANDLE);
    AssertMsgReturn((ssize_t)cbKey != EVP_CIPHER_key_length(pThis->pCipher),
                    ("%zu, expected %d\n", cbKey, EVP_CIPHER_key_length(pThis->pCipher)),
                    VERR_CR_CIPHER_INVALID_KEY_LENGTH);
    AssertMsgReturn((ssize_t)cbInitVector != EVP_CIPHER_iv_length(pThis->pCipher),
                    ("%zu, expected %d\n", cbInitVector, EVP_CIPHER_iv_length(pThis->pCipher)),
                    VERR_CR_CIPHER_INVALID_INITIALIZATION_VECTOR_LENGTH);
    AssertReturn(cbPlainText > 0, VERR_NO_DATA);

    Assert(EVP_CIPHER_block_size(pThis->pCipher) <= 1); /** @todo more complicated ciphers later */
    size_t const cbNeeded = cbEncrypted;
    if (pcbPlainText)
    {
        *pcbPlainText = cbNeeded;
        AssertReturn(cbPlainText >= cbNeeded, VERR_BUFFER_OVERFLOW);
    }
    else
        AssertReturn(cbPlainText == cbNeeded, VERR_INVALID_PARAMETER);
    AssertReturn((size_t)(int)cbEncrypted == cbEncrypted && (int)cbEncrypted > 0, VERR_OUT_OF_RANGE);

    /*
     * Allocate and initialize the cipher context.
     */
    int rc = VERR_NO_MEMORY;
# if OPENSSL_VERSION_NUMBER >= 0x10100000 && !defined(LIBRESSL_VERSION_NUMBER)
    EVP_CIPHER_CTX *pCipherCtx = EVP_CIPHER_CTX_new();
    if (pCipherCtx)
# else
    EVP_CIPHER_CTX  CipherCtx;
    EVP_CIPHER_CTX *pCipherCtx = &CipherCtx;
    RT_ZERO(CipherCtx);
# endif
    {
        int rcOssl = EVP_DecryptInit(pCipherCtx, pThis->pCipher, (unsigned char const *)pvKey,
                                     (unsigned char const *)pvInitVector);
        if (rcOssl > 0)
        {
            /*
             * Do the decryption.
             */
            int cbDecrypted1 = 0;
            rcOssl = EVP_DecryptUpdate(pCipherCtx, (unsigned char *)pvPlainText, &cbDecrypted1,
                                       (unsigned char const *)pvEncrypted, (int)cbEncrypted);
            if (rcOssl > 0)
            {
                Assert(cbDecrypted1 <= (ssize_t)cbNeeded);
                int cbDecrypted2 = 0;
                rcOssl = EVP_DecryptFinal(pCipherCtx, (unsigned char *)pvPlainText + cbDecrypted1, &cbDecrypted2);
                if (rcOssl > 0)
                {
                    Assert(cbDecrypted1 + cbDecrypted2 == (ssize_t)cbNeeded);
                    if (pcbPlainText)
                        *pcbPlainText = cbDecrypted1 + cbDecrypted2;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_CR_CIPHER_OSSL_DECRYPT_FINAL_FAILED;
            }
            else
                rc = VERR_CR_CIPHER_OSSL_DECRYPT_UPDATE_FAILED;
        }
        else
            rc = VERR_CR_CIPHER_OSSL_DECRYPT_INIT_FAILED;

# if OPENSSL_VERSION_NUMBER >= 0x10100000 && !defined(LIBRESSL_VERSION_NUMBER)
        EVP_CIPHER_CTX_free(pCipherCtx);
# else
        EVP_CIPHER_CTX_cleanup(&CipherCtx);
# endif
    }
    return rc;
}

#endif /* IPRT_WITH_OPENSSL */

