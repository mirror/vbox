/* $Id$ */
/** @file
 * ICertificate COM class implementations.
 */

/*
 * Copyright (C) 2008-2016 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/path.h>
#include <iprt/cpp/utils.h>
#include <VBox/com/array.h>
#include <iprt/crypto/x509.h>

#include "ProgressImpl.h"
#include "ApplianceImpl.h"
#include "ApplianceImplPrivate.h"
#include "CertificateImpl.h"
#include "AutoCaller.h"
#include "Global.h"
#include "Logging.h"

using namespace std;

struct CertificateData
{
    CertificateData()
        : fTrusted(false)
        , fValidX509(false)
    {
        RT_ZERO(X509);
    }

    ~CertificateData()
    {
        if (fValidX509)
        {
            RTCrX509Certificate_Delete(&X509);
            RT_ZERO(X509);
            fValidX509 = false;
        }
    }

    /** Whether the certificate is trusted.  */
    bool fTrusted;
    /** Valid data in mX509. */
    bool fValidX509;
    /** Clone of the X.509 certificate. */
    RTCRX509CERTIFICATE X509;

private:
    CertificateData(const CertificateData &rTodo) { AssertFailed(); NOREF(rTodo); }
    CertificateData &operator=(const CertificateData &rTodo) { AssertFailed(); NOREF(rTodo); return *this; }
};

struct Certificate::Data
{
    Backupable<CertificateData> m;
};

///////////////////////////////////////////////////////////////////////////////////
//
// Certificate constructor / destructor
//
// ////////////////////////////////////////////////////////////////////////////////

DEFINE_EMPTY_CTOR_DTOR(Certificate)

HRESULT Certificate::FinalConstruct()
{
    return BaseFinalConstruct();
}

void Certificate::FinalRelease()
{
    uninit();
    BaseFinalRelease();
}

HRESULT Certificate::init(Appliance* appliance)
{
    HRESULT rc = S_OK;
    LogFlowThisFuncEnter();

    /* Enclose the state transition NotReady->InInit->Ready */
    AutoInitSpan autoInitSpan(this);
    AssertReturn(autoInitSpan.isOk(), E_FAIL);
    if(appliance!=NULL)
    {
        LogFlowThisFunc(("m_appliance: %d \n", m_appliance));
        m_appliance = appliance;
    }
    else
        rc = E_FAIL;

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

/**
 * Initializes a certificate instance.
 *
 * @returns COM status code.
 * @param   a_pCert         The certificate.
 * @param   a_fTrusted      Whether the caller trusts the certificate or not.
 */
HRESULT Certificate::initCertificate(PCRTCRX509CERTIFICATE a_pCert, bool a_fTrusted)
{
    HRESULT rc = S_OK;
    LogFlowThisFuncEnter();

    mData = new Data();
    mData->m.allocate();

    int vrc = RTCrX509Certificate_Clone(&mData->m->X509, a_pCert, &g_RTAsn1DefaultAllocator);
    if (RT_SUCCESS(vrc))
    {
        mData->m->fValidX509 = true;
        mData->m->fTrusted  = a_fTrusted;
    }
    else
        rc = Global::vboxStatusCodeToCOM(vrc);

    LogFlowThisFunc(("rc=%Rhrc\n", rc));
    LogFlowThisFuncLeave();

    return rc;
}

void Certificate::uninit()
{
    /* Enclose the state transition Ready->InUninit->NotReady */
    AutoUninitSpan autoUninitSpan(this);
    if (autoUninitSpan.uninitDone())
        return;

    mData->m.free();
    delete mData;
    mData = NULL;
}

/**
 * Private method implementation.
 * @param aVersionNumber
 * @return
 */
HRESULT Certificate::getVersionNumber(CertificateVersion_T *aVersionNumber)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    /* version 1 has value 0, so +1.*/
    *aVersionNumber = (CertificateVersion_T)(mData->m->X509.TbsCertificate.T0.Version.uValue.u + 1);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSerialNumber
 * @return
 */
HRESULT Certificate::getSerialNumber(com::Utf8Str &aSerialNumber)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);

    char szTmp[_2K];
    int vrc = RTAsn1Integer_ToString(&mData->m->X509.TbsCertificate.SerialNumber, szTmp, sizeof(szTmp), 0, NULL);
    if (RT_SUCCESS(vrc))
        aSerialNumber = szTmp;
    else
        return Global::vboxStatusCodeToCOM(vrc);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSignatureAlgorithmOID
 * @return
 */
