/** @file
 * IPRT - C++ Representational State Transfer (REST) Client Classes.
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

#ifndef ___iprt_cpp_restclient_h
#define ___iprt_cpp_restclient_h

#include <iprt/http.h>
#include <iprt/cpp/restbase.h>


/** @defgroup grp_rt_cpp_restclient     C++ Representational State Transfer (REST) Client Classes.
 * @ingroup grp_rt_cpp
 * @{
 */

/**
 * Base class for REST client requests.
 *
 * This encapsulates parameters and helps transform them into a HTTP request.
 *
 * Parameters can be transfered in a number of places:
 *      - Path part of the URL.
 *      - Query part of the URL.
 *      - HTTP header fields.
 *      - FORM body.
 *      - JSON body.
 *      - XML body.
 *      - ...
 *
 * They can be require or optional.  The latter may have default values.  In
 * swagger 3 they can also be nullable, which means the null-indicator cannot
 * be used for tracking optional parameters.
 */
class RT_DECL_CLASS RTCRestClientRequestBase
{
public:
    RTCRestClientRequestBase();
    virtual ~RTCRestClientRequestBase();
    RTCRestClientRequestBase(RTCRestClientRequestBase const &a_rThat);
    RTCRestClientRequestBase &operator=(RTCRestClientRequestBase const &a_rThat);

    /**
     * Reset all members to default values.
     * @returns IPRT status code.
     */
    virtual int resetToDefault() = 0;

    /**
     * Prepares the HTTP handle for transmitting this request.
     *
     * @returns IPRT status code.
     * @param   a_pStrPath  Where to set path parameters.  Will be appended to the base path.
     * @param   a_pStrQuery Where to set query parameters.
     * @param   a_hHttp     Where to set header parameters and such.
     * @param   a_pStrBody  Where to set body parameters.
     */
    virtual int xmitPrepare(RTCString *a_pStrPath, RTCString *a_pStrQuery, RTHTTP a_hHttp, RTCString *a_pStrBody) const = 0;

    /**
     * Always called after the request has been transmitted.
     *
     * @param   a_rcStatus  Negative numbers are IPRT errors, positive are HTTP status codes.
     * @param   a_hHttp     The HTTP handle the request was performed on.
     */
    virtual void xmitComplete(int a_rcStatus, RTHTTP a_hHttp) const = 0;

    /**
     * Checks if there are were any assignment errors.
     */
    bool hasAssignmentErrors() const { return m_fErrorSet != 0; }

protected:
    /** Set of fields that have been explicitly assigned a value. */
    uint64_t m_fIsSet;
    /** Set of fields where value assigning failed. */
    uint64_t m_fErrorSet;

    /** Path parameter descriptor. */
    typedef struct
    {
        const char                 *pszName;    /**< The name string to replace (including {}). */
        size_t                      cchName;    /**< Length of pszName. */
        uint32_t                    fFlags;     /**< The toString flags. */
        uint8_t                     iBitNo;     /**< The parameter bit number. */
    } PATHPARAMDESC;

    /** Path parameter state. */
    typedef struct
    {
        RTCRestObjectBase const    *pObj;       /**< Pointer to the parameter object. */
        size_t                      offName;    /**< Maintained by worker. */
    } PATHPARAMSTATE;

    /**
     * Do path parameters.
     *
     * @returns IPRT status code
     * @param   a_pStrPath          The destination path.
     * @param   a_pszPathTemplate   The path template string.
     * @param   a_cchPathTemplate   The length of the path template string.
     * @param   a_paPathParams      The path parameter descriptors (static).
     * @param   a_paPathParamStates The path parameter objects and states.
     * @param   a_cPathParams       Number of path parameters.
     */
    int doPathParameters(RTCString *a_pStrPath, const char *a_pszPathTemplate, size_t a_cchPathTemplate,
                         PATHPARAMDESC const *a_paPathParams, PATHPARAMSTATE *a_paPathParamStates, size_t a_cPathParams) const;

    /** Query parameter descriptor. */
    typedef struct
    {
        const char *pszName;            /**< The parameter name. */
        uint32_t    fFlags;             /**< The toString flags. */
        bool        fRequired;          /**< Required or not. */
        uint8_t     iBitNo;             /**< The parameter bit number. */
    } QUERYPARAMDESC;

    /**
     * Do query parameters.
     *
     * @returns IPRT status code
     * @param   a_pStrQuery         The destination string.
     * @param   a_paQueryParams     The query parameter descriptors.
     * @param   a_papQueryParamObjs The query parameter objects, parallel to @a a_paQueryParams.
     * @param   a_cQueryParams      Number of query parameters.
     */
    int doQueryParameters(RTCString *a_pStrQuery, QUERYPARAMDESC const *a_paQueryParams,
                          RTCRestObjectBase const **a_papQueryParamObjs, size_t a_cQueryParams) const;

    /** Header parameter descriptor. */
    typedef struct
    {
        const char *pszName;            /**< The parameter name. */
        uint32_t    fFlags;             /**< The toString flags. */
        bool        fRequired;          /**< Required or not. */
        uint8_t     iBitNo;             /**< The parameter bit number. */
        bool        fMapCollection;     /**< Collect headers starting with pszName into a map. */
    } HEADERPARAMDESC;

