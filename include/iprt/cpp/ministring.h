/** @file
 * IPRT - Mini C++ string class.
 */

/*
 * Copyright (C) 2007-2009 Oracle Corporation
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

#ifndef ___VBox_ministring_h
#define ___VBox_ministring_h

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>

#include <new>

namespace iprt
{

/**
 * @brief Mini C++ string class.
 *
 * "MiniString" is a small C++ string class that does not depend on anything
 * else except IPRT memory management functions.  Semantics are like in
 * std::string, except it can do a lot less.
 *
 * Note that MiniString does not differentiate between NULL strings and
 * empty strings. In other words, MiniString("") and MiniString(NULL)
 * behave the same. In both cases, MiniString allocates no memory, reports
 * a zero length and zero allocated bytes for both, and returns an empty
 * C string from c_str().
 */
#ifdef VBOX
 /** @remarks Much of the code in here used to be in com::Utf8Str so that
  *          com::Utf8Str can now derive from MiniString and only contain code
  *          that is COM-specific, such as com::Bstr conversions.  Compared to
  *          the old Utf8Str though, MiniString always knows the length of its
  *          member string and the size of the buffer so it can use memcpy()
  *          instead of strdup().
  */
#endif
class RT_DECL_CLASS MiniString
{
public:
    /**
     * Creates an empty string that has no memory allocated.
     */
    MiniString()
        : m_psz(NULL),
          m_cch(0),
          m_cbAllocated(0)
    {
    }

    /**
     * Creates a copy of another MiniString.
     *
     * This allocates s.length() + 1 bytes for the new instance, unless s is empty.
     *
     * @param   s               The source string.
     *
     * @throws  std::bad_alloc
     */
    MiniString(const MiniString &s)
    {
        copyFrom(s);
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
    MiniString(const char *pcsz)
    {
        copyFrom(pcsz);
    }

    /**
     * Create a new string given the format string and its arguments.
     *
     * @param   a_pszFormat     Pointer to the format string (UTF-8),
     *                          @see pg_rt_str_format.
     * @param   a_va            Argument vector containing the arguments
     *                          specified by the format string.
     * @sa      printfV
     */
    MiniString(const char *a_pszFormat, va_list a_va)
        : m_psz(NULL),
          m_cch(0),
          m_cbAllocated(0)

    {
        printfV(a_pszFormat, a_va);
    }

    /**
     * Destructor.
     */
    virtual ~MiniString()
    {
        cleanup();
    }

    /**
     * String length in bytes.
     *
     * Returns the length of the member string, which is equal to strlen(c_str()).
     * In other words, this does not count unicode codepoints but returns the number
     * of bytes.  This is always cached so calling this is cheap and requires no
     * strlen() invocation.
     *
     * @returns m_cbLength.
     */
    size_t length() const
    {
        return m_cch;
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
            int vrc = RTStrRealloc(&m_psz, cb);
            if (RT_SUCCESS(vrc))
                m_cbAllocated = cb;
#ifdef RT_EXCEPTIONS_ENABLED
            else
                throw std::bad_alloc();
#endif
        }
    }