HRESULT Certificate::getSignatureAlgorithmOID(com::Utf8Str &aSignatureAlgorithmOID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    aSignatureAlgorithmOID = mData->m->X509.TbsCertificate.Signature.Algorithm.szObjId;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSignatureAlgorithmID
 * @return
 */
HRESULT Certificate::getSignatureAlgorithmName(com::Utf8Str &aSignatureAlgorithmName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getAlgorithmName(&mData->m->X509.TbsCertificate.Signature, aSignatureAlgorithmName);
}

/**
 * Private method implementation.
 * @param aIssuerName
 * @return
 */
HRESULT Certificate::getIssuerName(std::vector<com::Utf8Str> &aIssuerName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getX509Name(&mData->m->X509.TbsCertificate.Issuer, aIssuerName);
}

/**
 * Private method implementation.
 * @param aSubjectName
 * @return
 */
HRESULT Certificate::getSubjectName(std::vector<com::Utf8Str> &aSubjectName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getX509Name(&mData->m->X509.TbsCertificate.Subject, aSubjectName);
}

/**
 * Private method implementation.
 * @param aValidityPeriodNotBefore
 * @return
 */
HRESULT Certificate::getValidityPeriodNotBefore(com::Utf8Str &aValidityPeriodNotBefore)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getTime(&mData->m->X509.TbsCertificate.Validity.NotBefore, aValidityPeriodNotBefore);
}

/**
 * Private method implementation.
 * @param aValidityPeriodNotAfter
 * @return
 */
HRESULT Certificate::getValidityPeriodNotAfter(com::Utf8Str &aValidityPeriodNotAfter)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getTime(&mData->m->X509.TbsCertificate.Validity.NotAfter, aValidityPeriodNotAfter);
}

HRESULT Certificate::getPublicKeyAlgorithmOID(com::Utf8Str &aPublicKeyAlgorithmOID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    aPublicKeyAlgorithmOID = mData->m->X509.TbsCertificate.SubjectPublicKeyInfo.Algorithm.Algorithm.szObjId;
    return S_OK;
}

/**
 * Private method implementation.
 * @param aPublicKeyAlgorithm
 * @return
 */
HRESULT Certificate::getPublicKeyAlgorithm(com::Utf8Str &aPublicKeyAlgorithm)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    return i_getAlgorithmName(&mData->m->X509.TbsCertificate.SubjectPublicKeyInfo.Algorithm, aPublicKeyAlgorithm);
}

/**
 * Private method implementation.
 * @param aSubjectPublicKey
 * @return
 */
HRESULT Certificate::getSubjectPublicKey(std::vector<BYTE> &aSubjectPublicKey)
{

    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); /* Getting encoded ASN.1 bytes may make changes to X509. */
    return i_getEncodedBytes(&mData->m->X509.TbsCertificate.SubjectPublicKeyInfo.SubjectPublicKey.Asn1Core, aSubjectPublicKey);
}

/**
 * Private method implementation.
 * @param aIssuerUniqueIdentifier
 * @return
 */
HRESULT Certificate::getIssuerUniqueIdentifier(com::Utf8Str &aIssuerUniqueIdentifier)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return i_getUniqueIdentifier(&mData->m->X509.TbsCertificate.T1.IssuerUniqueId, aIssuerUniqueIdentifier);
}

/**
 * Private method implementation.
 * @param aSubjectUniqueIdentifier
 * @return
 */
