/* $Id$ */

/** @file
 * MS COM / XPCOM Abstraction Layer:
 * UTF-8 and UTF-16 string classes
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

#include <iprt/cpp/utils.h>
#include <iprt/alloc.h>
#include <iprt/cpp/ministring.h>

namespace com
{

class Utf8Str;

/**
 *  String class used universally in Main for COM-style Utf-16 strings.
 *  Unfortunately COM on Windows uses UTF-16 everywhere, requiring conversions
 *  back and forth since most of VirtualBox and our libraries use UTF-8.
 *  The Bstr class makes such conversions easier.
 *
 *  Whereas the BSTR identifier is a typedef for a pointer to a wide character
 *  array (const char *uint_16 effectively, depending on the platform),
 *  the Bstr class is a fully featured string class with memory management
 *  for such strings.
 *
 *  Bstr uses COM/XPCOM-provided memory management routines (SysAlloc* etc.)
 *  to allocate and free string buffers. This makes it possible to use it as
 *  a type of member variables of COM/XPCOM components and pass their values
 *  to callers through component methods' output parameters using the #cloneTo()
 *  operation. Also, the class can adopt (take ownership of) string buffers
 *  returned in output parameters of COM methods using the #asOutParam()
 *  operation and correctly free them afterwards.
 *
 *  As opposed to the Ut8Str class, which is very efficient, Bstr does not
 *  cache the length of its member string. As a result, copying Bstr's is
 *  more expensive, and Bstr really should only be used to capture arguments
 *  from and return data to public COM methods.
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

    /**
     * Creates an empty string that has no memory allocated.
     */
    Bstr()
        : m_bstr(NULL)
    {
    }

    /**
     * Creates a copy of another Bstr.
     *
     * This allocates s.length() + 1 wide characters for the new instance, unless s is empty.
     *
     * @param   s               The source string.
     *
     * @throws  std::bad_alloc
     */
    Bstr(const Bstr &s)
    {
        copyFrom(s);
    }

    /**
     * Creates a copy of a wide char string buffer.
     *
     * This allocates SysStringLen(pw) + 1 wide characters for the new instance, unless s is empty.
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc
     */
    Bstr(CBSTR pw)
    {
        copyFrom(pw);
    }

#if defined (VBOX_WITH_XPCOM)
    Bstr(const wchar_t *pw)
    {
        AssertCompile(sizeof(wchar_t) == sizeof(OLECHAR));
        copyFrom((CBSTR)pw);
    }
