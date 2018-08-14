/* $Id$ */
/** @file
 * IPRT - Crypto - Cryptographic Keys, File I/O.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/key.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/memsafer.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/crypto/rsa.h>
#include <iprt/crypto/pkix.h>
#include <iprt/crypto/x509.h>

#include "internal/magics.h"
#include "key-internal.h"


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
/** RSA public key marker words. */
static RTCRPEMMARKERWORD const g_aWords_RsaPublicKey[]  =
{ { RT_STR_TUPLE("RSA") }, { RT_STR_TUPLE("PUBLIC") }, { RT_STR_TUPLE("KEY") } };
/** Generic public key marker words. */
static RTCRPEMMARKERWORD const g_aWords_PublicKey[] =
{                          { RT_STR_TUPLE("PUBLIC") }, { RT_STR_TUPLE("KEY") } };

/** Public key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyPublicMarkers[] =
{
    { g_aWords_RsaPublicKey, RT_ELEMENTS(g_aWords_RsaPublicKey) },
    { g_aWords_PublicKey,    RT_ELEMENTS(g_aWords_PublicKey) },
};
/** Number of entries in g_aRTCrKeyPublicMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyPublicMarkers = RT_ELEMENTS(g_aRTCrKeyPublicMarkers);


/** RSA private key marker words. */
static RTCRPEMMARKERWORD const g_aWords_RsaPrivateKey[] =
{ { RT_STR_TUPLE("RSA") }, { RT_STR_TUPLE("PRIVATE") }, { RT_STR_TUPLE("KEY") } };
/** Generic private key marker words. */
static RTCRPEMMARKERWORD const g_aWords_PrivateKey[] =
{                          { RT_STR_TUPLE("PRIVATE") }, { RT_STR_TUPLE("KEY") } };

/** Private key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyPrivateMarkers[] =
{
    { g_aWords_RsaPrivateKey, RT_ELEMENTS(g_aWords_RsaPrivateKey) },
    { g_aWords_PrivateKey,    RT_ELEMENTS(g_aWords_PrivateKey) },
};
/** Number of entries in g_aRTCrKeyPrivateMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyPrivateMarkers = RT_ELEMENTS(g_aRTCrKeyPrivateMarkers);


/** Private and public key markers. */
RT_DECL_DATA_CONST(RTCRPEMMARKER const) g_aRTCrKeyAllMarkers[] =
{
    { g_aWords_RsaPublicKey,  RT_ELEMENTS(g_aWords_RsaPublicKey) },
    { g_aWords_PublicKey,     RT_ELEMENTS(g_aWords_PublicKey) },
    { g_aWords_RsaPrivateKey, RT_ELEMENTS(g_aWords_RsaPrivateKey) },
    { g_aWords_PrivateKey,    RT_ELEMENTS(g_aWords_PrivateKey) },
};
/** Number of entries in g_aRTCrKeyAllMarkers. */
RT_DECL_DATA_CONST(uint32_t const) g_cRTCrKeyAllMarkers = RT_ELEMENTS(g_aRTCrKeyAllMarkers);



