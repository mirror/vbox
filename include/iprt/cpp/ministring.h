/** @file
 * IPRT - C++ string class.
 */

/*
 * Copyright (C) 2007-2015 Oracle Corporation
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

#ifndef ___iprt_cpp_ministring_h
#define ___iprt_cpp_ministring_h

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/cpp/list.h>

#include <new>


/** @defgroup grp_rt_cpp_string     C++ String support
 * @ingroup grp_rt_cpp
 * @{
 */

/** @brief  C++ string class.
 *
 * This is a C++ string class that does not depend on anything else except IPRT
 * memory management functions.  Semantics are like in std::string, except it
 * can do a lot less.
 *
 * Note that RTCString does not differentiate between NULL strings
 * and empty strings.  In other words, RTCString("") and RTCString(NULL)
 * behave the same.  In both cases, RTCString allocates no memory, reports
 * a zero length and zero allocated bytes for both, and returns an empty
 * C string from c_str().
 *
 * @note    RTCString ASSUMES that all strings it deals with are valid UTF-8.
 *          The caller is responsible for not breaking this assumption.
 */
#ifdef VBOX
 /** @remarks Much of the code in here used to be in com::Utf8Str so that
  *          com::Utf8Str can now derive from RTCString and only contain code
  *          that is COM-specific, such as com::Bstr conversions.  Compared to
  *          the old Utf8Str though, RTCString always knows the length of its
  *          member string and the size of the buffer so it can use memcpy()
  *          instead of strdup().
  */
#endif
class RT_DECL_CLASS RTCString
{
public:
    /**
     * Creates an empty string that has no memory allocated.
     */
    RTCString()
        : m_psz(NULL),
          m_cch(0),
          m_cbAllocated(0)
    {
    }