#endif


    /**
     * Creates a copy of an IPRT MiniString (which includes Utf8Str).
     *
     * This allocates s.length() + 1 wide characters for the new instance, unless s is empty.
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc
     */
    Bstr(const iprt::MiniString &s)
    {
        copyFrom(s.c_str());        // @todo the source string length is know, we can probably speed this up
    }

    /**
     * Creates a copy of a C string.
     *
     * This allocates strlen(pcsz) + 1 bytes for the new instance, unless s is empty.
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc
     */
    Bstr(const char *pcsz)
    {
        copyFrom(pcsz);
    }

    /**
     * String length in wide characters.
     *
     * Returns the length of the member string, which is equal to SysStringLen(raw()).
     * In other words, this returns neither bytes nor the number of unicode codepoints.
     *
     * As opposed to Utf8Str::length(), this is _not_ cached and expensive.
     */
    size_t length() const
    {
        return ::SysStringLen(m_bstr);
    }

    /**
     * Deallocates all memory.
     */
    inline void setNull()
    {
        cleanup();
    }

    /**
     * Returns a const pointer to the member string. If the member string is empty,
     * returns a pointer to a static null character.
     * @return
     */
    CBSTR raw() const
    {
        return (m_bstr) ? m_bstr : (CBSTR)L"";
    }

    /**
     * Empty string or not?
     *
     * Returns true if the member string has no length.
     *
     * @returns true if empty, false if not.
     */
    bool isEmpty() const
    {
        return (m_bstr == NULL);
    }

    operator bool() const
    {
        return !isEmpty();
    }

    bool operator!() const
    {
        return isEmpty();
    }

    /** Case sensitivity selector. */
    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };

    int compare(CBSTR str, CaseSensitivity cs = CaseSensitive) const
    {
        if (m_bstr == str)
            return 0;
        if (m_bstr == NULL)
            return -1;
        if (str == NULL)
            return 1;

        if (cs == CaseSensitive)
            return ::RTUtf16Cmp((PRTUTF16)m_bstr, (PRTUTF16)str);
        else
            return ::RTUtf16ICmp((PRTUTF16)m_bstr, (PRTUTF16)str);
    }

    int compare(const Bstr &bstr, CaseSensitivity cs = CaseSensitive) const
    {
        return compare(bstr.raw(), cs);
    }

    /** @name Comparison operators.
     * @{  */
    bool operator==(const Bstr &that) const { return !compare(that.raw()); }
    bool operator!=(const Bstr &that) const { return !!compare(that.raw()); }
    bool operator<( const Bstr &that) const { return compare(that.raw()) < 0; }
    bool operator>( const Bstr &that) const { return compare(that.raw()) > 0; }

    bool operator==(CBSTR that) const { return !compare(that); }
    bool operator!=(CBSTR that) const { return !!compare(that); }
    bool operator<( CBSTR that) const { return compare(that) < 0; }
    bool operator>( CBSTR that) const { return compare(that) > 0; }

    // the following two are necessary or stupid MSVC will complain with "ambiguous operator=="
    bool operator==( BSTR that) const { return !compare(that); }
    bool operator!=( BSTR that) const { return !!compare(that); }

    /** @} */

    /** Intended to to pass instances as |CBSTR| input parameters to methods. */
    operator CBSTR() const { return raw(); }

    /**
     * Intended to to pass instances as |BSTR| input parameters to methods.
     * Note that we have to provide this mutable BSTR operator since in MS COM
     * input BSTR parameters of interface methods are not const.
     */
    operator BSTR() { return (BSTR)raw(); }

    /**
     *  Intended to assign copies of instances to |BSTR| out parameters from
     *  within the interface method. Transfers the ownership of the duplicated
     *  string to the caller.
     *
     *  This allocates a single 0 byte in the target if the member string is empty.
     */
    const Bstr& cloneTo(BSTR *pstr) const
    {
        if (pstr)
            *pstr = ::SysAllocString((const OLECHAR*)raw());
                    // raw() never returns NULL, so we always allocate something here
        return *this;
    }

    /**
     *  Intended to assign instances to |BSTR| out parameters from within the
     *  interface method. Transfers the ownership of the original string to the
     *  caller and resets the instance to null.
     *
     *  As opposed to cloneTo(), this method doesn't create a copy of the
     *  string.
     *
     *  This allocates a single 0 byte in the target if the member string is empty.
     */
    Bstr& detachTo(BSTR *pstr)
    {
        *pstr = (m_bstr) ? m_bstr : ::SysAllocString((const OLECHAR*)"");
        m_bstr = NULL;
        return *this;
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

    /**
     * Destructor implementation, also used to clean up in operator=() before
     * assigning a new string.
     */
    void cleanup()
    {
        if (m_bstr)
        {
            ::SysFreeString(m_bstr);
            m_bstr = NULL;
        }
    }

    /**
     *  Protected internal helper which allocates memory for a string capable of
     *  storing \a aSize - 1 characters (not bytes, not codepoints); in other words,
     *  aSize includes the terminating null character.
     *
     *  Does NOT call cleanup() before allocating!
     *
     * @throws  std::bad_alloc  On allocation failure. The object is left describing
     *             a NULL string.
     */
    void alloc(size_t cw)
    {
        if (cw)
        {
            m_bstr = ::SysAllocStringLen(NULL, (unsigned int)cw - 1);
#ifdef RT_EXCEPTIONS_ENABLED
            if (!m_bstr)
                throw std::bad_alloc();
#endif
        }
    }

    /**
     * Protected internal helper to copy a string, ignoring the previous object state.
     *
     * copyFrom() unconditionally sets the members to a copy of the given other
     * strings and makes no assumptions about previous contents. Can therefore be
     * used both in copy constructors, when member variables have no defined value,
     * and in assignments after having called cleanup().
     *
     * This variant copies from another Bstr. Since Bstr does _not_ cache string lengths,
     * this is not fast.
     *
     * @param   s               The source string.
     *
     * @throws  std::bad_alloc  On allocation failure. The object is left describing
     *             a NULL string.
     */
    void copyFrom(const Bstr &s)
    {
        copyFrom(s.raw());
    }

    /**
     * Protected internal helper to copy a string, ignoring the previous object state.
     *
     * See copyFrom() above.
     *
     * This variant copies from a wide char C string.
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc  On allocation failure. The object is left describing
     *             a NULL string.
     */
    void copyFrom(CBSTR pw)
    {
        size_t cwLength;
        if (    (pw)
             && ((cwLength = ::SysStringLen((BSTR)pw)))
           )
        {
            size_t cwAllocated = cwLength + 1;
            alloc(cwAllocated);
            memcpy(m_bstr, pw, cwAllocated * sizeof(OLECHAR));     // include 0 terminator
        }
        else
            m_bstr = NULL;
    }

    /**
     * Protected internal helper to copy a string, ignoring the previous object state.
     *
     * See copyFrom() above.
     *
     * This variant converts from a Utf-8 C string.
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc  On allocation failure. The object is left describing
     *             a NULL string.
     */
    void copyFrom(const char *pcsz)
    {
        if (pcsz && *pcsz)
        {
            // @todo r=dj apparently this was copied twice in the original because our buffers
            // use memory from SysAllocMem and IPRT doesn't, but check if this can be made faster
            PRTUTF16 s = NULL;
            ::RTStrToUtf16(pcsz, &s);
            copyFrom((BSTR)s);              // @todo r=dj this is not exception safe
            ::RTUtf16Free(s);
        }
        else
            m_bstr = NULL;
    }

    BSTR    m_bstr;                     /**< The string buffer. */
};

