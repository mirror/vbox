
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
#include "Logging.h"

using namespace std;

struct CertificateData
{
    CertificateData() :
        mVersionNumber(0),
        mSerialNumber(0),
        mSelfSigned(FALSE),
        mTrusted(FALSE),
        mCertificateAuthority(FALSE)
    {}

    uint64_t mVersionNumber;
    uint64_t mSerialNumber;
    Utf8Str mSignatureAlgorithmOID;
    Utf8Str mSignatureAlgorithmName;
    Utf8Str mPublicKeyAlgorithmOID;
    Utf8Str mPublicKeyAlgorithmName;
    PCRTTIME mValidityPeriodNotBefore;
    PCRTTIME mValidityPeriodNotAfter;
    BOOL mSelfSigned;
    BOOL mTrusted;
    BOOL mCertificateAuthority;

    std::vector<Utf8Str> mIssuerName;
    std::vector<Utf8Str> mSubjectName;
    std::vector<BYTE> mSubjectPublicKey;
    std::vector<BYTE> mIssuerUniqueIdentifier;
    std::vector<BYTE> mSubjectUniqueIdentifier;
    std::vector<ULONG> mKeyUsage;
    std::vector<Utf8Str> mExtendedKeyUsage;
    std::vector<BYTE> mRawCertData;

};

struct Certificate::Data
{
    Backupable<CertificateData> m;
};

const char* const strUnknownAlgorithm = "Unknown Algorithm";
const char* const strRsaEncription = "rsaEncryption";
const char* const strMd2WithRSAEncryption = "md2WithRSAEncryption";
const char* const strMd4WithRSAEncryption = "md4WithRSAEncryption";
const char* const strMd5WithRSAEncryption = "md5WithRSAEncryption";
const char* const strSha1WithRSAEncryption = "sha1WithRSAEncryption";
const char* const strSha256WithRSAEncryption = "sha256WithRSAEncryption";
const char* const strSha384WithRSAEncryption = "sha384WithRSAEncryption";
const char* const strSha512WithRSAEncryption = "sha512WithRSAEncryption";
const char* const strSha224WithRSAEncryption = "sha224WithRSAEncryption";

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

    mData = new Data();
    mData->m.allocate();
    mData->m->mSignatureAlgorithmOID.setNull();
    mData->m->mSignatureAlgorithmName.setNull();
    mData->m->mPublicKeyAlgorithmOID.setNull();
    mData->m->mPublicKeyAlgorithmName.setNull();
    mData->m->mIssuerName.resize(0);
    mData->m->mSubjectName.resize(0);

    mData->m->mSubjectPublicKey.resize(0);
    mData->m->mIssuerUniqueIdentifier.resize(0);
    mData->m->mSubjectUniqueIdentifier.resize(0);
    mData->m->mKeyUsage.resize(0);
    mData->m->mExtendedKeyUsage.resize(0);
    mData->m->mRawCertData.resize(0);

    /* Confirm a successful initialization when it's the case */
    if (SUCCEEDED(rc))
        autoInitSpan.setSucceeded();

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
 * Public method implementation.
 * @param aSignatureAlgorithmOID
 * @return
 */