    /**
     * Creates a copy of another RTCString.
     *
     * This allocates s.length() + 1 bytes for the new instance, unless s is empty.
     *
     * @param   a_rSrc          The source string.
     *
     * @throws  std::bad_alloc
     */
    RTCString(const RTCString &a_rSrc)
    {
        copyFromN(a_rSrc.m_psz, a_rSrc.m_cch);
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
    RTCString(const char *pcsz)
    {
        copyFromN(pcsz, pcsz ? strlen(pcsz) : 0);
    }

    /**
     * Create a partial copy of another RTCString.
     *
     * @param   a_rSrc          The source string.
     * @param   a_offSrc        The byte offset into the source string.
     * @param   a_cchSrc        The max number of chars (encoded UTF-8 bytes)
     *                          to copy from the source string.
     */
    RTCString(const RTCString &a_rSrc, size_t a_offSrc, size_t a_cchSrc = npos)
    {
        if (a_offSrc < a_rSrc.m_cch)
            copyFromN(&a_rSrc.m_psz[a_offSrc], RT_MIN(a_cchSrc, a_rSrc.m_cch - a_offSrc));
        else
        {
            m_psz = NULL;
            m_cch = 0;
            m_cbAllocated = 0;
        }
    }

    /**
     * Create a partial copy of a C string.
     *
     * @param   a_pszSrc        The source string (UTF-8).
     * @param   a_cchSrc        The max number of chars (encoded UTF-8 bytes)
     *                          to copy from the source string.  This must not
     *                          be '0' as the compiler could easily mistake
     *                          that for the va_list constructor.
     */
    RTCString(const char *a_pszSrc, size_t a_cchSrc)
    {
        size_t cchMax = a_pszSrc ? RTStrNLen(a_pszSrc, a_cchSrc) : 0;
        copyFromN(a_pszSrc, RT_MIN(a_cchSrc, cchMax));
    }

    /**
     * Create a string containing @a a_cTimes repetitions of the character @a
     * a_ch.
     *
     * @param   a_cTimes        The number of times the character is repeated.
     * @param   a_ch            The character to fill the string with.
     */
    RTCString(size_t a_cTimes, char a_ch)
        : m_psz(NULL),
          m_cch(0),
          m_cbAllocated(0)
    {
        Assert((unsigned)a_ch < 0x80);
        if (a_cTimes)
        {
            reserve(a_cTimes + 1);
            memset(m_psz, a_ch, a_cTimes);
            m_psz[a_cTimes] = '\0';
            m_cch = a_cTimes;
        }
    }

    /**
     * Create a new string given the format string and its arguments.
     *
     * @param   a_pszFormat     Pointer to the format string (UTF-8),
     *                          @see pg_rt_str_format.
     * @param   a_va            Argument vector containing the arguments
     *                          specified by the format string.
     * @sa      printfV
     * @remarks Not part of std::string.
     */
    RTCString(const char *a_pszFormat, va_list a_va)
        : m_psz(NULL),
          m_cch(0),
          m_cbAllocated(0)
    {
        printfV(a_pszFormat, a_va);
    }

    /**
     * Destructor.
     */
    virtual ~RTCString()
    {
        cleanup();
    }

    /**
     * String length in bytes.
     *
     * Returns the length of the member string in bytes, which is equal to strlen(c_str()).
     * In other words, this does not count unicode codepoints; use utf8length() for that.
     * The byte length is always cached so calling this is cheap and requires no
     * strlen() invocation.
     *
     * @returns m_cbLength.
     */
    size_t length() const
    {
        return m_cch;
    }

    /**
     * String length in unicode codepoints.
     *
     * As opposed to length(), which returns the length in bytes, this counts
     * the number of unicode codepoints.  This is *not* cached so calling this
     * is expensive.
     *
     * @returns Number of codepoints in the member string.
     */
    size_t uniLength() const
    {
        return m_psz ? RTStrUniLen(m_psz) : 0;
    }

    /**
     * The allocated buffer size (in bytes).
     *
     * Returns the number of bytes allocated in the internal string buffer, which is
     * at least length() + 1 if length() > 0; for an empty string, this returns 0.
     *
     * @returns m_cbAllocated.
     */
    size_t capacity() const
    {
        return m_cbAllocated;
    }

    /**
     * Make sure at that least cb of buffer space is reserved.
     *
     * Requests that the contained memory buffer have at least cb bytes allocated.
     * This may expand or shrink the string's storage, but will never truncate the
     * contained string.  In other words, cb will be ignored if it's smaller than
     * length() + 1.
     *
     * @param   cb              New minimum size (in bytes) of member memory buffer.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     */
    void reserve(size_t cb)
    {
        if (    cb != m_cbAllocated
             && cb > m_cch + 1
           )
        {
            int rc = RTStrRealloc(&m_psz, cb);
            if (RT_SUCCESS(rc))
                m_cbAllocated = cb;
#ifdef RT_EXCEPTIONS_ENABLED
            else
                throw std::bad_alloc();
#endif
        }
    }

    /**
     * A C like version of the reserve method, i.e. return code instead of throw.
     *
     * @returns VINF_SUCCESS or VERR_NO_STRING_MEMORY.
     * @param   cb              New minimum size (in bytes) of member memory buffer.
     */
    int reserveNoThrow(size_t cb)
    {
        if (    cb != m_cbAllocated
             && cb > m_cch + 1
           )
        {
            int rc = RTStrRealloc(&m_psz, cb);
            if (RT_SUCCESS(rc))
                m_cbAllocated = cb;
            else
                return rc;
        }
        return VINF_SUCCESS;
    }

    /**
     * Deallocates all memory.
     */
    inline void setNull()
    {
        cleanup();
    }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();

    /**
     * Assigns a copy of pcsz to "this".
     *
     * @param   pcsz            The source string.
     *
     * @throws  std::bad_alloc  On allocation failure.  The object is left describing
     *             a NULL string.
     *
     * @returns Reference to the object.
     */
    RTCString &operator=(const char *pcsz)
    {
        if (m_psz != pcsz)
        {
            cleanup();
            copyFromN(pcsz, pcsz ? strlen(pcsz) : 0);
        }
        return *this;
    }

    /**
     * Assigns a copy of s to "this".
     *
     * @param   s               The source string.
     *
     * @throws  std::bad_alloc  On allocation failure.  The object is left describing
     *             a NULL string.
     *
     * @returns Reference to the object.
     */
    RTCString &operator=(const RTCString &s)
    {
        if (this != &s)
        {
            cleanup();
            copyFromN(s.m_psz, s.m_cch);
        }
        return *this;
    }

    /**
     * Assigns the output of the string format operation (RTStrPrintf).
     *
     * @param   pszFormat       Pointer to the format string,
     *                          @see pg_rt_str_format.
     * @param   ...             Ellipsis containing the arguments specified by
     *                          the format string.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &printf(const char *pszFormat, ...);

    /**
     * Assigns the output of the string format operation (RTStrPrintfV).
     *
     * @param   pszFormat       Pointer to the format string,
     *                          @see pg_rt_str_format.
     * @param   va              Argument vector containing the arguments
     *                          specified by the format string.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &printfV(const char *pszFormat, va_list va);

    /**
     * Appends the string "that" to "this".
     *
     * @param   that            The string to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &append(const RTCString &that);

    /**
     * Appends the string "that" to "this".
     *
     * @param   pszThat         The C string to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &append(const char *pszThat);

    /**
     * Appends the given character to "this".
     *
     * @param   ch              The character to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &append(char ch);

    /**
     * Appends the given unicode code point to "this".
     *
     * @param   uc              The unicode code point to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    RTCString &appendCodePoint(RTUNICP uc);

    /**
     * Shortcut to append(), RTCString variant.
     *
     * @param that              The string to append.
     *
     * @returns Reference to the object.
     */
    RTCString &operator+=(const RTCString &that)
    {
        return append(that);
    }

    /**
     * Shortcut to append(), const char* variant.
     *
     * @param pszThat           The C string to append.
     *
     * @returns                 Reference to the object.
     */
    RTCString &operator+=(const char *pszThat)
    {
        return append(pszThat);
    }

    /**
     * Shortcut to append(), char variant.
     *
     * @param pszThat           The character to append.
     *
     * @returns                 Reference to the object.
     */
    RTCString &operator+=(char c)
    {
        return append(c);
    }

    /**
     * Converts the member string to upper case.
     *
     * @returns Reference to the object.
     */
    RTCString &toUpper()
    {
        if (length())
        {
            /* Folding an UTF-8 string may result in a shorter encoding (see
               testcase), so recalculate the length afterwards. */
            ::RTStrToUpper(m_psz);
            size_t cchNew = strlen(m_psz);
            Assert(cchNew <= m_cch);
            m_cch = cchNew;
        }
        return *this;
    }

    /**
     * Converts the member string to lower case.
     *
     * @returns Reference to the object.
     */
    RTCString &toLower()
    {
        if (length())
        {
            /* Folding an UTF-8 string may result in a shorter encoding (see
               testcase), so recalculate the length afterwards. */
            ::RTStrToLower(m_psz);
            size_t cchNew = strlen(m_psz);
            Assert(cchNew <= m_cch);
            m_cch = cchNew;
        }
        return *this;
    }

    /**
     * Index operator.
     *
     * Returns the byte at the given index, or a null byte if the index is not
     * smaller than length().  This does _not_ count codepoints but simply points
     * into the member C string.
     *
     * @param   i       The index into the string buffer.
     * @returns char at the index or null.
     */
    inline char operator[](size_t i) const
    {
        if (i < length())
            return m_psz[i];
        return '\0';
    }

    /**
     * Returns the contained string as a C-style const char* pointer.
     * This never returns NULL; if the string is empty, this returns a
     * pointer to static null byte.
     *
     * @returns const pointer to C-style string.
     */
    inline const char *c_str() const
    {
        return (m_psz) ? m_psz : "";
    }

    /**
     * Returns a non-const raw pointer that allows to modify the string directly.
     * As opposed to c_str() and raw(), this DOES return NULL for an empty string
     * because we cannot return a non-const pointer to a static "" global.
     *
     * @warning
     *      -# Be sure not to modify data beyond the allocated memory! Call
     *         capacity() to find out how large that buffer is.
     *      -# After any operation that modifies the length of the string,
     *         you _must_ call RTCString::jolt(), or subsequent copy operations
     *         may go nowhere.  Better not use mutableRaw() at all.
     */
    char *mutableRaw()
    {
        return m_psz;
    }

    /**
     * Clean up after using mutableRaw.
     *
     * Intended to be called after something has messed with the internal string
     * buffer (e.g. after using mutableRaw() or Utf8Str::asOutParam()).  Resets the
     * internal lengths correctly.  Otherwise subsequent copy operations may go
     * nowhere.
     */
    void jolt()
    {
        if (m_psz)
        {
            m_cch = strlen(m_psz);
            m_cbAllocated = m_cch + 1; /* (Required for the Utf8Str::asOutParam case) */
        }
        else
        {
            m_cch = 0;
            m_cbAllocated = 0;
        }
    }

    /**
     * Returns @c true if the member string has no length.
     *
     * This is @c true for instances created from both NULL and "" input
     * strings.
     *
     * This states nothing about how much memory might be allocated.
     *
     * @returns @c true if empty, @c false if not.
     */
    bool isEmpty() const
    {
        return length() == 0;
    }

    /**
     * Returns @c false if the member string has no length.
     *
     * This is @c false for instances created from both NULL and "" input
     * strings.
     *
     * This states nothing about how much memory might be allocated.
     *
     * @returns @c false if empty, @c true if not.
     */
    bool isNotEmpty() const
    {
        return length() != 0;
    }

    /** Case sensitivity selector. */
    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };

    /**
     * Compares the member string to a C-string.
     *
     * @param   pcszThat    The string to compare with.
     * @param   cs          Whether comparison should be case-sensitive.
     * @returns 0 if equal, negative if this is smaller than @a pcsz, positive
     *          if larger.
     */
    int compare(const char *pcszThat, CaseSensitivity cs = CaseSensitive) const
    {
        /* This klugde is for m_cch=0 and m_psz=NULL.  pcsz=NULL and psz=""
           are treated the same way so that str.compare(str2.c_str()) works. */
        if (length() == 0)
            return pcszThat == NULL || *pcszThat == '\0' ? 0 : -1;

        if (cs == CaseSensitive)
            return ::RTStrCmp(m_psz, pcszThat);
        return ::RTStrICmp(m_psz, pcszThat);
    }

    /**
     * Compares the member string to another RTCString.
     *
     * @param   pcszThat    The string to compare with.
     * @param   cs          Whether comparison should be case-sensitive.
     * @returns 0 if equal, negative if this is smaller than @a pcsz, positive
     *          if larger.
     */
    int compare(const RTCString &that, CaseSensitivity cs = CaseSensitive) const
    {
        if (cs == CaseSensitive)
            return ::RTStrCmp(m_psz, that.m_psz);
        return ::RTStrICmp(m_psz, that.m_psz);
    }

