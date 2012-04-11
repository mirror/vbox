/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer - Smart string classes declaration.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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

#ifndef ___VBox_com_string_h
#define ___VBox_com_string_h

/* Make sure all the stdint.h macros are included - must come first! */
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif
#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif

#if defined(VBOX_WITH_XPCOM)
# include <nsMemory.h>
#endif

#include "VBox/com/defs.h"
#include "VBox/com/assert.h"

#include <iprt/mem.h>
#include <iprt/cpp/ministring.h>

namespace com
{

class Utf8Str;

// global constant in glue/string.cpp that represents an empty BSTR
extern const BSTR g_bstrEmpty;

/**
 *  String class used universally in Main for COM-style Utf-16 strings.
 *
 * Unfortunately COM on Windows uses UTF-16 everywhere, requiring conversions
 * back and forth since most of VirtualBox and our libraries use UTF-8.
 *
 * To make things more obscure, on Windows, a COM-style BSTR is not just a
 * pointer to a null-terminated wide character array, but the four bytes (32
 * bits) BEFORE the memory that the pointer points to are a length DWORD. One
 * must therefore avoid pointer arithmetic and always use SysAllocString and
 * the like to deal with BSTR pointers, which manage that DWORD correctly.
 *
 * For platforms other than Windows, we provide our own versions of the Sys*
 * functions in Main/xpcom/helpers.cpp which do NOT use length prefixes though
 * to be compatible with how XPCOM allocates string parameters to public
 * functions.
 *
 * The Bstr class hides all this handling behind a std::string-like interface
 * and also provides automatic conversions to RTCString and Utf8Str instances.
 *
 * The one advantage of using the SysString* routines is that this makes it
 * possible to use it as a type of member variables of COM/XPCOM components and
 * pass their values to callers through component methods' output parameters
 * using the #cloneTo() operation.  Also, the class can adopt (take ownership
 * of) string buffers returned in output parameters of COM methods using the
 * #asOutParam() operation and correctly free them afterwards.
 *
 * Starting with VirtualBox 3.2, like Utf8Str, Bstr no longer differentiates
 * between NULL strings and empty strings. In other words, Bstr("") and
 * Bstr(NULL) behave the same. In both cases, Bstr allocates no memory,
 * reports a zero length and zero allocated bytes for both, and returns an
 * empty C wide string from raw().
 *
 * @note    All Bstr methods ASSUMES valid UTF-16 or UTF-8 input strings.
 *          The VirtualBox policy in this regard is to validate strings coming
 *          from external sources before passing them to Bstr or Utf8Str.
 */
class Bstr
{
public:

    Bstr()
        : m_bstr(NULL)
    { }

    Bstr(const Bstr &that)
    {
        copyFrom((const OLECHAR *)that.m_bstr);
    }

    Bstr(CBSTR that)
    {
        copyFrom((const OLECHAR *)that);
    }

#if defined(VBOX_WITH_XPCOM)
    Bstr(const wchar_t *that)
    {
        AssertCompile(sizeof(wchar_t) == sizeof(OLECHAR));
        copyFrom((const OLECHAR *)that);
    }
#endif

    Bstr(const RTCString &that)
    {
        copyFrom(that.c_str());
    }

    Bstr(const char *that)
    {
        copyFrom(that);
    }

    Bstr(const char *a_pThat, size_t a_cchMax)
    {
        copyFromN(a_pThat, a_cchMax);
    }

    ~Bstr()
    {
        setNull();
    }

    Bstr& operator=(const Bstr &that)
    {
        cleanup();
        copyFrom((const OLECHAR *)that.m_bstr);
        return *this;
    }

    Bstr& operator=(CBSTR that)
    {
        cleanup();
        copyFrom((const OLECHAR *)that);
        return *this;
    }

#if defined(VBOX_WITH_XPCOM)
    Bstr& operator=(const wchar_t *that)
    {
        cleanup();
        copyFrom((const OLECHAR *)that);
        return *this;
    }
#endif

    Bstr& setNull()
    {
        cleanup();
        return *this;
    }

#ifdef _MSC_VER
# if _MSC_VER >= 1400
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
# endif
#else
    RTMEMEF_NEW_AND_DELETE_OPERATORS();
#endif

