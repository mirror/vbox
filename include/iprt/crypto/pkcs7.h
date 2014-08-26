/** @file
 * IPRT - PKCS \#7, Cryptographic Message Syntax Standard (aka CMS).
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

#ifndef ___iprt_crypto_pkcs7_h
#define ___iprt_crypto_pkcs7_h

#include <iprt/asn1.h>
#include <iprt/crypto/x509.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_rt_crpkcs7 RTCrPkcs7 - PKCS \#7, Cryptographic Message Syntax Standard (aka CMS).
 * @ingroup grp_rt_crypto
 * @{
 */


/**
 * PKCS \#7 IssuerAndSerialNumber (IPRT representation).
 */
typedef struct RTCRPKCS7ISSUERANDSERIALNUMBER
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The certificate name. */
    RTCRX509NAME                        Name;
    /** The certificate serial number. */
    RTASN1INTEGER                       SerialNumber;
} RTCRPKCS7ISSUERANDSERIALNUMBER;
/** Pointer to the IPRT representation of a PKCS \#7 IssuerAndSerialNumber. */
typedef RTCRPKCS7ISSUERANDSERIALNUMBER *PRTCRPKCS7ISSUERANDSERIALNUMBER;
/** Pointer to the const IPRT representation of a PKCS \#7
 * IssuerAndSerialNumber. */
typedef RTCRPKCS7ISSUERANDSERIALNUMBER const *PCRTCRPKCS7ISSUERANDSERIALNUMBER;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7ISSUERANDSERIALNUMBER, RTDECL, RTCrPkcs7IssuerAndSerialNumber, SeqCore.Asn1Core);


/** Pointer to the IPRT representation of a PKCS \#7 SignerInfo. */
typedef struct RTCRPKCS7SIGNERINFO *PRTCRPKCS7SIGNERINFO;
/** Pointer to the const IPRT representation of a PKCS \#7 SignerInfo. */
typedef struct RTCRPKCS7SIGNERINFO const *PCRTCRPKCS7SIGNERINFO;
RTASN1_IMPL_GEN_SET_OF_TYPEDEFS_AND_PROTOS(RTCRPKCS7SIGNERINFOS, RTCRPKCS7SIGNERINFO, RTDECL, RTCrPkcs7SignerInfos);


/**
 * Attribute value type (for the union).
 */
typedef enum RTCRPKCS7ATTRIBUTETYPE
{
    /** Zero is invalid. */
    RTCRPKCS7ATTRIBUTETYPE_INVALID = 0,
    /** Not present, union is NULL. */
    RTCRPKCS7ATTRIBUTETYPE_NOT_PRESENT,
    /** Unknown values, pCores. */
    RTCRPKCS7ATTRIBUTETYPE_UNKNOWN,
    /** Object IDs, use pObjIds. */
    RTCRPKCS7ATTRIBUTETYPE_OBJ_IDS,
    /** Octet strings, use pOctetStrings. */
    RTCRPKCS7ATTRIBUTETYPE_OCTET_STRINGS,
    /** Counter signatures (PKCS \#9), use pCounterSignatures. */
    RTCRPKCS7ATTRIBUTETYPE_COUNTER_SIGNATURES,
    /** Signing time (PKCS \#9), use pSigningTime. */
    RTCRPKCS7ATTRIBUTETYPE_SIGNING_TIME,
    /** Blow the type up to 32-bits. */
    RTCRPKCS7ATTRIBUTETYPE_32BIT_HACK = 0x7fffffff
} RTCRPKCS7ATTRIBUTETYPE;

/**
 * PKCS \#7 Attribute (IPRT representation).
 */
