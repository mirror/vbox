/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Keys, Internal Header.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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


#ifndef ___common_crypto_keys_internal_h
#define ___common_crypto_keys_internal_h

#include <iprt/crypto/key.h>
#include <iprt/bignum.h>


/**
 * Cryptographic key - core bits.
 */
typedef struct RTCRKEYINT
{
    /** Magic value (RTCRKEYINT_MAGIC). */
    uint32_t                    u32Magic;
    /** Reference counter. */
    uint32_t volatile           cRefs;
    /** The key type. */
    RTCRKEYTYPE                 enmType;
    /** Flags, RTCRKEYINT_F_XXX.  */
    uint32_t                    fFlags;
    /** Number of bits in the key. */
    uint32_t                    cBits;

#if defined(IPRT_WITH_OPENSSL)
    /** Size of raw key copy. */
    uint32_t                    cbEncoded;
    /** Raw copy of the key, for openssl and such.
     * If sensitive, this is a safer allocation, otherwise it follows the structure. */
    uint8_t                    *pbEncoded;
#endif

    /** Type specific data. */
    union
    {
        /** RTCRKEYTYPE_RSA_PRIVATE. */
        struct
        {
            /** The modulus.  */
            RTBIGNUM                Modulus;
            /** The private exponent.  */
            RTBIGNUM                PrivateExponent;
            /** The public exponent.  */
            RTBIGNUM                PublicExponent;
            /** @todo add more bits as needed. */
        } RsaPrivate;

        /** RTCRKEYTYPE_RSA_PUBLIC. */
        struct
        {
            /** The modulus.  */
            RTBIGNUM                Modulus;
            /** The exponent.  */
            RTBIGNUM                Exponent;
        } RsaPublic;
    } u;
} RTCRKEYINT;
/** Pointer to a crypographic key. */
typedef RTCRKEYINT *PRTCRKEYINT;
/** Pointer to a const crypographic key. */
typedef RTCRKEYINT const *PCRTCRKEYINT;



/** @name RTCRKEYINT_F_XXX.
 * @{ */
/** Key contains sensitive information, so no unnecessary copies. */
#define RTCRKEYINT_F_SENSITIVE          UINT32_C(0x00000001)
/** Set if private key bits are present. */
#define RTCRKEYINT_F_PRIVATE            UINT32_C(0x00000002)
/** Set if public key bits are present. */
#define RTCRKEYINT_F_PUBLIC             UINT32_C(0x00000004)
/** @} */

DECLHIDDEN(int) rtCrKeyCreateWorker(PRTCRKEYINT *ppThis, RTCRKEYTYPE enmType, uint32_t fFlags,
                                    void const *pvEncoded, uint32_t cbEncoded);
DECLHIDDEN(int) rtCrKeyCreateRsaPublic(PRTCRKEY phKey, const void *pvKeyBits, uint32_t cbKeyBits,
                                       PRTERRINFO pErrInfo, const char *pszErrorTag);
DECLHIDDEN(int) rtCrKeyCreateRsaPrivate(PRTCRKEY phKey, const void *pvKeyBits, uint32_t cbKeyBits,
                                        PRTERRINFO pErrInfo, const char *pszErrorTag);

#endif