HRESULT Certificate::setData(RTCRX509CERTIFICATE *inCert)
{

    /*set mData.m->mVersionNumber */
    setVersionNumber(inCert->TbsCertificate.T0.Version.uValue.u);

    /*set mData.m->mSerialNumber */
    setSerialNumber(inCert->TbsCertificate.SerialNumber.uValue.u);

    /*set mData.m->mValidityPeriodNotBefore */
    setValidityPeriodNotBefore(&inCert->TbsCertificate.Validity.NotBefore.Time);

    /*set mData.m->mValidityPeriodNotAfter */
    setValidityPeriodNotAfter(&inCert->TbsCertificate.Validity.NotAfter.Time);

    /*set mData.m->mPublicKeyAlgorithmOID and mData.m->mPublicKeyAlgorithmName*/
    setPublicKeyAlgorithmOID(inCert->TbsCertificate.SubjectPublicKeyInfo.Algorithm.Algorithm.szObjId);

    /*set mData.m->mSignatureAlgorithmOID and mData.m->mSignatureAlgorithmName*/
    setSignatureAlgorithmOID(inCert->TbsCertificate.Signature.Algorithm.szObjId);

    if (   inCert->TbsCertificate.T3.pBasicConstraints
        && inCert->TbsCertificate.T3.pBasicConstraints->CA.fValue)
    {
        /*set mData.m->mCertificateAuthority TRUE*/
        setCertificateAuthority(true);
    }

    if (RTCrX509Certificate_IsSelfSigned(inCert))
    {
        /*set mData.m->mSelfSigned TRUE*/
        setSelfSigned(true);
    }

    char szCharArray[512];
    szCharArray[sizeof(szCharArray) - 1] = '\0';

    /* 
    * get inCert->TbsCertificate.Subject as string 
    * and set mData.m->mSignatureAlgorithmName 
    */ 
    RTCrX509Name_FormatAsString(&inCert->TbsCertificate.Subject, szCharArray, sizeof(szCharArray) - 1, NULL);
    com::Utf8Str aSubjectName = szCharArray;
    setSubjectName(aSubjectName);
    /* 
    * get inCert->TbsCertificate.Issuer as string 
    * and set mData.m->mSignatureAlgorithmName 
    */ 
    RTCrX509Name_FormatAsString(&inCert->TbsCertificate.Issuer, szCharArray, sizeof(szCharArray) - 1, NULL);
    com::Utf8Str aIssuerName = szCharArray;
    setIssuerName(aIssuerName);

    return S_OK;
}

/**
 * Public method implementation.
 * @param aSignatureAlgorithmOID
 * @return
 */
HRESULT Certificate::setSignatureAlgorithmName(const char *aSignatureAlgorithmOID)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    const char *actualName = strUnknownAlgorithm;

    if (aSignatureAlgorithmOID == NULL)
        rc = E_FAIL;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_MD2_WITH_RSA))
        actualName = strMd2WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_MD4_WITH_RSA))
        actualName = strMd4WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_MD5_WITH_RSA))
        actualName = strMd5WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_SHA1_WITH_RSA))
        actualName = strSha1WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_SHA224_WITH_RSA))
        actualName = strSha224WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_SHA256_WITH_RSA))
        actualName = strSha256WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_SHA384_WITH_RSA))
        actualName = strSha384WithRSAEncryption;
    else if(!strcmp(aSignatureAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_SHA512_WITH_RSA))
        actualName = strSha512WithRSAEncryption;
    else
    {
        rc = E_FAIL;
    }

    mData->m->mSignatureAlgorithmName = actualName;

    if(FAILED(rc))
        Log1Warning(("Can't find an appropriate signature algorithm name for given OID %s", aSignatureAlgorithmOID));

    return rc;

}

/**
 * Public method implementation.
 * @param aSignatureAlgorithmOID
 * @return
 */
HRESULT Certificate::setSignatureAlgorithmOID(const char *aSignatureAlgorithmOID)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mSignatureAlgorithmOID = aSignatureAlgorithmOID;

    setSignatureAlgorithmName(mData->m->mSignatureAlgorithmOID.c_str());

    return S_OK;
}

/**
 * Private method implementation.
 * @param aPublicKeyAlgorithm
 * @return
 */
HRESULT Certificate::setPublicKeyAlgorithmName(const char *aPublicKeyAlgorithmOID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    const char *foundName = strUnknownAlgorithm;

    if (!strcmp(aPublicKeyAlgorithmOID, RTCRX509ALGORITHMIDENTIFIERID_RSA))
        foundName = strRsaEncription;
    else
    {
        rc = E_FAIL;
    }

    mData->m->mPublicKeyAlgorithmName = foundName;

    if(FAILED(rc))
        Log1Warning(("Can't find an appropriate public key name for given OID %s", aPublicKeyAlgorithmOID));

    return rc;
}