    /**
     * Deallocates all memory.
     */
    inline void setNull()
    {
        cleanup();
    }

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
    MiniString &operator=(const char *pcsz)
    {
        if (m_psz != pcsz)
        {
            cleanup();
            copyFrom(pcsz);
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
    MiniString &operator=(const MiniString &s)
    {
        if (this != &s)
        {
            cleanup();
            copyFrom(s);
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
    MiniString &printf(const char *pszFormat, ...);

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
    MiniString &printfV(const char *pszFormat, va_list va);

    /**
     * Appends the string "that" to "this".
     *
     * @param   that            The string to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    MiniString &append(const MiniString &that);

    /**
     * Appends the string "that" to "this".
     *
     * @param   pszThat         The C string to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    MiniString &append(const char *pszThat);

    /**
     * Appends the given character to "this".
     *
     * @param   ch              The character to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    MiniString &append(char ch);

    /**
     * Appends the given unicode code point to "this".
     *
     * @param   uc              The unicode code point to append.
     *
     * @throws  std::bad_alloc  On allocation error.  The object is left unchanged.
     *
     * @returns Reference to the object.
     */
    MiniString &appendCodePoint(RTUNICP uc);

    /**
     * Shortcut to append(), MiniString variant.
     *
     * @param that              The string to append.
     *
     * @returns Reference to the object.
     */
    MiniString &operator+=(const MiniString &that)
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
    MiniString &operator+=(const char *pszThat)
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
    MiniString &operator+=(char c)
    {
        return append(c);
    }

    /**
     * Converts the member string to upper case.
     *
     * @returns Reference to the object.
     */
    MiniString &toUpper()
    {
        if (length())
        {
            /* Folding an UTF-8 string may result in a shorter encoding (see
               testcase), so recalculate the length afterwars. */
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
    MiniString &toLower()
    {
        if (length())
        {
            /* Folding an UTF-8 string may result in a shorter encoding (see
               testcase), so recalculate the length afterwars. */
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
     *         you _must_ call MiniString::jolt(), or subsequent copy operations
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
     * Compares the member string to another MiniString.
     *
     * @param   pcszThat    The string to compare with.
     * @param   cs          Whether comparison should be case-sensitive.
     * @returns 0 if equal, negative if this is smaller than @a pcsz, positive
     *          if larger.
     */
    int compare(const MiniString &that, CaseSensitivity cs = CaseSensitive) const
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
    bool equals(const MiniString &that) const
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
    bool equalsIgnoreCase(const MiniString &that) const
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
    bool operator==(const MiniString &that) const { return equals(that); }
    bool operator!=(const MiniString &that) const { return !equals(that); }
    bool operator<( const MiniString &that) const { return compare(that) < 0; }
    bool operator>( const MiniString &that) const { return compare(that) > 0; }

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
     * Looks for pcszFind in "this" starting at "pos" and returns its position,
     * counting from the beginning of "this" at 0.
     *
     * @param   pcszFind        The substring to find.
     * @param   pos             The (byte) offset into the string buffer to start
     *                          searching.
     *
     * @returns 0 based position of pcszFind. npos if not found.
     */
    size_t find(const char *pcszFind, size_t pos = 0) const;

    /**
     * Returns a substring of "this" as a new Utf8Str.
     *
     * Works exactly like its equivalent in std::string except that this interprets
     * pos and n as unicode codepoints instead of bytes.  With the default
     * parameters "0" and "npos", this always copies the entire string.
     *
     * @param   pos             Index of first unicode codepoint to copy from
     *                          "this", counting from 0.
     * @param   n               Number of unicode codepoints to copy, starting with
     *                          the one at "pos".  The copying will stop if the null
     *                          terminator is encountered before n codepoints have
     *                          been copied.
     *
     * @remarks This works on code points, not bytes!
     */
    iprt::MiniString substr(size_t pos = 0, size_t n = npos) const;

    /**
     * Returns true if "this" ends with "that".
     *
     * @param   that    Suffix to test for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool endsWith(const iprt::MiniString &that, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Returns true if "this" begins with "that".
     * @param   that    Prefix to test for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool startsWith(const iprt::MiniString &that, CaseSensitivity cs = CaseSensitive) const;

    /**
     * Returns true if "this" contains "that" (strstr).
     *
     * @param   that    Substring to look for.
     * @param   cs      Case sensitivity selector.
     * @returns true if match, false if mismatch.
     */
    bool contains(const iprt::MiniString &that, CaseSensitivity cs = CaseSensitive) const;

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
     * Protected internal helper to copy a string. This ignores the previous object
     * state, so either call this from a constructor or call cleanup() first.
     *
     * copyFrom() unconditionally sets the members to a copy of the given other
     * strings and makes no assumptions about previous contents. Can therefore be
     * used both in copy constructors, when member variables have no defined value,
     * and in assignments after having called cleanup().
     *
     * This variant copies from another MiniString and is fast since
     * the length of the source string is known.
     *
     * @param   s               The source string.
     *
     * @throws  std::bad_alloc  On allocation failure. The object is left describing
     *             a NULL string.
     */
    void copyFrom(const MiniString &s)
    {
        if ((m_cch = s.m_cch))
        {
            m_cbAllocated = m_cch + 1;
            m_psz = (char *)RTStrAlloc(m_cbAllocated);
            if (RT_LIKELY(m_psz))
                memcpy(m_psz, s.m_psz, m_cbAllocated);      // include 0 terminator
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
            m_cbAllocated = 0;
            m_psz = NULL;
        }
    }

    /**
     * Protected internal helper to copy a string. This ignores the previous object
     * state, so either call this from a constructor or call cleanup() first.
     *
     * See copyFrom() above.
     *
     * This variant copies from a C string and needs to call strlen()
     * on it. It's therefore slower than the one above.
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
            m_cch = strlen(pcsz);
            m_cbAllocated = m_cch + 1;
            m_psz = (char *)RTStrAlloc(m_cbAllocated);
            if (RT_LIKELY(m_psz))
                memcpy(m_psz, pcsz, m_cbAllocated);     // include 0 terminator
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

} // namespace iprt

#endif