    /** Case sensitivity selector. */
    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };

    /**
     * Compares the member string to str.
     * @param str
     * @param cs Whether comparison should be case-sensitive.
     * @return
     */
    int compare(CBSTR str, CaseSensitivity cs = CaseSensitive) const
    {
        if (cs == CaseSensitive)
            return ::RTUtf16Cmp((PRTUTF16)m_bstr, (PRTUTF16)str);
        return ::RTUtf16LocaleICmp((PRTUTF16)m_bstr, (PRTUTF16)str);
    }

    int compare(BSTR str, CaseSensitivity cs = CaseSensitive) const
    {
        return compare((CBSTR)str, cs);
    }

    int compare(const Bstr &that, CaseSensitivity cs = CaseSensitive) const
    {
        return compare(that.m_bstr, cs);
    }

    bool operator==(const Bstr &that) const { return !compare(that.m_bstr); }
    bool operator!=(const Bstr &that) const { return !!compare(that.m_bstr); }
    bool operator==(CBSTR that) const { return !compare(that); }
    bool operator==(BSTR that) const { return !compare(that); }

    bool operator!=(CBSTR that) const { return !!compare(that); }
    bool operator!=(BSTR that) const { return !!compare(that); }
    bool operator<(const Bstr &that) const { return compare(that.m_bstr) < 0; }
    bool operator<(CBSTR that) const { return compare(that) < 0; }
    bool operator<(BSTR that) const { return compare(that) < 0; }

    /**
     * Returns true if the member string has no length.
     * This is true for instances created from both NULL and "" input strings.
     *
     * @note Always use this method to check if an instance is empty. Do not
     * use length() because that may need to run through the entire string
     * (Bstr does not cache string lengths).
     */
    bool isEmpty() const { return m_bstr == NULL || *m_bstr == 0; }

    /**
     * Returns true if the member string has a length of one or more.
     *
     * @returns true if not empty, false if empty (NULL or "").
     */
    bool isNotEmpty() const { return m_bstr != NULL && *m_bstr != 0; }

    size_t length() const { return isEmpty() ? 0 : ::RTUtf16Len((PRTUTF16)m_bstr); }

#if defined(VBOX_WITH_XPCOM)
    /**
     *  Returns a pointer to the raw member UTF-16 string. If the member string is empty,
     *  returns a pointer to a global variable containing an empty BSTR with a proper zero
     *  length prefix so that Windows is happy.
     */
    CBSTR raw() const
    {
        if (m_bstr)
            return m_bstr;

        return g_bstrEmpty;
    }
#else
    /**
     *  Windows-only hack, as the automatically generated headers use BSTR.
     *  So if we don't want to cast like crazy we have to be more loose than
     *  on XPCOM.
     *
     *  Returns a pointer to the raw member UTF-16 string. If the member string is empty,
     *  returns a pointer to a global variable containing an empty BSTR with a proper zero
     *  length prefix so that Windows is happy.
     */
    BSTR raw() const
    {
        if (m_bstr)
            return m_bstr;

        return g_bstrEmpty;
    }
#endif

    /**
     *  Returns a non-const raw pointer that allows to modify the string directly.
     *  As opposed to raw(), this DOES return NULL if the member string is empty
     *  because we cannot return a mutable pointer to the global variable with the
     *  empty string.
     *
     *  @warning
     *      Be sure not to modify data beyond the allocated memory! The
     *      guaranteed size of the allocated memory is at least #length()
     *      bytes after creation and after every assignment operation.
     */
    BSTR mutableRaw() { return m_bstr; }

    /**
     *  Intended to assign copies of instances to |BSTR| out parameters from
     *  within the interface method. Transfers the ownership of the duplicated
     *  string to the caller.
     *
     *  If the member string is empty, this allocates an empty BSTR in *pstr
     *  (i.e. makes it point to a new buffer with a null byte).
     *
     *  @deprecated Use cloneToEx instead to avoid throwing exceptions.
     */
    void cloneTo(BSTR *pstr) const
    {
        if (pstr)
        {
            *pstr = ::SysAllocString((const OLECHAR *)raw());       // raw() returns a pointer to "" if empty
#ifdef RT_EXCEPTIONS_ENABLED
            if (!*pstr)
                throw std::bad_alloc();
#endif
        }
    }