RTDECL(int) RTCrKeyCreateFromPemSection(PRTCRKEY phKey, PCRTCRPEMSECTION pSection, uint32_t fFlags,
                                        PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(!(fFlags & (~RTCRKEYFROM_F_VALID_MASK | RTCRKEYFROM_F_ONLY_PEM)), VERR_INVALID_FLAGS);

    AssertPtrReturn(phKey, VERR_INVALID_POINTER);
    *phKey = NIL_RTCRKEY;
    AssertPtrReturn(pSection, VERR_INVALID_POINTER);

    /*
     * If the source is PEM section, try identify the format from the markers.
     */
    enum
    {
        kKeyFormat_Unknown = 0,
        kKeyFormat_RsaPrivateKey,
        kKeyFormat_RsaPublicKey,
        kKeyFormat_PrivateKeyInfo,
        kKeyFormat_SubjectPublicKeyInfo
    }               enmFormat     = kKeyFormat_Unknown;
    PCRTCRPEMMARKER pMarker       = pSection->pMarker;
    if (pMarker)
    {
        if (   pMarker->cWords == 3
            && strcmp(pMarker->paWords[0].pszWord, "RSA") == 0
            && strcmp(pMarker->paWords[2].pszWord, "KEY") == 0)
        {
            if (strcmp(pMarker->paWords[1].pszWord, "PUBLIC") == 0)
                enmFormat  = kKeyFormat_RsaPublicKey;
            else if (strcmp(pMarker->paWords[1].pszWord, "PRIVATE") == 0)
                enmFormat  = kKeyFormat_RsaPrivateKey;
            else
                AssertFailed();
        }
        else if (   pMarker->cWords == 2
                 && strcmp(pMarker->paWords[1].pszWord, "KEY") == 0)
        {
            if (strcmp(pMarker->paWords[0].pszWord, "PUBLIC") == 0)
                enmFormat = kKeyFormat_SubjectPublicKeyInfo;
            else if (strcmp(pMarker->paWords[0].pszWord, "PRIVATE") == 0)
                enmFormat = kKeyFormat_PrivateKeyInfo;
            else
                AssertFailed();
        }
        else
            AssertFailed();
    }

    /*
     * Try guess the format from the binary data if needed.
     */
    RTASN1CURSORPRIMARY PrimaryCursor;
    if (   enmFormat == kKeyFormat_Unknown
        && pSection->cbData > 10)
    {
        RTAsn1CursorInitPrimary(&PrimaryCursor, pSection->pbData, (uint32_t)pSection->cbData,
                                pErrInfo, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, "probing/0");

        /*
         * First the must be a sequence.
         */
        RTASN1CORE Tag;
        int rc = RTAsn1CursorReadHdr(&PrimaryCursor.Cursor, &Tag, "#1");
        if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_SEQUENCE)
        {
            RTASN1CURSOR Cursor2;
            RTAsn1CursorInitSubFromCore(&PrimaryCursor.Cursor, &Tag, &Cursor2, "probing/1");
            rc = RTAsn1CursorReadHdr(&Cursor2, &Tag, "#2");

            /*
             * SEQUENCE SubjectPublicKeyInfo.Algorithm?
             */
            if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_SEQUENCE)
            {
                RTASN1CURSOR Cursor3;
                RTAsn1CursorInitSubFromCore(&Cursor2, &Tag, &Cursor3, "probing/2");
                rc = RTAsn1CursorReadHdr(&Cursor3, &Tag, "#3");

                /* SEQUENCE SubjectPublicKeyInfo.Algorithm.Algorithm? */
                if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_OID)
                    enmFormat = kKeyFormat_SubjectPublicKeyInfo;
            }
            /*
             * INTEGER PrivateKeyInfo.Version?
             * INTEGER RsaPublicKey.Modulus?
             * INTEGER RsaPrivateKey.Version?
             */
            else if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
            {
                rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#4");

                /* OBJECT PrivateKeyInfo.privateKeyAlgorithm? */
                if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_OID)
                    enmFormat = kKeyFormat_PrivateKeyInfo;
                /* INTEGER RsaPublicKey.PublicExponent?
                   INTEGER RsaPrivateKey.Modulus? */
                else if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                {
                    /* RsaPublicKey.PublicExponent is at the end. */
                    if (RTAsn1CursorIsEnd(&Cursor2))
                        enmFormat = kKeyFormat_RsaPublicKey;
                    else
                    {
                        /* Check for INTEGER RsaPrivateKey.PublicExponent nad PrivateExponent before concluding. */
                        rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#5");
                        if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                        {
                            rc = RTAsn1CursorReadHdr(RTAsn1CursorSkip(&Cursor2, Tag.cb), &Tag, "#6");
                            if (RT_SUCCESS(rc) && Tag.uTag == ASN1_TAG_INTEGER)
                                enmFormat = kKeyFormat_RsaPrivateKey;
                        }
                    }
                }
            }
        }
    }

    if (enmFormat == kKeyFormat_Unknown)
        return RTErrInfoSetF(pErrInfo, VERR_CR_KEY_UNKNOWN_TYPE,
                             "Unable to identify the key format (%.*Rhxs)", RT_MIN(16, pSection->cbData), pSection->pbData);

    /*
     * Do the reading.
     */
    int rc;
    switch (enmFormat)
    {
        case kKeyFormat_RsaPrivateKey:
            rc = rtCrKeyCreateRsaPrivate(phKey, pSection->pbData, (uint32_t)pSection->cbData, pErrInfo, pszErrorTag);
            break;

        case kKeyFormat_RsaPublicKey:
            rc = rtCrKeyCreateRsaPrivate(phKey, pSection->pbData, (uint32_t)pSection->cbData, pErrInfo, pszErrorTag);
            break;

        case kKeyFormat_SubjectPublicKeyInfo:
        {
            RTAsn1CursorInitPrimary(&PrimaryCursor, pSection->pbData, (uint32_t)pSection->cbData,
                                    pErrInfo, &g_RTAsn1DefaultAllocator, RTASN1CURSOR_FLAGS_DER, pszErrorTag);
            RTCRX509SUBJECTPUBLICKEYINFO SubjectPubKeyInfo;
            RT_ZERO(SubjectPubKeyInfo);
            rc = RTCrX509SubjectPublicKeyInfo_DecodeAsn1(&PrimaryCursor.Cursor, 0, &SubjectPubKeyInfo, "SubjectPubKeyInfo");
            if (RT_SUCCESS(rc))
            {
                rc = RTCrKeyCreateFromSubjectPublicKeyInfo(phKey, &SubjectPubKeyInfo, pErrInfo, pszErrorTag);
                RTCrX509SubjectPublicKeyInfo_Delete(&SubjectPubKeyInfo);
            }
            break;
        }

        case kKeyFormat_PrivateKeyInfo:
            rc = RTErrInfoSet(pErrInfo, VERR_CR_KEY_FORMAT_NOT_SUPPORTED,
                              "Support for PKCS#8 PrivateKeyInfo is not yet implemented");
            break;

        default:
            AssertFailedStmt(rc = VERR_INTERNAL_ERROR_4);
    }
    return rc;
}