HRESULT Certificate::getSubjectUniqueIdentifier(com::Utf8Str &aSubjectUniqueIdentifier)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return i_getUniqueIdentifier(&mData->m->X509.TbsCertificate.T2.SubjectUniqueId, aSubjectUniqueIdentifier);
}

/**
 * Private method implementation.
 * @param aCertificateAuthority
 * @return
 */
HRESULT Certificate::getCertificateAuthority(BOOL *aCertificateAuthority)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCertificateAuthority = mData->m->X509.TbsCertificate.T3.pBasicConstraints
                          && mData->m->X509.TbsCertificate.T3.pBasicConstraints->CA.fValue;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aKeyUsage
 * @return
 */
HRESULT Certificate::getKeyUsage(ULONG *aKeyUsage)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aKeyUsage = mData->m->X509.TbsCertificate.T3.fKeyUsage;
    return S_OK;
}

/**
 * Private method implementation.
 * @param aExtendedKeyUsage
 * @return
 */
HRESULT Certificate::getExtendedKeyUsage(std::vector<com::Utf8Str> &aExtendedKeyUsage)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    NOREF(aExtendedKeyUsage);
    return E_NOTIMPL;
}

/**
 * Private method implementation.
 * @param aRawCertData
 * @return
 */
HRESULT Certificate::getRawCertData(std::vector<BYTE> &aRawCertData)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS); /* Getting encoded ASN.1 bytes may make changes to X509. */
    return i_getEncodedBytes(&mData->m->X509.SeqCore.Asn1Core, aRawCertData);
}

/**
 * Private method implementation.
 * @param aSelfSigned
 * @return
 */
HRESULT Certificate::getSelfSigned(BOOL *aSelfSigned)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    Assert(mData->m->fValidX509);
    *aSelfSigned = RTCrX509Certificate_IsSelfSigned(&mData->m->X509);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aTrusted
 * @return
 */
HRESULT Certificate::getTrusted(BOOL *aTrusted)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aTrusted = mData->m->fTrusted;

    return S_OK;
}

/**
 * Private method implementation. 
 * @param aWhat 
 * @param aResult
 * @return
 */
HRESULT Certificate::queryInfo(LONG aWhat, com::Utf8Str &aResult)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    /* Insurance. */
    NOREF(aResult);
    return setError(E_FAIL, "Unknown item %u", aWhat);
}

/**
 * Private method implementation.
 * @param aPresence
 * @return aPresence
 */
HRESULT Certificate::getPresence(BOOL *aPresence)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aPresence = m_appliance->m->fSignerCertLoaded;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aVerified
 * @return aVerified
 */
HRESULT Certificate::getVerified(BOOL *aVerified)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVerified = (m_appliance->m->pbSignedDigest &&
                  m_appliance->m->fCertificateValid && 
                  m_appliance->m->fCertificateValidTime) ? true:false;

    return S_OK;
}

HRESULT Certificate::i_getAlgorithmName(PCRTCRX509ALGORITHMIDENTIFIER a_pAlgId, com::Utf8Str &a_rReturn)
{
    const char *pszOid = a_pAlgId->Algorithm.szObjId;
    const char *pszName;
    if (!pszOid)    pszName = "";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_RSA))                 pszName = "rsaEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA))        pszName = "md2WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA))        pszName = "md4WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA))        pszName = "md5WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA))       pszName = "sha1WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA))     pszName = "sha224WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA))     pszName = "sha256WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA))     pszName = "sha384WithRSAEncryption";
    else if (strcmp(pszOid, RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA))     pszName = "sha512WithRSAEncryption";
    else
        pszName = pszOid;
    a_rReturn = pszName;
    return S_OK;
}

