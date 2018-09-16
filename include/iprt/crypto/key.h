/** @file
 * IPRT - Cryptographic Keys
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

#ifndef ___iprt_crypto_key_h
#define ___iprt_crypto_key_h

#include <iprt/crypto/x509.h>
#include <iprt/crypto/taf.h>
#include <iprt/sha.h>


RT_C_DECLS_BEGIN

struct RTCRPEMSECTION;
struct RTCRX509SUBJECTPUBLICKEYINFO;

/** @defgroup grp_rt_crkey      RTCrKey - Crypotgraphic Keys.
 * @ingroup grp_rt_crypto
 * @{
 */

/**
 * Key types.
 */
typedef enum RTCRKEYTYPE
{
    /** Invalid zero value. */
    RTCRKEYTYPE_INVALID = 0,
    /** RSA private key. */
    RTCRKEYTYPE_RSA_PRIVATE,
    /** RSA public key. */
    RTCRKEYTYPE_RSA_PUBLIC,
    /** End of key types. */
    RTCRKEYTYPE_END,
    /** The usual type size hack. */
    RTCRKEYTYPE_32BIT_HACK = 0x7fffffff
} RTCRKEYTYPE;


RTDECL(int)             RTCrKeyCreateFromSubjectPublicKeyInfo(PRTCRKEY phKey, struct RTCRX509SUBJECTPUBLICKEYINFO const *pSrc,
                                                              PRTERRINFO pErrInfo, const char *pszErrorTag);
RTDECL(int)             RTCrKeyCreateFromPublicAlgorithmAndBits(PRTCRKEY phKey,  PCRTASN1OBJID pAlgorithm,
                                                                PCRTASN1BITSTRING pPublicKey,
                                                                PRTERRINFO pErrInfo, const char *pszErrorTag);
RTDECL(int)             RTCrKeyCreateFromPemSection(PRTCRKEY phKey, uint32_t fFlags, struct RTCRPEMSECTION const *pSection,
                                                    const char *pszPassword, PRTERRINFO pErrInfo, const char *pszErrorTag);
RTDECL(int)             RTCrKeyCreateFromBuffer(PRTCRKEY phKey, uint32_t fFlags, void const *pvSrc, size_t cbSrc,
                                                const char *pszPassword, PRTERRINFO pErrInfo, const char *pszErrorTag);
RTDECL(int)             RTCrKeyCreateFromFile(PRTCRKEY phKey, uint32_t fFlags, const char *pszFilename,
                                              const char *pszPassword, PRTERRINFO pErrInfo);
/** @todo add support for decrypting private keys.  */
/** @name RTCRKEYFROM_F_XXX
 * @{ */
/** Only PEM sections, no binary fallback.
 * @sa RTCRPEMREADFILE_F_ONLY_PEM */
#define RTCRKEYFROM_F_ONLY_PEM                      RT_BIT(1)
/** Valid flags.   */
#define RTCRKEYFROM_F_VALID_MASK                    UINT32_C(0x00000002)
/** @} */

RTDECL(int)             RTCrKeyCreateNewRsa(PRTCRKEY phKey, uint32_t cBits, uint32_t uPubExp, uint32_t fFlags);


RTDECL(uint32_t)        RTCrKeyRetain(RTCRKEY hKey);
RTDECL(uint32_t)        RTCrKeyRelease(RTCRKEY hKey);
RTDECL(RTCRKEYTYPE)     RTCrKeyGetType(RTCRKEY hKey);
RTDECL(bool)            RTCrKeyHasPrivatePart(RTCRKEY hKey);
RTDECL(bool)            RTCrKeyHasPublicPart(RTCRKEY hKey);
RTDECL(uint32_t)        RTCrKeyGetBitCount(RTCRKEY hKey);
RTDECL(int)             RTCrKeyQueryRsaModulus(RTCRKEY hKey, PRTBIGNUM pModulus);
RTDECL(int)             RTCrKeyQueryRsaPrivateExponent(RTCRKEY hKey, PRTBIGNUM pPrivateExponent);

/** Public key markers. */
extern RT_DECL_DATA_CONST(RTCRPEMMARKER const)  g_aRTCrKeyPublicMarkers[];
/** Number of entries in g_aRTCrKeyPublicMarkers. */
extern RT_DECL_DATA_CONST(uint32_t const)       g_cRTCrKeyPublicMarkers;
/** Private key markers. */
extern RT_DECL_DATA_CONST(RTCRPEMMARKER const)  g_aRTCrKeyPrivateMarkers[];
/** Number of entries in g_aRTCrKeyPrivateMarkers. */
extern RT_DECL_DATA_CONST(uint32_t const)       g_cRTCrKeyPrivateMarkers;
/** Private and public key markers. */
extern RT_DECL_DATA_CONST(RTCRPEMMARKER const)  g_aRTCrKeyAllMarkers[];
/** Number of entries in g_aRTCrKeyAllMarkers. */
extern RT_DECL_DATA_CONST(uint32_t const)       g_cRTCrKeyAllMarkers;

/** @} */

RT_C_DECLS_END

#endif