    /**
     *  A version of cloneTo that does not throw any out of memory exceptions, but
     *  returns E_OUTOFMEMORY intead.
     *  @returns S_OK or E_OUTOFMEMORY.
     */
    HRESULT cloneToEx(BSTR *pstr) const
    {
        if (!pstr)
            return S_OK;
        *pstr = ::SysAllocString((const OLECHAR *)raw());       // raw() returns a pointer to "" if empty
        return pstr ? S_OK : E_OUTOFMEMORY;
    }

    /**
     *  Intended to assign instances to |BSTR| out parameters from within the
     *  interface method. Transfers the ownership of the original string to the
     *  caller and resets the instance to null.
     *
     *  As opposed to cloneTo(), this method doesn't create a copy of the
     *  string.
     *
     *  If the member string is empty, this allocates an empty BSTR in *pstr
     *  (i.e. makes it point to a new buffer with a null byte).
     *
     * @param   pbstrDst        The BSTR variable to detach the string to.
     *
     * @throws  std::bad_alloc if we failed to allocate a new empty string.
     */
    void detachTo(BSTR *pbstrDst)
    {
        if (m_bstr)
        {
            *pbstrDst = m_bstr;
            m_bstr = NULL;
        }
        else
        {
            // allocate null BSTR
            *pbstrDst = ::SysAllocString((const OLECHAR *)g_bstrEmpty);
#ifdef RT_EXCEPTIONS_ENABLED
            if (!*pbstrDst)
                throw std::bad_alloc();
#endif
        }
    }

    /**
     *  A version of detachTo that does not throw exceptions on out-of-memory
     *  conditions, but instead returns E_OUTOFMEMORY.
     *
     * @param   pbstrDst        The BSTR variable to detach the string to.
     * @returns S_OK or E_OUTOFMEMORY.
     */
    HRESULT detachToEx(BSTR *pbstrDst)
    {
        if (m_bstr)
        {
            *pbstrDst = m_bstr;
            m_bstr = NULL;
        }
        else
        {
            // allocate null BSTR
            *pbstrDst = ::SysAllocString((const OLECHAR *)g_bstrEmpty);
            if (!*pbstrDst)
                return E_OUTOFMEMORY;
        }
        return S_OK;
    }

    /**
     *  Intended to pass instances as |BSTR| out parameters to methods.
     *  Takes the ownership of the returned data.
     */
    BSTR *asOutParam()
    {
        cleanup();
        return &m_bstr;
    }

    /**
     *  Static immutable empty-string object. May be used for comparison purposes.
     */
    static const Bstr Empty;

protected:

    void cleanup()
    {
        if (m_bstr)
        {
            ::SysFreeString(m_bstr);
            m_bstr = NULL;
        }
    }

    /**
     * Protected internal helper to copy a string. This ignores the previous object
     * state, so either call this from a constructor or call cleanup() first.
     *
     * This variant copies from a zero-terminated UTF-16 string (which need not
     * be a BSTR, i.e. need not have a length prefix).
     *
     * If the source is empty, this sets the member string to NULL.
     *
     * @param   a_bstrSrc           The source string.  The caller guarantees
     *                              that this is valid UTF-16.
     *
     * @throws  std::bad_alloc - the object is representing an empty string.
     */
    void copyFrom(const OLECHAR *a_bstrSrc)
    {
        if (a_bstrSrc && *a_bstrSrc)
        {
            m_bstr = ::SysAllocString(a_bstrSrc);
#ifdef RT_EXCEPTIONS_ENABLED
            if (!m_bstr)
                throw std::bad_alloc();
#endif
        }
        else
            m_bstr = NULL;
    }

    /**
     * Protected internal helper to copy a string. This ignores the previous object
     * state, so either call this from a constructor or call cleanup() first.
     *
     * This variant copies and converts from a zero-terminated UTF-8 string.
     *
     * If the source is empty, this sets the member string to NULL.
     *
     * @param   a_pszSrc            The source string.  The caller guarantees
     *                              that this is valid UTF-8.
     *
     * @throws  std::bad_alloc - the object is representing an empty string.
     */
    void copyFrom(const char *a_pszSrc)
    {
        copyFromN(a_pszSrc, RTSTR_MAX);
    }

