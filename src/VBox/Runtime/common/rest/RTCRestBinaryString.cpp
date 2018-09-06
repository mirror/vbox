/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestBinaryString implementation.
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
#include <iprt/cpp/restbinarystring.h>

#include <iprt/assert.h>
#include <iprt/err.h>



/** Default constructor. */
RTCRestBinaryString::RTCRestBinaryString()
    : RTCRestObjectBase()
    , m_pbData(NULL)
    , m_cbData(UINT64_MAX)
    , m_pvCallbackData(NULL)
    , m_pfnConsumer(NULL)
    , m_pfnProducer(NULL)
    , m_fFreeData(false)
    , m_strContentType()
{
}


/**
 * Destructor.
 */
RTCRestBinaryString::~RTCRestBinaryString()
{
    if (m_pbData)
    {
        if (m_fFreeData)
            RTMemFree(m_pbData);
        m_pbData = NULL;
    }
    m_fFreeData      = false;
    m_pvCallbackData = NULL;
    m_pfnProducer    = NULL;
    m_pfnConsumer    = NULL;
}


/**
 * Safe copy assignment method.
 */
int RTCRestBinaryString::assignCopy(RTCRestBinaryString const &a_rThat)
{
    RT_NOREF(a_rThat);
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


/*********************************************************************************************************************************
*   Overridden methods                                                                                                           *
*********************************************************************************************************************************/

int RTCRestBinaryString::resetToDefault()
{
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
}


RTCRestOutputBase &RTCRestBinaryString::serializeAsJson(RTCRestOutputBase &a_rDst) const
{
    AssertMsgFailed(("We should never get here!\n"));
    a_rDst.printf("null");
    return a_rDst;
}


int RTCRestBinaryString::deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
{
    return a_rCursor.m_pPrimary->addError(a_rCursor, VERR_NOT_SUPPORTED, "RTCRestBinaryString does not support deserialization!");
}


int RTCRestBinaryString::toString(RTCString *a_pDst, uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/) const
{
    RT_NOREF(a_pDst, a_fFlags);
    AssertFailedReturn(VERR_NOT_SUPPORTED);
}


int RTCRestBinaryString::fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo/*= NULL*/,
                                    uint32_t a_fFlags /*= kCollectionFormat_Unspecified*/)
{
    RT_NOREF(a_rValue, a_pszName, a_fFlags);
    AssertFailedReturn(RTErrInfoSet(a_pErrInfo, VERR_NOT_SUPPORTED, "RTCRestBinaryString does not support fromString()!"));
}


RTCRestObjectBase::kTypeClass RTCRestBinaryString::typeClass(void) const
{
    return kTypeClass_BinaryString;
}


const char *RTCRestBinaryString::typeName(void) const
{
    return "RTCRestBinaryString";
}


/** Factory method. */
/*static*/ DECLCALLBACK(RTCRestObjectBase *) RTCRestBinaryString::createInstance(void)
{
    return new (std::nothrow) RTCRestBinaryString();
}


/*********************************************************************************************************************************
*   Transmit related methods                                                                                                     *
*********************************************************************************************************************************/

int RTCRestBinaryString::xmitPrepare(RTHTTP a_hHttp) const
{
    /*
     * Set the content type if given.
     */
    if (m_strContentType.isNotEmpty())
    {
        Assert(!RTHttpGetHeader(a_hHttp, RT_STR_TUPLE("Content-Type")));
        int rc = RTHttpAddHeader(a_hHttp, "Content-Type", m_strContentType.c_str(), m_strContentType.length(),
                                 RTHTTPADDHDR_F_BACK);
        AssertRCReturn(rc, rc);
    }

    /*
     * Set the content length if given.
     */
    if (m_cbData != UINT64_MAX)
    {
        const char *pszContentLength = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE("Content-Length"));
        AssertMsgReturn(!pszContentLength || RTStrToUInt64(pszContentLength) == m_cbData,
                        ("pszContentLength=%s does not match m_cbData=%RU64\n", pszContentLength, m_cbData),
                        VERR_MISMATCH);
        if (!pszContentLength)
        {
            char szValue[64];
            ssize_t cchValue = RTStrFormatU64(szValue, sizeof(szValue), m_cbData, 10, 0, 0, 0);
            int rc = RTHttpAddHeader(a_hHttp, "Content-Length", szValue, cchValue, RTHTTPADDHDR_F_BACK);
            AssertRCReturn(rc, rc);
        }
    }

    /*
     * Register an upload callback.
     */
    AssertReturn(m_pbData != NULL || m_pfnProducer != NULL || m_cbData == 0, VERR_INVALID_STATE);

    int rc = RTHttpSetUploadCallback(a_hHttp, m_cbData, xmitHttpCallback, (RTCRestBinaryString *)this);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/*static*/ DECLCALLBACK(int)
RTCRestBinaryString::xmitHttpCallback(RTHTTP hHttp, void *pvBuf, size_t cbBuf,
                                      uint64_t offContent, size_t *pcbActual, void *pvUser)
{
    RTCRestBinaryString *pThis = (RTCRestBinaryString *)pvUser;

    /*
     * Call the user upload callback if we've got one.
     */
    if (pThis->m_pfnProducer)
        return pThis->m_pfnProducer(pThis, pvBuf, cbBuf, offContent, pcbActual);

    /*
     * Feed from the memory buffer.
     */
    if (offContent < pThis->m_cbData)
    {
        uint64_t const cbLeft = pThis->m_cbData - offContent;
        size_t const cbToCopy = cbLeft >= cbBuf ? cbBuf : (size_t)cbLeft;
        memcpy(pvBuf, &pThis->m_pbData[(size_t)offContent], cbToCopy);
        *pcbActual = cbToCopy;
    }
    else
        *pcbActual = 0;

    RT_NOREF(hHttp);
    return VINF_SUCCESS;
}


void RTCRestBinaryString::xmitComplete(RTHTTP a_hHttp) const
{
    /* Unset the callback. */
    int rc = RTHttpSetUploadCallback(a_hHttp, m_cbData, NULL, NULL);
    AssertRC(rc);
}


