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


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The default maximum download size. */
#if ARCH_BITS == 32
# define RTCREST_MAX_DOWNLOAD_SIZE_DEFAULT  _32M
#else
# define RTCREST_MAX_DOWNLOAD_SIZE_DEFAULT  _128M
#endif


/** Default constructor. */
RTCRestBinaryString::RTCRestBinaryString()
    : RTCRestObjectBase()
    , m_pbData(NULL)
    , m_cbAllocated(0)
    , m_fFreeData(false)
    , m_cbContentLength(UINT64_MAX)
    , m_pvCallbackData(NULL)
    , m_pfnProducer(NULL)
    , m_strContentType()
    , m_pfnConsumer(NULL)
    , m_cbDownloaded(0)
    , m_cbMaxDownload(RTCREST_MAX_DOWNLOAD_SIZE_DEFAULT)
{
}


/**
 * Destructor.
 */
RTCRestBinaryString::~RTCRestBinaryString()
{
    freeData();
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


void RTCRestBinaryString::freeData()
{
    if (m_pbData)
    {
        if (m_fFreeData)
            RTMemFree(m_pbData);
        m_pbData = NULL;
    }
    m_fFreeData       = false;
    m_cbAllocated     = 0;
    m_cbDownloaded    = 0;
    m_cbContentLength = UINT64_MAX;
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

int RTCRestBinaryString::setContentType(const char *a_pszContentType)
{
    return m_strContentType.assignNoThrow(a_pszContentType);
}


int RTCRestBinaryString::setUploadData(void const *a_pvData, size_t a_cbData, bool a_fCopy /*= true*/)
{
    freeData();

    if (a_cbData != 0)
    {
        if (a_fCopy)
        {
            m_pbData = (uint8_t *)RTMemDup(a_pvData, a_cbData);
            AssertReturn(m_pbData, VERR_NO_MEMORY);
            m_fFreeData   = true;
            m_cbAllocated = a_cbData;
        }
        else
        {
            AssertPtrReturn(a_pvData, VERR_INVALID_POINTER);
            m_pbData = (uint8_t *)a_pvData;
        }
    }
    m_cbContentLength = a_cbData;

    return VINF_SUCCESS;
}


void RTCRestBinaryString::setProducerCallback(PFNPRODUCER a_pfnProducer, void *a_pvCallbackData /*= NULL*/,
                                              uint64_t a_cbContentLength /*= UINT64_MAX*/)
{
    freeData();

    m_cbContentLength = a_cbContentLength;
    m_pfnProducer     = a_pfnProducer;
    m_pvCallbackData  = a_pvCallbackData;
}


int RTCRestBinaryString::xmitPrepare(RTHTTP a_hHttp) const
{
    AssertReturn(m_pbData != NULL || m_pfnProducer != NULL || m_cbContentLength == 0, VERR_INVALID_STATE);


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
    if (m_cbContentLength != UINT64_MAX)
    {
        const char *pszContentLength = RTHttpGetHeader(a_hHttp, RT_STR_TUPLE("Content-Length"));
        AssertMsgReturn(!pszContentLength || RTStrToUInt64(pszContentLength) == m_cbContentLength,
                        ("pszContentLength=%s does not match m_cbContentLength=%RU64\n", pszContentLength, m_cbContentLength),
                        VERR_MISMATCH);
        if (!pszContentLength)
        {
            char szValue[64];
            ssize_t cchValue = RTStrFormatU64(szValue, sizeof(szValue), m_cbContentLength, 10, 0, 0, 0);
            int rc = RTHttpAddHeader(a_hHttp, "Content-Length", szValue, cchValue, RTHTTPADDHDR_F_BACK);
            AssertRCReturn(rc, rc);
        }
    }

    /*
     * Register an upload callback.
     */
    int rc = RTHttpSetUploadCallback(a_hHttp, m_cbContentLength, xmitHttpCallback, (RTCRestBinaryString *)this);
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
    if (offContent < pThis->m_cbContentLength)
    {
        uint64_t const cbLeft = pThis->m_cbContentLength - offContent;
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
    int rc = RTHttpSetUploadCallback(a_hHttp, UINT64_MAX, NULL, NULL);
    AssertRC(rc);
}


/*********************************************************************************************************************************
*   Download related methods                                                                                                     *
*********************************************************************************************************************************/

void RTCRestBinaryString::setMaxDownloadSize(size_t a_cbMaxDownload)
{
    if (a_cbMaxDownload == 0)
        m_cbMaxDownload = RTCREST_MAX_DOWNLOAD_SIZE_DEFAULT;
    else
        m_cbMaxDownload = a_cbMaxDownload;
}


void RTCRestBinaryString::setConsumerCallback(PFNCONSUMER a_pfnConsumer, void *a_pvCallbackData /*= NULL*/)
{
    freeData();

    a_pfnConsumer    = a_pfnConsumer;
    m_pvCallbackData = a_pvCallbackData;
}


int RTCRestBinaryString::receivePrepare(RTHTTP a_hHttp, uint32_t a_fCallbackFlags)
{
    /*
     * Register an download callback.
     */
    int rc = RTHttpSetDownloadCallback(a_hHttp, a_fCallbackFlags, receiveHttpCallback, this);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}


void RTCRestBinaryString::receiveComplete(RTHTTP a_hHttp)
{
    /* Unset the callback. */
    int rc = RTHttpSetDownloadCallback(a_hHttp, RTHTTPDOWNLOAD_F_ANY_STATUS, NULL, NULL);
    AssertRC(rc);
}


/*static*/ DECLCALLBACK(int)
RTCRestBinaryString::receiveHttpCallback(RTHTTP hHttp, void const *pvBuf, size_t cbBuf, uint32_t uHttpStatus,
                                         uint64_t offContent, uint64_t cbContent, void *pvUser)
{
    RTCRestBinaryString *pThis = (RTCRestBinaryString *)pvUser;
    Assert(offContent == pThis->m_cbDownloaded);
    pThis->m_cbContentLength = cbContent;

    /*
     * Call the user download callback if we've got one.
     */
    if (pThis->m_pfnConsumer)
    {
        int rc = pThis->m_pfnConsumer(pThis, pvBuf, cbBuf, uHttpStatus, offContent, cbContent);
        if (RT_SUCCESS(rc))
            pThis->m_cbDownloaded = offContent + cbBuf;
        return rc;
    }

    /*
     * Check download limit before adding more data.
     */
    AssertMsgReturn(offContent + cbBuf <= pThis->m_cbMaxDownload,
                    ("%RU64 + %zu = %RU64; max=%RU64", offContent, cbBuf, offContent + cbBuf, pThis->m_cbMaxDownload),
                    VERR_TOO_MUCH_DATA);
    if (offContent == 0 && cbContent != UINT64_MAX)
        AssertMsgReturn(cbContent <= pThis->m_cbMaxDownload, ("cbContent: %RU64; max=%RU64", cbContent,  pThis->m_cbMaxDownload),
                        VERR_TOO_MUCH_DATA);

    /*
     * Make sure we've got buffer space before we copy in the data.
     */
    if (offContent + cbBuf <= pThis->m_cbAllocated)
    { /* likely, except for the first time. */ }
    else if (offContent == 0 && cbContent != UINT64_MAX)
    {
        void *pvNew = RTMemRealloc(pThis->m_pbData, (size_t)cbContent);
        if (!pvNew)
            return VERR_NO_MEMORY;
        pThis->m_pbData = (uint8_t *)pvNew;
        pThis->m_cbAllocated = (size_t)cbContent;
    }
    else
    {
        size_t cbNeeded = offContent + cbBuf;
        size_t cbNew;
        if (pThis->m_cbAllocated == 0)
            cbNew = RT_MAX(_64K, RT_ALIGN_Z(cbNeeded, _64K));
        else if (pThis->m_cbAllocated < _64M && cbNeeded <= _64M)
        {
            cbNew = pThis->m_cbAllocated * 2;
            while (cbNew < cbNeeded)
                cbNew *= 2;
        }
        else
            cbNew = RT_ALIGN_Z(cbNeeded, _32M);

        void *pvNew = RTMemRealloc(pThis->m_pbData, cbNew);
        if (!pvNew)
            return VERR_NO_MEMORY;
        pThis->m_pbData = (uint8_t *)pvNew;
        pThis->m_cbAllocated = cbNew;
    }

    /*
     * Do the copying.
     */
    memcpy(&pThis->m_pbData[(size_t)offContent], pvBuf, cbBuf);
    pThis->m_cbDownloaded = offContent + cbBuf;

    RT_NOREF(hHttp);
    return VINF_SUCCESS;
}

