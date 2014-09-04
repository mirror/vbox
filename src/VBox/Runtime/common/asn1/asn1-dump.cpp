/* $Id$ */
/** @file
 * IPRT - ASN.1, Structure Dumper.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
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
#include <iprt/asn1.h>

#include <iprt/err.h>
#include <iprt/log.h>
#ifdef IN_RING3
# include <iprt/stream.h>
#endif
#include <iprt/string.h>

#include <iprt/formats/asn1.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Dump data structure.
 */
typedef struct RTASN1DUMPDATA
{
    /** RTASN1DUMP_F_XXX. */
    uint32_t                fFlags;
    /** The printfv like output function. */
    PFNRTDUMPPRINTFV        pfnPrintfV;
    /** PrintfV user argument. */
    void                   *pvUser;
} RTASN1DUMPDATA;
/** Pointer to a dump data structure. */
typedef RTASN1DUMPDATA *PRTASN1DUMPDATA;


/**
 * Wrapper around FNRTASN1DUMPPRINTFV.
 *
 * @param   pData               The dump data structure.
 * @param   pszFormat           Format string.
 * @param   ...                 Format arguments.
 */
static void rtAsn1DumpPrintf(PRTASN1DUMPDATA pData, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    pData->pfnPrintfV(pData->pvUser, pszFormat, va);
    va_end(va);
}


/**
 * Prints indentation.
 *
 * @param   pData               The dump data structure.
 * @param   uDepth              The indentation depth.
 */
static void rtAsn1DumpPrintIdent(PRTASN1DUMPDATA pData, uint32_t uDepth)
{
    uint32_t cchLeft = uDepth * 2;
    while (cchLeft > 0)
    {
        static char const s_szSpaces[] = "                                        ";
        uint32_t cch = RT_MIN(cchLeft, sizeof(s_szSpaces) - 1);
        rtAsn1DumpPrintf(pData, &s_szSpaces[sizeof(s_szSpaces) - 1 - cch]);
        cchLeft -= cch;
    }
}


/**
 * Dumps UTC TIME and GENERALIZED TIME
 *
 * @param   pData               The dump data structure.
 * @param   pAsn1Core           The ASN.1 core object representation.
 * @param   pszType             The time type name.
 */
static void rtAsn1DumpTime(PRTASN1DUMPDATA pData, PCRTASN1CORE pAsn1Core, const char *pszType)
{
    if ((pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT))
    {
        PCRTASN1TIME pTime = (PCRTASN1TIME)pAsn1Core;
        rtAsn1DumpPrintf(pData, "%s -- %04u-%02u-%02u %02u:%02u:%02.%09Z\n",
                         pszType,
                         pTime->Time.i32Year, pTime->Time.u8Month,  pTime->Time.u8MonthDay,
                         pTime->Time.u8Hour, pTime->Time.u8Minute, pTime->Time.u8Second,
                         pTime->Time.u32Nanosecond);
    }
    else if (pAsn1Core->cb > 0 && pAsn1Core->cb < 32 && pAsn1Core->uData.pch)
        rtAsn1DumpPrintf(pData, "%s '%.*s'\n", pszType, (size_t)pAsn1Core->cb, pAsn1Core->uData.pch);
    else
        rtAsn1DumpPrintf(pData, "%s -- cb=%u\n", pszType, pAsn1Core->cb);
}


/**
 * Dumps strings sharing the RTASN1STRING structure.
 *
 * @param   pData               The dump data structure.
 * @param   pAsn1Core           The ASN.1 core object representation.
 * @param   pszType             The string type name.
 * @param   uDepth              The current identation level.
 */
static void rtAsn1DumpString(PRTASN1DUMPDATA pData, PCRTASN1CORE pAsn1Core, const char *pszType, uint32_t uDepth)
{
    rtAsn1DumpPrintf(pData, "%s", pszType);

    const char     *pszPostfix  = "'\n";
    bool            fUtf8       = false;
    const char     *pch         = pAsn1Core->uData.pch;
    uint32_t        cch         = pAsn1Core->cb;
    PCRTASN1STRING  pString     = (PCRTASN1STRING)pAsn1Core;
    if (   (pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT)
        && pString->pszUtf8
        && pString->cchUtf8)
    {
        fUtf8 = true;
        pszPostfix = "' -- utf-8\n";
    }

    if (cch == 0 || !pch)
        rtAsn1DumpPrintf(pData, "-- cb=%u\n", pszType, pAsn1Core->cb);
    else
    {
        if (cch >= 48)
        {
            rtAsn1DumpPrintf(pData, "\n");
            rtAsn1DumpPrintIdent(pData, uDepth + 1);
        }
        rtAsn1DumpPrintf(pData, " '");

        /** @todo Handle BMP and UNIVERSIAL strings specially. */
        do
        {
            const char *pchStart = pch;
            while (   cch > 0
                   && (uint8_t)*pch >= 0x20
                   && (!fUtf8 ? (uint8_t)*pch < 0x7f : (uint8_t)*pch != 0x7f)
                   && *pch != '\'')
                cch--, pch++;
            if (pchStart != pch)
                rtAsn1DumpPrintf(pData, "%.*s", pch - pchStart, pchStart);

            while (   cch > 0
                   && (   (uint8_t)*pch < 0x20
                       || (!fUtf8 ? (uint8_t)*pch >= 0x7f : (uint8_t)*pch == 0x7f)
                       || (uint8_t)*pch == '\'') )
            {
                rtAsn1DumpPrintf(pData, "\\x%02x", *pch);
                cch--;
                pch++;
            }
        } while (cch > 0);

        rtAsn1DumpPrintf(pData, pszPostfix);
    }
}