typedef struct RTCRPKCS7ATTRIBUTE
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The attribute type (object ID). */
    RTASN1OBJID                         Type;
    /** The type of data found in the values union. */
    RTCRPKCS7ATTRIBUTETYPE              enmType;
    /** Value allocation. */
    RTASN1ALLOCATION                    Allocation;
    /** Values.  */
    union
    {
        /** ASN.1 cores (RTCRPKCS7ATTRIBUTETYPE_UNKNOWN). */
        PRTASN1SETOFCORES               pCores;
        /** ASN.1 object identifiers (RTCRPKCS7ATTRIBUTETYPE_OBJ_IDS). */
        PRTASN1SETOFOBJIDS              pObjIds;
        /** ASN.1 octet strings (RTCRPKCS7ATTRIBUTETYPE_OCTET_STRINGS). */
        PRTASN1SETOFOCTETSTRINGS        pOctetStrings;
        /** Counter signatures RTCRPKCS7ATTRIBUTETYPE_COUNTER_SIGNATURES(). */
        PRTCRPKCS7SIGNERINFOS           pCounterSignatures;
        /** Signing time(s) (RTCRPKCS7ATTRIBUTETYPE_SIGNING_TIME). */
        PRTASN1SETOFTIMES               pSigningTime;
    } uValues;
} RTCRPKCS7ATTRIBUTE;
/** Pointer to the IPRT representation of a PKCS \#7 Attribute. */
typedef RTCRPKCS7ATTRIBUTE *PRTCRPKCS7ATTRIBUTE;
/** Pointer to the const IPRT representation of a PKCS \#7 Attribute. */
typedef RTCRPKCS7ATTRIBUTE const *PCRTCRPKCS7ATTRIBUTE;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7ATTRIBUTE, RTDECL, RTCrPkcs7Attribute, SeqCore.Asn1Core);

RTASN1_IMPL_GEN_SET_OF_TYPEDEFS_AND_PROTOS(RTCRPKCS7ATTRIBUTES, RTCRPKCS7ATTRIBUTE, RTDECL, RTCrPkcs7Attributes);


/**
 * One PKCS \#7 SignerInfo (IPRT representation).
 */
typedef struct RTCRPKCS7SIGNERINFO
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The structure version (RTCRPKCS7SIGNERINFO_V1). */
    RTASN1INTEGER                       Version;
    /** The issuer and serial number of the certificate used to produce the
     * encrypted digest below. */
    RTCRPKCS7ISSUERANDSERIALNUMBER      IssuerAndSerialNumber;
    /** The digest algorithm use to digest the signed content. */
    RTCRX509ALGORITHMIDENTIFIER         DigestAlgorithm;
    /** Authenticated attributes, optional [0].
     * @todo Check how other producers formats this. The microsoft one does not
     *       have explicit tags, but combines it with the SET OF. */
    RTCRPKCS7ATTRIBUTES                 AuthenticatedAttributes;
    /** The digest encryption algorithm use to encrypt the digest of the signed
     * content. */
    RTCRX509ALGORITHMIDENTIFIER         DigestEncryptionAlgorithm;
    /** The encrypted digest. */
    RTASN1OCTETSTRING                   EncryptedDigest;
    /** Unauthenticated attributes, optional [1].
     * @todo Check how other producers formats this. The microsoft one does not
     *       have explicit tags, but combines it with the SET OF. */
    RTCRPKCS7ATTRIBUTES                 UnauthenticatedAttributes;
} RTCRPKCS7SIGNERINFO;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7SIGNERINFO, RTDECL, RTCrPkcs7SignerInfo, SeqCore.Asn1Core);

/** RTCRPKCS7SIGNERINFO::Version value.  */
#define RTCRPKCS7SIGNERINFO_V1    1

/** @name PKCS \#9 Attribute IDs
 * @{ */
/** Content type (RFC-2630 11.1).
 * Value: Object Identifier */
#define RTCR_PKCS9_ID_CONTENT_TYPE_OID      "1.2.840.113549.1.9.3"
/** Message digest (RFC-2630 11.2).
 * Value: Octet string. */
#define RTCR_PKCS9_ID_MESSAGE_DIGEST_OID    "1.2.840.113549.1.9.4"
/** Signing time (RFC-2630 11.3).
 * Value: Octet string. */
#define RTCR_PKCS9_ID_SIGNING_TIME_OID      "1.2.840.113549.1.9.5"
/** Counter signature (RFC-2630 11.4).
 * Value: SignerInfo. */
#define RTCR_PKCS9_ID_COUNTER_SIGNATURE_OID "1.2.840.113549.1.9.6"
/** @} */


/**
 * PKCS \#7 ContentInfo (IPRT representation).
 */
