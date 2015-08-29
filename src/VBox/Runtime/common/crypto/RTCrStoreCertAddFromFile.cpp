/* $Id$ */
/** @file
 * IPRT - Cryptographic (Certificate) Store, RTCrStoreCertAddFromFile.
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "internal/iprt.h"
#include <iprt/crypto/store.h>

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/crypto/pem.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** BEGIN CERTIFICATE / END CERTIFICATE. */
static RTCRPEMMARKERWORD const g_aWords_Certificate[] =
{
    { RT_STR_TUPLE("CERTIFICATE") }
};

/** BEGIN TRUSTED CERTIFICATE / END TRUSTED CERTIFICATE. */
static RTCRPEMMARKERWORD const g_aWords_TrustedCertificate[] =
{
    { RT_STR_TUPLE("TRUSTED") },
    { RT_STR_TUPLE("CERTIFICATE") }
};

/** BEGIN X509 CERTIFICATE / END X509 CERTIFICATE. (old) */
static RTCRPEMMARKERWORD const g_aWords_X509Certificate[] =
{
    { RT_STR_TUPLE("X509") },
    { RT_STR_TUPLE("CERTIFICATE") }
};

/**
 * X509 Certificate markers.
 *
 * @remark See crypto/pem/pem.h in OpenSSL for a matching list.
 */
static RTCRPEMMARKER     const g_aCertificateMarkers[] =
{
    { g_aWords_Certificate,         RT_ELEMENTS(g_aWords_Certificate) },
    { g_aWords_TrustedCertificate,  RT_ELEMENTS(g_aWords_TrustedCertificate) },
    { g_aWords_X509Certificate,     RT_ELEMENTS(g_aWords_X509Certificate) }
};


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
    AssertReturn(!(fFlags & ~(RTCRCERTCTX_F_ADD_IF_NOT_FOUND | RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR)), VERR_INVALID_FLAGS);

    size_t      cbContent;
    void        *pvContent;
    int rc = RTFileReadAllEx(pszFilename, 0, 64U*_1M, RTFILE_RDALL_O_DENY_WRITE, &pvContent, &cbContent);
    if (RT_SUCCESS(rc))
    {
        /*
         * Is it a java key store file?
         */
        if (   cbContent > 32
            && ((uint32_t const *)pvContent)[0] == RT_H2BE_U32_C(UINT32_C(0xfeedfeed)) /* magic */
            && ((uint32_t const *)pvContent)[1] == RT_H2BE_U32_C(UINT32_C(0x00000002)) /* version */ )
            rc = RTCrStoreCertAddFromJavaKeyStoreInMem(hStore, fFlags, pvContent, cbContent, pszFilename, pErrInfo);
        /*
         * No assume PEM or DER encoded binary certificate.
         */
        else
        {
            PCRTCRPEMSECTION pSectionHead;
            rc = RTCrPemParseContent(pvContent, cbContent, fFlags, g_aCertificateMarkers, RT_ELEMENTS(g_aCertificateMarkers),
                                     &pSectionHead, pErrInfo);
            if (RT_SUCCESS(rc))
            {
                PCRTCRPEMSECTION pCurSec = pSectionHead;
                while (pCurSec)
                {
                    int rc2 = RTCrStoreCertAddEncoded(hStore,
                                                      RTCRCERTCTX_F_ENC_X509_DER | (fFlags & RTCRCERTCTX_F_ADD_IF_NOT_FOUND),
                                                      pCurSec->pbData, pCurSec->cbData,
                                                      !RTErrInfoIsSet(pErrInfo) ? pErrInfo : NULL);
                    if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
                    {
                        rc = rc2;
                        if (!(fFlags & RTCRCERTCTX_F_ADD_CONTINUE_ON_ERROR))
                            break;
                    }
                    pCurSec = pCurSec->pNext;
                }

                RTCrPemFreeSections(pSectionHead);
            }
        }
        RTFileReadAllFree(pvContent, cbContent);
    }
    else
        rc = RTErrInfoSetF(pErrInfo, rc, "RTFileReadAllEx failed with %Rrc on '%s'", rc, pszFilename);
    return rc;
}

