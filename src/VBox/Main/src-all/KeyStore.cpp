/* $Id$ */
/** @file
 * Main - Simple keystore handling for encrypted media.
 */

/*
 * Copyright (C) 2015 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "KeyStore.h"

#include <iprt/assert.h>
#include <iprt/base64.h>
#include <iprt/string.h>
#include <iprt/memsafer.h>

#include <openssl/evp.h>
#include <openssl/rand.h>

/*******************************************************************************
*  Structures and Typedefs                                                     *
*******************************************************************************/

/**
 * Key store structure.
 *
 * @note Everything is stored little endian.
 */
#pragma pack(1)
typedef struct VBoxKeyStore
{
    /** Magic value. */
    uint32_t u32Magic;
    /** Version of the header */
    uint16_t u16Version;
    /** Cipher string */
    char     aszCipher[32];
    /** Key derivation function used. */
    char     aszKeyDeriv[32];
    /** Key size in bytes. */
    uint32_t cbKey;
    /** The DEK digest for verification of the password. */
    uint8_t  abDekDigest[32];
    /** Size of the DEK digest. */
    uint32_t cbDekDigest;
    /** Salt for the DEK digest. */
    uint8_t  abDekDigestSalt[32];
    /** Iterations count of the DEK digest. */
    uint32_t cDekDigestIterations;
    /** Salt for the DEK. */
    uint8_t  abDekSalt[32];
    /** iterations count for the DEK. */
    uint32_t cDekIterations;
    /** Size of the encrypted key in bytes. */
    uint32_t cbDekEnc;
    /** The encrypted DEK. */
    uint8_t  abDekEnc[64];
} VBoxKeyStore;
#pragma pack()
typedef VBoxKeyStore *PVBoxKeyStore;

/** Key store magic (ENCS). */
#define VBOX_KEYSTORE_MAGIC   UINT32_C(0x454e4353)
/** Version identifier. */
#define VBOX_KEYSTORE_VERSION UINT16_C(0x0100)

/** Default iterations.
 * @todo: Implement benchmark to tune the iterations parameter based on the
 *        computing power of the machine.
 */
#define VBOX_KEYSTORE_ITERATIONS 2000

/**
 * Returns the appropriate openssl digest engine as specified in the key store.
 *
 * @return  Pointer to the digest engine or NULL if the digest specified in the key store
 *          is not supported.
 * @param   pKeyStore    The key store.
 */
static const EVP_MD *vboxKeyStoreGetDigest(PVBoxKeyStore pKeyStore)
{
    const EVP_MD *pMsgDigest = NULL;
    if (!strncmp(&pKeyStore->aszKeyDeriv[0], "PBKDF2-SHA1", sizeof(pKeyStore->aszKeyDeriv)))
        pMsgDigest = EVP_sha1();
    else if (!strncmp(&pKeyStore->aszKeyDeriv[0], "PBKDF2-SHA256", sizeof(pKeyStore->aszKeyDeriv)))
        pMsgDigest = EVP_sha256();
    else if (!strncmp(&pKeyStore->aszKeyDeriv[0], "PBKDF2-SHA512", sizeof(pKeyStore->aszKeyDeriv)))
        pMsgDigest = EVP_sha512();

    return pMsgDigest;
}

/**
 * Returns the appropriate openssl cipher engine as specified in the key store.
 *
 * @return  Pointer to the cipher engine or NULL if the cipher specified in the key store
 *          is not supported.
 * @param   pKeyStore    The key store.
 */
static const EVP_CIPHER *vboxKeyStoreGetCipher(PVBoxKeyStore pKeyStore)
{
    const EVP_CIPHER *pCipher = NULL;

    if (!strncmp(&pKeyStore->aszCipher[0], "AES-XTS128-PLAIN64", sizeof(pKeyStore->aszCipher)))
        pCipher = EVP_aes_128_xts();
    else if (!strncmp(&pKeyStore->aszCipher[0], "AES-XTS256-PLAIN64", sizeof(pKeyStore->aszCipher)))
        pCipher = EVP_aes_256_xts();

    return pCipher;
}

/**
 * Derives a key from the given password.
 *
 * @return  IPRT status code.
 * @param   pszPassword    The password to derive the key from.
 * @param   pKeyStore      The key store containing the deriviation parameters.
 * @param   ppbDerivKey    Where to store the derived key on success. Must be freed with
 *                         RTMemSaferFree().
 */
