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

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/json.h>
#include <iprt/stdarg.h>
#include <iprt/time.h>
#include <iprt/cpp/ministring.h>


/** @defgroup grp_rt_cpp_restbase   C++ Representational State Transfer (REST) Base Classes.
 * @ingroup grp_rt_cpp
 * @{
 */



/**
 * Abstract base class for serializing data objects.
 */
class RT_DECL_CLASS RTCRestOutputBase
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
    inline unsigned setIndent(unsigned uIndent)
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
    inline unsigned incrementIndent()
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
class RT_DECL_CLASS RTCRestOutputToString : public RTCRestOutputBase
{
public:
    /**
     * Creates an instance that appends to @a a_pDst.
     * @param   a_pDst      Pointer to the destination string object.
     *                      NULL is not accepted and will assert.
     * @param   a_fAppend   Whether to append to the current string value, or
     *                      nuke the string content before starting the output.
     */
    RTCRestOutputToString(RTCString *a_pDst, bool a_fAppend = false);
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
struct RT_DECL_CLASS RTCRestJsonCursor
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
class RT_DECL_CLASS RTCRestJsonPrimaryCursor
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
 *
 * The only information this keeps is the null indicator.
 */
class RT_DECL_CLASS RTCRestObjectBase
{
public:
    RTCRestObjectBase();
    RTCRestObjectBase(RTCRestObjectBase const &a_rThat);
    virtual ~RTCRestObjectBase();

    /**
     * Tests if the object is @a null.
     * @returns true if null, false if not.
     */
    inline bool isNull(void) const { return m_fNullIndicator; };

    /**
     * Sets the object to @a null and fills it with defaults.
     * @returns IPRT status code (from resetToDefault).
     */
    virtual int setNull(void);

    /**
     * Sets the object to not-null state (i.e. undoes setNull()).
     * @remarks Only really important for strings.
     */
    virtual void setNotNull(void);

    /**
     * Resets the object to all default values.
     * @returns IPRT status code.
     */
    virtual int resetToDefault() = 0;

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
     *
     * @note When adding collection format types, make sure to also
     *       update RTCRestArrayBase::toString().
     * @note Bit 24 is reserved (for kHdrField_MapCollection).
     */
    enum
    {
        kCollectionFormat_Unspecified = 0,  /**< Not specified. */
        kCollectionFormat_csv,              /**< Comma-separated list. */
        kCollectionFormat_ssv,              /**< Space-separated list. */
        kCollectionFormat_tsv,              /**< Tab-separated list. */
        kCollectionFormat_pipes,            /**< Pipe-separated list. */
        kCollectionFormat_multi,            /**< Special collection type that must be handled by caller of toString. */
        kCollectionFormat_Mask = 7,         /**< Collection type mask. */

        kToString_Append = 8                /**< Append to the string (rather than assigning). */
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

    /** Type classification */
    typedef enum kTypeClass
    {
        kTypeClass_Invalid = 0,
        kTypeClass_Bool,                /**< Primitive: bool. */
        kTypeClass_Int64,               /**< Primitive: int64_t. */
        kTypeClass_Int32,               /**< Primitive: int32_t. */
        kTypeClass_Int16,               /**< Primitive: int16_t. */
        kTypeClass_Double,              /**< Primitive: double. */
        kTypeClass_String,              /**< Primitive: string. */
        kTypeClass_Date,                /**< Date. */
        kTypeClass_Uuid,                /**< UUID. */
        kTypeClass_Binary,              /**< Binary blob. */
        kTypeClass_Object,              /**< Object (any kind of data model object). */
        kTypeClass_Array,               /**< Array (containing any kind of object). */
        kTypeClass_StringMap,           /**< String map (containing any kind of object). */
        kTypeClass_StringEnum           /**< String enum. */
    } kTypeClass;

    /**
     * Returns the object type class.
     */
    virtual kTypeClass typeClass(void) const;

