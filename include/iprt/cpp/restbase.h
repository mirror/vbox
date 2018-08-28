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

    virtual size_t vprintf(const char *pszFormat, va_list va) RT_OVERRIDE;

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
    RTCRestObjectBase();
    virtual ~RTCRestObjectBase();

    /** @todo Add some kind of state? */

    /**
     * Resets the object to all default values.
     */
    virtual void resetToDefault() = 0;

    /**
     * Serialize the object as JSON.
     *
     * @returns a_rDst
     * @param   a_rDst      The destination for the serialization.
     */
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const = 0;

    /**
     * Deserialize object from the given JSON iterator.
     *
     * @returns IPRT status code.
     * @param   a_rCursor    The JSON cursor.
     */
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) = 0;

    /**
     * Flags for toString().
     *
     * The kCollectionFormat_xxx bunch controls multiple values in arrays
     * are formatted.  They are ignored by everyone else.
     */
    enum
    {
        kCollectionFormat_Unspecified = 0,  /**< Not specified. */
        kCollectionFormat_csv,              /**< Comma-separated list. */
        kCollectionFormat_ssv,              /**< Space-separated list. */
        kCollectionFormat_tsv,              /**< Tab-separated list. */
        kCollectionFormat_pipes,            /**< Pipe-separated list. */
        kCollectionFormat_Mask = 7          /**< Collection type mask. */
    };

    /**
     * String conversion.
     *
     * The default implementation of is a wrapper around serializeAsJson().
     *
     * @returns IPRT status code.
     * @param   a_pDst      Pointer to the destionation string.
     * @param   a_fFlags    kCollectionFormat_xxx.
     */
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const;

    /**
     * String convertsion, naive variant.
     *
     * @returns String represenation.
     */
    RTCString toString() const;

    /**
     * Convert from (header) string value.
     *
     * The default implementation of is a wrapper around deserializeFromJson().
     *
     * @returns IPRT status code.
     * @param   a_rValue    The string value string to parse.
     * @param   a_pszName   Field name or similar.
     * @param   a_pErrInfo  Where to return additional error info.  Optional.
     * @param   a_fFlags    kCollectionFormat_xxx.
     */
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified);

    /**
     * Returns the object type name.
     */
    virtual const char *getType(void) = 0;

    /**
     * Factory method.
     * @returns Pointer to new object on success, NULL if out of memory.
     */
    typedef DECLCALLBACK(RTCRestObjectBase *) FNCREATEINSTANCE(void);
    /** Pointer to factory method. */
    typedef FNCREATEINSTANCE *PFNCREATEINSTANCE;
};


/**
 * Class wrapping 'bool'.
 */
class RTCRestBool : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestBool();
    /** Copy constructor. */
    RTCRestBool(RTCRestBool const &a_rThat);
    /** From value constructor. */
    RTCRestBool(bool fValue);
    /** Destructor. */
    virtual ~RTCRestBool();
    /** Copy assignment operator. */
    RTCRestBool &operator=(RTCRestBool const &a_rThat);

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    bool    m_fValue;
};


/**
 * Class wrapping 'int64_t'.
 */
class RTCRestInt64 : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestInt64();
    /** Copy constructor. */
    RTCRestInt64(RTCRestInt64 const &a_rThat);
    /** From value constructor. */
    RTCRestInt64(int64_t iValue);
    /** Destructor. */
    virtual ~RTCRestInt64();
    /** Copy assignment operator. */
    RTCRestInt64 &operator=(RTCRestInt64 const &a_rThat);

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int64_t m_iValue;
};


/**
 * Class wrapping 'int32_t'.
 */
class RTCRestInt32 : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestInt32();
    /** Copy constructor. */
    RTCRestInt32(RTCRestInt32 const &a_rThat);
    /** From value constructor. */
    RTCRestInt32(int32_t iValue);
    /** Destructor. */
    virtual ~RTCRestInt32();
    /** Copy assignment operator. */
    RTCRestInt32 &operator=(RTCRestInt32 const &a_rThat);

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int32_t m_iValue;
};


/**
 * Class wrapping 'int16_t'.
 */
class RTCRestInt16 : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestInt16();
    /** Copy constructor. */
    RTCRestInt16(RTCRestInt16 const &a_rThat);
    /** From value constructor. */
    RTCRestInt16(int16_t iValue);
    /** Destructor. */
    virtual ~RTCRestInt16();
    /** Copy assignment operator. */
    RTCRestInt16 &operator=(RTCRestInt16 const &a_rThat);

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int16_t m_iValue;
};


/**
 * Class wrapping 'double'.
 */
class RTCRestDouble : public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestDouble();
    /** Copy constructor. */
    RTCRestDouble(RTCRestDouble const &a_rThat);
    /** From value constructor. */
    RTCRestDouble(double rdValue);
    /** Destructor. */
    virtual ~RTCRestDouble();
    /** Copy assignment operator. */
    RTCRestDouble &operator=(RTCRestDouble const &a_rThat);

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    double m_rdValue;
};