typedef struct RTCRPKCS7CONTENTINFO
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** Object ID identifying the content below. */
    RTASN1OBJID                         ContentType;
    /** Content, optional, explicit tag 0.
     *
     * Hack alert! This should've been an explict context tag 0 structure with a
     * type selected according to ContentType.  However, it's simpler to replace the
     * explicit context with an OCTET STRING with implict tag 0.  Then we can tag
     * along on the encapsulation logic RTASN1OCTETSTRING provides for the dynamic
     * inner type.  The default decoder code will detect known structures as
     * outlined in the union below, and decode the octet string content as an
     * anonymous RTASN1CORE if not known.
     *
     * If the user want to decode the octet string content differently, it can do so
     * by destroying and freeing the current encapsulated pointer, replacing it with
     * it's own.  (Of course following the RTASN1OCTETSTRING rules.)  Just remember
     * to also update the value in the union.
     *
     * @remarks What's signed and verified is Content.pEncapsulated->uData.pv.
     */
    RTASN1OCTETSTRING                   Content;
    /** Same as Content.pEncapsulated, except a choice of known types. */
    union
    {
        /** ContentType is RTCRPKCS7SIGNEDDATA_OID. */
        struct RTCRPKCS7SIGNEDDATA         *pSignedData;
        /** ContentType is RTCRSPCINDIRECTDATACONTENT_OID. */
        struct RTCRSPCINDIRECTDATACONTENT  *pIndirectDataContent;
        /** Generic / Unknown / User. */
        PRTASN1CORE                         pCore;
    } u;
} RTCRPKCS7CONTENTINFO;
/** Pointer to the IPRT representation of a PKCS \#7 ContentInfo. */
typedef RTCRPKCS7CONTENTINFO *PRTCRPKCS7CONTENTINFO;
/** Pointer to the const IPRT representation of a PKCS \#7 ContentInfo. */
typedef RTCRPKCS7CONTENTINFO const *PCRTCRPKCS7CONTENTINFO;

RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7CONTENTINFO, RTDECL, RTCrPkcs7ContentInfo, SeqCore.Asn1Core);

RTDECL(bool) RTCrPkcs7ContentInfo_IsSignedData(PCRTCRPKCS7CONTENTINFO pThis);


/**
 * PKCS \#7 SignedData (IPRT representation).
 */
typedef struct RTCRPKCS7SIGNEDDATA
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The structure version value (1). */
    RTASN1INTEGER                       Version;
    /** The digest algorithms that are used to signed the content (ContentInfo). */
    RTCRX509ALGORITHMIDENTIFIERS        DigestAlgorithms;
    /** The content that's being signed. */
    RTCRPKCS7CONTENTINFO                ContentInfo;
    /** Certificates, optional, implicit tag 0. (Required by Authenticode.) */
    RTCRX509CERTIFICATES                Certificates;
    /** Certificate revocation lists, optional, implicit tag 1.
     * Not used by Authenticode, so currently stubbed. */
    RTASN1CORE                          Crls;
    /** Signer infos. */
    RTCRPKCS7SIGNERINFOS                SignerInfos;
} RTCRPKCS7SIGNEDDATA;
/** Pointer to the IPRT representation of a PKCS \#7 SignedData. */
typedef RTCRPKCS7SIGNEDDATA *PRTCRPKCS7SIGNEDDATA;
/** Pointer to the const IPRT representation of a PKCS \#7 SignedData. */
typedef RTCRPKCS7SIGNEDDATA const *PCRTCRPKCS7SIGNEDDATA;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7SIGNEDDATA, RTDECL, RTCrPkcs7SignedData, SeqCore.Asn1Core);

/** PKCS \#7 SignedData object ID.  */
#define RTCRPKCS7SIGNEDDATA_OID "1.2.840.113549.1.7.2"

/** PKCS \#7 SignedData version number 1.  */
#define RTCRPKCS7SIGNEDDATA_V1    1


/** @name RTCRPKCS7SIGNEDDATA_SANITY_F_XXX - Flags for RTPkcs7SignedDataCheckSantiy.
 * @{ */
/** Check for authenticode restrictions. */
#define RTCRPKCS7SIGNEDDATA_SANITY_F_AUTHENTICODE         RT_BIT_32(0)
/** Check that all the hash algorithms are known to IPRT. */
#define RTCRPKCS7SIGNEDDATA_SANITY_F_ONLY_KNOWN_HASH      RT_BIT_32(1)
/** Require signing certificate to be present. */
#define RTCRPKCS7SIGNEDDATA_SANITY_F_SIGNING_CERT_PRESENT RT_BIT_32(2)
/** @} */


/**
 * PKCS \#7 DigestInfo (IPRT representation).
 */