    /**
     * Returns the object type name.
     */
    virtual const char *typeName(void) const = 0;

protected:
    /** Null indicator.
     * @remarks The null values could be mapped onto C/C++ NULL pointer values,
     *          with the consequence that all data members in objects and such would
     *          have had to been allocated individually, even simple @a bool members.
     *          Given that we're overly paranoid about heap allocations (std::bad_alloc),
     *          it's more fitting to use a null indicator for us.
     */
    bool    m_fNullIndicator;
};


/**
 * Class wrapping 'bool'.
 */
class RT_DECL_CLASS RTCRestBool : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestBool();
    /** Copy constructor. */
    RTCRestBool(RTCRestBool const &a_rThat);
    /** From value constructor. */
    RTCRestBool(bool fValue);
    /** Destructor. */
    virtual ~RTCRestBool();
    /** Copy assignment operator. */
    RTCRestBool &operator=(RTCRestBool const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestBool const &a_rThat);
    /** Assign value and clear null indicator. */
    void assignValue(bool a_fValue);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    bool    m_fValue;
};


/**
 * Class wrapping 'int64_t'.
 */
class RT_DECL_CLASS RTCRestInt64 : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestInt64();
    /** Copy constructor. */
    RTCRestInt64(RTCRestInt64 const &a_rThat);
    /** From value constructor. */
    RTCRestInt64(int64_t a_iValue);
    /** Destructor. */
    virtual ~RTCRestInt64();
    /** Copy assignment operator. */
    RTCRestInt64 &operator=(RTCRestInt64 const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestInt64 const &a_rThat);
    /** Assign value and clear null indicator. */
    void assignValue(int64_t a_iValue);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int64_t m_iValue;
};


/**
 * Class wrapping 'int32_t'.
 */
class RT_DECL_CLASS RTCRestInt32 : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestInt32();
    /** Copy constructor. */
    RTCRestInt32(RTCRestInt32 const &a_rThat);
    /** From value constructor. */
    RTCRestInt32(int32_t iValue);
    /** Destructor. */
    virtual ~RTCRestInt32();
    /** Copy assignment operator. */
    RTCRestInt32 &operator=(RTCRestInt32 const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestInt32 const &a_rThat);
    /** Assign value and clear null indicator. */
    void assignValue(int32_t a_iValue);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int32_t m_iValue;
};


/**
 * Class wrapping 'int16_t'.
 */
class RT_DECL_CLASS RTCRestInt16 : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestInt16();
    /** Copy constructor. */
    RTCRestInt16(RTCRestInt16 const &a_rThat);
    /** From value constructor. */
    RTCRestInt16(int16_t iValue);
    /** Destructor. */
    virtual ~RTCRestInt16();
    /** Copy assignment operator. */
    RTCRestInt16 &operator=(RTCRestInt16 const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestInt16 const &a_rThat);
    /** Assign value and clear null indicator. */
    void assignValue(int16_t a_iValue);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    int16_t m_iValue;
};


/**
 * Class wrapping 'double'.
 */
class RT_DECL_CLASS RTCRestDouble : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestDouble();
    /** Copy constructor. */
    RTCRestDouble(RTCRestDouble const &a_rThat);
    /** From value constructor. */
    RTCRestDouble(double rdValue);
    /** Destructor. */
    virtual ~RTCRestDouble();
    /** Copy assignment operator. */
    RTCRestDouble &operator=(RTCRestDouble const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestDouble const &a_rThat);
    /** Assign value and clear null indicator. */
    void assignValue(double a_rdValue);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

public:
    /** The value. */
    double m_rdValue;
};


/**
 * Class wrapping 'RTCString'.
 */
class RT_DECL_CLASS RTCRestString : public RTCString, public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestString();
    /** Destructor. */
    virtual ~RTCRestString();

    /** Copy constructor. */
    RTCRestString(RTCRestString const &a_rThat);
    /** From value constructor. */
    RTCRestString(RTCString const &a_rThat);
    /** From value constructor. */
    RTCRestString(const char *a_pszSrc);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestString const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCString const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(const char *a_pszThat);

    /* Overridden methods: */
    virtual int setNull(void) RT_OVERRIDE; /* (ambigious, so overrider it to make sure.) */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

    /** @name RTCString assignment methods we need to replace to manage the null indicator
     * @{ */
    int assignNoThrow(const RTCString &a_rSrc) RT_NOEXCEPT;
    int assignNoThrow(const char *a_pszSrc) RT_NOEXCEPT;
    int assignNoThrow(const RTCString &a_rSrc, size_t a_offSrc, size_t a_cchSrc = npos) RT_NOEXCEPT;
    int assignNoThrow(const char *a_pszSrc, size_t a_cchSrc) RT_NOEXCEPT;
    int assignNoThrow(size_t a_cTimes, char a_ch) RT_NOEXCEPT;
    int printfNoThrow(const char *pszFormat, ...) RT_NOEXCEPT RT_IPRT_FORMAT_ATTR(1, 2);
    int printfVNoThrow(const char *pszFormat, va_list va) RT_NOEXCEPT RT_IPRT_FORMAT_ATTR(1, 0);
    RTCRestString &operator=(const char *a_pcsz);
    RTCRestString &operator=(const RTCString &a_rThat);
    RTCRestString &operator=(const RTCRestString &a_rThat);
    RTCRestString &assign(const RTCString &a_rSrc);
    RTCRestString &assign(const char *a_pszSrc);
    RTCRestString &assign(const RTCString &a_rSrc, size_t a_offSrc, size_t a_cchSrc = npos);
    RTCRestString &assign(const char *a_pszSrc, size_t a_cchSrc);
    RTCRestString &assign(size_t a_cTimes, char a_ch);
    RTCRestString &printf(const char *pszFormat, ...) RT_IPRT_FORMAT_ATTR(1, 2);
    RTCRestString &printfV(const char *pszFormat, va_list va) RT_IPRT_FORMAT_ATTR(1, 0);
    /** @} */
};


