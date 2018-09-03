/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientApiBase implementation, OCI specific bits.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restclient.h>

#include <iprt/base64.h>
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/log.h>
#include <iprt/uri.h>
#include <iprt/sha.h>
#include <iprt/crypto/digest.h>
#include <iprt/crypto/pkix.h>


/**
 * Worker for ociSignRequestAddAllFields().
 */
static int ociSignRequestAddField(RTCRDIGEST hDigest, RTCString *pStrAuth, const char *pszField, size_t cchField,
                                  const char *pszValue, size_t cchValue)
{
    /* First? */
    bool const fFirst = pStrAuth->endsWith("\"");

    /* Append the field to the list, in lowercased form: */
    int rc = VINF_SUCCESS;
    if (!fFirst)
        rc = pStrAuth->appendNoThrow(' ');
    if (RT_SUCCESS(rc))
    {
        size_t offStart = pStrAuth->length();
        rc = pStrAuth->appendNoThrow(pszField, cchField);
        if (RT_SUCCESS(rc))
        {
            RTStrToLower(pStrAuth->mutableRaw() + offStart);

            /* 2.3 (3) If not the first field, add newline separator. */
            rc = RTCrDigestUpdate(hDigest, "\n", 1);


            /* Update the digest with the field name followed by ': ' */
            rc = RTCrDigestUpdate(hDigest, pStrAuth->c_str() + offStart, pStrAuth->length() - offStart);
            if (RT_SUCCESS(rc))
            {
                rc = RTCrDigestUpdate(hDigest, RT_STR_TUPLE(": "));
                if (RT_SUCCESS(rc))
                {
                    /* Update the digest with the field value: */
                    if (cchValue == RTSTR_MAX)
                        cchValue = strlen(pszValue);
                    rc = RTCrDigestUpdate(hDigest, pszValue, cchValue);
                }
            }
        }
    }
    return rc;
}


/**
 * Worker for ociSignRequest().
 */