/* symmetric compare operators */
// inline bool operator==(CBSTR l, const Bstr &r) { return r.operator==(l); }
// inline bool operator!=(CBSTR l, const Bstr &r) { return r.operator!=(l); }
// inline bool operator==(BSTR l, const Bstr &r) { return r.operator==(l); }
// inline bool operator!=(BSTR l, const Bstr &r) { return r.operator!=(l); }

////////////////////////////////////////////////////////////////////////////////

/**
 *  String class used universally in Main for Utf-8 strings.
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
        copyFrom(that.raw());
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
        copyFrom(that.raw());
        return *this;
    }

    Utf8Str& operator=(CBSTR that)
    {
        cleanup();
        copyFrom(that);
        return *this;
    }

    /**
     * Intended to assign instances to |char *| out parameters from within the
     * interface method. Transfers the ownership of the duplicated string to the
     * caller.
     *
     * This allocates a single 0 byte in the target if the member string is empty.
     *
     * @remarks The returned string must be freed by RTStrFree, not RTMemFree.
     */
    const Utf8Str& cloneTo(char **pstr) const
    {
        if (pstr)
        {
            *pstr = RTStrDup(raw());
#ifdef RT_EXCEPTIONS_ENABLED
            if (!*pstr)
                throw std::bad_alloc();
#endif
        }
        return *this;
    }

    /**
     *  Intended to assign instances to |BSTR| out parameters from within the
     *  interface method. Transfers the ownership of the duplicated string to the
     *  caller.
     *
     *  This allocates a single 0 byte in the target if the member string is empty.
     */
    const Utf8Str& cloneTo(BSTR *pstr) const
    {
        if (pstr)
        {
            Bstr bstr(c_str());
            *pstr = NULL;
            bstr.detachTo(pstr);
        }
        return *this;
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
     * Intended to pass instances as out (|char **|) parameters to methods. Takes
     * the ownership of the returned data.
     *
     * @remarks    See ministring::jolt().
     */
    char **asOutParam()
    {
        cleanup();
        return &m_psz;
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
        if (s)
        {
            RTUtf16ToUtf8((PRTUTF16)s, &m_psz); /** @todo r=bird: This technically requires using RTStrFree, ministring::cleanup() uses RTMemFree. */
#ifdef RT_EXCEPTIONS_ENABLED
            if (!m_psz)
                throw std::bad_alloc();
#endif
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

// work around error C2593 of the stupid MSVC 7.x ambiguity resolver
// @todo r=dj if I enable this I get about five warnings every time this header
// is included, figure out what that is... for now I have modified the calling code instead
// WORKAROUND_MSVC7_ERROR_C2593_FOR_BOOL_OP(Bstr)

////////////////////////////////////////////////////////////////////////////////

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