static int vboxKeyStoreDeriveKeyFromPassword(const char *pszPassword, PVBoxKeyStore pKeyStore,
                                             uint8_t **ppbDerivKey)
{
    /* Query key derivation function. */
    const EVP_MD *pMsgDigest = vboxKeyStoreGetDigest(pKeyStore);
    if (!pMsgDigest)
        return VERR_INVALID_PARAMETER;

    /* Allocate enough memory for the derived key. */
    uint8_t *pbDerivKey = NULL;
    int rc = RTMemSaferAllocZEx((void **)&pbDerivKey, pKeyStore->cbKey, 0 /* fFlags */);
    if (RT_FAILURE(rc))
        return rc;

    int rcOpenSsl = PKCS5_PBKDF2_HMAC(pszPassword, strlen(pszPassword), &pKeyStore->abDekSalt[0],
                                      sizeof(pKeyStore->abDekSalt), pKeyStore->cDekIterations,
                                      pMsgDigest, pKeyStore->cbKey, pbDerivKey);
    if (!rcOpenSsl)
    {
        RTMemSaferFree(pbDerivKey, pKeyStore->cbKey);
        rc = VERR_ACCESS_DENIED; /** @todo: Better status code. */
    }
    else
        *ppbDerivKey = pbDerivKey;

    return rc;
}

/**
 * Decrypts the DEK in the given key store with the given key.
 *
 * @return  IPRT status code.
 * @param   pKeyStore          The key store containing the encrpted DEK.
 * @param   pbKey              The key to decrypt the DEK with.
 * @param   ppbDekDecrypted    Where to store the decrypted DEK on success.
 *                             Must be freed with RTMemSaferFree().
 */
static int vboxKeyStoreDekDecryptWithKey(PVBoxKeyStore pKeyStore, const uint8_t *pbKey,
                                         uint8_t **ppbDekDecrypted)
{
    const EVP_CIPHER *pCipher = vboxKeyStoreGetCipher(pKeyStore);
    if (!pCipher)
        return VERR_INVALID_PARAMETER;

    EVP_CIPHER_CTX CipherCtx;
    int cbDecrypted = 0;
    uint8_t *pbDek = NULL;
    uint8_t abIv[16];
    int rc = RTMemSaferAllocZEx((void **)&pbDek, pKeyStore->cbKey, 0 /* fFlags */);
    if (RT_FAILURE(rc))
        return rc;

    RT_ZERO(abIv);
    if (EVP_DecryptInit(&CipherCtx, pCipher, pbKey, &abIv[0]))
    {
        if (EVP_DecryptUpdate(&CipherCtx, pbDek, &cbDecrypted, &pKeyStore->abDekEnc[0], pKeyStore->cbDekEnc))
        {
            if (!EVP_DecryptFinal(&CipherCtx, pbDek + cbDecrypted, &cbDecrypted))
                rc = VERR_INVALID_STATE;
        }
        else
            rc = VERR_INVALID_STATE;

        EVP_CIPHER_CTX_cleanup(&CipherCtx);
    }
    else
        rc = VERR_ACCESS_DENIED; /** @todo: better status code. */

    if (RT_FAILURE(rc))
        RTMemSaferFree(pbDek, pKeyStore->cbKey);
    else
        *ppbDekDecrypted = pbDek;

    return rc;
}

/**
 * Checks the given DEK against the digest stored in the key store.
 *
 * @return  IPRT status code
 * @retval  VINF_SUCCESS if the DEK matches the digeststored in the key store.
 * @retval  VERR_ACCESS_DENIED if the DEK is incorrect.
 * @param   pKeyStore    The key store containing the DEk digest.
 * @param   pbDek        The DEK to check.
 */
static int vboxKeyStoreCheckDekAgainstDigest(PVBoxKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    const EVP_MD *pMsgDigest = vboxKeyStoreGetDigest(pKeyStore);
    if (!pMsgDigest)
        return VERR_INVALID_PARAMETER;

    /* Allocate enough memory for the digest. */
    int rc = VINF_SUCCESS;
    uint8_t *pbDekDigest = (uint8_t *)RTMemAllocZ(EVP_MD_size(pMsgDigest));
    if (!pbDekDigest)
        return VERR_NO_MEMORY;

    int rcOpenSsl = PKCS5_PBKDF2_HMAC((const char *)pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                      sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                      pMsgDigest, pKeyStore->cbDekDigest, pbDekDigest);
    if (   !rcOpenSsl
        || memcmp(pbDekDigest, pKeyStore->abDekDigest, EVP_MD_size(pMsgDigest)))
        rc = VERR_ACCESS_DENIED; /** @todo: Better status code. */

    RTMemFree(pbDekDigest);
    return rc;
}

/**
 * Generate a digest of the given DEK and store in the given key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the digest in.
 * @param   pbDek        The DEK to generate the digest from.
 */