    /**
     * Compares the two strings.
     *
     * @returns true if equal, false if not.
     * @param   that    The string to compare with.
     */
    bool equals(const RTCString &that) const
    {
        return that.length() == length()
            && memcmp(that.m_psz, m_psz, length()) == 0;
    }

    /**
     * Compares the two strings.
     *
     * @returns true if equal, false if not.
     * @param   pszThat The string to compare with.
     */
    bool equals(const char *pszThat) const
    {
        /* This klugde is for m_cch=0 and m_psz=NULL.  pcsz=NULL and psz=""
           are treated the same way so that str.equals(str2.c_str()) works. */
        if (length() == 0)
            return pszThat == NULL || *pszThat == '\0';
        return RTStrCmp(pszThat, m_psz) == 0;
    }

    /**
     * Compares the two strings ignoring differences in case.
     *
     * @returns true if equal, false if not.
     * @param   that    The string to compare with.
     */
    bool equalsIgnoreCase(const RTCString &that) const
    {
        /* Unfolded upper and lower case characters may require different
           amount of encoding space, so the length optimization doesn't work. */
        return RTStrICmp(that.m_psz, m_psz) == 0;
    }

    /**
     * Compares the two strings ignoring differences in case.
     *
     * @returns true if equal, false if not.
     * @param   pszThat The string to compare with.
     */
    bool equalsIgnoreCase(const char *pszThat) const
    {
        /* This klugde is for m_cch=0 and m_psz=NULL.  pcsz=NULL and psz=""
           are treated the same way so that str.equalsIgnoreCase(str2.c_str()) works. */
        if (length() == 0)
            return pszThat == NULL || *pszThat == '\0';
        return RTStrICmp(pszThat, m_psz) == 0;
    }