/**
 * Returns a name for the given object ID.
 *
 * This is just to make some of the dumps a little easier to read.  It's no our
 * intention to have the whole ODI repository encoded here.
 *
 * @returns Name if available, NULL if not.
 * @param   pszObjId            The dotted object identifier string.
 */
static const char *rtAsn1DumpLookupObjIdName(const char *pszObjId)
{
#define STARTS_WITH_1(a_off, a_uValue) \
    ( pszObjId[a_off] == (a_uValue) + '0' && pszObjId[a_off + 1] == '.' )
#define STARTS_WITH_2(a_off, a_uValue) \
    ( pszObjId[a_off] == (a_uValue) / 10 + '0' && pszObjId[a_off + 1] == (a_uValue) % 10 + '0' && pszObjId[a_off + 2] == '.' )
#define STARTS_WITH_3(a_off, a_uValue) \
    (   pszObjId[a_off]     == (a_uValue) / 100 + '0' \
     && pszObjId[a_off + 1] == ((a_uValue) % 100) / 10 + '0' \
     && pszObjId[a_off + 2] == (a_uValue) % 10 + '0' \
     && pszObjId[a_off + 3] == '.' )
#define STARTS_WITH_6(a_off, a_uValue) \
    (   pszObjId[a_off]     == (a_uValue) / 100000 + '0' \
     && pszObjId[a_off + 1] == ((a_uValue) % 100000) / 10000 + '0' \
     && pszObjId[a_off + 2] == ((a_uValue) % 10000) / 1000 + '0' \
     && pszObjId[a_off + 3] == ((a_uValue) % 1000) / 100 + '0' \
     && pszObjId[a_off + 4] == ((a_uValue) % 100) / 10 + '0' \
     && pszObjId[a_off + 5] == (a_uValue) % 10 + '0' \
     && pszObjId[a_off + 6] == '.' )

#define ENDS_WITH_1(a_off, a_uValue) \
    ( pszObjId[a_off] == (a_uValue) + '0' && !pszObjId[a_off + 1] )
#define ENDS_WITH_2(a_off, a_uValue) \
    ( pszObjId[a_off] == (a_uValue) / 10 + '0' && pszObjId[a_off + 1] == (a_uValue) % 10 + '0' && !pszObjId[a_off + 2] )

    if (STARTS_WITH_1(0, 0))        /* ITU-T assigned - top level 0. */
    {

    }
    else if (STARTS_WITH_1(0, 1))   /* ISO assigned - top level 1. */
    {
        if (STARTS_WITH_1(2, 0))        /* ISO standard - 1.0. */
        {
            /* */
        }
        else if (STARTS_WITH_1(2, 2))   /* ISO member body - 1.2. */
        {
            if (STARTS_WITH_3(4, 840))      /* USA - 1.2.840. */
            {
                if (STARTS_WITH_6(8, 113549))   /* RSADSI / RSA Data Security inc - 1.2.840.113549. */
                {
                    if (STARTS_WITH_1(15, 1))       /* PKCS - 1.2.840.113549.1. */
                    {
                        if (STARTS_WITH_1(17, 1))       /* PKCS-1 - 1.2.840.113549.1.1. */
                        {
                                  if (ENDS_WITH_1(19, 1))    return "pkcs1-RsaEncryption";
                             else if (ENDS_WITH_1(19, 2))    return "pkcs1-Md2WithRsaEncryption";
                             else if (ENDS_WITH_1(19, 3))    return "pkcs1-Md4WithRsaEncryption";
                             else if (ENDS_WITH_1(19, 4))    return "pkcs1-Md5WithRsaEncryption";
                             else if (ENDS_WITH_1(19, 5))    return "pkcs1-Sha1WithRsaEncryption";
                             else if (ENDS_WITH_2(19, 10))   return "pkcs1-RsaPss";
                             else if (ENDS_WITH_2(19, 11))   return "pkcs1-Sha256WithRsaEncryption";
                             else if (ENDS_WITH_2(19, 12))   return "pkcs1-Sha384WithRsaEncryption";
                             else if (ENDS_WITH_2(19, 13))   return "pkcs1-Sha512WithRsaEncryption";
                             else if (ENDS_WITH_2(19, 14))   return "pkcs1-Sha224WithRsaEncryption";
                        }
                        else if (STARTS_WITH_1(17, 9))  /* PKCS-9 signatures - 1.2.840.113549.1.9. */
                        {
                                 if (ENDS_WITH_1(19, 1))    return "pkcs9-EMailAddress";
                            else if (ENDS_WITH_1(19, 2))    return "pkcs9-UntrustedName";
                            else if (ENDS_WITH_1(19, 3))    return "pkcs9-ContentType";
                            else if (ENDS_WITH_1(19, 4))    return "pkcs9-MessageDigest";
                            else if (ENDS_WITH_1(19, 5))    return "pkcs9-SigningTime";
                            else if (ENDS_WITH_1(19, 6))    return "pkcs9-CounterSignature";
                            else if (ENDS_WITH_1(19, 7))    return "pkcs9-challengePassword";
                            else if (ENDS_WITH_1(19, 8))    return "pkcs9-UnstructuredAddress";
                            else if (ENDS_WITH_1(19, 9))    return "pkcs9-ExtendedCertificateAttributes";
                            else if (ENDS_WITH_2(19, 13))   return "pkcs9-SigningDescription";
                            else if (ENDS_WITH_2(19, 14))   return "pkcs9-ExtensionRequest";
                            else if (ENDS_WITH_2(19, 15))   return "pkcs9-SMimeCapabilities";
                        }
                    }
                    else if (STARTS_WITH_1(15, 2))  /* PKCS #2 - 1.2.840.113549.2. */
                    {
                    }
                }
            }
        }
        else if (STARTS_WITH_1(2, 3))   /* ISO identified organiziation - 1.3. */
        {
            if (STARTS_WITH_1(4, 6))        /* DOD - 1.3.6. */
            {
                if (STARTS_WITH_1(6, 1))        /* Internet - 1.3.6.1. */
                {
                    if (STARTS_WITH_1(8, 4))        /* Private - 1.3.6.1.4. */
                    {
                        if (STARTS_WITH_1(10, 1))       /* IANA-registered Private Enterprises. */
                        {
                            if (STARTS_WITH_3(12, 311))     /* Microsoft - 1.3.6.1.4.1.311 */
                            {
                                if (STARTS_WITH_1(16, 1))       /* 1.3.6.1.4.1.311.1. */
                                {
                                }
                                else if (STARTS_WITH_1(16, 2))  /* 1.3.6.1.4.1.311.2 */
                                {
                                    if (STARTS_WITH_1(18, 1))       /* 1.3.6.1.4.1.311.2.1. */
                                    {
                                             if (ENDS_WITH_1(20, 1))     return "Ms-??-2.1";
                                        else if (ENDS_WITH_1(20, 4))     return "Ms-SpcIndirectDataContext";
                                        else if (ENDS_WITH_2(20, 10))    return "Ms-SpcAgencyInfo";
                                        else if (ENDS_WITH_2(20, 11))    return "Ms-SpcStatemntType";
                                        else if (ENDS_WITH_2(20, 12))    return "Ms-SpcOpusInfo";
                                        else if (ENDS_WITH_2(20, 14))    return "Ms-CertReqExtensions";
                                        else if (ENDS_WITH_2(20, 15))    return "Ms-SpcPeImageData";
                                        else if (ENDS_WITH_2(20, 18))    return "Ms-SpcRawFileData";
                                        else if (ENDS_WITH_2(20, 19))    return "Ms-SpcStructuredStorageData";
                                        else if (ENDS_WITH_2(20, 20))    return "Ms-SpcJavaClassDataType1";
                                        else if (ENDS_WITH_2(20, 21))    return "Ms-SpcIndividualCodeSigning";
                                        else if (ENDS_WITH_2(20, 22))    return "Ms-SpcCommericalSigning";
                                        else if (ENDS_WITH_2(20, 25))    return "Ms-SpcLinkType2-Aka-CabData";
                                        else if (ENDS_WITH_2(20, 26))    return "Ms-SpcMinimalCriterialInfo";
                                        else if (ENDS_WITH_2(20, 27))    return "Ms-SpcFinacialCriterialInfo";
                                        else if (ENDS_WITH_2(20, 28))    return "Ms-SpcLinkType3";
                                    }
                                    else if (STARTS_WITH_1(18, 3))  /* 1.3.6.1.4.1.311.2.3. */
                                    {
                                             if (ENDS_WITH_1(20, 1))     return "Ms-SpcPeImagePageHashesV1";
                                        else if (ENDS_WITH_1(20, 2))     return "Ms-SpcPeImagePageHashesV2";
                                    }
                                }
                                else if (STARTS_WITH_1(16, 3))  /* 1.3.6.1.4.1.311.3 */
                                {
                                    if (STARTS_WITH_1(18, 3))       /* 1.3.6.1.4.1.311.3.3. */
                                    {
                                             if (ENDS_WITH_1(20, 1))     return "Ms-CounterSign";
                                        else if (ENDS_WITH_1(20, 2))     return "Ms-??-3.2";
                                    }
                                }
                                else if (STARTS_WITH_2(16, 10)) /* 1.3.6.1.4.1.311.10 */
                                {
                                    if (STARTS_WITH_1(19, 3))       /* . */
                                    {
                                             if (ENDS_WITH_1(21, 1))     return "Ms-CertTrustListSigning";
                                        else if (ENDS_WITH_1(21, 2))     return "Ms-TimeStampSigning";
                                        else if (ENDS_WITH_1(21, 4))     return "Ms-EncryptedFileSystem";
                                        else if (ENDS_WITH_1(21, 5))     return "Ms-WhqlCrypto";
                                        else if (ENDS_WITH_1(21, 6))     return "Ms-Nt5Crypto";
                                        else if (ENDS_WITH_1(21, 7))     return "Ms-OemWhqlCrypto";
                                        else if (ENDS_WITH_1(21, 8))     return "Ms-EmbeddedNtCrypto";
                                        else if (ENDS_WITH_1(21, 9))     return "Ms-RootListSigner";
                                        else if (ENDS_WITH_2(21, 10))    return "Ms-QualifiedSubordination";
                                        else if (ENDS_WITH_2(21, 11))    return "Ms-KeyRecovery";
                                        else if (ENDS_WITH_2(21, 12))    return "Ms-DocumentSigning";
                                        else if (ENDS_WITH_2(21, 13))    return "Ms-LifetimeSigning";
                                    }
                                    else if (STARTS_WITH_1(19, 5))  /* . */
                                    {
                                             if (ENDS_WITH_1(21, 1))     return "Ms-Drm";
                                        else if (ENDS_WITH_1(21, 2))     return "Ms-DrmIndividualization";
                                    }
                                    else if (STARTS_WITH_1(19, 9))  /* . */
                                    {
                                        if (ENDS_WITH_1(21, 1))     return "Ms-CrossCertDistPoints";
                                    }
                                }
                                else if (STARTS_WITH_2(16, 20)) /* 1.3.6.1.4.1.311.20 */
                                {
                                         if (ENDS_WITH_1(19, 1))     return "Ms-AutoEnrollCtlUsage";
                                    else if (ENDS_WITH_1(19, 2))     return "Ms-EnrollCerttypeExtension";
                                }
                                else if (STARTS_WITH_2(16, 21)) /* CertSrv Infrastructure - 1.3.6.1.4.1.311.21 */
                                {
                                         if (ENDS_WITH_1(19, 1))     return "Ms-CaKeyCertIndexPair";
                                    else if (ENDS_WITH_1(19, 2))     return "Ms-CertSrvPreviousCertHash";
                                    else if (ENDS_WITH_1(19, 3))     return "Ms-CrlVirtualBase";
                                    else if (ENDS_WITH_1(19, 4))     return "Ms-CrlNextPublish";
                                    else if (ENDS_WITH_1(19, 6))     return "Ms-KeyRecovery";
                                    else if (ENDS_WITH_1(19, 7))     return "Ms-CertificateTemplate";
                                    else if (ENDS_WITH_1(19, 9))     return "Ms-DummySigner";
                                }
                            }
                        }
                    }
                    else if (STARTS_WITH_1(8, 5))   /* Security - 1.3.6.1.5. */
                    {
                        if (STARTS_WITH_1(10, 5))       /* Mechanisms - 1.3.6.1.5.5. */
                        {
                            if (STARTS_WITH_1(12, 7))       /* Public-Key Infrastructure (X.509) - 1.3.6.1.5.5.7. */
                            {
                                if (STARTS_WITH_1(14, 1))        /* Private Extension - 1.3.6.1.5.5.7.1. */
                                {
                                         if (ENDS_WITH_1(16, 1))    return "pkix-AuthorityInfoAccess";
                                    else if (ENDS_WITH_2(16, 12))   return "pkix-LogoType";
                                }
                                else if (STARTS_WITH_1(14, 2))   /* Private Extension - 1.3.6.1.5.5.7.2. */
                                {
                                         if (ENDS_WITH_1(16, 1))    return "id-qt-CPS";
                                    else if (ENDS_WITH_1(16, 2))    return "id-qt-UNotice";
                                    else if (ENDS_WITH_1(16, 3))    return "id-qt-TextNotice";
                                    else if (ENDS_WITH_1(16, 4))    return "id-qt-ACPS";
                                    else if (ENDS_WITH_1(16, 5))    return "id-qt-ACUNotice";
                                }
                                else if (STARTS_WITH_1(14, 3))   /* Private Extension - 1.3.6.1.5.5.7.3. */
                                {
                                         if (ENDS_WITH_1(16,  1))   return "id-kp-ServerAuth";
                                    else if (ENDS_WITH_1(16,  2))   return "id-kp-ClientAuth";
                                    else if (ENDS_WITH_1(16,  3))   return "id-kp-CodeSigning";
                                    else if (ENDS_WITH_1(16,  4))   return "id-kp-EmailProtection";
                                    else if (ENDS_WITH_1(16,  5))   return "id-kp-IPSecEndSystem";
                                    else if (ENDS_WITH_1(16,  6))   return "id-kp-IPSecTunnel";
                                    else if (ENDS_WITH_1(16,  7))   return "id-kp-IPSecUser";
                                    else if (ENDS_WITH_1(16,  8))   return "id-kp-Timestamping";
                                    else if (ENDS_WITH_1(16,  9))   return "id-kp-OCSPSigning";
                                    else if (ENDS_WITH_2(16, 10))   return "id-kp-DVCS";
                                    else if (ENDS_WITH_2(16, 11))   return "id-kp-SBGPCertAAServiceAuth";
                                    else if (ENDS_WITH_2(16, 13))   return "id-kp-EAPOverPPP";
                                    else if (ENDS_WITH_2(16, 14))   return "id-kp-EAPOverLAN";
                                }
                            }
                        }
                    }
                }
            }
            else if (STARTS_WITH_2(4, 14))  /* 1.3.14. */
            {
                if (STARTS_WITH_1(7, 3))        /* OIW Security Special Interest Group - 1.3.14.3. */
                {
                    if (STARTS_WITH_1(9, 2))        /* OIW SSIG algorithms - 1.3.14.3.2. */
                    {
                             if (ENDS_WITH_1(11, 2))     return "oiw-ssig-Md4WithRsa";
                        else if (ENDS_WITH_1(11, 3))     return "oiw-ssig-Md5WithRsa";
                        else if (ENDS_WITH_1(11, 4))     return "oiw-ssig-Md4WithRsaEncryption";
                        else if (ENDS_WITH_2(11, 15))    return "oiw-ssig-ShaWithRsaEncryption";
                        else if (ENDS_WITH_2(11, 24))    return "oiw-ssig-Md2WithRsaEncryption";
                        else if (ENDS_WITH_2(11, 25))    return "oiw-ssig-Md5WithRsaEncryption";
                        else if (ENDS_WITH_2(11, 26))    return "oiw-ssig-Sha1";
                        else if (ENDS_WITH_2(11, 29))    return "oiw-ssig-Sha1WithRsaEncryption";
                    }
                }
            }
        }
    }
    else if (STARTS_WITH_1(0, 2))  /* Joint ISO/ITU-T assigned - top level 2.*/
    {
        if (STARTS_WITH_1(2, 1))        /* ASN.1 - 2.1. */
        {
        }
        else if (STARTS_WITH_1(2, 5))   /* Directory (X.500) - 2.5. */
        {
            if (STARTS_WITH_1(4, 4))        /* X.500 Attribute types - 2.5.4. */
            {
                     if (ENDS_WITH_1(6, 3))     return "x500-CommonName";
                else if (ENDS_WITH_1(6, 6))     return "x500-CountryName";
                else if (ENDS_WITH_1(6, 7))     return "x500-LocalityName";
                else if (ENDS_WITH_1(6, 8))     return "x500-StatOrProvinceName";
                else if (ENDS_WITH_2(6, 10))    return "x500-OrganizationName";
                else if (ENDS_WITH_2(6, 11))    return "x500-OrganizationUnitName";
            }
            else if (STARTS_WITH_2(4, 29))  /* certificateExtension (id-ce) - 2.5.29. */
            {
                     if (ENDS_WITH_1(7, 1))     return "id-ce-AuthorityKeyIdentifier-Deprecated";
                else if (ENDS_WITH_1(7, 2))     return "id-ce-KeyAttributes-Deprecated";
                else if (ENDS_WITH_1(7, 3))     return "id-ce-CertificatePolicies-Deprecated";
                else if (ENDS_WITH_1(7, 4))     return "id-ce-KeyUsageRestriction-Deprecated";
                else if (ENDS_WITH_1(7, 7))     return "id-ce-SubjectAltName-Deprecated";
                else if (ENDS_WITH_1(7, 8))     return "id-ce-IssuerAltName-Deprecated";
                else if (ENDS_WITH_2(7, 14))    return "id-ce-SubjectKeyIdentifier";
                else if (ENDS_WITH_2(7, 15))    return "id-ce-KeyUsage";
                else if (ENDS_WITH_2(7, 16))    return "id-ce-PrivateKeyUsagePeriod";
                else if (ENDS_WITH_2(7, 17))    return "id-ce-SubjectAltName";
                else if (ENDS_WITH_2(7, 18))    return "id-ce-issuerAltName";
                else if (ENDS_WITH_2(7, 19))    return "id-ce-BasicConstraints";
                else if (ENDS_WITH_2(7, 25))    return "id-ce-CrlDistributionPoints";
                else if (ENDS_WITH_2(7, 29))    return "id-ce-CertificateIssuer";
                else if (ENDS_WITH_2(7, 30))    return "id-ce-NameConstraints";
                else if (ENDS_WITH_2(7, 31))    return "id-ce-CrlDistributionPoints";
                else if (ENDS_WITH_2(7, 32))    return "id-ce-CertificatePolicies";
                else if (STARTS_WITH_2(7, 32))
                {
                    if (ENDS_WITH_1(10, 0))         return "id-ce-cp-anyPolicy";
                }
                else if (ENDS_WITH_2(7, 35))    return "id-ce-AuthorityKeyIdentifier";
                else if (ENDS_WITH_2(7, 36))    return "id-ce-PolicyConstraints";
                else if (ENDS_WITH_2(7, 37))    return "id-ce-ExtKeyUsage";
            }
        }
        else if (STARTS_WITH_2(2, 16))  /* Join assignments by country - 2.16. */
        {
            if (0)
            {
            }
            else if (STARTS_WITH_3(5, 840)) /* USA - 2.16.840. */
            {
                if (STARTS_WITH_1(9, 1))        /* US company arc. */
                {
                    if (STARTS_WITH_3(11, 101))     /* US Government */
                    {
                        if (STARTS_WITH_1(15, 3))       /* Computer Security Objects Register */
                        {
                            if (STARTS_WITH_1(17, 4))       /* NIST Algorithms - 2.16.840.1.101.3.4. */
                            {
                                if (STARTS_WITH_1(19, 1))       /* AES - 2.16.840.1.101.3.4.1. */
                                {
                                }
                                else if (STARTS_WITH_1(19, 2))  /* Hash algorithms - 2.16.840.1.101.3.4.2. */
                                {
                                         if (ENDS_WITH_1(21, 1))    return "nist-Sha256";
                                    else if (ENDS_WITH_1(21, 2))    return "nist-Sha384";
                                    else if (ENDS_WITH_1(21, 3))    return "nist-Sha512";
                                    else if (ENDS_WITH_1(21, 4))    return "nist-Sha224";
                                }
                            }
                        }
                    }
                    else if (STARTS_WITH_6(11, 113730)) /* Netscape - 2.16.840.1.113730. */
                    {
                        if (STARTS_WITH_1(18, 1))           /* Netscape - 2.16.840.1.113730.1. */
                        {
                                 if (ENDS_WITH_1(20, 1))    return "netscape-cert-type";
                            else if (ENDS_WITH_1(20, 2))    return "netscape-base-url";
                            else if (ENDS_WITH_1(20, 3))    return "netscape-revocation-url";
                            else if (ENDS_WITH_1(20, 4))    return "netscape-ca-revocation-url";
                            else if (ENDS_WITH_1(20, 7))    return "netscape-cert-renewal-url";
                            else if (ENDS_WITH_1(20, 8))    return "netscape-ca-policy-url";
                            else if (ENDS_WITH_1(20, 9))    return "netscape-HomePage-url";
                            else if (ENDS_WITH_2(20, 10))   return "netscape-EntityLogo";
                            else if (ENDS_WITH_2(20, 11))   return "netscape-UserPicture";
                            else if (ENDS_WITH_2(20, 12))   return "netscape-ssl-server-name";
                            else if (ENDS_WITH_2(20, 13))   return "netscape-comment";
                        }
                        else if (STARTS_WITH_1(18, 4))      /* Netscape - 2.16.840.1.113730.4. */
                        {
                            if (ENDS_WITH_1(20, 1))    return "netscape-eku-serverGatedCrypto";
                        }
                    }
                    else if (STARTS_WITH_6(11, 113733)) /* Verisign, Inc. - 2.16.840.1.113733. */
                    {
                        if (STARTS_WITH_1(18, 1))           /* Verisign PKI Sub Tree - 2.16.840.1.113733.1. */
                        {
                                 if (ENDS_WITH_1(20, 6))    return "verisign-pki-extensions-subtree";
                            else if (STARTS_WITH_1(20, 6))      /* 2.16.840.1.113733.1.6. */
                            {
                                if (ENDS_WITH_1(22, 7))         return "verisign-pki-ext-RolloverID";
                            }
                            else if (ENDS_WITH_1(20, 7))    return "verisign-pki-policies";
                            else if (STARTS_WITH_1(20, 7))      /* 2.16.840.1.113733.1.7. */
                            {
                                     if (ENDS_WITH_1(22, 9))    return "verisign-pki-policy-9";
                                else if (ENDS_WITH_2(22, 21))   return "verisign-pki-policy-21";
                                else if (ENDS_WITH_2(22, 23))   return "verisign-pki-policy-vtn-cp";
                                else if (STARTS_WITH_2(22, 23))
                                {
                                         if (ENDS_WITH_1(25, 1))    return "verisign-pki-policy-vtn-cp-class1";
                                    else if (ENDS_WITH_1(25, 2))    return "verisign-pki-policy-vtn-cp-class2";
                                    else if (ENDS_WITH_1(25, 3))    return "verisign-pki-policy-vtn-cp-class3";
                                    else if (ENDS_WITH_1(25, 4))    return "verisign-pki-policy-vtn-cp-class4";
                                    else if (ENDS_WITH_1(25, 6))    return "verisign-pki-policy-vtn-cp-6";
                                }
                            }
                            else if (STARTS_WITH_1(20, 8))      /* 2.16.840.1.113733.1.8. */
                            {
                                if (ENDS_WITH_1(22, 1))     return "verisign-pki-eku-IssStrongCrypto";
                            }
                            else if (ENDS_WITH_2(22, 46))   return "verisign-pki-policy-cis";
                            else if (STARTS_WITH_2(22, 46))
                            {
                                     if (ENDS_WITH_1(25, 1))    return "verisign-pki-policy-cis-type1";
                                else if (ENDS_WITH_1(25, 2))    return "verisign-pki-policy-cis-type2";
                            }
                            else if (ENDS_WITH_2(22, 48))   return "verisign-pki-policy-thawte";
                            else if (STARTS_WITH_2(22, 48))
                            {
                                if (ENDS_WITH_1(25, 1))         return "verisign-pki-policy-thawte-cps-1";
                            }
                        }
                    }
                }
            }
        }
    }
    return NULL;
}