/**
 * Private method implementation.
 * @param aPublicKeyAlgorithmOID
 * @return
 */
HRESULT Certificate::setPublicKeyAlgorithmOID(const char *aPublicKeyAlgorithmOID)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mPublicKeyAlgorithmOID = aPublicKeyAlgorithmOID;

    setPublicKeyAlgorithmName(mData->m->mPublicKeyAlgorithmOID.c_str());

    return S_OK;
}

/**
 * Public method implementation.
 * @param inVersionNumber
 * @return
 */
HRESULT Certificate::setVersionNumber(uint64_t inVersionNumber)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mVersionNumber = inVersionNumber;

    return S_OK;
}

/**
 * Public method implementation.
 * @param inVersionNumber
 * @return
 */
HRESULT Certificate::setSerialNumber(uint64_t inSerialNumber)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mSerialNumber = inSerialNumber;

    return S_OK;
}

/**
 * Public method implementation.
 * @param aCertificateAuthority
 * @return
 */
HRESULT Certificate::setCertificateAuthority(BOOL aCertificateAuthority)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mCertificateAuthority = aCertificateAuthority;

    return S_OK;
}

/**
 * Public method implementation.
 * @param aIssuerName
 * @return
 */
HRESULT Certificate::setIssuerName(com::Utf8Str &aIssuerName)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    RTCList<RTCString> l_IssueNameAsList = aIssuerName.split(",");
    uint32_t l_sz = l_IssueNameAsList.size();
    if(l_sz!=0)
        mData->m->mIssuerName.clear();
    for(uint32_t i = 0; i<l_sz; ++i)
        mData->m->mIssuerName.push_back(l_IssueNameAsList[i]);

    return S_OK;
}

/**
 * Public method implementation.
 * @param aSubjectName
 * @return
 */
HRESULT Certificate::setSubjectName(com::Utf8Str &aSubjectName)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    RTCList<RTCString> l_SubjectNameAsList = aSubjectName.split(",");
    uint32_t l_sz = l_SubjectNameAsList.size();
    if(l_sz!=0)
        mData->m->mSubjectName.clear();
    for(uint32_t i = 0; i<l_sz; ++i)
        mData->m->mSubjectName.push_back(l_SubjectNameAsList[i]);

    return S_OK;
}

/**
 * Public method implementation.
 * @param aSelfSigned
 * @return
 */
HRESULT Certificate::setSelfSigned(BOOL aSelfSigned)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mSelfSigned = aSelfSigned;

    return S_OK;
}

/**
 * Public method implementation.
 * @param aTrusted
 * @return
 */
HRESULT Certificate::setTrusted(BOOL aTrusted)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mTrusted = aTrusted;

    return S_OK;
}

/**
 * Public method implementation.
 * @param aValidityPeriodNotBefore
 * @return
 */
HRESULT Certificate::setValidityPeriodNotBefore(PCRTTIME aValidityPeriodNotBefore)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mValidityPeriodNotBefore = aValidityPeriodNotBefore;

    return S_OK;
}

/**
 * Public method implementation.
 * @param aValidityPeriodNotAfter
 * @return
 */
HRESULT Certificate::setValidityPeriodNotAfter(PCRTTIME aValidityPeriodNotAfter)
{
    AutoWriteLock alock(this COMMA_LOCKVAL_SRC_POS);

    mData->m->mValidityPeriodNotAfter = aValidityPeriodNotAfter;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aVersionNumber
 * @return
 */
HRESULT Certificate::getVersionNumber(com::Utf8Str &aVersionNumber)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);
    Utf8StrFmt strVer("%Lu", mData->m->mVersionNumber);
    aVersionNumber = strVer;
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
    Utf8StrFmt strVer("%llx", mData->m->mSerialNumber);
    aSerialNumber = strVer;
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

    aSignatureAlgorithmOID = mData->m->mSignatureAlgorithmOID;

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

    aSignatureAlgorithmName = mData->m->mSignatureAlgorithmName;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aIssuerName
 * @return
 */