    /** @name Comparison operators.
     * @{  */
    bool operator==(const RTCString &that) const { return equals(that); }
    bool operator!=(const RTCString &that) const { return !equals(that); }
    bool operator<( const RTCString &that) const { return compare(that) < 0; }
    bool operator>( const RTCString &that) const { return compare(that) > 0; }

    bool operator==(const char *pszThat) const    { return equals(pszThat); }
    bool operator!=(const char *pszThat) const    { return !equals(pszThat); }
    bool operator<( const char *pszThat) const    { return compare(pszThat) < 0; }
    bool operator>( const char *pszThat) const    { return compare(pszThat) > 0; }
    /** @} */

    /** Max string offset value.
     *
     * When returned by a method, this indicates failure.  When taken as input,
     * typically a default, it means all the way to the string terminator.
     */
    static const size_t npos;

    /**
     * Find the given substring.
     *
     * Looks for pcszFind in "this" starting at "pos" and returns its position
     * as a byte (not codepoint) offset, counting from the beginning of "this" at 0.
     *
     * @param   pcszFind        The substring to find.
     * @param   pos             The (byte) offset into the string buffer to start
     *                          searching.
     *
     * @returns 0 based position of pcszFind. npos if not found.
     */
    size_t find(const char *pcszFind, size_t pos = 0) const;

    /**
     * Replaces all occurences of cFind with cReplace in the member string.
     * In order not to produce invalid UTF-8, the characters must be ASCII
     * values less than 128; this is not verified.
     *
     * @param   chFind      Character to replace. Must be ASCII < 128.
     * @param   chReplace   Character to replace cFind with. Must be ASCII < 128.
     */
    void findReplace(char chFind, char chReplace);

    /**
     * Count the occurences of the specified character in the string.
     *
     * @param   ch          What to search for. Must be ASCII < 128.
     * @remarks QString::count
     */
    size_t count(char ch) const;