    /**
     * Do header parameters.
     *
     * @returns IPRT status code
     * @param   a_hHttp                 Where to set header parameters.
     * @param   a_paHeaderParams        The header parameter descriptors.
     * @param   a_papHeaderParamObjs    The header parameter objects, parallel to @a a_paHeaderParams.
     * @param   a_cHeaderParams         Number of header parameters.
     */
    int doHeaderParameters(RTHTTP a_hHttp, HEADERPARAMDESC const *a_paHeaderParams,
                           RTCRestObjectBase const **a_papHeaderParamObjs, size_t a_cHeaderParams) const;
};


/**
 * Base class for REST client responses.
 */
class RT_DECL_CLASS RTCRestClientResponseBase
{
public:
    /** Default constructor. */
    RTCRestClientResponseBase();
    /** Destructor. */
    virtual ~RTCRestClientResponseBase();
    /** Copy constructor. */
    RTCRestClientResponseBase(RTCRestClientResponseBase const &a_rThat);
    /** Copy assignment operator. */
    RTCRestClientResponseBase &operator=(RTCRestClientResponseBase const &a_rThat);

    /**
     * Resets the object state.
     */
    virtual void reset(void);

    /**
     * Prepares the HTTP handle for receiving the response.
     *
     * This may install callbacks and such like.
     *
     * @returns IPRT status code.
     * @param   a_hHttp     The HTTP handle to prepare for receiving.
     * @param   a_pppvHdr   If a header callback handler is installed, set the value pointed to to NULL.
     * @param   a_pppvBody  If a body callback handler is installed, set the value pointed to to NULL.
     */
    virtual int receivePrepare(RTHTTP a_hHttp, void ***a_pppvHdr, void ***a_pppvBody);

    /**
     * Called when the HTTP request has been completely received.
     *
     * @param   a_rcStatus  Negative numbers are IPRT errors, positive are HTTP status codes.
     * @param   a_hHttp     The HTTP handle the request was performed on.
     *                      This can be NIL_RTHTTP should something fail early, in
     *                      which case it is possible receivePrepare() wasn't called.
     *
     * @note    Called before consumeHeaders() and consumeBody().
     */
    virtual void receiveComplete(int a_rcStatus, RTHTTP a_hHttp);

    /**
     * Callback that consumes HTTP header data from the server.
     *
     * @param   a_pchData  Body data.
     * @param   a_cbData   Amount of body data.
     *
     * @note    Called after receiveComplete()..
     */
    virtual void consumeHeaders(const char *a_pchData, size_t a_cbData);

    /**
     * Callback that consumes HTTP body data from the server.
     *
     * @param   a_pchData  Body data.
     * @param   a_cbData   Amount of body data.
     *
     * @note    Called after consumeHeaders().
     */
    virtual void consumeBody(const char *a_pchData, size_t a_cbData);

    /**
     * Called after status, headers and body all have been presented.
     *
     * @returns IPRT status code.
     */
    virtual void receiveFinal();

    /**
     * Getter for m_rcStatus.
     * @returns Negative numbers are IPRT errors, positive are HTTP status codes.
     */
    int getStatus() { return m_rcStatus; }

    /**
     * Getter for m_rcHttp.
     * @returns HTTP status code or VERR_NOT_AVAILABLE.
     */
    int getHttpStatus() { return m_rcHttp; }

    /**
     * Getter for m_pErrInfo.
     */
    PCRTERRINFO getErrInfo(void) const { return m_pErrInfo; }

    /**
     * Getter for m_strContentType.
     */
    RTCString const &getContentType(void) const { return m_strContentType; }


protected:
    /** Negative numbers are IPRT errors, positive are HTTP status codes. */
    int         m_rcStatus;
    /** The HTTP status code, VERR_NOT_AVAILABLE if not set. */
    int         m_rcHttp;
    /** Error information. */
    PRTERRINFO  m_pErrInfo;
    /** The value of the Content-Type header field. */
    RTCString   m_strContentType;

    PRTERRINFO  getErrInfoInternal(void);
    void        deleteErrInfo(void);
    void        copyErrInfo(PCRTERRINFO pErrInfo);

    /**
     * Reports an error (or warning if a_rc non-negative).
     *
     * @returns a_rc
     * @param   a_rc        The status code to report and return.  The first
     *                      error status is assigned to m_rcStatus, subsequent
     *                      ones as well as informational statuses are not
     *                      recorded by m_rcStatus.
     * @param   a_pszFormat The message format string.
     * @param   ...         Message arguments.
     */
    int addError(int a_rc, const char *a_pszFormat, ...);

    /** Field flags. */
    enum
    {
        /** Collection map, name is a prefix followed by '*'. */
        kHdrField_MapCollection   = RT_BIT_32(24),
    };