/**
 * Date class.
 *
 * There are numerous ways of formatting a timestamp and the specifications
 * we're currently working with doesn't have a way of telling it seems.
 * Thus, decoding need to have fail safes built in so the user can give hints.
 * The formatting likewise needs to be told which format to use by the user.
 *
 * Two side-effects of the format stuff is that the default constructor creates
 * an object that is null, and resetToDefault will do the same bug leave the
 * format as a hint.
 */
class RT_DECL_CLASS RTCRestDate : public RTCRestObjectBase
{
public:
    /** Default constructor.
     * @note The result is a null-object.   */
    RTCRestDate();
    /** Copy constructor. */
    RTCRestDate(RTCRestDate const &a_rThat);
    /** Destructor. */
    virtual ~RTCRestDate();
    /** Copy assignment operator. */
    RTCRestDate &operator=(RTCRestDate const &a_rThat);
    /** Safe copy assignment method. */
    int assignCopy(RTCRestDate const &a_rThat);

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = 0) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

    /** Date formats. */
    typedef enum
    {
        kFormat_Invalid = 0,
        kFormat_Rfc2822,            /**< Format it according to RFC-2822. */
        kFormat_Rfc7131,            /**< Format it according to RFC-7131 (HTTP). */
        kFormat_Rfc3339,            /**< Format it according to RFC-3339 (ISO-8601) (no fraction). */
        kFormat_Rfc3339_Fraction_2, /**< Format it according to RFC-3339 (ISO-8601) with two digit fraction (hundreths). */
        kFormat_Rfc3339_Fraction_3, /**< Format it according to RFC-3339 (ISO-8601) with three digit fraction (milliseconds). */
        kFormat_Rfc3339_Fraction_6, /**< Format it according to RFC-3339 (ISO-8601) with six digit fraction (microseconds). */
        kFormat_Rfc3339_Fraction_9, /**< Format it according to RFC-3339 (ISO-8601) with nine digit fraction (nanoseconds). */
        kFormat_End
    } kFormat;

    /**
     * Assigns the value, formats it as a string and clears the null indicator.
     *
     * @returns VINF_SUCCESS, VERR_NO_STR_MEMORY or VERR_INVALID_PARAMETER.
     * @param   a_pTimeSpec     The time spec to set.
     * @param   a_enmFormat     The date format to use when formatting it.
     */
    int assignValue(PCRTTIMESPEC a_pTimeSpec, kFormat a_enmFormat);
    int assignValueRfc2822(PCRTTIMESPEC a_pTimeSpec); /**< Convenience method for email/whatnot. */
    int assignValueRfc7131(PCRTTIMESPEC a_pTimeSpec); /**< Convenience method for HTTP date. */
    int assignValueRfc3339(PCRTTIMESPEC a_pTimeSpec); /**< Convenience method for ISO-8601 timstamp. */

    /**
     * Assigns the current UTC time and clears the null indicator .
     *
     * @returns VINF_SUCCESS, VERR_NO_STR_MEMORY or VERR_INVALID_PARAMETER.
     * @returns VINF_SUCCESS or VERR_NO_STR_MEMORY.
     * @param   a_enmFormat     The date format to use when formatting it.
     */
    int assignNow(kFormat a_enmFormat);
    int assignNowRfc2822(); /**< Convenience method for email/whatnot. */
    int assignNowRfc7131(); /**< Convenience method for HTTP date. */
    int assignNowRfc3339(); /**< Convenience method for ISO-8601 timstamp. */

    /**
     * Sets the format to help deal with decoding issues.
     *
     * This can also be used to change the date format for an okay timespec.
     * @returns IPRT status code.
     * @param   a_enmFormat     The date format to try/set.
     */
    int setFormat(kFormat a_enmFormat);

    /** Check if the value is okay (m_TimeSpec & m_Exploded). */
    inline bool              isOkay() const             { return m_fTimeSpecOkay; }
    /** Get the timespec value. */
    inline RTTIMESPEC const &getTimeSpec() const        { return m_TimeSpec; }
    /** Get the exploded time. */
    inline RTTIME const     &getExploded() const        { return m_Exploded; }
    /** Gets the format. */
    inline kFormat           getFormat() const          { return m_enmFormat; }
    /** Get the formatted/raw string value. */
    inline RTCString const  &getString() const          { return m_strFormatted; }

    /** Get nanoseconds since unix epoch. */
    inline int64_t           getEpochNano() const       { return RTTimeSpecGetNano(&m_TimeSpec); }
    /** Get seconds since unix epoch. */
    inline int64_t           getEpochSeconds() const    { return RTTimeSpecGetSeconds(&m_TimeSpec); }
    /** Checks if UTC time. */
    inline bool              isUtc() const              { return (m_Exploded.fFlags & RTTIME_FLAGS_TYPE_MASK) != RTTIME_FLAGS_TYPE_LOCAL; }
    /** Checks if local time. */
    inline bool              isLocal() const            { return (m_Exploded.fFlags & RTTIME_FLAGS_TYPE_MASK) == RTTIME_FLAGS_TYPE_LOCAL; }

