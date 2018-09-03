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
    bool isNull(void) const { return m_fNullIndicator; };

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
        kTypeClass_Int64,               /**< Primitive: bool. */
        kTypeClass_Int32,               /**< Primitive: bool. */
        kTypeClass_Int16,               /**< Primitive: bool. */
        kTypeClass_Double,              /**< Primitive: bool. */
        kTypeClass_String,              /**< Primitive: bool. */
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

    /**
     * Factory method.
     * @returns Pointer to new object on success, NULL if out of memory.
     */
    typedef DECLCALLBACK(RTCRestObjectBase *) FNCREATEINSTANCE(void);
    /** Pointer to factory method. */
    typedef FNCREATEINSTANCE *PFNCREATEINSTANCE;

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
};


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
    int assignCopy(RTCString const &a_rThat)    { return setByString(a_rThat); }
    /** Safe copy assignment method. */
    int assignCopy(const char *a_pszThat)       { return setByString(a_pszThat); }

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
 * Dynamic REST object.
 *
 * @todo figure this one out. it's possible this is only used in maps and
 *       could be a specialized map implementation.
 */
class /*RT_DECL_CLASS*/ RTCRestObject : public RTCRestObjectBase
{
public:
    /** Default constructor. */
    RTCRestObject();
    /** Destructor. */
    virtual ~RTCRestObject();

    /** Copy constructor. */
    RTCRestObject(RTCRestObject const &a_rThat);
    /** Copy assignment operator. */
    RTCRestObject &operator=(RTCRestObject const &a_rThat);
    /** Safe Safe copy assignment method. */
    int assignCopy(RTCRestObject const &a_rThat);

    /* Overridden methods: */
    virtual int setNull(void) RT_OVERRIDE;
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

protected:
    /** @todo figure out the value stuff here later... */
};


/** @} */

#endif

