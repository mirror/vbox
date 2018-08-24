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
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/http.h>
#include <iprt/json.h>
#include <iprt/stdarg.h>
#include <iprt/cpp/ministring.h>
#include <iprt/cpp/utils.h>


/** @defgroup grp_rt_cpp_restbase   C++ Representational State Transfer (REST) Base Classes.
 * @ingroup grp_rt_cpp
 * @{
 */



/**
 * Abstract base class for serializing data objects.
 */
class RTCRestOutputBase
{
public:
    RTCRestOutputBase()
        : m_uIndent(0)
    { }
    virtual ~RTCRestOutputBase()
    { }

    /**
     * RTStrPrintf like function (see @ref pg_rt_str_format).
     *
     * @returns Number of bytes outputted.
     * @param   pszFormat   The format string.
     * @param   ...         Argument specfied in @a pszFormat.
     */
    size_t         printf(const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(2, 3)
    {
        va_list va;
        va_start(va, pszFormat);
        size_t cchWritten = this->vprintf(pszFormat, va);
        va_end(va);
        return cchWritten;
    }

    /**
     * RTStrPrintfV like function (see @ref pg_rt_str_format).
     *
     * @returns Number of bytes outputted.
     * @param   pszFormat   The format string.
     * @param   va          Argument specfied in @a pszFormat.
     */
    virtual size_t vprintf(const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(2, 0) = 0;

    /**
     * Sets the indentation level for use when pretty priting things.
     *
     * @returns Previous indentation level.
     * @param   uIndent     The indentation level.
     */
    unsigned setIndent(unsigned uIndent)
    {
        unsigned const uRet = m_uIndent;
        m_uIndent = uIndent;
        return uRet;
    }

    /**
     * Increases the indentation level.
     *
     * @returns Previous indentation level.
     */
    unsigned incrementIndent()
    {
        unsigned const uRet = m_uIndent;
        m_uIndent = uRet + 1;
        return uRet;
    }

protected:
    /** The current indentation level. */
    unsigned m_uIndent;
};


/**
 * Serialize to a string object.
 */
class RTCRestOutputToString : public RTCRestOutputBase
{
public:
    /**
     * Creates an instance that appends to @a a_pDst.
     * @param   a_pDst      Pointer to the destination string object.
     *                      NULL is not accepted and will assert.
     */
    RTCRestOutputToString(RTCString *a_pDst);
    virtual ~RTCRestOutputToString();

    size_t vprintf(const char *pszFormat, va_list va);

    /**
     * Finalizes the output and releases the string object to the caller.
     *
     * @returns The released string object.  NULL if we ran out of memory or if
     *          called already.
     *
     * @remark  This sets m_pDst to NULL and the object cannot be use for any
     *          more output afterwards.
     */
    virtual RTCString *finalize();


protected:
    /** Pointer to the destination string.  NULL after finalize().   */
    RTCString  *m_pDst;
    /** Set if we ran out of memory and should ignore subsequent calls. */
    bool        m_fOutOfMemory;

    /** @callback_method_impl{FNRTSTROUTPUT} */
    static DECLCALLBACK(size_t) strOutput(void *pvArg, const char *pachChars, size_t cbChars);

    /* Make non-copyable (RTCNonCopyable causes warnings): */
    RTCRestOutputToString(RTCRestOutputToString const &);
    RTCRestOutputToString *operator=(RTCRestOutputToString const &);
};


/* forward decl: */
class RTCRestJsonPrimaryCursor;

/**
 * JSON cursor structure.
 *
 * This reduces the number of parameters passed around when deserializing JSON
 * input and also helps constructing full object name for logging and error reporting.
 */
struct RTCRestJsonCursor
{
    /** Handle to the value being parsed. */
    RTJSONVAL                           m_hValue;
    /** Name of the value. */
    const char                         *m_pszName;
    /** Pointer to the parent, NULL if primary. */
    struct RTCRestJsonCursor const     *m_pParent;
    /** Pointer to the primary cursor structure. */
    RTCRestJsonPrimaryCursor           *m_pPrimary;

    RTCRestJsonCursor(struct RTCRestJsonCursor const &a_rParent)
        : m_hValue(NIL_RTJSONVAL), m_pszName(NULL), m_pParent(&a_rParent), m_pPrimary(a_rParent.m_pPrimary)
    { }

    RTCRestJsonCursor(RTJSONVAL hValue, const char *pszName, struct RTCRestJsonCursor *pParent)
        : m_hValue(hValue), m_pszName(pszName), m_pParent(pParent), m_pPrimary(pParent->m_pPrimary)
    { }

    RTCRestJsonCursor(RTJSONVAL hValue, const char *pszName)
        : m_hValue(hValue), m_pszName(pszName), m_pParent(NULL), m_pPrimary(NULL)
    { }

    ~RTCRestJsonCursor()
    {
        if (m_hValue != NIL_RTJSONVAL)
        {
            RTJsonValueRelease(m_hValue);
            m_hValue = NIL_RTJSONVAL;
        }
    }
};


/**
 * The primary JSON cursor class.
 */
class RTCRestJsonPrimaryCursor
{
public:
    /** The cursor for the first level. */
    RTCRestJsonCursor   m_Cursor;
    /** Error info keeper. */
    PRTERRINFO          m_pErrInfo;

    /** Creates a primary json cursor with optiona error info. */
    RTCRestJsonPrimaryCursor(RTJSONVAL hValue, const char *pszName, PRTERRINFO pErrInfo = NULL)
        : m_Cursor(hValue, pszName)
        , m_pErrInfo(pErrInfo)
    {
        m_Cursor.m_pPrimary = this;
    }

    virtual ~RTCRestJsonPrimaryCursor()
    {  }

    /**
     * Add an error message.
     *
     * @returns a_rc
     * @param   a_rCursor       The cursor reporting the error.
     * @param   a_rc            The status code.
     * @param   a_pszFormat     Format string.
     * @param   ...             Format string arguments.
     */
    virtual int addError(RTCRestJsonCursor const &a_rCursor, int a_rc, const char *a_pszFormat, ...);

    /**
     * Reports that the current field is not known.
     *
     * @returns Status to propagate.
     * @param   a_rCursor       The cursor for the field.
     */
    virtual int unknownField(RTCRestJsonCursor const &a_rCursor);

    /**
     * Copies the full path into pszDst.
     *
     * @returns pszDst
     * @param   a_rCursor       The cursor to start walking at.
     * @param   a_pszDst        Where to put the path.
     * @param   a_cbDst         Size of the destination buffer.
     */
    virtual char *getPath(RTCRestJsonCursor const &a_rCursor, char *a_pszDst, size_t a_cbDst) const;
};


/**
 * Abstract base class for REST data objects.
 */
class RTCRestObjectBase
{
public:
    RTCRestObjectBase() {}
    virtual ~RTCRestObjectBase() {}

    /** @todo Add some kind of state? */

    /**
     * Resets the object to all default values.
     */
    virtual void resetToDefaults() = 0;

    /**
     * Serialize the object as JSON.
     *
     * @returns a_rDst
     * @param   a_rDst      The destination for the serialization.
     */
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) = 0;

    /**
     * Deserialize object from the given JSON iterator.
     *
     * @returns IPRT status code.
     * @parm    a_rCursor    The JSON cursor.
     */
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) = 0;