protected:
    /** The value. */
    RTTIMESPEC  m_TimeSpec;
    /** The exploded time value. */
    RTTIME      m_Exploded;
    /** Set if m_TimeSpec is okay, consult m_strFormatted if not. */
    bool        m_fTimeSpecOkay;
    /** The format / format hint. */
    kFormat     m_enmFormat;
    /** The formatted date string.
     * This will be the raw input string for a deserialized value, where as for
     * a value set by the user it will be the formatted value. */
    RTCString   m_strFormatted;

    /**
     * Explodes and formats the m_TimeSpec value.
     *
     * Sets m_Exploded, m_strFormatted, m_fTimeSpecOkay, and m_enmFormat, clears m_fNullIndicator.
     *
     * @returns VINF_SUCCESS or VERR_NO_STR_MEMORY.
     * @param   a_enmFormat The format to use.
     */
    int explodeAndFormat(kFormat a_enmFormat);

    /**
     * Formats the m_Exploded value.
     *
     * Sets m_strFormatted, m_fTimeSpecOkay, and m_enmFormat, clears m_fNullIndicator.
     *
     * @returns VINF_SUCCESS or VERR_NO_STR_MEMORY.
     * @param   a_enmFormat The format to use.
     */
    int format(kFormat a_enmFormat);

    /**
     * Internal worker that attempts to decode m_strFormatted.
     *
     * Sets m_fTimeSpecOkay.
     *
     * @returns IPRT status code.
     * @param   enmFormat   Specific format to try, kFormat_Invalid (default) to try guess it.
     */
    int decodeFormattedString(kFormat enmFormat = kFormat_Invalid);
};


/** We should provide a proper UUID class eventually.  Currently it is not used. */
typedef RTCRestString RTCRestUuid;


/**
 * String enum base class.
 */
class RT_DECL_CLASS RTCRestStringEnumBase : public RTCRestObjectBase
{
public:
    /** Enum map entry. */
    typedef struct ENUMMAPENTRY
    {
        const char *pszName;
        uint32_t    cchName;
        int32_t     iValue;
    } ENUMMAPENTRY;

    /** Default constructor. */
    RTCRestStringEnumBase();
    /** Destructor. */
    virtual ~RTCRestStringEnumBase();

    /** Copy constructor. */
    RTCRestStringEnumBase(RTCRestStringEnumBase const &a_rThat);
    /** Copy assignment operator. */
    RTCRestStringEnumBase &operator=(RTCRestStringEnumBase const &a_rThat);

    /** Safe copy assignment method. */
    int assignCopy(RTCRestStringEnumBase const &a_rThat);
    /** Safe copy assignment method. */
    inline int assignCopy(RTCString const &a_rThat)    { return setByString(a_rThat); }
    /** Safe copy assignment method. */
    inline int assignCopy(const char *a_pszThat)       { return setByString(a_pszThat); }

    /* Overridden methods: */
    virtual int resetToDefault() RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;

