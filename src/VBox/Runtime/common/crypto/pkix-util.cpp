/* $Id$ */
/** @file
 * IPRT - Crypto - Public Key Infrastructure API, Utilities.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/crypto/pkix.h>

#include <iprt/asn1.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/crypto/rsa.h>

#ifdef IPRT_WITH_OPENSSL
# include "internal/iprt-openssl.h"
# include "internal/openssl-pre.h"
# include <openssl/evp.h>
# include "internal/openssl-post.h"
#endif




RTDECL(const char *) RTCrPkixGetCiperOidFromSignatureAlgorithm(PCRTASN1OBJID pAlgorithm)
{
    /*
     * This is all hardcoded, at least for the time being.
     */
    if (RTAsn1ObjId_StartsWith(pAlgorithm, RTCR_PKCS1_OID))
    {
        if (RTAsn1ObjIdCountComponents(pAlgorithm) == 7)
            switch (RTAsn1ObjIdGetLastComponentsAsUInt32(pAlgorithm))
            {
                case 2:
                case 3:
                case 4:
                case 5:
                case 11:
                case 12:
                case 13:
                case 14:
                    return RTCR_PKCS1_RSA_OID;
                case 1: AssertFailed();
                    RT_FALL_THRU();
                default:
                    return NULL;
            }
    }
    /*
     * OIW oddballs.
     */
    else if (RTAsn1ObjId_StartsWith(pAlgorithm, "1.3.14.3.2"))
    {
        if (RTAsn1ObjIdCountComponents(pAlgorithm) == 6)
            switch (RTAsn1ObjIdGetLastComponentsAsUInt32(pAlgorithm))
            {
                case 11:
                case 14:
                case 15:
                case 24:
                case 25:
                case 29:
                    return RTCR_PKCS1_RSA_OID;
                default:
                    return NULL;
            }
    }


    return NULL;
}


RTDECL(bool) RTCrPkixPubKeyCanHandleDigestType(PCRTCRX509SUBJECTPUBLICKEYINFO pPublicKeyInfo, RTDIGESTTYPE enmDigestType,
                                               PRTERRINFO pErrInfo)
{
    bool fRc = false;
    if (RTCrX509SubjectPublicKeyInfo_IsPresent(pPublicKeyInfo))
    {
        void const * const  pvKeyBits = RTASN1BITSTRING_GET_BIT0_PTR(&pPublicKeyInfo->SubjectPublicKey);
        uint32_t const      cbKeyBits = RTASN1BITSTRING_GET_BYTE_SIZE(&pPublicKeyInfo->SubjectPublicKey);
        RTASN1CURSORPRIMARY PrimaryCursor;
        union
        {
            RTCRRSAPUBLICKEY    RsaPublicKey;
        } u;

        if (RTAsn1ObjId_CompareWithString(&pPublicKeyInfo->Algorithm.Algorithm, RTCR_PKCS1_RSA_OID) == 0)
        {
            /*
             * RSA.
             */
            RTAsn1CursorInitPrimary(&PrimaryCursor, pvKeyBits, cbKeyBits, pErrInfo, &g_RTAsn1DefaultAllocator,
                                    RTASN1CURSOR_FLAGS_DER, "rsa");

            RT_ZERO(u.RsaPublicKey);
            int rc = RTCrRsaPublicKey_DecodeAsn1(&PrimaryCursor.Cursor, 0, &u.RsaPublicKey, "PublicKey");
            if (RT_SUCCESS(rc))
                fRc = RTCrRsaPublicKey_CanHandleDigestType(&u.RsaPublicKey, enmDigestType, pErrInfo);
            RTCrRsaPublicKey_Delete(&u.RsaPublicKey);
        }
        else
        {
            RTErrInfoSetF(pErrInfo, VERR_CR_PKIX_CIPHER_ALGO_NOT_KNOWN, "%s", pPublicKeyInfo->Algorithm.Algorithm.szObjId);
            AssertMsgFailed(("unknown key algorithm: %s\n", pPublicKeyInfo->Algorithm.Algorithm.szObjId));
            fRc = true;
        }
    }
    return fRc;
}


RTDECL(bool) RTCrPkixCanCertHandleDigestType(PCRTCRX509CERTIFICATE pCertificate, RTDIGESTTYPE enmDigestType, PRTERRINFO pErrInfo)
{
    if (RTCrX509Certificate_IsPresent(pCertificate))
        return RTCrPkixPubKeyCanHandleDigestType(&pCertificate->TbsCertificate.SubjectPublicKeyInfo, enmDigestType, pErrInfo);
    return false;
}