/**
 * Dumps the type and value of an universal ASN.1 type.
 *
 * @returns True if it opens a child, false if not.
 * @param   pData               The dumper data.
 * @param   pAsn1Core           The ASN.1 object to dump.
 * @param   uDepth              The current depth (for indentation).
 */
static bool rtAsn1DumpUniversalTypeAndValue(PRTASN1DUMPDATA pData, PCRTASN1CORE pAsn1Core, uint32_t uDepth)
{
    const char *pszValuePrefix = "-- value:";
    const char *pszDefault = "";
    if (pAsn1Core->fFlags & RTASN1CORE_F_DEFAULT)
    {
        pszValuePrefix = "DEFAULT";
        pszDefault = "DEFAULT ";
    }

    bool fOpen = false;
    switch (pAsn1Core->uRealTag)
    {
        case ASN1_TAG_BOOLEAN:
            if (pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT)
                rtAsn1DumpPrintf(pData, "BOOLEAN %s %RTbool\n", pszValuePrefix, ((PCRTASN1BOOLEAN)pAsn1Core)->fValue);
            else if (pAsn1Core->cb == 1 && pAsn1Core->uData.pu8)
                rtAsn1DumpPrintf(pData, "BOOLEAN %s %u\n", pszValuePrefix, *pAsn1Core->uData.pu8);
            else
                rtAsn1DumpPrintf(pData, "BOOLEAN -- cb=%u\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_INTEGER:
            if ((pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT) && pAsn1Core->cb <= 8)
                rtAsn1DumpPrintf(pData, "INTEGER %s %llu / %#llx\n", pszValuePrefix,
                                 ((PCRTASN1INTEGER)pAsn1Core)->uValue, ((PCRTASN1INTEGER)pAsn1Core)->uValue);
            else if (pAsn1Core->cb == 0 || pAsn1Core->cb >= 512 || !pAsn1Core->uData.pu8)
                rtAsn1DumpPrintf(pData, "INTEGER -- cb=%u\n", pAsn1Core->cb);
            else if (pAsn1Core->cb <= 32)
                rtAsn1DumpPrintf(pData, "INTEGER %s %.*Rhxs\n", pszValuePrefix, (size_t)pAsn1Core->cb, pAsn1Core->uData.pu8);
            else
                rtAsn1DumpPrintf(pData, "INTEGER %s\n%.*Rhxd\n", pszValuePrefix, (size_t)pAsn1Core->cb, pAsn1Core->uData.pu8);
            break;

        case ASN1_TAG_BIT_STRING:
            if ((pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT))
            {
                PCRTASN1BITSTRING pBitString = (PCRTASN1BITSTRING)pAsn1Core;
                rtAsn1DumpPrintf(pData, "BIT STRING %s-- cb=%u cBits=%#x cMaxBits=%#x",
                                 pszDefault, pBitString->Asn1Core.cb, pBitString->cBits, pBitString->cMaxBits);
                if (pBitString->cBits <= 64)
                    rtAsn1DumpPrintf(pData, " value=%#llx\n", RTAsn1BitString_GetAsUInt64(pBitString));
                else
                    rtAsn1DumpPrintf(pData, "\n");
            }
            else
                rtAsn1DumpPrintf(pData, "BIT STRING %s-- cb=%u\n", pszDefault, pAsn1Core->cb);
            fOpen = pAsn1Core->pOps != NULL;
            break;

        case ASN1_TAG_OCTET_STRING:
            rtAsn1DumpPrintf(pData, "OCTET STRING %s-- cb=%u\n", pszDefault, pAsn1Core->cb);
            fOpen = pAsn1Core->pOps != NULL;
            break;

        case ASN1_TAG_NULL:
            rtAsn1DumpPrintf(pData, "NULL\n");
            break;

        case ASN1_TAG_OID:
            if ((pAsn1Core->fFlags & RTASN1CORE_F_PRIMITE_TAG_STRUCT))
            {
                const char *pszObjIdName = rtAsn1DumpLookupObjIdName(((PCRTASN1OBJID)pAsn1Core)->szObjId);
                if (pszObjIdName)
                    rtAsn1DumpPrintf(pData, "OBJECT IDENTIFIER %s%s ('%s')\n",
                                     pszDefault, pszObjIdName, ((PCRTASN1OBJID)pAsn1Core)->szObjId);
                else
                    rtAsn1DumpPrintf(pData, "OBJECT IDENTIFIER %s'%s'\n", pszDefault, ((PCRTASN1OBJID)pAsn1Core)->szObjId);
            }
            else
                rtAsn1DumpPrintf(pData, "OBJECT IDENTIFIER %s -- cb=%u\n", pszDefault, pAsn1Core->cb);
            break;

        case ASN1_TAG_OBJECT_DESCRIPTOR:
            rtAsn1DumpPrintf(pData, "OBJECT DESCRIPTOR -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_EXTERNAL:
            rtAsn1DumpPrintf(pData, "EXTERNAL -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_REAL:
            rtAsn1DumpPrintf(pData, "REAL -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_ENUMERATED:
            rtAsn1DumpPrintf(pData, "ENUMERATED -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_EMBEDDED_PDV:
            rtAsn1DumpPrintf(pData, "EMBEDDED PDV -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_UTF8_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "UTF8 STRING", uDepth);
            break;

        case ASN1_TAG_RELATIVE_OID:
            rtAsn1DumpPrintf(pData, "RELATIVE OBJECT IDENTIFIER -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        case ASN1_TAG_SEQUENCE:
            rtAsn1DumpPrintf(pData, "SEQUENCE -- cb=%u\n", pAsn1Core->cb);
            fOpen = true;
            break;
        case ASN1_TAG_SET:
            rtAsn1DumpPrintf(pData, "SET -- cb=%u\n", pAsn1Core->cb);
            fOpen = true;
            break;

        case ASN1_TAG_NUMERIC_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "NUMERIC STRING", uDepth);
            break;

        case ASN1_TAG_PRINTABLE_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "PRINTABLE STRING", uDepth);
            break;

        case ASN1_TAG_T61_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "T61 STRING", uDepth);
            break;

        case ASN1_TAG_VIDEOTEX_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "VIDEOTEX STRING", uDepth);
            break;

        case ASN1_TAG_IA5_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "IA5 STRING", uDepth);
            break;

        case ASN1_TAG_GRAPHIC_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "GRAPHIC STRING", uDepth);
            break;

        case ASN1_TAG_VISIBLE_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "VISIBLE STRING", uDepth);
            break;

        case ASN1_TAG_GENERAL_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "GENERAL STRING", uDepth);
            break;

        case ASN1_TAG_UNIVERSAL_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "UNIVERSAL STRING", uDepth);
            break;

        case ASN1_TAG_BMP_STRING:
            rtAsn1DumpString(pData, pAsn1Core, "BMP STRING", uDepth);
            break;

        case ASN1_TAG_UTC_TIME:
            rtAsn1DumpTime(pData, pAsn1Core, "UTC TIME");
            break;

        case ASN1_TAG_GENERALIZED_TIME:
            rtAsn1DumpTime(pData, pAsn1Core, "GENERALIZED TIME");
            break;

        case ASN1_TAG_CHARACTER_STRING:
            rtAsn1DumpPrintf(pData, "CHARACTER STRING -- cb=%u TODO\n", pAsn1Core->cb);
            break;

        default:
            rtAsn1DumpPrintf(pData, "[UNIVERSAL %u]\n", pAsn1Core->uTag);
            break;
    }
    return fOpen;
}


