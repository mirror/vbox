/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Smart string classes declaration
 */

/*
 * Copyright (C) 2006-2009 Oracle Corporation
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

#if defined (VBOX_WITH_XPCOM)
# include <nsMemory.h>
#endif

#include "VBox/com/defs.h"
#include "VBox/com/assert.h"

#include <iprt/alloc.h>
#include <iprt/cpp/ministring.h>

namespace com
{

class Utf8Str;

// global constant in glue/string.cpp that represents an empty BSTR
extern const BSTR g_bstrEmpty;

/**
 *  String class used universally in Main for COM-style Utf-16 strings.
 *  Unfortunately COM on Windows uses UTF-16 everywhere, requiring conversions
 *  back and forth since most of VirtualBox and our libraries use UTF-8.
 *
 *  To make things more obscure, on Windows, a COM-style BSTR is not just a
 *  pointer to a null-terminated wide character array, but the four bytes
 *  (32 bits) BEFORE the memory that the pointer points to are a length
 *  DWORD. One must therefore avoid pointer arithmetic and always use
 *  SysAllocString and the like to deal with BSTR pointers, which manage
 *  that DWORD correctly.
 *
 *  For platforms other than Windows, we provide our own versions of the
 *  Sys* functions in Main/xpcom/helpers.cpp which do NOT use length
 *  prefixes though to be compatible with how XPCOM allocates string
 *  parameters to public functions.
 *
 *  The Bstr class hides all this handling behind a std::string-like interface
 *  and also provides automatic conversions to MiniString and Utf8Str instances.
 *
 *  The one advantage of using the SysString* routines is that this makes it
 *  possible to use it as a type of member variables of COM/XPCOM components
 *  and pass their values to callers through component methods' output parameters
 *  using the #cloneTo() operation. Also, the class can adopt (take ownership of)
 *  string buffers returned in output parameters of COM methods using the
 *  #asOutParam() operation and correctly free them afterwards.
 *
 *  Starting with VirtualBox 3.2, like Utf8Str, Bstr no longer differentiates
 *  between NULL strings and empty strings. In other words, Bstr("") and
 *  Bstr(NULL) behave the same. In both cases, Bstr allocates no memory,
 *  reports a zero length and zero allocated bytes for both, and returns an
 *  empty C wide string from raw().
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

#if defined (VBOX_WITH_XPCOM)
    Bstr(const wchar_t *that)
    {
        AssertCompile(sizeof(wchar_t) == sizeof(OLECHAR));
        copyFrom((const OLECHAR *)that);
    }
#endif

    Bstr(const iprt::MiniString &that)
    {
        copyFrom(that.raw());
    }

    Bstr(const char *that)
    {
        copyFrom(that);
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

#if defined (VBOX_WITH_XPCOM)
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

    int compare(BSTR str) const
    {
        return compare((CBSTR)str);
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
     * (Bstr does not cache string lengths). Also do not use operator bool();
     * for one, MSVC is really annoying with its thinking that that is ambiguous,
     * and even though operator bool() is protected with Bstr, at least gcc uses
     * operator CBSTR() when a construct like "if (string)" is encountered, which
     * is always true now since it raw() never returns an empty string. Again,
     * always use isEmpty() even though if (string) may compile!
     */
    bool isEmpty() const { return m_bstr == NULL || *m_bstr == 0; }

    size_t length() const { return isEmpty() ? 0 : ::RTUtf16Len((PRTUTF16)m_bstr); }

    /**
     * Returns true if the member string is not empty. I'd like to make this
     * private but since we require operator BSTR() it's futile anyway because
     * the compiler will then (wrongly) use that one instead. Also if this is
     * private the odd WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP macro below
     * will fail on Windows.
     */
    operator bool() const
    {
        return m_bstr != NULL;
    }

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

    /**
     * Convenience operator so that Bstr's can be passed to CBSTR input parameters
     * of COM methods.
     */
    operator CBSTR() const { return raw(); }

    /**
     * Convenience operator so that Bstr's can be passed to CBSTR input parameters
     * of COM methods. Unfortunately this is required for Windows since with
     * MSCOM, input BSTR parameters of interface methods are not const.
     */
    operator BSTR() { return (BSTR)raw(); }

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
     *  Intended to assign instances to |BSTR| out parameters from within the
     *  interface method. Transfers the ownership of the original string to the
     *  caller and resets the instance to null.
     *
     *  As opposed to cloneTo(), this method doesn't create a copy of the
     *  string.
     *
     *  If the member string is empty, this allocates an empty BSTR in *pstr
     *  (i.e. makes it point to a new buffer with a null byte).
     */
    void detachTo(BSTR *pstr)
    {
        if (m_bstr)
            *pstr = m_bstr;
        else
        {
            // allocate null BSTR
            *pstr = ::SysAllocString((const OLECHAR *)g_bstrEmpty);
#ifdef RT_EXCEPTIONS_ENABLED
            if (!*pstr)
                throw std::bad_alloc();
#endif
        }
        m_bstr = NULL;
    }

    /**
     *  Intended to pass instances as |BSTR| out parameters to methods.
     *  Takes the ownership of the returned data.
     */
    BSTR* asOutParam()
    {
        cleanup();
        return &m_bstr;
    }

    /**
     *  Static immutable null object. May be used for comparison purposes.
     */
    static const Bstr Null;

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
     * @param rs
     */
    void copyFrom(const OLECHAR *rs)
    {
        if (rs && *rs)
        {
            m_bstr = ::SysAllocString(rs);
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
     * @param rs
     */
    void copyFrom(const char *rs)
    {
        if (rs && *rs)
        {
            PRTUTF16 s = NULL;
            ::RTStrToUtf16(rs, &s);
#ifdef RT_EXCEPTIONS_ENABLED
            if (!s)
                throw std::bad_alloc();
#endif
            copyFrom((const OLECHAR *)s);            // allocates BSTR from zero-terminated input string
            ::RTUtf16Free(s);
        }
        else
            m_bstr = NULL;
    }

    BSTR m_bstr;

    friend class Utf8Str; /* to access our raw_copy() */
};

/* symmetric compare operators */
inline bool operator==(CBSTR l, const Bstr &r) { return r.operator==(l); }
inline bool operator!=(CBSTR l, const Bstr &r) { return r.operator!=(l); }
inline bool operator==(BSTR l, const Bstr &r) { return r.operator==(l); }
inline bool operator!=(BSTR l, const Bstr &r) { return r.operator!=(l); }

// work around error C2593 of the stupid MSVC 7.x ambiguity resolver
WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP(Bstr)

////////////////////////////////////////////////////////////////////////////////

/**
 *  String class used universally in Main for UTF-8 strings.
 *
 *  This is based on iprt::MiniString, to which some functionality has been
 *  moved. Here we keep things that are specific to Main, such as conversions
 *  with UTF-16 strings (Bstr).
 *
 *  Like iprt::MiniString, Utf8Str does not differentiate between NULL strings
 *  and empty strings. In other words, Utf8Str("") and Utf8Str(NULL)
 *  behave the same. In both cases, MiniString allocates no memory, reports
 *  a zero length and zero allocated bytes for both, and returns an empty
 *  C string from c_str().
 */
class Utf8Str : public iprt::MiniString
{
public:

    Utf8Str() {}

    Utf8Str(const MiniString &that)
        : MiniString(that)
    {}

    Utf8Str(const char *that)
        : MiniString(that)
    {}

    Utf8Str(const Bstr &that)
    {
        copyFrom(that);
    }

    Utf8Str(CBSTR that)
    {
        copyFrom(that);
    }

    Utf8Str& operator=(const MiniString &that)
    {
        MiniString::operator=(that);
        return *this;
    }

    Utf8Str& operator=(const char *that)
    {
        MiniString::operator=(that);
        return *this;
    }

    Utf8Str& operator=(const Bstr &that)
    {
        cleanup();
        copyFrom(that);
        return *this;
    }

    Utf8Str& operator=(CBSTR that)
    {
        cleanup();
        copyFrom(that);
        return *this;
    }

#if defined (VBOX_WITH_XPCOM)
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
     * Converts "this" to lower case by calling RTStrToLower().
     * @return
     */
    Utf8Str& toLower();

    /**
     * Converts "this" to upper case by calling RTStrToUpper().
     * @return
     */
    Utf8Str& toUpper();

    /**
     * Removes a trailing slash from the member string, if present.
     * Calls RTPathStripTrailingSlash() without having to mess with mutableRaw().
     */
    void stripTrailingSlash();

    /**
     * Removes a trailing filename from the member string, if present.
     * Calls RTPathStripFilename() without having to mess with mutableRaw().
     */
    void stripFilename();

    /**
     * Removes a trailing file name extension from the member string, if present.
     * Calls RTPathStripExt() without having to mess with mutableRaw().
     */
    void stripExt();

    /**
     * Attempts to convert the member string into a 32-bit integer.
     *
     * @returns 32-bit unsigned number on success.
     * @returns 0 on failure.
     */
    int toInt32() const
    {
        return RTStrToInt32(m_psz);
    }

    /**
     * Attempts to convert the member string into an unsigned 32-bit integer.
     *
     * @returns 32-bit unsigned number on success.
     * @returns 0 on failure.
     */
    int toUInt32() const
    {
        return RTStrToUInt32(m_psz);
    }

    /**
     *  Static immutable null object. May be used for comparison purposes.
     */
    static const Utf8Str Null;

protected:

    /**
     * As with the ministring::copyFrom() variants, this unconditionally
     * sets the members to a copy of the given other strings and makes
     * no assumptions about previous contents. This can therefore be used
     * both in copy constructors, when member variables have no defined
     * value, and in assignments after having called cleanup().
     *
     * This variant converts from a UTF-16 string, most probably from
     * a Bstr assignment.
     *
     * @param rs
     */
    void copyFrom(CBSTR s)
    {
        if (s && *s)
        {
            RTUtf16ToUtf8((PRTUTF16)s, &m_psz); /** @todo r=bird: This isn't throwing std::bad_alloc / handling return codes.
                                                 * Also, this technically requires using RTStrFree, ministring::cleanup() uses RTMemFree. */
            m_cbLength = strlen(m_psz);         /** @todo optimize by using a different RTUtf* function */
            m_cbAllocated = m_cbLength + 1;
        }
        else
        {
            m_cbLength = 0;
            m_cbAllocated = 0;
            m_psz = NULL;
        }
    }

    friend class Bstr; /* to access our raw_copy() */
};

/**
 *  This class is a printf-like formatter for Utf8Str strings. Its purpose is
 *  to construct Utf8Str objects from a format string and a list of arguments
 *  for the format string.
 *
 *  The usage of this class is like the following:
 *  <code>
 *      Utf8StrFmt string ("program name = %s", argv[0]);
 *  </code>
 */
class Utf8StrFmt : public Utf8Str
{
public:

    /**
     *  Constructs a new string given the format string and the list
     *  of the arguments for the format string.
     *
     *  @param format   printf-like format string (in UTF-8 encoding)
     *  @param ...      list of the arguments for the format string
     */
    explicit Utf8StrFmt(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        init(format, args);
        va_end(args);
    }

protected:

    Utf8StrFmt() {}

    void init(const char *format, va_list args);

private:

    static DECLCALLBACK(size_t) strOutput(void *pvArg, const char *pachChars,
                                          size_t cbChars);
};

/**
 *  This class is a vprintf-like formatter for Utf8Str strings. It is
 *  identical to Utf8StrFmt except that its constructor takes a va_list
 *  argument instead of ellipsis.
 *
 *  Note that a separate class is necessary because va_list is defined as
 *  |char *| on most platforms. For this reason, if we had two overloaded
 *  constructors in Utf8StrFmt (one taking ellipsis and another one taking
 *  va_list) then composing a constructor call using exactly two |char *|
 *  arguments would cause the compiler to use the va_list overload instead of
 *  the ellipsis one which is obviously wrong. The compiler would choose
 *  va_list because ellipsis has the lowest rank when it comes to resolving
 *  overloads, as opposed to va_list which is an exact match for |char *|.
 */
class Utf8StrFmtVA : public Utf8StrFmt
{
public:

    /**
     *  Constructs a new string given the format string and the list
     *  of the arguments for the format string.
     *
     *  @param format   printf-like format string (in UTF-8 encoding)
     *  @param args     list of arguments for the format string
     */
    Utf8StrFmtVA(const char *format, va_list args) { init(format, args); }
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
        copyFrom(Utf8StrFmtVA(aFormat, args).c_str());
        va_end(args);
    }
};

/**
 * The BstrFmtVA class is a shortcut to <tt>Bstr(Utf8StrFmtVA(...))</tt>.
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
        copyFrom(Utf8StrFmtVA(aFormat, aArgs).c_str());
    }
};

} /* namespace com */

#endif /* !___VBox_com_string_h */

