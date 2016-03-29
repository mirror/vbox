/** @file
 * VirtualBox COM class implementation
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_CERTIFICATEIMPL
#define ____H_CERTIFICATEIMPL

/* VBox includes */
#include <VBox/settings.h>


#include "CertificateWrap.h"

#include <vector>

using namespace std;

/* VBox forward declarations */
class Appliance;
struct RTCRX509CERTIFICATE;

class ATL_NO_VTABLE Certificate :
    public CertificateWrap
{

public:

    DECLARE_EMPTY_CTOR_DTOR(Certificate)

    HRESULT init(Appliance* appliance);
    void uninit();

    HRESULT FinalConstruct();
    void FinalRelease();

    HRESULT setData(RTCRX509CERTIFICATE *inCert);

private:

    const Appliance* m_appliance;

    HRESULT setVersionNumber(uint64_t inVersionNumber);
    HRESULT setSerialNumber(uint64_t inSerialNumber);
    HRESULT setPublicKeyAlgorithmOID(const char *aPublicKeyAlgorithmOID);
    HRESULT setPublicKeyAlgorithmName(const char *aPublicKeyAlgorithmOID);
    HRESULT setSignatureAlgorithmOID(const char *aSignatureAlgorithmOID);
    HRESULT setSignatureAlgorithmName(const char *aSignatureAlgorithmOID);
    HRESULT setIssuerName(com::Utf8Str &aIssuerName);
    HRESULT setSubjectName(com::Utf8Str &aSubjectName);
    HRESULT setValidityPeriodNotBefore(PCRTTIME aValidityPeriodNotBefore);
    HRESULT setValidityPeriodNotAfter(PCRTTIME aValidityPeriodNotAfter);
    HRESULT setCertificateAuthority(BOOL aCertificateAuthority);
    HRESULT setSelfSigned(BOOL aSelfSigned);
    HRESULT setTrusted(BOOL aTrusted);
//  HRESULT setSubjectPublicKey(std::vector<BYTE> aSubjectPublicKey);
//  HRESULT setIssuerUniqueIdentifier(std::vector<BYTE> aIssuerUniqueIdentifier);
//  HRESULT setSubjectUniqueIdentifier(std::vector<BYTE> aSubjectUniqueIdentifier);
//  HRESULT setKeyUsage(std::vector<ULONG> aKeyUsage);
//  HRESULT setExtendedKeyUsage(std::vector<com::Utf8Str> aExtendedKeyUsage);
//  HRESULT setRawCertData(std::vector<BYTE> aRawCertData);

    // wrapped ICertificate properties
    HRESULT getVersionNumber(com::Utf8Str &aVersionNumber);
    HRESULT getSerialNumber(com::Utf8Str &aSerialNumber);
    HRESULT getSignatureAlgorithmOID(com::Utf8Str &aSignatureAlgorithmOID);
    HRESULT getSignatureAlgorithmName(com::Utf8Str &aSignatureAlgorithmName);
    HRESULT getPublicKeyAlgorithm(com::Utf8Str &aPublicKeyAlgorithm);
    HRESULT getIssuerName(std::vector<com::Utf8Str> &aIssuerName);
    HRESULT getSubjectName(std::vector<com::Utf8Str> &aSubjectName);
    HRESULT getValidityPeriodNotBefore(com::Utf8Str &aValidityPeriodNotBefore);
    HRESULT getValidityPeriodNotAfter(com::Utf8Str &aValidityPeriodNotAfter);
    HRESULT getSubjectPublicKey(std::vector<BYTE> &aSubjectPublicKey);
    HRESULT getIssuerUniqueIdentifier(std::vector<BYTE> &aIssuerUniqueIdentifier);
    HRESULT getSubjectUniqueIdentifier(std::vector<BYTE> &aSubjectUniqueIdentifier);
    HRESULT getCertificateAuthority(BOOL *aCertificateAuthority);
    HRESULT getKeyUsage(std::vector<ULONG> &aKeyUsage);
    HRESULT getExtendedKeyUsage(std::vector<com::Utf8Str> &aExtendedKeyUsage);
    HRESULT getRawCertData(std::vector<BYTE> &aRawCertData);
    HRESULT getSelfSigned(BOOL *aSelfSigned);
    HRESULT getTrusted(BOOL *aTrusted);

    // wrapped ICertificate methods
    HRESULT queryInfo(LONG aWhat, com::Utf8Str &aResult);
    HRESULT checkExistence(BOOL *aPresence);
    HRESULT isVerified(BOOL *aVerified);
    //data
    struct Data;
    Data *mData;

};

#endif // !____H_CERTIFICATEIMPL

