/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientResponseBase implementation.
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
#include <iprt/cpp/restbase.h>

#include <iprt/ctype.h>
#include <iprt/err.h>


/**
 * Default constructor.
 */
RTCRestClientResponseBase::RTCRestClientResponseBase()
    : m_rcStatus(VERR_WRONG_ORDER)
    , m_rcHttp(VERR_NOT_AVAILABLE)
    , m_pErrInfo(NULL)
{
}


/**
 * Destructor.
 */
RTCRestClientResponseBase::~RTCRestClientResponseBase()
{
    deleteErrInfo();
}


/**
 * Copy constructor.
 */
RTCRestClientResponseBase::RTCRestClientResponseBase(RTCRestClientResponseBase const &a_rThat)
    : m_rcStatus(a_rThat.m_rcStatus)
    , m_rcHttp(a_rThat.m_rcHttp)
    , m_pErrInfo(NULL)
    , m_strContentType(a_rThat.m_strContentType)
{
    if (m_pErrInfo)
        copyErrInfo(m_pErrInfo);
}


/**
 * Copy assignment operator.
 */
RTCRestClientResponseBase &RTCRestClientResponseBase::operator=(RTCRestClientResponseBase const &a_rThat)
{
    m_rcStatus       = a_rThat.m_rcStatus;
    m_rcHttp         = a_rThat.m_rcHttp;
    m_strContentType = a_rThat.m_strContentType;
    if (a_rThat.m_pErrInfo)
        copyErrInfo(a_rThat.m_pErrInfo);
    else if (m_pErrInfo)
        deleteErrInfo();

    return *this;
}


int RTCRestClientResponseBase::receivePrepare(RTHTTP a_hHttp, void ***a_pppvHdr, void ***a_pppvBody)
{
    RT_NOREF(a_hHttp, a_pppvHdr, a_pppvBody);
    return VINF_SUCCESS;
}


void RTCRestClientResponseBase::receiveComplete(int a_rcStatus, RTHTTP a_hHttp)
{
    RT_NOREF_PV(a_hHttp);
    m_rcStatus = a_rcStatus;
    if (a_rcStatus >= 0)
        m_rcHttp = a_rcStatus;
}


void RTCRestClientResponseBase::consumeHeaders(const char *a_pchData, size_t a_cbData)
{
    /*
     * Get the the content type.
     */
    int rc = extractHeaderFromBlob(RT_STR_TUPLE("Content-Type"), a_pchData, a_cbData, &m_strContentType);
    if (rc == VERR_NOT_FOUND)
        rc = VINF_SUCCESS;
    AssertRCReturnVoidStmt(rc, m_rcStatus = rc);
}


void RTCRestClientResponseBase::consumeBody(const char *a_pchData, size_t a_cbData)
{
    RT_NOREF(a_pchData, a_cbData);
}


void RTCRestClientResponseBase::receiveFinal()
{
}


PRTERRINFO RTCRestClientResponseBase::allocErrInfo(void)
{
    size_t cbMsg = _4K;
    m_pErrInfo = (PRTERRINFO)RTMemAllocZ(sizeof(*m_pErrInfo) + cbMsg);
    if (m_pErrInfo)
        return RTErrInfoInit(m_pErrInfo, (char *)(m_pErrInfo + 1), cbMsg);
    return NULL;
}


void RTCRestClientResponseBase::deleteErrInfo(void)
{
    if (m_pErrInfo)
    {
        RTMemFree(m_pErrInfo);
        m_pErrInfo = NULL;
    }
}


void RTCRestClientResponseBase::copyErrInfo(PCRTERRINFO pErrInfo)
{
    deleteErrInfo();
    m_pErrInfo = (PRTERRINFO)RTMemDup(pErrInfo, pErrInfo->cbMsg + sizeof(*pErrInfo));
    if (m_pErrInfo)
    {
        m_pErrInfo->pszMsg = (char *)(m_pErrInfo + 1);
        m_pErrInfo->apvReserved[0] = NULL;
        m_pErrInfo->apvReserved[1] = NULL;
    }
}


int RTCRestClientResponseBase::extractHeaderFromBlob(const char *a_pszField, size_t a_cchField,
                                                     const char *a_pchData, size_t a_cbData,
                                                     RTCString *a_pStrDst)
{
    char const chUpper0 = RT_C_TO_UPPER(a_pszField[0]);
    char const chLower0 = RT_C_TO_LOWER(a_pszField[0]);
    Assert(!RT_C_IS_SPACE(chUpper0));

    while (a_cbData > a_cchField)
    {
        /* Determine length of the header name:value combo.
           Note! Multi-line field values are not currently supported. */
        const char *pchEol = (const char *)memchr(a_pchData, '\n', a_cbData);
        while (pchEol && (pchEol == a_pchData || pchEol[-1] != '\r'))
            pchEol = (const char *)memchr(pchEol, '\n', a_cbData - (pchEol - a_pchData));

        size_t cchField = pchEol ? pchEol - a_pchData + 1 : a_cbData;

        /* Try match */
        if (   a_pchData[a_cchField] == ':'
            && (   a_pchData[0] == chUpper0
                || a_pchData[0] == chLower0)
            && RTStrNICmpAscii(a_pchData, a_pszField, a_cchField) == 0)
        {
            /* Drop CRLF. */
            if (pchEol)
                cchField -= 2;

            /* Drop the field name and optional whitespace. */
            cchField  -= a_cchField + 1;
            a_pchData += a_cchField + 1;
            if (cchField > 0 && RT_C_IS_BLANK(*a_pchData))
            {
                a_pchData++;
                cchField--;
            }

            /* Return the value. */
            int rc = a_pStrDst->assignNoThrow(a_pchData, cchField);
            if (RT_SUCCESS(rc))
                RTStrPurgeEncoding(a_pStrDst->mutableRaw());
            return rc;
        }

        /* Advance to the next field. */
        a_pchData += cchField;
        a_cbData  -= cchField;
    }

    return VERR_NOT_FOUND;
}