static int ociSignRequestAddAllFields(RTHTTP a_hHttp, RTCString const &a_rStrFullUrl, RTHTTPMETHOD a_enmHttpMethod,
                                      RTCString const &a_rStrXmitBody, uint32_t a_fFlags,
                                      RTCRDIGEST hDigest, RTCString *pStrAuth)
{
    char szTmp[256];

    /** @todo This is a little ugly.  I think that instead of this uglyness, we should just
     *  ensure the presence of required fields and sign all headers.  That way all parameters
     *  will be covered by the signature. */

    /*
     * (request-target) and host.
     */
    RTURIPARSED ParsedUrl;
    int rc = RTUriParse(a_rStrFullUrl.c_str(), &ParsedUrl);
    AssertRCReturn(rc, rc);

    const char *pszMethod = NULL;
    switch (a_enmHttpMethod)
    {
        case RTHTTPMETHOD_GET:      pszMethod = "get "; break;
        case RTHTTPMETHOD_PUT:      pszMethod = "put "; break;
        case RTHTTPMETHOD_POST:     pszMethod = "post "; break;
        case RTHTTPMETHOD_PATCH:    pszMethod = "patch "; break;
        case RTHTTPMETHOD_DELETE:   pszMethod = "delete "; break;
        case RTHTTPMETHOD_HEAD:     pszMethod = "head "; break;
        case RTHTTPMETHOD_OPTIONS:  pszMethod = "options "; break;
        case RTHTTPMETHOD_TRACE:    pszMethod = "trace "; break;
        case RTHTTPMETHOD_END:
        case RTHTTPMETHOD_INVALID:
        case RTHTTPMETHOD_32BIT_HACK:
            break;
    }
    AssertReturn(pszMethod, VERR_REST_INTERAL_ERROR_6);
    rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE("(request-target)"),
                                pszMethod, strlen(pszMethod));
    AssertRCReturn(rc, rc);
    const char *pszValue = a_rStrFullUrl.c_str() + ParsedUrl.offPath; /* Add the path. */
    rc = RTCrDigestUpdate(hDigest, pszValue, strlen(pszValue));

    rc = ociSignRequestAddField(hDigest,pStrAuth, RT_STR_TUPLE("host"),
                                a_rStrFullUrl.c_str() + ParsedUrl.offAuthorityHost, ParsedUrl.cchAuthorityHost);
    AssertRCReturn(rc, rc);

    /*
     * Content-Length - required for POST and PUT.
     * Note! We add it to the digest a little bit later to preserve documented order..
     */
    static char s_szContentLength[] = "content-length";
    const char *pszContentLength = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szContentLength));
    if (   !pszContentLength
        && (   a_rStrXmitBody.isNotEmpty()
            || a_enmHttpMethod == RTHTTPMETHOD_POST
            || a_enmHttpMethod == RTHTTPMETHOD_PUT))
    {
        RTStrPrintf(szTmp, sizeof(szTmp), "%zu", a_rStrXmitBody.length());

        rc = RTHttpAppendHeader(a_hHttp, s_szContentLength, szTmp, 0);
        AssertRCReturn(rc, rc);
        pszContentLength = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szContentLength));
        AssertPtrReturn(pszContentLength, VERR_REST_INTERAL_ERROR_4);
    }
    if (pszContentLength)
    {
        rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE(s_szContentLength), pszContentLength, RTSTR_MAX);
        AssertRCReturn(rc, rc);
    }

    /*
     * x-content-sha256 - required when there is a body.
     */
    static char s_szXContentSha256[] = "x-content-sha256";
    pszValue = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szXContentSha256));
    if (   !pszValue
        && pszContentLength
        && strcmp(pszContentLength, "0") != 0
        && !(a_fFlags & RTCRestClientApiBase::kDoCall_OciReqSignExcludeBody) )
    {
#ifdef RT_STRICT
        RTStrPrintf(szTmp, sizeof(szTmp), "%zu", a_rStrXmitBody.length());
        AssertMsgReturn(strcmp(szTmp, pszContentLength) == 0, ("szTmp=%s; pszContentLength=%s\n", szTmp, pszContentLength),
                        VERR_REST_INTERAL_ERROR_5);
#endif

        uint8_t abHash[RTSHA256_HASH_SIZE];
        RTSha256(a_rStrXmitBody.c_str(), a_rStrXmitBody.length(), abHash);
        rc = RTBase64EncodeEx(abHash, sizeof(abHash), RTBASE64_FLAGS_NO_LINE_BREAKS, szTmp, sizeof(szTmp), NULL);

        rc = RTHttpAppendHeader(a_hHttp, s_szXContentSha256, szTmp, 0);
        AssertRCReturn(rc, rc);
        pszValue = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szXContentSha256));
        AssertPtrReturn(pszValue, VERR_REST_INTERAL_ERROR_4);
    }
    if (pszValue)
    {
        rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE(s_szXContentSha256), pszValue, RTSTR_MAX);
        AssertRCReturn(rc, rc);
    }

    /*
     * Content-Type
     */
    pszValue = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE("content-type"));
    Assert(   pszValue
           || !pszContentLength
           || strcmp(pszContentLength, "0") == 0);
    if (pszValue)
    {
        rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE("content-type"), pszValue, RTSTR_MAX);
        AssertRCReturn(rc, rc);
    }

    /*
     * x-date or/and date.
     */
    static char const s_szDate[] = "date";
    pszValue = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szDate));
    static char const s_szXDate[] = "x-date";
    const char *pszXDate = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szXDate));
    if (!pszValue && !pszXDate)
    {
        RTTIMESPEC TimeSpec;
        RTTIME     Time;
        RT_ZERO(Time); /* paranoia */
        RTTimeExplode(&Time, RTTimeNow(&TimeSpec));

        /* Date format: Tue, 16 Nov 2018 12:15:00 GMT */
        /** @todo make RTTimeXxx api that does exactly this (RFC-1123). */
        static const char * const s_apszWeekDays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
        static const char * const s_apszMonths[] = { "000", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
        RTStrPrintf(szTmp, sizeof(szTmp), "%s, %u %s %u %u02:%u02:%u02 GMT",
                    s_apszWeekDays[Time.u8WeekDay], Time.u8MonthDay, s_apszMonths[Time.u8Month], Time.i32Year,
                    Time.u8Hour, Time.u8Minute, Time.u8Second);

        rc = RTHttpAppendHeader(a_hHttp, s_szXDate, szTmp, 0);
        AssertRCReturn(rc, rc);
        pszXDate = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE(s_szXDate));
        AssertPtrReturn(pszXDate, VERR_REST_INTERAL_ERROR_4);
    }
    if (pszXDate)
        rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE(s_szXDate), pszXDate, RTSTR_MAX);
    else
        rc = ociSignRequestAddField(hDigest, pStrAuth, RT_STR_TUPLE(s_szDate), pszValue, RTSTR_MAX);
    AssertRCReturn(rc, rc);

    /*
     * We probably should add all parameter fields ...
     */
    /** @todo sign more header fields */
    return VINF_SUCCESS;
}


