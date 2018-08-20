/** @file
 * IPRT - C++ Representational State Transfer (REST) Base Classes.
 */

/*
 * Copyright (C) 2008-2018 Oracle Corporation
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

#ifndef ___iprt_cpp_restbase_h
#define ___iprt_cpp_restbase_h

#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/cpp/utils.h>


/** @defgroup grp_rt_cpp_restbase   C++ Representational State Transfer (REST) Base Classes.
 * @ingroup grp_rt_cpp
 * @{
 */


/**
 * Base class for REST data objects.
 */
class RTCSRestObjectBase
{
public:
    RTCSRestObjectBase() {}
    virtual ~RTCSRestObjectBase() {}
    /** @todo Add some kind of state? */
};


/**
 * Base class for REST client requests.
 */
class RTCRestClientRequestBase
{
public:
    RTCRestClientRequestBase() {}
    virtual ~RTCRestClientRequestBase() {};

    /**
     * Prepares the HTTP handle for transmitting this request.
     *
     * @returns IPRT status code.
     * @param   a_hHttp     The HTTP handle to prepare for transmitting.
     */
    virtual int xmitPrepare(RTHTTP a_hHttp);

    /**
     * Always called after the request has been transmitted.
     *
     * @param   a_rcStatus  Negative numbers are IPRT errors, positive are HTTP status codes.
     * @param   a_hHttp     The HTTP handle the request was performed on.
     */
    virtual void xmitComplete(int a_rcStatus, RTHTTP a_hHttp);
};


/**
 * Base class for REST client responses.
 */
class RTCRestClientResponseBase
{
public:
    RTCRestClientResponseBase()
        : m_rcStatus(VERR_WRONG_ORDER)
    {}
    RTCRestClientResponseBase(RTCRestClientResponseBase const &a_rThat)
        : m_rcStatus(a_rThat.m_rcStatus)
    {}
    virtual ~RTCRestClientResponseBase();

    /**
     * Prepares the HTTP handle for receiving the response.
     *
     * This may install callbacks and such like.
     *
     * @returns IPRT status code.
     * @param   a_hHttp     The HTTP handle to prepare for receiving.
     */
    virtual int receivePrepare(RTHTTP a_hHttp);

    virtual int consumeHeader(const char *a_pvData, size_t a_cbData); ///< ??
    virtual int consumeBody(const char *a_pvData, size_t a_cbData);   ///< ??

    /**
     * Called when the HTTP request has been completely received.
     *
     * @returns IPRT status code?
     * @param   a_rcStatus  Negative numbers are IPRT errors, positive are HTTP status codes.
     * @param   a_hHttp     The HTTP handle the request was performed on.
     */
    virtual int receiveComplete(int a_rcStatus, RTHTTP a_hHttp);

    /**
     * Getter for m_rcStatus.
     * @returns Negative numbers are IPRT errors, positive are HTTP status codes.
     */
    int getStatus() { return m_rcStatus; }

private:
    /** Negative numbers are IPRT errors, positive are HTTP status codes. */
    int m_rcStatus;

};


/**
 * Base class for REST client responses.
 */
class RTCRestClientApiBase : public RTCNonCopyable
{
public:
    RTCRestClientApiBase()
        : m_hHttp(NIL_RTHTTP)
    {}
    virtual ~RTCRestClientApiBase();

    bool    isConnected();

protected:
    /** Handle to the HTTP connection object. */
    RTHTTP  m_hHttp;
};


#endif