static int vboxKeyStoreDekDigestGenerate(PVBoxKeyStore pKeyStore, const uint8_t *pbDek)
{
    /* Query key derivation function. */
    const EVP_MD *pMsgDigest = vboxKeyStoreGetDigest(pKeyStore);
    if (!pMsgDigest)
        return VERR_INVALID_PARAMETER;

    /* Create salt. */
    int rcOpenSsl = RAND_bytes(&pKeyStore->abDekDigestSalt[0], sizeof(pKeyStore->abDekDigestSalt));
    if (!rcOpenSsl)
        return VERR_INVALID_STATE;

    pKeyStore->cDekDigestIterations = VBOX_KEYSTORE_ITERATIONS;

    /* Generate digest. */
    rcOpenSsl = PKCS5_PBKDF2_HMAC((const char *)pbDek, pKeyStore->cbKey, &pKeyStore->abDekDigestSalt[0],
                                  sizeof(pKeyStore->abDekDigestSalt), pKeyStore->cDekDigestIterations,
                                  pMsgDigest, EVP_MD_size(pMsgDigest), &pKeyStore->abDekDigest[0]);
    if (!rcOpenSsl)
        return VERR_ACCESS_DENIED; /** @todo: Better status code. */

    pKeyStore->cbDekDigest = EVP_MD_size(pMsgDigest);
    return VINF_SUCCESS;
}

/**
 * Encrypt the given DEK with the given key and store it into the key store.
 *
 * @return  IPRT status code.
 * @param   pKeyStore    The key store to store the encrypted DEK in.
 * @param   pbKey        The key to encrypt the DEK with.
 * @param   pbDek        The DEK to encrypt.
 */
static int vboxKeyStoreDekEncryptWithKey(PVBoxKeyStore pKeyStore, const uint8_t *pbKey,
                                         const uint8_t *pbDek)
{
    const EVP_CIPHER *pCipher = vboxKeyStoreGetCipher(pKeyStore);
    if (!pCipher)
        return VERR_INVALID_PARAMETER;

    EVP_CIPHER_CTX CipherCtx;
    int rc = VINF_SUCCESS;
    int cbEncrypted = 0;
    uint8_t abIv[16];

    RT_ZERO(abIv);
    if (EVP_EncryptInit(&CipherCtx, pCipher, pbKey, &abIv[0]))
    {
        if (EVP_EncryptUpdate(&CipherCtx, &pKeyStore->abDekEnc[0], &cbEncrypted, pbDek, pKeyStore->cbKey))
        {
            int cbEncryptedFinal = 0;

            if (!EVP_EncryptFinal(&CipherCtx, (&pKeyStore->abDekEnc[0]) + cbEncrypted, &cbEncryptedFinal))
                rc = VERR_INVALID_STATE;
            else
                pKeyStore->cbDekEnc = cbEncrypted + cbEncryptedFinal;
        }
        else
            rc = VERR_INVALID_STATE;

        EVP_CIPHER_CTX_cleanup(&CipherCtx);
    }
    else
        rc = VERR_ACCESS_DENIED; /** @todo: better status code. */

    return rc;
}

/**
 * Encodes the given key store in a base64 string.
 *
 * @return IPRT status code.
 * @param  pKeyStore    The key store to encode.
 * @param  ppszEnc      Where to store the encoded key store on success.
 *                      Must be freed with RTMemFree().
 */
static int vboxKeyStoreEncode(PVBoxKeyStore pKeyStore, char **ppszEnc)
{
    pKeyStore->u32Magic             = RT_H2LE_U32(pKeyStore->u32Magic);
    pKeyStore->u16Version           = RT_H2LE_U16(pKeyStore->u16Version);
    pKeyStore->cbKey                = RT_H2LE_U32(pKeyStore->cbKey);
    pKeyStore->cbDekDigest          = RT_H2LE_U32(pKeyStore->cbDekDigest);
    pKeyStore->cDekDigestIterations = RT_H2LE_U32(pKeyStore->cDekDigestIterations);
    pKeyStore->cDekIterations       = RT_H2LE_U32(pKeyStore->cDekIterations);
    pKeyStore->cbDekEnc             = RT_H2LE_U32(pKeyStore->cbDekEnc);

    size_t cbEncoded = RTBase64EncodedLength(sizeof(*pKeyStore)) + 1;
    char *pszEnc = (char *)RTMemAllocZ(cbEncoded);
    if (!pszEnc)
        return VERR_NO_MEMORY;

    int rc = RTBase64Encode(pKeyStore, sizeof(*pKeyStore), pszEnc, cbEncoded, NULL);
    if (RT_SUCCESS(rc))
        *ppszEnc = pszEnc;
    else
        RTMemFree(pszEnc);
    return rc;
}