RTDECL(int) RTCrKeyCreateFromBuffer(PRTCRKEY phKey, uint32_t fFlags, void const *pvSrc, size_t cbSrc,
                                    PRTERRINFO pErrInfo, const char *pszErrorTag)
{
    AssertReturn(!(fFlags & ~RTCRKEYFROM_F_VALID_MASK), VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemParseContent(pvSrc, cbSrc, fFlags, g_aRTCrKeyAllMarkers, g_cRTCrKeyAllMarkers, &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (pSectionHead)
        {
            rc = RTCrKeyCreateFromPemSection(phKey, pSectionHead, fFlags  & ~RTCRKEYFROM_F_ONLY_PEM, pErrInfo, pszErrorTag);
            RTCrPemFreeSections(pSectionHead);
        }
        else
            rc = rc != VINF_SUCCESS ? -rc : VERR_INTERNAL_ERROR_2;
    }
    return rc;
}


RTDECL(int) RTCrKeyCreateFromFile(PRTCRKEY phKey, uint32_t fFlags, const char *pszFilename, PRTERRINFO pErrInfo)
{
    AssertReturn(!(fFlags & ~RTCRKEYFROM_F_VALID_MASK), VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemReadFile(pszFilename, fFlags, g_aRTCrKeyAllMarkers, g_cRTCrKeyAllMarkers, &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        if (pSectionHead)
        {
            rc = RTCrKeyCreateFromPemSection(phKey, pSectionHead, fFlags & ~RTCRKEYFROM_F_ONLY_PEM,
                                             pErrInfo, RTPathFilename(pszFilename));
            RTCrPemFreeSections(pSectionHead);
        }
        else
            rc = rc != VINF_SUCCESS ? -rc : VERR_INTERNAL_ERROR_2;
    }
    return rc;
}

