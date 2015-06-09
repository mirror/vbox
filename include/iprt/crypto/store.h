/** @file
 * IPRT - Cryptographic (Certificate) Store.
 */

/*
 * Copyright (C) 2006-2015 Oracle Corporation
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

#ifndef ___iprt_crypto_store_h
#define ___iprt_crypto_store_h

#include <iprt/crypto/x509.h>
#include <iprt/crypto/taf.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_crstore    RTCrStore - Crypotgraphic (Certificate) Store.
 * @ingroup grp_rt_crypto
 * @{
 */


/**
 * A certificate store search.
 *
 * Used by the store provider to keep track of the current location of a
 * certificate search.
 */
typedef struct RTCRSTORECERTSEARCH
{
    /** Opaque provider specific storage.
     *
     * Provider restriction: The provider is only allowed to use the two first
     * entries for the find-all searches, because the front-end API may want the
     * last two for implementing specific searches on top of it. */
    uintptr_t   auOpaque[4];
} RTCRSTORECERTSEARCH;
/** Pointer to a certificate store search. */
typedef RTCRSTORECERTSEARCH *PRTCRSTORECERTSEARCH;


RTDECL(int) RTCrStoreCreateInMem(PRTCRSTORE phStore, uint32_t cSizeHint);

RTDECL(uint32_t) RTCrStoreRetain(RTCRSTORE hStore);
RTDECL(uint32_t) RTCrStoreRelease(RTCRSTORE hStore);
RTDECL(PCRTCRCERTCTX) RTCrStoreCertByIssuerAndSerialNo(RTCRSTORE hStore, PCRTCRX509NAME pIssuer, PCRTASN1INTEGER pSerialNo);
RTDECL(int) RTCrStoreCertAddEncoded(RTCRSTORE hStore, uint32_t fFlags, void const *pvSrc, size_t cbSrc, PRTERRINFO pErrInfo);
RTDECL(int) RTCrStoreCertAddFromFile(RTCRSTORE hStore, uint32_t fFlags, const char *pszFilename, PRTERRINFO pErrInfo);

RTDECL(int) RTCrStoreCertFindAll(RTCRSTORE hStore, PRTCRSTORECERTSEARCH pSearch);
RTDECL(int) RTCrStoreCertFindBySubjectOrAltSubjectByRfc5280(RTCRSTORE hStore, PCRTCRX509NAME pSubject,
                                                            PRTCRSTORECERTSEARCH pSearch);
RTDECL(PCRTCRCERTCTX) RTCrStoreCertSearchNext(RTCRSTORE hStore, PRTCRSTORECERTSEARCH pSearch);
RTDECL(int) RTCrStoreCertSearchDestroy(RTCRSTORE hStore, PRTCRSTORECERTSEARCH pSearch);

RTDECL(int) RTCrStoreConvertToOpenSslCertStore(RTCRSTORE hStore, uint32_t fFlags, void **ppvOpenSslStore);
RTDECL(int) RTCrStoreConvertToOpenSslCertStack(RTCRSTORE hStore, uint32_t fFlags, void **ppvOpenSslStack);


/** @} */


/** @defgroup grp_rt_crcertctx  RTCrCertCtx - (Store) Certificate Context.
 * @{  */


/**
 * Certificate context.
 *
 * This is returned by the certificate store APIs and is part of a larger
 * reference counted structure.  All the data is read only.
 */
typedef struct RTCRCERTCTX
{
    /** Flags, RTCRCERTCTX_F_XXX.  */
    uint32_t                    fFlags;
    /** The size of the (DER) encoded certificate. */
    uint32_t                    cbEncoded;
    /** Pointer to the (DER) encoded certificate. */
    uint8_t const              *pabEncoded;
    /** Pointer to the decoded X.509 representation of the certificate.
     * This can be NULL when pTaInfo is present.  */
    PCRTCRX509CERTIFICATE       pCert;
    /** Pointer to the decoded TrustAnchorInfo for the certificate.  This can be
     * NULL, even for trust anchors, as long as pCert isn't. */
    PCRTCRTAFTRUSTANCHORINFO    pTaInfo;
    /** Reserved for future use. */
    void                       *paReserved[2];
} RTCRCERTCTX;

/** @name RTCRCERTCTX_F_XXX.
 * @{ */
/** Encoding mask. */
#define RTCRCERTCTX_F_ENC_MASK         UINT32_C(0x000000ff)
/** X.509 certificate, DER encoded. */
#define RTCRCERTCTX_F_ENC_X509_DER     UINT32_C(0x00000000)
/** RTF-5914 trust anchor info, DER encoded. */
#define RTCRCERTCTX_F_ENC_TAF_DER      UINT32_C(0x00000001)
#if 0
/** Extended certificate, DER encoded. */
#define RTCRCERTCTX_F_ENC_PKCS6_DER    UINT32_C(0x00000002)
#endif
/** @} */


RTDECL(uint32_t) RTCrCertCtxRetain(PCRTCRCERTCTX pCertCtx);
RTDECL(uint32_t) RTCrCertCtxRelease(PCRTCRCERTCTX pCertCtx);

/** @} */

RT_C_DECLS_END

#endif