DECLHIDDEN(int) VBoxKeyStoreGetDekFromEncoded(const char *pszEnc, const char *pszPassword,
                                              uint8_t **ppbDek, size_t *pcbDek, char **ppszCipher)
{
    VBoxKeyStore KeyStore;

    /* Convert to binary data and host endianess. */
    int rc = RTBase64Decode(pszEnc, &KeyStore, sizeof(VBoxKeyStore), NULL, NULL);
    if (RT_FAILURE(rc))
        return rc;

    KeyStore.u32Magic             = RT_LE2H_U32(KeyStore.u32Magic);
    KeyStore.u16Version           = RT_LE2H_U16(KeyStore.u16Version);
    KeyStore.cbKey                = RT_LE2H_U32(KeyStore.cbKey);
    KeyStore.cbDekDigest          = RT_LE2H_U32(KeyStore.cbDekDigest);
    KeyStore.cDekDigestIterations = RT_LE2H_U32(KeyStore.cDekDigestIterations);
    KeyStore.cDekIterations       = RT_LE2H_U32(KeyStore.cDekIterations);
    KeyStore.cbDekEnc             = RT_LE2H_U32(KeyStore.cbDekEnc);
    if (   KeyStore.u32Magic != VBOX_KEYSTORE_MAGIC
        || KeyStore.u16Version != VBOX_KEYSTORE_VERSION)
        return VERR_INVALID_MAGIC;

    /* A validation checks. */
    if (   KeyStore.cbKey > _1M
        || KeyStore.cbDekDigest > sizeof(KeyStore.abDekDigest)
        || KeyStore.cbDekEnc > sizeof(KeyStore.abDekEnc))
        return VERR_INVALID_STATE;

    char *pszCipher = RTStrDupN(&KeyStore.aszCipher[0], sizeof(KeyStore.aszCipher));
    if (!pszCipher)
        return VERR_NO_STR_MEMORY;

    uint8_t *pbDerivKey = NULL;
    rc = vboxKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey);
    if (RT_SUCCESS(rc))
    {
        /* Use the derived key to decrypt the DEK. */
        uint8_t *pbDekDecrypted = NULL;
        rc = vboxKeyStoreDekDecryptWithKey(&KeyStore, pbDerivKey, &pbDekDecrypted);
        if (RT_SUCCESS(rc))
        {
            /* Check the decrypted key with the digest. */
            rc = vboxKeyStoreCheckDekAgainstDigest(&KeyStore, pbDekDecrypted);
            if (RT_SUCCESS(rc))
            {
                *pcbDek = KeyStore.cbKey;
                *ppbDek = pbDekDecrypted;
                *ppszCipher = pszCipher;
            }
            else
                RTMemSaferFree(pbDekDecrypted, KeyStore.cbKey);
        }
    }

    if (pbDerivKey)
        RTMemSaferFree(pbDerivKey, KeyStore.cbKey);

    if (RT_FAILURE(rc))
        RTStrFree(pszCipher);

    return rc;
}

DECLHIDDEN(int) VBoxKeyStoreCreate(const char *pszPassword, const uint8_t *pbDek, size_t cbDek,
                                   const char *pszCipher, char **ppszEnc)
{
    VBoxKeyStore KeyStore;
    RT_ZERO(KeyStore);

    KeyStore.u32Magic       = VBOX_KEYSTORE_MAGIC;
    KeyStore.u16Version     = VBOX_KEYSTORE_VERSION;
    KeyStore.cDekIterations = VBOX_KEYSTORE_ITERATIONS;

    /* Generate the salt for the DEK encryption. */
    int rcOpenSsl = RAND_bytes(&KeyStore.abDekSalt[0], sizeof(KeyStore.abDekSalt));
    if (!rcOpenSsl)
        return VERR_INVALID_STATE;

    int rc = RTStrCopy(&KeyStore.aszCipher[0], sizeof(KeyStore.aszCipher), pszCipher);
    if (RT_SUCCESS(rc))
    {
        KeyStore.cbKey = cbDek;
        strcpy(&KeyStore.aszKeyDeriv[0], "PBKDF2-SHA256");
        rc = vboxKeyStoreDekDigestGenerate(&KeyStore, pbDek);
        if (RT_SUCCESS(rc))
        {
            uint8_t *pbDerivKey = NULL;
            rc = vboxKeyStoreDeriveKeyFromPassword(pszPassword, &KeyStore, &pbDerivKey);
            if (RT_SUCCESS(rc))
            {
                rc = vboxKeyStoreDekEncryptWithKey(&KeyStore, pbDerivKey, pbDek);
                if (RT_SUCCESS(rc))
                    rc = vboxKeyStoreEncode(&KeyStore, ppszEnc);

                RTMemSaferFree(pbDerivKey, KeyStore.cbKey);
            }
        }
    }

    return rc;
}