HRESULT Certificate::getIssuerName(std::vector<com::Utf8Str> &aIssuerName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aIssuerName = mData->m->mIssuerName;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSubjectName
 * @return
 */
HRESULT Certificate::getSubjectName(std::vector<com::Utf8Str> &aSubjectName)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aSubjectName = mData->m->mSubjectName;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aValidityPeriodNotBefore
 * @return
 */
HRESULT Certificate::getValidityPeriodNotBefore(com::Utf8Str &aValidityPeriodNotBefore)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;

    size_t arrSize = 40;
    char* charArr = new char[arrSize];
    char* strTimeFormat = RTTimeToString(mData->m->mValidityPeriodNotBefore, charArr, arrSize);

    if (strTimeFormat == NULL)
    {
        rc=E_FAIL;
    }

    aValidityPeriodNotBefore = charArr;
    delete[] charArr;

    return rc;
}

/**
 * Private method implementation.
 * @param aValidityPeriodNotAfter
 * @return
 */
HRESULT Certificate::getValidityPeriodNotAfter(com::Utf8Str &aValidityPeriodNotAfter)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    HRESULT rc = S_OK;
    size_t arrSize = 40;
    char* charArr = new char[arrSize];
    char* strTimeFormat = RTTimeToString(mData->m->mValidityPeriodNotAfter, charArr, arrSize);

    if (strTimeFormat == NULL)
    {
        rc = E_FAIL;
    }

    aValidityPeriodNotAfter = charArr;
    delete[] charArr;
    return rc;
}

/**
 * Private method implementation.
 * @param aPublicKeyAlgorithm
 * @return
 */
HRESULT Certificate::getPublicKeyAlgorithm(com::Utf8Str &aPublicKeyAlgorithm)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    aPublicKeyAlgorithm = mData->m->mPublicKeyAlgorithmName;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSubjectPublicKey
 * @return
 */
HRESULT Certificate::getSubjectPublicKey(std::vector<BYTE> &aSubjectPublicKey)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aIssuerUniqueIdentifier
 * @return
 */
HRESULT Certificate::getIssuerUniqueIdentifier(std::vector<BYTE> &aIssuerUniqueIdentifier)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSubjectUniqueIdentifier
 * @return
 */
HRESULT Certificate::getSubjectUniqueIdentifier(std::vector<BYTE> &aSubjectUniqueIdentifier)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aCertificateAuthority
 * @return
 */
HRESULT Certificate::getCertificateAuthority(BOOL *aCertificateAuthority)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aCertificateAuthority = mData->m->mCertificateAuthority;

    return S_OK;
}

/**
 * Private method implementation.
 * @param aKeyUsage
 * @return
 */
HRESULT Certificate::getKeyUsage(std::vector<ULONG> &aKeyUsage)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

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

    return S_OK;
}

/**
 * Private method implementation.
 * @param aRawCertData
 * @return
 */
HRESULT Certificate::getRawCertData(std::vector<BYTE> &aRawCertData)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    return S_OK;
}

/**
 * Private method implementation.
 * @param aSelfSigned
 * @return
 */
HRESULT Certificate::getSelfSigned(BOOL *aSelfSigned)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aSelfSigned = mData->m->mSelfSigned;

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

    *aTrusted = mData->m->mTrusted;

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

    return S_OK;
}

/**
 * Private method implementation. 
 * @param aPresence
 * @return aPresence
 */
HRESULT Certificate::checkExistence(BOOL *aPresence)
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
HRESULT Certificate::isVerified(BOOL *aVerified)
{
    AutoReadLock alock(this COMMA_LOCKVAL_SRC_POS);

    *aVerified = (m_appliance->m->pbSignedDigest &&
                  m_appliance->m->fCertificateValid && 
                  m_appliance->m->fCertificateValidTime) ? true:false;

    return S_OK;
}