    /**
     * Count the occurences of the specified sub-string in the string.
     *
     * @param   psz         What to search for.
     * @param   cs          Case sensitivity selector.
     * @remarks QString::count
     */
    size_t count(const char *psz, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Count the occurences of the specified sub-string in the string.
     *
     * @param   pStr        What to search for.
     * @param   cs          Case sensitivity selector.
     * @remarks QString::count
     */
    size_t count(const RTCString *pStr, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Returns a substring of "this" as a new Utf8Str.
     *
     * Works exactly like its equivalent in std::string. With the default
     * parameters "0" and "npos", this always copies the entire string. The
     * "pos" and "n" arguments represent bytes; it is the caller's responsibility
     * to ensure that the offsets do not copy invalid UTF-8 sequences. When
     * used in conjunction with find() and length(), this will work.
     *
     * @param   pos             Index of first byte offset to copy from "this", counting from 0.
     * @param   n               Number of bytes to copy, starting with the one at "pos".
     *                          The copying will stop if the null terminator is encountered before
     *                          n bytes have been copied.
     */
    RTCString substr(size_t pos = 0, size_t n = npos) const
    {
        return RTCString(*this, pos, n);
    }

    /**
     * Returns a substring of "this" as a new Utf8Str. As opposed to substr(),
     * this variant takes codepoint offsets instead of byte offsets.
     *
     * @param   pos             Index of first unicode codepoint to copy from
     *                          "this", counting from 0.
     * @param   n               Number of unicode codepoints to copy, starting with
     *                          the one at "pos".  The copying will stop if the null
     *                          terminator is encountered before n codepoints have
     *                          been copied.
     */
    RTCString substrCP(size_t pos = 0, size_t n = npos) const;

    /**
     * Returns true if "this" ends with "that".
     *
     * @param   that    Suffix to test for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool endsWith(const RTCString &that, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Returns true if "this" begins with "that".
     * @param   that    Prefix to test for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool startsWith(const RTCString &that, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Returns true if "this" contains "that" (strstr).
     *
     * @param   that    Substring to look for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool contains(const RTCString &that, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Attempts to convert the member string into a 32-bit integer.
     *
     * @returns 32-bit unsigned number on success.
     * @returns 0 on failure.
     */
    int32_t toInt32() const
    {
        return RTStrToInt32(m_psz);
    }

    /**
     * Attempts to convert the member string into an unsigned 32-bit integer.
     *
     * @returns 32-bit unsigned number on success.
     * @returns 0 on failure.
     */
    uint32_t toUInt32() const
    {
        return RTStrToUInt32(m_psz);
    }

    /**
     * Attempts to convert the member string into an 64-bit integer.
     *
     * @returns 64-bit unsigned number on success.
     * @returns 0 on failure.
     */
    int64_t toInt64() const
    {
        return RTStrToInt64(m_psz);
    }

    /**
     * Attempts to convert the member string into an unsigned 64-bit integer.
     *
     * @returns 64-bit unsigned number on success.
     * @returns 0 on failure.
     */
    uint64_t toUInt64() const
    {
        return RTStrToUInt64(m_psz);
    }

    /**
     * Attempts to convert the member string into an unsigned 64-bit integer.
     *
     * @param   i       Where to return the value on success.
     * @returns IPRT error code, see RTStrToInt64.
     */
    int toInt(uint64_t &i) const;

    /**
     * Attempts to convert the member string into an unsigned 32-bit integer.
     *
     * @param   i       Where to return the value on success.
     * @returns IPRT error code, see RTStrToInt32.
     */
    int toInt(uint32_t &i) const;

    /** Splitting behavior regarding empty sections in the string. */
    enum SplitMode
    {
        KeepEmptyParts,  /**< Empty parts are added as empty strings to the result list. */
        RemoveEmptyParts /**< Empty parts are skipped. */
    };

    /**
     * Splits a string separated by strSep into its parts.
     *
     * @param   a_rstrSep   The separator to search for.
     * @param   a_enmMode   How should empty parts be handled.
     * @returns separated strings as string list.
     */
    RTCList<RTCString, RTCString *> split(const RTCString &a_rstrSep,
                                          SplitMode a_enmMode = RemoveEmptyParts) const;

    /**
     * Joins a list of strings together using the provided separator and
     * an optional prefix for each item in the list.
     *
     * @param   a_rList         The list to join.
     * @param   a_rstrPrefix    The prefix used for appending to each item.
     * @param   a_rstrSep       The separator used for joining.
     * @returns joined string.
     */
    static RTCString joinEx(const RTCList<RTCString, RTCString *> &a_rList,
                            const RTCString &a_rstrPrefix /* = "" */,
                            const RTCString &a_rstrSep /* = "" */);

    /**
     * Joins a list of strings together using the provided separator.
     *
     * @param   a_rList     The list to join.
     * @param   a_rstrSep   The separator used for joining.
     * @returns joined string.
     */
    static RTCString join(const RTCList<RTCString, RTCString *> &a_rList,
                          const RTCString &a_rstrSep = "");

    /**
     * Swaps two strings in a fast way.
     *
     * Exception safe.
     *
     * @param   a_rThat  The string to swap with.
     */
    inline void swap(RTCString &a_rThat) throw()
    {
        char   *pszTmp         = m_psz;
        size_t  cchTmp         = m_cch;
        size_t  cbAllocatedTmp = m_cbAllocated;

        m_psz                  = a_rThat.m_psz;
        m_cch                  = a_rThat.m_cch;
        m_cbAllocated          = a_rThat.m_cbAllocated;

        a_rThat.m_psz          = pszTmp;
        a_rThat.m_cch          = cchTmp;
        a_rThat.m_cbAllocated  = cbAllocatedTmp;
    }

protected:

    /**
     * Hide operator bool() to force people to use isEmpty() explicitly.
     */
    operator bool() const;

    /**
     * Destructor implementation, also used to clean up in operator=() before
     * assigning a new string.
     */
    void cleanup()
    {
        if (m_psz)
        {
            RTStrFree(m_psz);
            m_psz = NULL;
            m_cch = 0;
            m_cbAllocated = 0;
        }
    }

    /**
     * Protected internal helper to copy a string.
     *
     * This ignores the previous object state, so either call this from a
     * constructor or call cleanup() first.  copyFromN() unconditionally sets
     * the members to a copy of the given other strings and makes no
     * assumptions about previous contents.  Can therefore be used both in copy
     * constructors, when member variables have no defined value, and in
     * assignments after having called cleanup().
     *
     * @param   pcszSrc         The source string.
     * @param   cchSrc          The number of chars (bytes) to copy from the
     *                          source strings.  RTSTR_MAX is NOT accepted.
     *
     * @throws  std::bad_alloc  On allocation failure.  The object is left
     *                          describing a NULL string.
     */
    void copyFromN(const char *pcszSrc, size_t cchSrc)
    {
        if (cchSrc)
        {
            m_psz = RTStrAlloc(cchSrc + 1);
            if (RT_LIKELY(m_psz))
            {
                m_cch = cchSrc;
                m_cbAllocated = cchSrc + 1;
                memcpy(m_psz, pcszSrc, cchSrc);
                m_psz[cchSrc] = '\0';
            }
            else
            {
                m_cch = 0;
                m_cbAllocated = 0;
#ifdef RT_EXCEPTIONS_ENABLED
                throw std::bad_alloc();
#endif
            }
        }
        else
        {
            m_cch = 0;
            m_cbAllocated = 0;
            m_psz = NULL;
        }
    }

    static DECLCALLBACK(size_t) printfOutputCallback(void *pvArg, const char *pachChars, size_t cbChars);

    char   *m_psz;                      /**< The string buffer. */
    size_t  m_cch;                      /**< strlen(m_psz) - i.e. no terminator included. */
    size_t  m_cbAllocated;              /**< Size of buffer that m_psz points to; at least m_cbLength + 1. */
};

/** @} */


/** @addtogroup grp_rt_cpp_string
 * @{
 */

/**
 * Concatenate two strings.
 *
 * @param   a_rstr1     String one.
 * @param   a_rstr2     String two.
 * @returns the concatenate string.
 *
 * @relates RTCString
 */
RTDECL(const RTCString) operator+(const RTCString &a_rstr1, const RTCString &a_rstr2);

/**
 * Concatenate two strings.
 *
 * @param   a_rstr1     String one.
 * @param   a_psz2      String two.
 * @returns the concatenate string.
 *
 * @relates RTCString
 */
RTDECL(const RTCString) operator+(const RTCString &a_rstr1, const char *a_psz2);

/**
 * Concatenate two strings.
 *
 * @param   a_psz1      String one.
 * @param   a_rstr2     String two.
 * @returns the concatenate string.
 *
 * @relates RTCString
 */
RTDECL(const RTCString) operator+(const char *a_psz1, const RTCString &a_rstr2);

/**
 * Class with RTCString::printf as constructor for your convenience.
 *
 * Constructing a RTCString string object from a format string and a variable
 * number of arguments can easily be confused with the other RTCString
 * constructors, thus this child class.
 *
 * The usage of this class is like the following:
 * @code
    RTCStringFmt strName("program name = %s", argv[0]);
   @endcode
 */
class RTCStringFmt : public RTCString
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
    explicit RTCStringFmt(const char *a_pszFormat, ...)
    {
        va_list va;
        va_start(va, a_pszFormat);
        printfV(a_pszFormat, va);
        va_end(va);
    }

    RTMEMEF_NEW_AND_DELETE_OPERATORS();

protected:
    RTCStringFmt() {}
};

/** @} */

#endif

