/** @file
 * VirtualBox mini C++ string class. This is a base for both Utf8Str and
 * other places where IPRT may want to use a lean C++ string class.
 */

/*
 * Copyright (C) 2007-2009 Sun Microsystems, Inc.
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

#ifndef ___VBox_ministring_h
#define ___VBox_ministring_h

#include <iprt/mem.h>
#include <iprt/string.h>

/**
 *  "ministring" is a small C++ string class that does not depend on anything
 *  else except IPRT memory management functions. This is used as the base of
 *  both the Utf8Str class that COM uses as well as C++ code in IPRT that
 *  prefers to have a string class, like in xml.cpp.
 *
 *  Semantics are like in std::string, except it can do a lot less.
 *
 *  Much of the code in here used to be in Utf8Str so that Utf8Str can now
 *  derive from ministring and only contain code that is COM-specific, such
 *  as com::Bstr conversions. Compared to the old Utf8Str though, ministring
 *  always knows the length of its member string and the size of the buffer
 *  so it can use memcpy() instead of strdup().
 */

class RT_DECL_CLASS ministring
{
public:
    ministring()
        : m_psz(NULL),
          m_cbLength(0),
          m_cbAllocated(0)
    {
    }

    ministring(const ministring &s)
        : m_psz(NULL)
    {
        copyFrom(s);
    }

    ministring(const char *pcsz)
        : m_psz(NULL)
    {
        copyFrom(pcsz);
    }

    virtual ~ministring()
    {
        cleanup();
    }

    size_t length() const
    {
        return m_cbLength;
    }

    /**
     * Returns the no. of bytes allocated in the internal string buffer,
     * which is at least m_cbLength + 1 if m_cbLength != 0.
     * @return
     */
    size_t capacity() const
    {
        return m_cbAllocated;
    }

    /**
     * Requests that the contained memory buffer have at least cb bytes allocated.
     * This may expand or shrink the string's storage, but will never truncate the
     * contained string.
     * @param cb new minimum size of member memory buffer
     */
    void reserve(size_t cb)
    {
        if (    (cb != m_cbAllocated)
             && (cb > m_cbLength + 1)
           )
        {
            m_psz = (char*)RTMemRealloc(m_psz, cb);
            m_cbAllocated = cb;
        }
    }

    inline void setNull()
    {
        cleanup();
    }

    /**
     *  Returns a non-const raw pointer that allows to modify the string directly.
     *  @warning
     *      1)  Be sure not to modify data beyond the allocated memory! Call
     *          capacity() to find out how large that buffer is.
     *      2)  After any operation that modifies the length of the string,
     *          you _must_ call ministring::jolt(), or subsequent copy operations
     *          may go nowhere. Better not use mutableRaw() at all.
     */
    char* mutableRaw()
    {
        return m_psz;
    }

    /**
     * Intended to be called after something has messed with the internal string
     * buffer (e.g. after using mutableRaw() or Utf8Str::asOutParam()). Resets
     * the internal lengths correctly. Otherwise subsequent copy operations may
     * go nowhere.
     */
    void jolt()
    {
        if (m_psz)
        {
            m_cbLength = strlen(m_psz);
            m_cbAllocated = m_cbLength + 1;
        }
        else
        {
            m_cbLength = 0;
            m_cbAllocated = 0;
        }
    }

    ministring& operator=(const char *pcsz)
    {
        cleanup();
        copyFrom(pcsz);
        return *this;
    }

    ministring& operator=(const ministring &s)
    {
        cleanup();
        copyFrom(s);
        return *this;
    }

    void append(const ministring &that)
    {
        size_t cbThis = length();
        size_t cbThat = that.length();

        if (cbThat)
        {
            size_t cbBoth = cbThis + cbThat + 1;

            reserve(cbBoth);
                // calls realloc(cbBoth) and sets m_cbAllocated

            memcpy(m_psz + cbThis, that.m_psz, cbThat);
            m_psz[cbThis + cbThat] = '\0';
            m_cbLength = cbBoth - 1;
        }
    }

    inline const char* c_str() const
    {
        return m_psz;
    }

    inline const char* raw() const
    {
        return m_psz;
    }

    /** Intended to to pass instances as input (|char *|) parameters to methods. */
    inline operator const char*() const
    {
        return c_str();
    }

    bool isEmpty() const
    {
        return length() == 0;
    }

    bool isNull() const
    {
        return m_psz == NULL;
    }

    enum CaseSensitivity
    {
        CaseSensitive,
        CaseInsensitive
    };

    int compare(const char *pcsz, CaseSensitivity cs = CaseSensitive) const
    {
        if (m_psz == pcsz)
            return 0;
        if (m_psz == NULL)
            return -1;
        if (pcsz == NULL)
            return 1;

        if (cs == CaseSensitive)
            return ::RTStrCmp(m_psz, pcsz);
        else
            return ::RTStrICmp(m_psz, pcsz);
    }

    int compare(const ministring &that, CaseSensitivity cs = CaseSensitive) const
    {
        return compare(that.m_psz, cs);
    }

    bool operator==(const ministring &that) const { return !compare(that); }
    bool operator!=(const ministring &that) const { return !!compare(that); }
    bool operator<(const ministring &that) const { return compare(that) < 0; }
    bool operator>(const ministring &that) const { return compare(that) > 0; }

    bool operator==(const char *that) const { return !compare(that); }
    bool operator!=(const char *that) const { return !!compare(that); }
    bool operator<(const char *that) const { return compare(that) < 0; }
    bool operator>(const char *that) const { return compare(that) > 0; }

protected:
    /**
     *  Destructor implementation, also used to clean up in operator=()
     *  before assigning a new string.
     */
    void cleanup()
    {
        if (m_psz)
        {
            RTMemFree(m_psz);
            m_psz = NULL;
            m_cbLength = 0;
            m_cbAllocated = 0;
        }
    }

    /**
     * copyFrom() unconditionally sets the members to a copy of the
     * given other strings and makes no assumptions about previous
     * contents. Can therefore be used both in copy constructors,
     * when member variables have no defined value, and in assignments
     * after having called cleanup().
     *
     * This variant copies from another ministring and is fast since
     * the length of source string is known.
     *
     * @param s
     */
    void copyFrom(const ministring &s)
    {
        if (&s != this)
        {
            m_cbLength = s.m_cbLength;
            m_cbAllocated = m_cbLength + 1;
            m_psz = (char*)RTMemAlloc(m_cbAllocated);
            memcpy(m_psz, s.m_psz, m_cbAllocated);      // include 0 terminator
        }
    }

    /**
     * See copyFrom() above.
     *
     * This variant copies from a C string and needs to call strlen()
     * on it. It's therefore slower than the one above.
     *
     * @param pcsz
     */
    void copyFrom(const char *pcsz)
    {
        if (pcsz != m_psz)  // leaves NULL as NULL also
        {
            if (pcsz)
            {
                m_cbLength = strlen(pcsz);
                m_cbAllocated = m_cbLength + 1;
                m_psz = (char*)RTMemAlloc(m_cbAllocated);
                memcpy(m_psz, pcsz, m_cbAllocated);      // include 0 terminator
            }
            else
            {
                m_cbLength = 0;
                m_cbAllocated = 0;
                m_psz = NULL;
            }
        }
    }

    char    *m_psz;
    size_t  m_cbLength;                     // strlen(m_psz)
    size_t  m_cbAllocated;                  // size of buffer that m_psz points to; at least m_cbLength + 1
};

#endif