    /**
     * Sets the value given a C-string value.
     *
     * @retval VINF_SUCCESS on success.
     * @retval VWRN_NOT_FOUND if not mappable to enum value.
     * @retval VERR_NO_STR_MEMORY if not mappable and we're out of memory.
     * @param   a_pszValue      The string value.
     * @param   a_cchValue      The string value length.  Optional.
     */
    int setByString(const char *a_pszValue, size_t a_cchValue = RTSTR_MAX);

    /**
     * Sets the value given a string value.
     *
     * @retval VINF_SUCCESS on success.
     * @retval VWRN_NOT_FOUND if not mappable to enum value.
     * @retval VERR_NO_STR_MEMORY if not mappable and we're out of memory.
     * @param   a_rValue        The string value.
     */
    int setByString(RTCString const &a_rValue);

    /**
     * Gets the string value.
     */
    const char *getString() const;

    /** Maps the given string value to an enum. */
    int stringToEnum(const char *a_pszValue, size_t a_cchValue = RTSTR_MAX);
    /** Maps the given string value to an enum. */
    int stringToEnum(RTCString const &a_rStrValue);
    /** Maps the given string value to an enum. */
    const char *enumToString(int a_iEnumValue, size_t *a_pcchString);


protected:
    /** The enum value. */
    int                 m_iEnumValue;
    /** The string value if not a match. */
    RTCString           m_strValue;

    /**
     * Worker for setting the object to the given enum value.
     *
     * @retval  true on success.
     * @retval  false if a_iEnumValue can't be translated.
     * @param   a_iEnumValue    The enum value to set.
     */
    bool                setWorker(int a_iEnumValue);

    /**
     * Gets the mapping table.
     *
     * @returns Pointer to the translation table.
     * @param   pcEntries   Where to return the translation table size.
     */
    virtual ENUMMAPENTRY const *getMappingTable(size_t *pcEntries) const = 0;
};


/**
 * Class for handling binary blobs (strings).
 *
 * There are specializations of this class for body parameters and responses,
 * see RTCRestBinaryParameter and RTCRestBinaryResponse.
 */
class RT_DECL_CLASS RTCRestBinary : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestBinary();
    /** Destructor. */
    virtual ~RTCRestBinary();

    /** Safe copy assignment method. */
    virtual int assignCopy(RTCRestBinary const &a_rThat);
    /** Safe buffer copy method. */
    virtual int assignCopy(void const *a_pvData, size_t a_cbData);

    /** Use the specified data buffer directly. */
    virtual int assignReadOnly(void const *a_pvData, size_t a_cbData);
    /** Use the specified data buffer directly. */
    virtual int assignWriteable(void *a_pvBuf, size_t a_cbBuf);
    /** Frees the data held by the object and resets it default state. */
    virtual void freeData();

    /** Returns a pointer to the data blob. */
    inline const uint8_t  *getPtr()  const { return m_pbData; }
    /** Gets the size of the data. */
    inline size_t          getSize() const { return m_cbData; }

    /* Overridden methods: */
    virtual int setNull(void) RT_OVERRIDE;
    virtual int resetToDefault(void) RT_OVERRIDE;
    virtual RTCRestOutputBase &serializeAsJson(RTCRestOutputBase &a_rDst) const RT_OVERRIDE;
    virtual int deserializeFromJson(RTCRestJsonCursor const &a_rCursor) RT_OVERRIDE;
    virtual int toString(RTCString *a_pDst, uint32_t a_fFlags = kCollectionFormat_Unspecified) const RT_OVERRIDE;
    virtual int fromString(RTCString const &a_rValue, const char *a_pszName, PRTERRINFO a_pErrInfo = NULL,
                           uint32_t a_fFlags = kCollectionFormat_Unspecified) RT_OVERRIDE;
    virtual kTypeClass typeClass(void) const RT_OVERRIDE;
    virtual const char *typeName(void) const RT_OVERRIDE;

    /** Factory method. */
    static DECLCALLBACK(RTCRestObjectBase *) createInstance(void);

protected:
    /** Pointer to data blob. */
    uint8_t    *m_pbData;
    /** Amount of valid data in the blob. */
    size_t      m_cbData;
    /** Number of bytes allocated for the m_pbData buffer. */
    size_t      m_cbAllocated;
    /** Set if the data is freeable, only ever clear if user data. */
    bool        m_fFreeable;
    /** Set if the data blob is readonly user provided data. */
    bool        m_fReadOnly;

private:
    /* No copy constructor or copy assignment: */
    RTCRestBinary(RTCRestBinary const &a_rThat);
    RTCRestBinary &operator=(RTCRestBinary const &a_rThat);
};


/** @} */

#endif

