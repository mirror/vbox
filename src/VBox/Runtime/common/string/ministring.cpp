/* $Id$ */
/** @file
 * IPRT - Mini C++ string class.
 *
 * This is a base for both Utf8Str and other places where IPRT may want to use
 * a lean C++ string class.
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

#include <iprt/ministring_cpp.h>

using namespace iprt;

const size_t MiniString::npos = ~(size_t)0;

MiniString &MiniString::append(const MiniString &that)
{
    size_t lenThat = that.length();
    if (lenThat)
    {
        size_t lenThis = length();
        size_t cbBoth = lenThis + lenThat + 1;

        reserve(cbBoth);
            // calls realloc(cbBoth) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
        AssertRelease(capacity() >= cbBoth);
#endif

        memcpy(m_psz + lenThis, that.m_psz, lenThat);
        m_psz[lenThis + lenThat] = '\0';
        m_cbLength = cbBoth - 1;
    }
    return *this;
}

MiniString& MiniString::append(char c)
{
    if (c)
    {
        // allocate in chunks of 20 in case this gets called several times
        if (m_cbLength + 1 >= m_cbAllocated)
        {
            reserve(m_cbLength + 10);
            // calls realloc(cbBoth) and sets m_cbAllocated; may throw bad_alloc.
#ifndef RT_EXCEPTIONS_ENABLED
            AssertRelease(capacity() >= m_cbLength + 1);
#endif
        }

        m_psz[m_cbLength] = c;
        m_psz[m_cbLength + 1] = '\0';
        ++m_cbLength;
    }
    return *this;
}

size_t MiniString::find(const char *pcszFind, size_t pos /*= 0*/)
    const
{
    const char *pszThis, *p;

    if (    ((pszThis = c_str()))
         && (pos < length())
         && ((p = strstr(pszThis + pos, pcszFind)))
       )
        return p - pszThis;

    return npos;
}

MiniString MiniString::substr(size_t pos /*= 0*/, size_t n /*= npos*/)
    const
{
    MiniString ret;

    if (n)
    {
        const char *psz;

        if ((psz = c_str()))
        {
            RTUNICP cp;

            // walk the UTF-8 characters until where the caller wants to start
            size_t i = pos;
            while (*psz && i--)
                if (RT_FAILURE(RTStrGetCpEx(&psz, &cp)))
                    return ret;     // return empty string on bad encoding

            const char *pFirst = psz;

            if (n == npos)
                // all the rest:
                ret = pFirst;
            else
            {
                i = n;
                while (*psz && i--)
                    if (RT_FAILURE(RTStrGetCpEx(&psz, &cp)))
                        return ret;     // return empty string on bad encoding

                size_t cbCopy = psz - pFirst;
                ret.reserve(cbCopy + 1); // may throw bad_alloc
#ifndef RT_EXCEPTIONS_ENABLED
                AssertRelease(capacity() >= cbCopy + 1);
#endif
                memcpy(ret.m_psz, pFirst, cbCopy);
                ret.m_cbLength = cbCopy;
                ret.m_psz[cbCopy] = '\0';
            }
        }
    }

    return ret;
}

bool MiniString::endsWith(const MiniString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    if (l1 == 0)
        return false;

    size_t l2 = that.length();
    if (l1 < l2)
        return false;
    /** @todo r=bird: If l2 is 0, then m_psz can be NULL and we will crash. See
     *        also handling of l2 == in startsWith. */

    size_t l = l1 - l2;
    if (cs == CaseSensitive)
        return ::RTStrCmp(&m_psz[l], that.m_psz) == 0;
    else
        return ::RTStrICmp(&m_psz[l], that.m_psz) == 0;
}

bool MiniString::startsWith(const MiniString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    size_t l2 = that.length();
    if (l1 == 0 || l2 == 0) /** @todo r=bird: this differs from endsWith, and I think other IPRT code. If l2 == 0, it matches anything. */
        return false;

    if (l1 < l2)
        return false;

    if (cs == CaseSensitive)
        return ::RTStrNCmp(m_psz, that.m_psz, l2) == 0;
    else
        return ::RTStrNICmp(m_psz, that.m_psz, l2) == 0;
}

bool MiniString::contains(const MiniString &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    /** @todo r-bird: Not checking for NULL strings like startsWith does (and
     *        endsWith only does half way). */
    if (cs == CaseSensitive)
        return ::RTStrStr(m_psz, that.m_psz) != NULL;
    else
        return ::RTStrIStr(m_psz, that.m_psz) != NULL;
}

int MiniString::toInt(uint64_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt64Ex(m_psz, NULL, 0, &i);
}

int MiniString::toInt(uint32_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt32Ex(m_psz, NULL, 0, &i);
}