int RTCRestClientApiBase::ociSignRequest(RTHTTP a_hHttp, RTCString const &a_rStrFullUrl, RTHTTPMETHOD a_enmHttpMethod,
                                         RTCString const &a_rStrXmitBody, uint32_t a_fFlags,
                                         RTCRKEY a_hKey, RTCString const &a_rStrKeyId)
{
    /*
     * Start the signature.
     */
    RTCString strAuth;
    int rc = strAuth.printfNoThrow("Signature version=\"1\",keyId=\"%s\",algorithm=\"rsa-sha256\",headers=\"",
                                   a_rStrKeyId.c_str());
    if (RT_SUCCESS(rc))
    {
        RTCRDIGEST hDigest;
        rc = RTCrDigestCreateByType(&hDigest, RTDIGESTTYPE_SHA256);
        if (RT_SUCCESS(rc))
        {
            /*
             * Call worker for adding the fields.  Simpler to clean up hDigest this way.
             */
            rc = ociSignRequestAddAllFields(a_hHttp, a_rStrFullUrl, a_enmHttpMethod, a_rStrXmitBody, a_fFlags, hDigest, &strAuth);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Do the signing.
                 */
                RTCRPKIXSIGNATURE hSigner;
                rc = RTCrPkixSignatureCreateByObjIdString(&hSigner, RTCR_PKCS1_SHA256_WITH_RSA_OID, a_hKey,
                                                          NULL, true /*fSigning*/);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    /* Figure the signature size first. */
                    size_t cbSignature = 0;
                    RTCrPkixSignatureSign(hSigner, hDigest, NULL, &cbSignature);
                    if (cbSignature == 0)
                        cbSignature = _32K;
                    size_t cbBase64Sign = RTBase64EncodedLengthEx(cbSignature, RTBASE64_FLAGS_NO_LINE_BREAKS) + 2;

                    /* Allocate temporary heap buffer and calc the signature. */
                    uint8_t *pbSignature = (uint8_t *)RTMemTmpAllocZ(cbSignature + cbBase64Sign);
                    if (pbSignature)
                    {
                        size_t cbActual = cbSignature;
                        rc = RTCrPkixSignatureSign(hSigner, hDigest, pbSignature, &cbActual);
                        AssertRC(rc);
                        if (RT_SUCCESS(rc))
                        {
                            /*
                             * Convert the signature to Base64 and add it to the auth value.
                             */
                            char *pszBase64 = (char *)(pbSignature + cbSignature);
                            rc = RTBase64EncodeEx(pbSignature, cbActual, RTBASE64_FLAGS_NO_LINE_BREAKS,
                                                  pszBase64, cbBase64Sign, NULL);
                            AssertRC(rc);
                            if (RT_SUCCESS(rc))
                            {
                                rc = strAuth.appendPrintfNoThrow("\",signature=\"Base64(RSA-SHA256(%s))\"", pszBase64);
                                if (RT_SUCCESS(rc))
                                {
                                    /*
                                     * Finally, add the authorization header.
                                     */
                                    rc = RTHttpAppendHeader(a_hHttp, "Authorization", strAuth.c_str(), 0);
                                    AssertRC(rc);
                                }
                            }
                        }
                        RT_BZERO(pbSignature, cbSignature + cbBase64Sign);
                        RTMemTmpFree(pbSignature);
                    }
                    else
                        rc = VERR_NO_TMP_MEMORY;
                    uint32_t cRefs = RTCrPkixSignatureRelease(hSigner);
                    Assert(cRefs == 0); NOREF(cRefs);
                }
            }
            RTCrDigestRelease(hDigest);
        }
    }
    return rc;
}