    /**
     * Variant of copyFrom for sub-string constructors.
     *
     * @param   a_pszSrc            The source string.  The caller guarantees
     *                              that this is valid UTF-8.
     * @param   a_cchMax            The maximum number of chars (not
     *                              codepoints) to copy.  If you pass RTSTR_MAX
     *                              it'll be exactly like copyFrom().
     *
     * @throws  std::bad_alloc - the object is representing an empty string.
     */
    void copyFromN(const char *a_pszSrc, size_t a_cchSrc);

    BSTR m_bstr;

    friend class Utf8Str; /* to access our raw_copy() */
};

/* symmetric compare operators */
inline bool operator==(CBSTR l, const Bstr &r) { return r.operator==(l); }
inline bool operator!=(CBSTR l, const Bstr &r) { return r.operator!=(l); }
inline bool operator==(BSTR l, const Bstr &r) { return r.operator==(l); }
inline bool operator!=(BSTR l, const Bstr &r) { return r.operator!=(l); }




/**
 * String class used universally in Main for UTF-8 strings.
 *
 * This is based on RTCString, to which some functionality has been
 * moved.  Here we keep things that are specific to Main, such as conversions
 * with UTF-16 strings (Bstr).
 *
 * Like RTCString, Utf8Str does not differentiate between NULL strings
 * and empty strings.  In other words, Utf8Str("") and Utf8Str(NULL) behave the
 * same.  In both cases, RTCString allocates no memory, reports
 * a zero length and zero allocated bytes for both, and returns an empty
 * C string from c_str().
 *
 * @note    All Utf8Str methods ASSUMES valid UTF-8 or UTF-16 input strings.
 *          The VirtualBox policy in this regard is to validate strings coming
 *          from external sources before passing them to Utf8Str or Bstr.
 */
class Utf8Str : public RTCString
{
public:

    Utf8Str() {}

    Utf8Str(const RTCString &that)
        : RTCString(that)
    {}

    Utf8Str(const char *that)
        : RTCString(that)
    {}

    Utf8Str(const Bstr &that)
    {
        copyFrom(that.raw());
    }

    Utf8Str(CBSTR that)
    {
        copyFrom(that);
    }

    /**
     * Constructs a new string given the format string and the list of the
     * arguments for the format string.
     *
     * @param   a_pszFormat     Pointer to the format string (UTF-8),
     *                          @see pg_rt_str_format.
     * @param   a_va            Argument vector containing the arguments
     *                          specified by the format string.
     * @sa      RTCString::printfV
     */
    Utf8Str(const char *a_pszFormat, va_list a_va)
        : RTCString(a_pszFormat, a_va)
    {
    }

    Utf8Str& operator=(const RTCString &that)
    {
        RTCString::operator=(that);
        return *this;
    }

    Utf8Str& operator=(const char *that)
    {
        RTCString::operator=(that);
        return *this;
    }

    Utf8Str& operator=(const Bstr &that)
    {
        cleanup();
        copyFrom(that.raw());
        return *this;
    }

    Utf8Str& operator=(CBSTR that)
    {
        cleanup();
        copyFrom(that);
        return *this;
    }

    bool operator<(const RTCString &that) const { return RTCString::operator<(that); }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();

#if defined(VBOX_WITH_XPCOM)
    /**
     * Intended to assign instances to |char *| out parameters from within the
     * interface method. Transfers the ownership of the duplicated string to the
     * caller.
     *
     * This allocates a single 0 byte in the target if the member string is empty.
     *
     * This uses XPCOM memory allocation and thus only works on XPCOM. MSCOM doesn't
     * like char* strings anyway.
     */
    void cloneTo(char **pstr) const;

    /**
     * A version of cloneTo that does not throw allocation errors but returns
     * E_OUTOFMEMORY instead.
     * @returns S_OK or E_OUTOFMEMORY (COM status codes).
     */
    HRESULT cloneToEx(char **pstr) const;
#endif

    /**
     *  Intended to assign instances to |BSTR| out parameters from within the
     *  interface method. Transfers the ownership of the duplicated string to the
     *  caller.
     */
    void cloneTo(BSTR *pstr) const
    {
        if (pstr)
        {
            Bstr bstr(*this);
            bstr.cloneTo(pstr);
        }
    }