HRESULT Certificate::i_getX509Name(PCRTCRX509NAME a_pName, std::vector<com::Utf8Str> &a_rReturn)
{
    if (RTCrX509Name_IsPresent(a_pName))
    {
        for (uint32_t i = 0; i < a_pName->cItems; i++)
        {
            PCRTCRX509RELATIVEDISTINGUISHEDNAME pRdn = &a_pName->paItems[i];
            for (uint32_t j = 0; j < pRdn->cItems; j++)
            {
                PCRTCRX509ATTRIBUTETYPEANDVALUE pComponent = &pRdn->paItems[j];

                AssertReturn(pComponent->Value.enmType == RTASN1TYPE_STRING,
                             setErrorVrc(VERR_CR_X509_NAME_NOT_STRING, "VERR_CR_X509_NAME_NOT_STRING"));

                /* Get the prefix for this name component. */
                const char *pszPrefix = RTCrX509Name_GetShortRdn(&pComponent->Type);
                AssertStmt(pszPrefix, pszPrefix = pComponent->Type.szObjId);

                /* Get the string. */
                const char *pszUtf8;
                int vrc = RTAsn1String_QueryUtf8(&pComponent->Value.u.String, &pszUtf8, NULL /*pcch*/);
                AssertRCReturn(vrc, setErrorVrc(vrc, "RTAsn1String_QueryUtf8(%u/%u,,) -> %Rrc", i, j, vrc));

                a_rReturn.push_back(Utf8StrFmt("%s=%s", pszPrefix, pszUtf8));
            }
        }
    }
    return S_OK;
}

HRESULT Certificate::i_getTime(PCRTASN1TIME a_pTime, com::Utf8Str &a_rReturn)
{
    char szTmp[128];
    if (RTTimeToString(&a_pTime->Time, szTmp, sizeof(szTmp)))
    {
        a_rReturn = szTmp;
        return S_OK;
    }
    AssertFailed();
    return E_FAIL;
}

HRESULT Certificate::i_getUniqueIdentifier(PCRTCRX509UNIQUEIDENTIFIER a_pUniqueId, com::Utf8Str &a_rReturn)
{
    /* The a_pUniqueId may not be present! */
    if (RTCrX509UniqueIdentifier_IsPresent(a_pUniqueId))
    {
        void const   *pvData = RTASN1BITSTRING_GET_BIT0_PTR(a_pUniqueId);
        size_t const  cbData = RTASN1BITSTRING_GET_BYTE_SIZE(a_pUniqueId);
        size_t const  cbFormatted = cbData * 3 - 1 + 1;
        a_rReturn.reserve(cbFormatted); /* throws */
        int vrc = RTStrPrintHexBytes(a_rReturn.mutableRaw(), cbFormatted, pvData, cbData, RTSTRPRINTHEXBYTES_F_SEP_COLON);
        a_rReturn.jolt();
        AssertRCReturn(vrc, Global::vboxStatusCodeToCOM(vrc));
    }
    else
        Assert(a_rReturn.isEmpty());
    return S_OK;
}

HRESULT Certificate::i_getEncodedBytes(PRTASN1CORE a_pAsn1Obj, std::vector<BYTE> &a_rReturn)
{
    HRESULT hrc = S_OK;
    Assert(a_rReturn.size() == 0);
    if (RTAsn1Core_IsPresent(a_pAsn1Obj))
    {
        uint32_t cbEncoded;
        int vrc = RTAsn1EncodePrepare(a_pAsn1Obj, 0, &cbEncoded, NULL);
        if (RT_SUCCESS(vrc))
        {
            a_rReturn.resize(cbEncoded);
            Assert(a_rReturn.size() == cbEncoded);
            if (cbEncoded)
            {
                vrc = RTAsn1EncodeToBuffer(a_pAsn1Obj, 0, &a_rReturn.front(), a_rReturn.size(), NULL);
                if (RT_FAILURE(vrc))
                    hrc = setErrorVrc(vrc, "RTAsn1EncodeToBuffer failed with %Rrc", vrc);
            }
        }
        else
            hrc = setErrorVrc(vrc, "RTAsn1EncodePrepare failed with %Rrc", vrc);
    }
    return hrc;
}


