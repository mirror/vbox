/** @file
 * IPRT - Crypto - Symmetric Ciphers.
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

#ifndef ___iprt_crypto_cipher_h
#define ___iprt_crypto_cipher_h

#include <iprt/asn1.h>


RT_C_DECLS_BEGIN

struct RTCRX509SUBJECTPUBLICKEYINFO;

/** @defgroup grp_rt_crcipher RTCrCipher - Symmetric Ciphers
 * @ingroup grp_rt_crypto
 * @{
 */

/**
 * A symmetric cipher handle.
 *
 * @remarks In OpenSSL terms this corresponds to a EVP_CIPHER, while in Microsoft
 *          terms it is an algorithm handle.  The latter is why a handle was
 *          choosen rather than constant descriptor structure pointer. */
typedef struct RTCRCIPHERINT   *RTCRCIPHER;
/** Pointer to a symmetric cipher handle. */
typedef RTCRCIPHER             *PRTCRCIPHER;
/** Nil symmetric cipher handle. */
#define NIL_RTCRCIPHER          ((RTCRCIPHER)0)

/**
 * Symmetric cipher types.
 *
 * @note Only add new types at the end, existing values must be stable.
 */
typedef enum RTCRCIPHERTYPE
{
    /** Invalid zero value. */
    RTCRCIPHERTYPE_INVALID = 0,
    /** XTS-AES-128 (NIST SP 800-38E). */
    RTCRCIPHERTYPE_XTS_AES_128,
    /** XTS-AES-256 (NIST SP 800-38E). */
    RTCRCIPHERTYPE_XTS_AES_256,
    /** End of valid symmetric cipher types. */
    RTCRCIPHERTYPE_END,
    /** Make sure the type is a 32-bit one. */
    RTCRCIPHERTYPE_32BIT_HACK = 0x7fffffff
} RTCRCIPHERTYPE;


RTDECL(int) RTCrCipherOpenByType(PRTCRCIPHER phCipher, RTCRCIPHERTYPE enmType, uint32_t fFlags);
RTDECL(uint32_t) RTCrCipherRetain(RTCRCIPHER hCipher);
RTDECL(uint32_t) RTCrCipherRelease(RTCRCIPHER hCipher);
RTDECL(uint32_t) RTCrCipherGetKeyLength(RTCRCIPHER hCipher);
RTDECL(uint32_t) RTCrCipherGetInitializationVectorLength(RTCRCIPHER hCipher);
RTDECL(uint32_t) RTCrCipherGetBlockSize(RTCRCIPHER hCipher);

RTDECL(int) RTCrCipherEncrypt(RTCRCIPHER hCipher, void const *pvKey, size_t cbKey,
                              void const *pvInitVector, size_t cbInitVector,
                              void const *pvPlainText, size_t cbPlainText,
                              void *pvEncrypted, size_t cbEncrypted, size_t *pcbEncrypted);
RTDECL(int) RTCrCipherDecrypt(RTCRCIPHER hCipher, void const *pvKey, size_t cbKey,
                              void const *pvInitVector, size_t cbInitVector,
                              void const *pvEncrypted, size_t cbEncrypted,
                              void *pvPlainText, size_t cbPlainText, size_t *pcbPlainText);

/** @} */

RT_C_DECLS_END

#endif