    /** Header field descriptor. */
    typedef struct
    {
        /** The header field name. */
        const char *pszName;
        /** The length of the field name.*/
        uint32_t    cchName;
        /** Flags, TBD. */
        uint32_t    fFlags;
        /** Object factory. */
        RTCRestObjectBase::PFNCREATEINSTANCE pfnCreateInstance;
    } HEADERFIELDDESC;

    /**
     * Helper that extracts fields from the HTTP headers.
     *
     * @param   a_paFieldDescs      Pointer to an array of field descriptors.
     * @param   a_pappFieldValues   Pointer to a parallel array of value pointer pointers.
     * @param   a_cFields           Number of field descriptors..
     * @param   a_pchData           The header blob to search.
     * @param   a_cbData            The size of the header blob to search.
     */
    void extracHeaderFieldsFromBlob(HEADERFIELDDESC const *a_paFieldDescs, RTCRestObjectBase ***a_pappFieldValues,
                                    size_t a_cFields, const char *a_pchData, size_t a_cbData);

    /**
     * Helper that extracts a header field.
     *
     * @retval  VINF_SUCCESS
     * @retval  VERR_NOT_FOUND if not found.
     * @retval  VERR_NO_STR_MEMORY
     * @param   a_pszField  The header field header name.
     * @param   a_cchField  The length of the header field name.
     * @param   a_pchData   The header blob to search.
     * @param   a_cbData    The size of the header blob to search.
     * @param   a_pStrDst   Where to store the header value on successs.
     */
    int extractHeaderFromBlob(const char *a_pszField, size_t a_cchField, const char *a_pchData, size_t a_cbData,
                              RTCString *a_pStrDst);

    /**
     * Helper that does the deserializing of the response body.
     *
     * @param   a_pDst      The destination object for the body content.
     * @param   a_pchData   The body blob.
     * @param   a_cbData    The size of the body blob.
     */
    void deserializeBody(RTCRestObjectBase *a_pDst, const char *a_pchData, size_t a_cbData);

    /**
     * Primary json cursor for parsing bodies.
     */
    class PrimaryJsonCursorForBody : public RTCRestJsonPrimaryCursor
    {
    public:
        RTCRestClientResponseBase *m_pThat; /**< Pointer to response object. */
        PrimaryJsonCursorForBody(RTJSONVAL hValue, const char *pszName, RTCRestClientResponseBase *a_pThat);
        virtual int addError(RTCRestJsonCursor const &a_rCursor, int a_rc, const char *a_pszFormat, ...) RT_OVERRIDE;
        virtual int unknownField(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    };
};


/**
 * Base class for REST client responses.
 */
class RT_DECL_CLASS RTCRestClientApiBase
{
public:
    RTCRestClientApiBase()
        : m_hHttp(NIL_RTHTTP)
    {}
    virtual ~RTCRestClientApiBase();

    /** @name Base path (URL) handling.
     * @{ */
    /**
     * Gets the base path we're using.
     *
     * @returns Base URL string.  If empty, we'll be using the default one.
     */
    RTCString const &getBasePath(void) const
    {
        return m_strBasePath;
    }

    /**
     * Sets the base path (URL) to use when talking to the server.
     *
     * Setting the base path is only required if there is a desire to use a
     * different server from the one specified in the API specification, like
     * for instance regional one.
     *
     * @param   a_pszPath   The base path to use.
     */
    virtual void setBasePath(const char *a_pszPath)
    {
        m_strBasePath = a_pszPath;
    }

    /**
     * Sets the base path (URL) to use when talking to the server.
     *
     * Setting the base path is only required if there is a desire to use a
     * different server from the one specified in the API specification, like
     * for instance regional one.
     *
     * @param   a_strPath   The base path to use.
     * @note    Defers to the C-string variant.
     */
    void setBasePath(RTCString const &a_strPath) { setBasePath(a_strPath.c_str()); }

    /**
     * Gets the default base path (URL) as specified in the specs.
     *
     * @returns Base path (URL) string.
     */
    virtual const char *getDefaultBasePath() = 0;
    /** @} */

protected:
    /** Handle to the HTTP connection object. */
    RTHTTP  m_hHttp;
    /** The base path to use. */
    RTCString m_strBasePath;

    /* Make non-copyable (RTCNonCopyable causes warnings): */
    RTCRestClientApiBase(RTCRestOutputToString const &);
    RTCRestClientApiBase *operator=(RTCRestOutputToString const &);

    /**
     * Re-initializes the HTTP instance.
     *
     * @returns IPRT status code.
     */
    virtual int reinitHttpInstance();

    /**
     * Implements stuff for making an API call.
     *
     * @returns a_pResponse->getStatus()
     * @param   a_rRequest      Reference to the request object.
     * @param   a_enmHttpMethod The HTTP request method.
     * @param   a_pResponse     Pointer to the response object.
     * @param   a_pszMethod     The method name, for logging purposes.
     */
    virtual int doCall(RTCRestClientRequestBase const &a_rRequest, RTHTTPMETHOD a_enmHttpMethod,
                       RTCRestClientResponseBase *a_pResponse, const char *a_pszMethod);

};

/** @} */

#endif