typedef struct RTCRPKCS7DIGESTINFO
{
    /** Sequence core. */
    RTASN1SEQUENCECORE                  SeqCore;
    /** The digest algorithm use to digest the signed content. */
    RTCRX509ALGORITHMIDENTIFIER         DigestAlgorithm;
    /** The digest. */
    RTASN1OCTETSTRING                   Digest;
} RTCRPKCS7DIGESTINFO;
/** Pointer to the IPRT representation of a PKCS \#7 DigestInfo object. */
typedef RTCRPKCS7DIGESTINFO *PRTCRPKCS7DIGESTINFO;
/** Pointer to the const IPRT representation of a PKCS \#7 DigestInfo object. */
typedef RTCRPKCS7DIGESTINFO const *PCRTCRPKCS7DIGESTINFO;
RTASN1TYPE_STANDARD_PROTOTYPES(RTCRPKCS7DIGESTINFO, RTDECL, RTCrPkcs7DigestInfo, SeqCore.Asn1Core);


/**
 * Callback function for use with RTCrPkcs7VerifySignedData.
 *
 * @returns IPRT status code.
 * @param   pCert               The certificate to verify.
 * @param   hCertPaths          Unless the certificate is trusted directly, this
 *                              is a reference to the certificate path builder
 *                              and verifier instance that we used to establish
 *                              at least valid trusted path to @a pCert.  The
 *                              callback can use this to enforce additional
 *                              certificate lineage requirements, effective
 *                              policy checks and whatnot.
 *                              This is NIL_RTCRX509CERTPATHS if the certificate
 *                              is directly trusted.
 * @param   pvUser              The user argument.
 * @param   pErrInfo            Optional error info buffer.
 */
typedef DECLCALLBACK(int) RTCRPKCS7VERIFYCERTCALLBACK(PCRTCRX509CERTIFICATE pCert, RTCRX509CERTPATHS hCertPaths,
                                                      void *pvUser, PRTERRINFO pErrInfo);
/** Pointer to a RTCRPKCS7VERIFYCERTCALLBACK callback. */
typedef RTCRPKCS7VERIFYCERTCALLBACK *PRTCRPKCS7VERIFYCERTCALLBACK;

/**
 * @callback_method_impl{RTCRPKCS7VERIFYCERTCALLBACK,
 *  Default implementation that checks for the DigitalSignature KeyUsage bit.}
 */
RTDECL(int) RTCrPkcs7VerifyCertCallbackDefault(PCRTCRX509CERTIFICATE pCert, RTCRX509CERTPATHS hCertPaths,
                                               void *pvUser, PRTERRINFO pErrInfo);

/**
 * @callback_method_impl{RTCRPKCS7VERIFYCERTCALLBACK,
 * Standard code signing.  Use this for Microsoft SPC.}
 */
RTDECL(int) RTCrPkcs7VerifyCertCallbackCodeSigning(PCRTCRX509CERTIFICATE pCert, RTCRX509CERTPATHS hCertPaths,
                                                   void *pvUser, PRTERRINFO pErrInfo);

/**
 * Verifies PKCS \#7 SignedData.
 *
 * For compatability with alternative crypto providers, the user must work on
 * the top level PKCS \#7 structure instead directly on the SignedData.
 *
 * @returns IPRT status code.
 * @param   pContentInfo        PKCS \#7 content info structure.
 * @param   fFlags              RTCRPKCS7VERIFY_SD_F_XXX.
 * @param   hAdditionalCerts    Store containing additional certificates to
 *                              supplement those mentioned in the signed data.
 * @param   hTrustedCerts       Store containing trusted certificates.
 * @param   pValidationTime     The time we're supposed to validate the
 *                              certificates chains at.
 * @param   pfnVerifyCert       Callback for checking that a certificate used
 *                              for signing the data is suitable.
 * @param   pvUser              User argument for the callback.
 * @param   pErrInfo            Optional error info buffer.
 */
RTDECL(int) RTCrPkcs7VerifySignedData(PCRTCRPKCS7CONTENTINFO pContentInfo, uint32_t fFlags,
                                      RTCRSTORE hAdditionalCerts, RTCRSTORE hTrustedCerts,
                                      PCRTTIMESPEC pValidationTime, PRTCRPKCS7VERIFYCERTCALLBACK pfnVerifyCert, void *pvUser,
                                      PRTERRINFO pErrInfo);

/** @name RTCRPKCS7VERIFY_SD_F_XXX - Flags for RTCrPkcs7VerifySignedData
 * @{ */
/** @} */

/** @} */

RT_C_DECLS_END

#endif