    /** @name Helpers for deserializing primitive types and strings.
     * @{ */
    static int deserialize_RTCString_FromJson(RTCRestJsonCursor const &a_rCursor, RTCString *a_pDst);
    static int deserialize_int64_t_FromJson(RTCRestJsonCursor const &a_rCursor, int64_t *a_piDst);
    static int deserialize_int32_t_FromJson(RTCRestJsonCursor const &a_rCursor, int32_t *a_piDst);
    static int deserialize_int16_t_FromJson(RTCRestJsonCursor const &a_rCursor, int16_t *a_piDst);
    static int deserialize_bool_FromJson(RTCRestJsonCursor const &a_rCursor, bool *a_pfDst);
    static int deserialize_double_FromJson(RTCRestJsonCursor const &a_rCursor, double *a_prdDst);
    /** @} */

    /** @name Helpers for serializing floating point types.
     * @{ */
    static RTCRestOutputBase &serialize_double_AsJson(RTCRestOutputBase &a_rDst, double const *a_prdValue);
    /** @} */
};


/**
 * Limited array class.
 */
template<class Type> class RTCRestArray : public RTCRestObjectBase
{
public:
    RTCRestArray() {};
    ~RTCRestArray() {};
/** @todo more later. */

    virtual void resetToDefaults();
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst);
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
    {
        RT_NOREF(a_rCursor);
        return VERR_NOT_IMPLEMENTED;
    }
};


/**
 * Limited map class.
 */
template<class Type> class RTCRestStringMap : public RTCRestObjectBase
{
public:
    RTCRestStringMap() {};
    ~RTCRestStringMap() {};
/** @todo more later. */

    virtual void resetToDefaults();
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst);
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor)
    {
        RT_NOREF(a_rCursor);
        return VERR_NOT_IMPLEMENTED;
    }
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
class RTCRestClientApiBase
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

    /* Make non-copyable (RTCNonCopyable causes warnings): */
    RTCRestClientApiBase(RTCRestOutputToString const &);
    RTCRestClientApiBase *operator=(RTCRestOutputToString const &);
};

/** @} */

#endif