/**
 * Class wrapping 'RTCString'.
 */
class RTCRestString : public RTCString, public RTCRestObjectBase
{
public:
    /** Default destructor. */
    RTCRestString();
    /** Copy constructor. */
    RTCRestString(RTCRestString const &a_rThat);
    /** From value constructor. */
    RTCRestString(RTCString const &a_rThat);
    /** From value constructor. */
    RTCRestString(const char *a_pszSrc);
    /** Destructor. */
    virtual ~RTCRestString();

    /* Overridden methods: */
    virtual void resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual const char *getType(void) RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);
};


/**
 * Limited array class.
 */
template<class ElementType> class RTCRestArray : public RTCRestObjectBase
{
public:
    RTCRestArray() {};
    ~RTCRestArray() {};
/** @todo more later. */

    virtual void resetToDefault() RT_OVERRIDE
    {
    }

    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE
    {
        RT_NOREF(a_rDst);
        return a_rDst;
    }

    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE
    {
        RT_NOREF(a_rCursor);
        return VERR_NOT_IMPLEMENTED;
    }

    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE
    {
        RT_NOREF(a_rValue, a_pszName, a_pErrInfo, a_fFlags);
        return VERR_NOT_IMPLEMENTED;
    }

    virtual const char *getType(void) RT_OVERRIDE
    {
        return "RTCRestArray<ElementType>";
    }

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void)
    {
        return new RTCRestArray<ElementType>();
    }

    /** Factory method for elements. */
    static DECLCALLBACK(RTCRestObjectBase *) createElementInstance(void)
    {
        return new ElementType();
    }
};


/**
 * Limited map class.
 */
template<class ElementType> class RTCRestStringMap : public RTCRestObjectBase
{
public:
    RTCRestStringMap() {};
    ~RTCRestStringMap() {};
/** @todo more later. */

    virtual void resetToDefault() RT_OVERRIDE
    {
    }

    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE
    {
        RT_NOREF(a_rDst);
        return a_rDst;
    }

    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE
    {
        RT_NOREF(a_rCursor);
        return VERR_NOT_IMPLEMENTED;
    }

    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE
    {
        RT_NOREF(a_rValue, a_pszName, a_pErrInfo, a_fFlags);
        return VERR_NOT_IMPLEMENTED;
    }

    virtual const char *getType(void) RT_OVERRIDE
    {
        return "RTCRestStringMap<ElementType>";
    }

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void)
    {
        return new RTCRestStringMap<ElementType>();
    }

    /** Factory method for elements. */
    static DECLCALLBACK(RTCRestObjectBase *) createElementInstance(void)
    {
        return new ElementType();
    }
};


/**
 * Base class for REST client requests.
 */
class RTCRestClientRequestBase
{
public:
    RTCRestClientRequestBase();
    virtual ~RTCRestClientRequestBase();

    /**
     * Reset all members to default values.
     */
    virtual void resetToDefault() = 0;

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

protected:
    /** Path replacement entry. */
    typedef struct
    {
        const char                 *pszName;    /**< The name string to replace (including {}). */
        size_t                      cchName;    /**< Length of pszName. */
        RTCRestObjectBase const    *pObj;       /**< Pointer to the parameter object. */
        size_t                      offName;    /**< Maintained by worker. */
        uint32_t                    fFlags;     /**< The toString flags. */
    } PATHREPLACEENTRY;

    /**
     *
     * @returns IPRT status code
     * @param   a_pStrPath          The destination path.
     * @param   a_pszPathTemplate   The path template string.
     * @param   a_cchPathTemplate   The length of the path template string.
     * @param   a_paPathParams      The path parameter descriptors.
     * @param   a_cPathParams       Number of path parameters.
     */
    int doPathParameters(RTCString *a_pStrPath, const char *a_pszPathTemplate, size_t a_cchPathTemplate,
                         PATHREPLACEENTRY *a_paPathParams, size_t a_cPathParams) const;
};


/**
 * Base class for REST client responses.
 */
class RTCRestClientResponseBase
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
    PCRTERRINFO  getErrIfno(void) const { return m_pErrInfo; }

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

    PRTERRINFO  getErrInfo(void);
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
        /** Array collection, i.e. the heade field may appear more than once. */
        kHdrField_ArrayCollection = RT_BIT_32(25)
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
     * @param   a_paFieldsDesc      Pointer to an array of field descriptors.
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
class RTCRestClientApiBase
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
     * @param   a_rRequest      Reference to the request object.
     * @param   a_enmHttpMethod The HTTP request method.
     * @param   a_pResponse     Pointer to the response object.
     */
    virtual void doCall(RTCRestClientRequestBase const &a_rRequest, RTHTTPMETHOD a_enmHttpMethod,
                        RTCRestClientResponseBase *a_pResponse);

};

/** @} */

#endif

