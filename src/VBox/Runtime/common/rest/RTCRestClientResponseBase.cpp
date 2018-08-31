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
#define LOG_GROUP RTLOGGROUP_REST
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
    if (a_rThat.m_pErrInfo)
        copyErrInfo(a_rThat.m_pErrInfo);
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


void RTCRestClientResponseBase::reset()
{
    /* Return to default constructor state. */
    m_rcStatus = VERR_WRONG_ORDER;
    m_rcHttp   = VERR_NOT_AVAILABLE;
    if (m_pErrInfo)
        deleteErrInfo();
    m_strContentType.setNull();
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


PRTERRINFO RTCRestClientResponseBase::getErrInfoInternal(void)
{
    if (m_pErrInfo)
        return m_pErrInfo;
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


int RTCRestClientResponseBase::addError(int rc, const char *pszFormat, ...)
{
    PRTERRINFO pErrInfo = getErrInfoInternal();
    if (pErrInfo)
    {
        va_list va;
        va_start(va, pszFormat);
        if (   !RTErrInfoIsSet(pErrInfo)
            || pErrInfo->cbMsg == 0
            || pErrInfo->pszMsg[pErrInfo->cbMsg - 1] == '\n')
            RTErrInfoAddV(pErrInfo, rc, pszFormat, va);
        else
            RTErrInfoAddF(pErrInfo, rc, "\n%N", pszFormat, &va);
        va_end(va);
    }
    if (RT_SUCCESS(m_rcStatus) && RT_FAILURE_NP(rc))
        m_rcStatus = rc;
    return rc;
}


void RTCRestClientResponseBase::extracHeaderFieldsFromBlob(HEADERFIELDDESC const *a_paFieldDescs,
                                                           RTCRestObjectBase ***a_pappFieldValues,
                                                           size_t a_cFields, const char *a_pchData, size_t a_cbData)

{
    RTCString strValue; /* (Keep it out here to encourage buffer allocation reuse and default construction call.) */

    /*
     * Work our way through the header blob.
     */
    while (a_cbData >= 2)
    {
        /*
         * Determine length of the header name:value combo.
         * Note! Multi-line field values are not currently supported.
         */
        const char *pchEol = (const char *)memchr(a_pchData, '\n', a_cbData);
        while (pchEol && (pchEol == a_pchData || pchEol[-1] != '\r'))
            pchEol = (const char *)memchr(pchEol, '\n', a_cbData - (pchEol - a_pchData));

        size_t const cchField       = pchEol ? pchEol - a_pchData + 1 : a_cbData;
        size_t const cchFieldNoCrLf = pchEol ? pchEol - a_pchData - 1 : a_cbData;

        const char *pchColon = (const char *)memchr(a_pchData, ':', cchFieldNoCrLf);
        Assert(pchColon);
        if (pchColon)
        {
            size_t const cchName  = pchColon - a_pchData;
            size_t const offValue = cchName + (RT_C_IS_BLANK(pchColon[1]) ? 2 : 1);
            size_t const cchValue = cchFieldNoCrLf - offValue;

            /*
             * Match headers.
             */
            bool fHaveValue = false;
            for (size_t i = 0; i < a_cFields; i++)
            {
                size_t const cchThisName = a_paFieldDescs[i].cchName;
                if (  !(a_paFieldDescs[i].fFlags & kHdrField_MapCollection)
                    ?    cchThisName == cchName
                      && RTStrNICmpAscii(a_pchData, a_paFieldDescs[i].pszName, cchThisName) == 0
                    :    cchThisName <= cchName
                      && RTStrNICmpAscii(a_pchData, a_paFieldDescs[i].pszName, cchThisName - 1) == 0)
                {
                    /* Get and clean the value. */
                    int rc = VINF_SUCCESS;
                    if (!fHaveValue)
                    {
                        rc = strValue.assignNoThrow(&a_pchData[offValue], cchValue);
                        if (RT_SUCCESS(rc))
                        {
                            RTStrPurgeEncoding(strValue.mutableRaw()); /** @todo this is probably a little wrong... */
                            fHaveValue = true;
                        }
                        else
                        {
                            addError(rc, "Error allocating %u bytes for header field %s", a_paFieldDescs[i].pszName);
                            break;
                        }
                    }

                    /*
                     * Create field to deserialize.
                     */
                    RTCRestObjectBase *pObj = NULL;
                    if (!(a_paFieldDescs[i].fFlags & (kHdrField_MapCollection | kHdrField_ArrayCollection)))
                    {
                        /* Only once. */
                        if (!*a_pappFieldValues[i])
                        {
                            pObj = a_paFieldDescs[i].pfnCreateInstance();
                            if (pObj)
                                *a_pappFieldValues[i] = pObj;
                            else
                            {
                                addError(VERR_NO_MEMORY, "out of memory");
                                continue;
                            }
                        }
                        else
                        {
                            addError(VERR_REST_RESPONSE_REPEAT_HEADER_FIELD, "Already saw header field '%s'", a_paFieldDescs[i].pszName);
                            continue;
                        }
                    }
                    else
                    {
                        Assert(a_paFieldDescs[i].pszName[cchThisName - 1] == '*');
                        AssertMsgFailed(("impl field collections"));
                        continue;
                    }

                    /*
                     * Deserialize it.
                     */
                    RTERRINFOSTATIC ErrInfo;
                    rc = pObj->fromString(strValue, a_paFieldDescs[i].pszName, RTErrInfoInitStatic(&ErrInfo),
                                          a_paFieldDescs[i].fFlags & RTCRestObjectBase::kCollectionFormat_Mask);
                    if (RT_SUCCESS(rc))
                    { /* likely */ }
                    else if (RTErrInfoIsSet(&ErrInfo.Core))
                        addError(rc, "Error %Rrc parsing header field '%s': %s",
                                 rc, a_paFieldDescs[i].pszName, ErrInfo.Core.pszMsg);
                    else
                        addError(rc, "Error %Rrc parsing header field '%s'", rc, a_paFieldDescs[i].pszName);
                }
            }
        }

        /*
         * Advance to the next field.
         */
        a_cbData  -= cchField;
        a_pchData += cchField;
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
                RTStrPurgeEncoding(a_pStrDst->mutableRaw()); /** @todo this is probably a little wrong... */
            return rc;
        }

        /* Advance to the next field. */
        a_pchData += cchField;
        a_cbData  -= cchField;
    }

    return VERR_NOT_FOUND;
}



RTCRestClientResponseBase::PrimaryJsonCursorForBody::PrimaryJsonCursorForBody(RTJSONVAL hValue, const char *pszName,
                                                                              RTCRestClientResponseBase *a_pThat)
    : RTCRestJsonPrimaryCursor(hValue, pszName, a_pThat->getErrInfoInternal())
    , m_pThat(a_pThat)
{
}


int RTCRestClientResponseBase::PrimaryJsonCursorForBody::addError(RTCRestJsonCursor const &a_rCursor, int a_rc,
                                                                  const char *a_pszFormat, ...)
{
    va_list va;
    va_start(va, a_pszFormat);
    char szPath[256];
    m_pThat->addError(a_rc, "response body/%s: %N", getPath(a_rCursor, szPath, sizeof(szPath)), a_pszFormat, &va);
    va_end(va);
    return a_rc;
}


int RTCRestClientResponseBase::PrimaryJsonCursorForBody::unknownField(RTCRestJsonCursor const &a_rCursor)
{
    char szPath[256];
    m_pThat->addError(VWRN_NOT_FOUND, "response body/%s: unknown field (type %s)",
                      getPath(a_rCursor, szPath, sizeof(szPath)), RTJsonValueTypeName(RTJsonValueGetType(a_rCursor.m_hValue)));
    return VWRN_NOT_FOUND;
}


void RTCRestClientResponseBase::deserializeBody(RTCRestObjectBase *a_pDst, const char *a_pchData, size_t a_cbData)
{
    if (m_strContentType.startsWith("application/json"))
    {
        int rc = RTStrValidateEncodingEx(a_pchData, a_cbData, RTSTR_VALIDATE_ENCODING_EXACT_LENGTH);
        if (RT_SUCCESS(rc))
        {
            RTERRINFOSTATIC ErrInfo;
            RTJSONVAL hValue;
            rc = RTJsonParseFromBuf(&hValue, (const uint8_t *)a_pchData, a_cbData, RTErrInfoInitStatic(&ErrInfo));
            if (RT_SUCCESS(rc))
            {
                PrimaryJsonCursorForBody PrimaryCursor(hValue, a_pDst->getType(), this); /* note: consumes hValue */
                a_pDst->deserializeFromJson(PrimaryCursor.m_Cursor);
            }
            else if (RTErrInfoIsSet(&ErrInfo.Core))
                addError(rc, "Error %Rrc parsing server response as JSON (type %s): %s",
                         rc, a_pDst->getType(), ErrInfo.Core.pszMsg);
            else
                addError(rc, "Error %Rrc parsing server response as JSON (type %s)", rc, a_pDst->getType());
        }
        else if (rc == VERR_INVALID_UTF8_ENCODING)
            addError(VERR_REST_RESPONSE_INVALID_UTF8_ENCODING, "Invalid UTF-8 body encoding (object type %s; Content-Type: %s)",
                     a_pDst->getType(), m_strContentType.c_str());
        else if (rc == VERR_BUFFER_UNDERFLOW)
            addError(VERR_REST_RESPONSE_EMBEDDED_ZERO_CHAR, "Embedded zero character in response (object type %s; Content-Type: %s)",
                     a_pDst->getType(), m_strContentType.c_str());
        else
            addError(rc, "Unexpected body validation error (object type %s; Content-Type: %s): %Rrc",
                     a_pDst->getType(), m_strContentType.c_str(), rc);
    }
    else
        addError(VERR_REST_RESPONSE_CONTENT_TYPE_NOT_SUPPORTED, "Unsupported content type for '%s': %s",
                 a_pDst->getType(), m_strContentType.c_str());
}

