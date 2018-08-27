/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestClientApiBase implementation.
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

#include <iprt/err.h>
#include <iprt/http.h>


/**
 * The destructor.
 */
RTCRestClientApiBase::~RTCRestClientApiBase()
{
    if (m_hHttp != NIL_RTHTTP)
    {
        int rc = RTHttpDestroy(m_hHttp);
        AssertRC(rc);
        m_hHttp = NIL_RTHTTP;
    }
}


int RTCRestClientApiBase::reinitHttpInstance()
{
    if (m_hHttp != NIL_RTHTTP)
        return RTHttpReset(m_hHttp);

    int rc = RTHttpCreate(&m_hHttp);
    if (RT_FAILURE(rc))
        m_hHttp = NIL_RTHTTP;
    return rc;
}


void RTCRestClientApiBase::doCall(RTCRestClientRequestBase const &a_rRequest, RTHTTPMETHOD a_enmHttpMethod,
                                  RTCRestClientResponseBase *a_pResponse)
{
    /*
     * Initialize the HTTP instance.
     */
    RTHTTP hHttp = NIL_RTHTTP;
    int rc = reinitHttpInstance();
    if (RT_SUCCESS(rc))
    {
        hHttp = m_hHttp;
        Assert(hHttp != NIL_RTHTTP);

        /*
         * Prepare the response side.  This may install output callbacks and
         * indicate this by clearing the ppvBody/ppvHdr variables.
         */
        size_t   cbHdrs  = 0;
        void    *pvHdrs  = NULL;
        void   **ppvHdrs = &pvHdrs;

        size_t   cbBody  = 0;
        void    *pvBody  = NULL;
        void   **ppvBody = &pvBody;

        rc = a_pResponse->receivePrepare(hHttp, &ppvBody, &ppvHdrs);
        if (RT_SUCCESS(rc))
        {
            /*
             * Prepare the request for the transmission.
             */
            RTCString strExtraPath;
            RTCString strQuery;
            RTCString strXmitBody;
            rc = a_rRequest.xmitPrepare(&strExtraPath, &strQuery, hHttp, &strXmitBody);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Construct the full URL.
                 */
                RTCString strFullUrl;
                rc = strFullUrl.assignNoThrow(m_strBasePath);
                if (strExtraPath.isNotEmpty())
                {
                    if (!strExtraPath.startsWith("/") && !strFullUrl.endsWith("/") && RT_SUCCESS(rc))
                        rc = strFullUrl.appendNoThrow('/');
                    if (RT_SUCCESS(rc))
                        rc = strFullUrl.appendNoThrow(strExtraPath);
                    strExtraPath.setNull();
                }
                if (strQuery.isNotEmpty())
                {
                    if (RT_SUCCESS(rc))
                        rc = strFullUrl.appendNoThrow('?');
                    if (RT_SUCCESS(rc))
                        rc = strFullUrl.appendNoThrow(strQuery);
                    strQuery.setNull();
                }
                if (RT_SUCCESS(rc))
                {
                    /*
                     * Perform HTTP request.
                     */
                    uint32_t uHttpStatus = 0;
                    rc = RTHttpPerform(hHttp, strFullUrl.c_str(), a_enmHttpMethod,
                                       &uHttpStatus, ppvHdrs, &cbHdrs, ppvBody, &cbBody);
                    if (RT_SUCCESS(rc))
                    {
                        a_rRequest.xmitComplete(uHttpStatus, hHttp);

                        /*
                         * Do response processing.
                         */
                        a_pResponse->receiveComplete(uHttpStatus, hHttp);
                        if (pvHdrs)
                        {
                            a_pResponse->consumeHeaders((const char *)pvHdrs, cbHdrs);
                            RTHttpFreeResponse(pvHdrs);
                        }
                        if (pvBody)
                        {
                            a_pResponse->consumeBody((const char *)pvBody, cbBody);
                            RTHttpFreeResponse(pvBody);
                        }
                        a_pResponse->receiveFinal();

                        return;
                    }
                }
            }
            a_rRequest.xmitComplete(rc, hHttp);
        }
    }
    a_pResponse->receiveComplete(rc, hHttp);
}