    /**
     * A version of cloneTo that does not throw allocation errors but returns
     * E_OUTOFMEMORY instead.
     *
     * @param   pbstr Where to store a clone of the string.
     * @returns S_OK or E_OUTOFMEMORY (COM status codes).
     */
    HRESULT cloneToEx(BSTR *pbstr) const
    {
        if (!pbstr)
            return S_OK;
        Bstr bstr(*this);
        return bstr.detachToEx(pbstr);
    }

    /**
     * Safe assignment from BSTR.
     *
     * @param   pbstrSrc    The source string.
     * @returns S_OK or E_OUTOFMEMORY (COM status codes).
     */
    HRESULT cloneEx(CBSTR pbstrSrc)
    {
        cleanup();
        return copyFromEx(pbstrSrc);
    }

    /**
     * Removes a trailing slash from the member string, if present.
     * Calls RTPathStripTrailingSlash() without having to mess with mutableRaw().
     */
    Utf8Str& stripTrailingSlash();

    /**
     * Removes a trailing filename from the member string, if present.
     * Calls RTPathStripFilename() without having to mess with mutableRaw().
     */
    Utf8Str& stripFilename();

    /**
     * Removes the path component from the member string, if present.
     * Calls RTPathFilename() without having to mess with mutableRaw().
     */
    Utf8Str& stripPath();

    /**
     * Removes a trailing file name extension from the member string, if present.
     * Calls RTPathStripExt() without having to mess with mutableRaw().
     */
    Utf8Str& stripExt();

    /**
     *  Static immutable empty-string object. May be used for comparison purposes.
     */
    static const Utf8Str Empty;
protected:

    void copyFrom(CBSTR a_pbstr);
    HRESULT copyFromEx(CBSTR a_pbstr);

    friend class Bstr; /* to access our raw_copy() */
};

/**
 * Class with RTCString::printf as constructor for your convenience.
 *
 * Constructing a Utf8Str string object from a format string and a variable
 * number of arguments can easily be confused with the other Utf8Str
 * constructures, thus this child class.
 *
 * The usage of this class is like the following:
 * @code
    Utf8StrFmt strName("program name = %s", argv[0]);
   @endcode
 */
class Utf8StrFmt : public Utf8Str
{
public:

    /**
     * Constructs a new string given the format string and the list of the
     * arguments for the format string.
     *
     * @param   a_pszFormat     Pointer to the format string (UTF-8),
     *                          @see pg_rt_str_format.
     * @param   ...             Ellipsis containing the arguments specified by
     *                          the format string.
     */
    explicit Utf8StrFmt(const char *a_pszFormat, ...)
    {
        va_list va;
        va_start(va, a_pszFormat);
        printfV(a_pszFormat, va);
        va_end(va);
    }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();

protected:
    Utf8StrFmt()
    { }

private:
};

/**
 * The BstrFmt class is a shortcut to <tt>Bstr(Utf8StrFmt(...))</tt>.
 */
class BstrFmt : public Bstr
{
public:

    /**
     * Constructs a new string given the format string and the list of the
     * arguments for the format string.
     *
     * @param aFormat   printf-like format string (in UTF-8 encoding).
     * @param ...       List of the arguments for the format string.
     */
    explicit BstrFmt(const char *aFormat, ...)
    {
        va_list args;
        va_start(args, aFormat);
        copyFrom(Utf8Str(aFormat, args).c_str());
        va_end(args);
    }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();
};

/**
 * The BstrFmtVA class is a shortcut to <tt>Bstr(Utf8Str(format,va))</tt>.
 */
class BstrFmtVA : public Bstr
{
public:

    /**
     * Constructs a new string given the format string and the list of the
     * arguments for the format string.
     *
     * @param aFormat   printf-like format string (in UTF-8 encoding).
     * @param aArgs     List of arguments for the format string
     */
    BstrFmtVA(const char *aFormat, va_list aArgs)
    {
        copyFrom(Utf8Str(aFormat, aArgs).c_str());
    }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();
};

} /* namespace com */

#endif /* !___VBox_com_string_h */

