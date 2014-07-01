/* $Id$ */
/** @file
 * IPRT - Cryptographic (Certificate) Store, RTCrStoreCertAddFromFile.
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
#include <iprt/crypto/store.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/crypto/pem.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTCRPEMMARKERWORD const g_aWords_Certificate[]  = { { RT_STR_TUPLE("CERTIFICATE") } };
/** X509 Certificate markers. */
static RTCRPEMMARKER     const g_aCertificateMarkers[] = { { g_aWords_Certificate, RT_ELEMENTS(g_aWords_Certificate) } };


#if 0
RTDECL(int) RTCrX509Certificates_ReadFromFile(const char *pszFilename, uint32_t fFlags,
                                              PRTCRX509CERTIFICATES pCertificates, PRTERRINFO pErrInfo)
{
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemReadFile(pszFilename, 0, g_aCertificateMarkers, RT_ELEMENTS(g_aCertificateMarkers), &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        pCertificates->Allocation

        PCRTCRPEMSECTION pCurSec = pSectionHead;
        while (pCurSec)
        {

            pCurSec = pCurSec->pNext;
        }

        RTCrPemFreeSections(pSectionHead);
    }
    return rc;
}
#endif


RTDECL(int) RTCrStoreCertAddFromFile(RTCRSTORE hStore, uint32_t fFlags, const char *pszFilename, PRTERRINFO pErrInfo)
{
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);
#if 0
    RTCRX509CERTIFICATES Certs;
    int rc = RTCrX509Certificates_ReadFromFile(pszFilename, 0, &Certs, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < Certs.cCerts; i++)
        {
            int rc2 = RTCrStoreCertAddEncoded(hStore, RTCRCERTCTX_F_ENC_X509_DER,
                                              RTASN1CORE_GET_RAW_ASN1_PTR(&Certs.paCerts[i].SeqCore.Asn1Core),
                                              RTASN1CORE_GET_RAW_ASN1_SIZE(&Certs.paCerts[i].SeqCore.Asn1Core),
                                              RT_SUCCESS(rc) ? pErrInfo : NULL);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
        }

        RTAsn1Destroy(&Certs.SetCore.Asn1Core);
    }
    return rc;
#else

    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemReadFile(pszFilename, 0, g_aCertificateMarkers, RT_ELEMENTS(g_aCertificateMarkers), &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        PCRTCRPEMSECTION pCurSec = pSectionHead;
        while (pCurSec)
        {
            int rc2 = RTCrStoreCertAddEncoded(hStore, RTCRCERTCTX_F_ENC_X509_DER, pCurSec->pbData, pCurSec->cbData,
                                              RT_SUCCESS(rc) ? pErrInfo : NULL);
            if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                rc = rc2;
            pCurSec = pCurSec->pNext;
        }

        RTCrPemFreeSections(pSectionHead);
    }
    return rc;
#endif
}

