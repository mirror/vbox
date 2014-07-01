/* $Id$ */
/** @file
 * IPRT - Crypto - X.509, File related APIs.
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
#include <iprt/crypto/x509.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/crypto/pem.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTCRPEMMARKERWORD const g_aWords_Certificate[]  = { { RT_STR_TUPLE("CERTIFICATE") } };
/** X509 Certificate markers. */
static RTCRPEMMARKER     const g_aCertificateMarkers[] = { { g_aWords_Certificate, RT_ELEMENTS(g_aWords_Certificate) } };


RTDECL(int) RTCrX509Certificate_ReadFromFile(PRTCRX509CERTIFICATE pCertificate, const char *pszFilename, uint32_t fFlags,
                                             PCRTASN1ALLOCATORVTABLE pAllocator, PRTERRINFO pErrInfo)
{
    AssertReturn(!fFlags, VERR_INVALID_FLAGS);
    PCRTCRPEMSECTION pSectionHead;
    int rc = RTCrPemReadFile(pszFilename, 0, g_aCertificateMarkers, RT_ELEMENTS(g_aCertificateMarkers), &pSectionHead, pErrInfo);
    if (RT_SUCCESS(rc))
    {
        RTCRX509CERTIFICATE TmpCert;
        RTASN1CURSORPRIMARY PrimaryCursor;
        RTAsn1CursorInitPrimary(&PrimaryCursor, pSectionHead->pbData, (uint32_t)RT_MIN(pSectionHead->cbData, UINT32_MAX),
                                pErrInfo, pAllocator, RTASN1CURSOR_FLAGS_DER, RTPathFilename(pszFilename));
        rc = RTCrX509Certificate_DecodeAsn1(&PrimaryCursor.Cursor, 0, &TmpCert, "Cert");
        if (RT_SUCCESS(rc))
        {
            rc = RTCrX509Certificate_CheckSanity(&TmpCert, 0, pErrInfo, "Cert");
            if (RT_SUCCESS(rc))
            {
                rc = RTCrX509Certificate_Clone(pCertificate, &TmpCert, &g_RTAsn1DefaultAllocator);
                if (RT_SUCCESS(rc))
                {
                    if (pSectionHead->pNext || PrimaryCursor.Cursor.cbLeft)
                        rc = VINF_ASN1_MORE_DATA;
                }
            }
            RTCrX509Certificate_Delete(&TmpCert);
        }
        RTCrPemFreeSections(pSectionHead);
    }
    return rc;
}



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