/** @callback_method_impl{FNRTASN1ENUMCALLBACK}  */
static DECLCALLBACK(int) rtAsn1DumpEnumCallback(PRTASN1CORE pAsn1Core, const char *pszName, uint32_t uDepth, void *pvUser)
{
    PRTASN1DUMPDATA pData = (PRTASN1DUMPDATA)pvUser;
    if (!pAsn1Core->fFlags)
        return VINF_SUCCESS;

    bool fOpen = false;
    rtAsn1DumpPrintIdent(pData, uDepth);
    switch (pAsn1Core->fClass & ASN1_TAGCLASS_MASK)
    {
        case ASN1_TAGCLASS_UNIVERSAL:
            rtAsn1DumpPrintf(pData, "%-16s ", pszName);
            fOpen = rtAsn1DumpUniversalTypeAndValue(pData, pAsn1Core, uDepth);
            break;

        case ASN1_TAGCLASS_CONTEXT:
            if ((pAsn1Core->fRealClass & ASN1_TAGCLASS_MASK) == ASN1_TAGCLASS_UNIVERSAL)
            {
                rtAsn1DumpPrintf(pData, "%-16s [%u] ", pszName, pAsn1Core->uTag);
                fOpen = rtAsn1DumpUniversalTypeAndValue(pData, pAsn1Core, uDepth);
            }
            else
            {
                rtAsn1DumpPrintf(pData, "%-16s [%u]\n", pszName, pAsn1Core->uTag);
                fOpen = true;
            }
            break;

        case ASN1_TAGCLASS_APPLICATION:
            if ((pAsn1Core->fRealClass & ASN1_TAGCLASS_MASK) == ASN1_TAGCLASS_UNIVERSAL)
            {
                rtAsn1DumpPrintf(pData, "%-16s [APPLICATION %u] ", pszName, pAsn1Core->uTag);
                fOpen = rtAsn1DumpUniversalTypeAndValue(pData, pAsn1Core, uDepth);
            }
            else
            {
                rtAsn1DumpPrintf(pData, "%-16s [APPLICATION %u]\n", pszName, pAsn1Core->uTag);
                fOpen = true;
            }
            break;

        case ASN1_TAGCLASS_PRIVATE:
            if (RTASN1CORE_IS_DUMMY(pAsn1Core))
                rtAsn1DumpPrintf(pData, "%-16s DUMMY\n", pszName);
            else
            {
                rtAsn1DumpPrintf(pData, "%-16s [PRIVATE %u]\n", pszName, pAsn1Core->uTag);
                fOpen = true;
            }
            break;
    }
    /** @todo {} */

    /*
     * Recurse.
     */
    if (   pAsn1Core->pOps
        && pAsn1Core->pOps->pfnEnum)
        pAsn1Core->pOps->pfnEnum(pAsn1Core, rtAsn1DumpEnumCallback, uDepth, pData);
    return VINF_SUCCESS;
}


RTDECL(int) RTAsn1Dump(PCRTASN1CORE pAsn1Core, uint32_t fFlags, uint32_t uLevel, PFNRTDUMPPRINTFV pfnPrintfV, void *pvUser)
{
    if (   pAsn1Core->pOps
        && pAsn1Core->pOps->pfnEnum)
    {
        RTASN1DUMPDATA Data;
        Data.fFlags     = fFlags;
        Data.pfnPrintfV = pfnPrintfV;
        Data.pvUser     = pvUser;

        return pAsn1Core->pOps->pfnEnum((PRTASN1CORE)pAsn1Core, rtAsn1DumpEnumCallback, uLevel, &Data);
    }
    return VINF_SUCCESS;
}

