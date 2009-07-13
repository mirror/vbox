/* $Id$ */

/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * Smart string classes definition
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "VBox/com/string.h"

#include <iprt/err.h>
#include <iprt/path.h>

namespace com
{

/* static */
const Bstr Bstr::Null; /* default ctor is OK */

/* static */
const Utf8Str Utf8Str::Null; /* default ctor is OK */

const size_t Utf8Str::npos = (size_t)-1;

size_t Utf8Str::find(const char *pcszFind,
                     size_t pos /*= 0*/)
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

Utf8Str Utf8Str::substr(size_t pos /*= 0*/, size_t n /*= npos*/)
    const
{
    Utf8Str ret;

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
                ret.reserve(cbCopy + 1);
                memcpy(ret.m_psz, pFirst, cbCopy);
                ret.m_cbLength = cbCopy;
                ret.m_psz[cbCopy] = '\0';
            }
        }
    }

    return ret;
}

bool Utf8Str::endsWith(const Utf8Str &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    if (l1 == 0)
        return false;

    size_t l2 = that.length();
    if (l1 < l2)
        return false;

    size_t l = l1 - l2;
    if (cs == CaseSensitive)
        return ::RTStrCmp(&m_psz[l], that.m_psz) == 0;
    else
        return ::RTStrICmp(&m_psz[l], that.m_psz) == 0;
}

bool Utf8Str::startsWith(const Utf8Str &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    size_t l1 = length();
    size_t l2 = that.length();
    if (l1 == 0 || l2 == 0)
        return false;

    if (l1 < l2)
        return false;

    if (cs == CaseSensitive)
        return ::RTStrNCmp(m_psz, that.m_psz, l2) == 0;
    else
        return ::RTStrNICmp(m_psz, that.m_psz, l2) == 0;
}

bool Utf8Str::contains(const Utf8Str &that, CaseSensitivity cs /*= CaseSensitive*/) const
{
    if (cs == CaseSensitive)
        return ::RTStrStr(m_psz, that.m_psz) != NULL;
    else
        return ::RTStrIStr(m_psz, that.m_psz) != NULL;
}

Utf8Str& Utf8Str::toLower()
{
    if (length())
        ::RTStrToLower(m_psz);
    return *this;
}

Utf8Str& Utf8Str::toUpper()
{
    if (length())
        ::RTStrToUpper(m_psz);
    return *this;
}

void Utf8Str::stripTrailingSlash()
{
    RTPathStripTrailingSlash(m_psz);
    jolt();
}

void Utf8Str::stripFilename()
{
    RTPathStripFilename(m_psz);
    jolt();
}

void Utf8Str::stripExt()
{
    RTPathStripExt(m_psz);
    jolt();
}

int Utf8Str::toInt(uint64_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt64Ex(m_psz, NULL, 0, &i);
}

int Utf8Str::toInt(uint32_t &i) const
{
    if (!m_psz)
        return VERR_NO_DIGITS;
    return RTStrToUInt32Ex(m_psz, NULL, 0, &i);
}

struct FormatData
{
    static const size_t CacheIncrement = 256;
    size_t size;
    size_t pos;
    char *cache;
};

void Utf8StrFmt::init (const char *format, va_list args)
{
    if (!format)
        return;

    // assume an extra byte for a terminating zero
    size_t fmtlen = strlen (format) + 1;

    FormatData data;
    data.size = FormatData::CacheIncrement;
    if (fmtlen >= FormatData::CacheIncrement)
        data.size += fmtlen;
    data.pos = 0;
    data.cache = (char *) ::RTMemTmpAllocZ (data.size);

    size_t n = ::RTStrFormatV (strOutput, &data, NULL, NULL, format, args);

    AssertMsg (n == data.pos,
               ("The number of bytes formatted doesn't match: %d and %d!",
                n, data.pos));
    NOREF (n);

    // finalize formatting
    data.cache [data.pos] = 0;
    (*static_cast <Utf8Str *> (this)) = data.cache;
    ::RTMemTmpFree (data.cache);
}

// static
DECLCALLBACK(size_t) Utf8StrFmt::strOutput (void *pvArg, const char *pachChars,
                                            size_t cbChars)
{
    Assert (pvArg);
    FormatData &data = *(FormatData *) pvArg;

    if (!(pachChars == NULL && cbChars == 0))
    {
        Assert (pachChars);

        // append to cache (always assume an extra byte for a terminating zero)
        size_t needed = cbChars + 1;
        if (data.pos + needed > data.size)
        {
            data.size += FormatData::CacheIncrement;
            if (needed >= FormatData::CacheIncrement)
                data.size += needed;
            data.cache = (char *) ::RTMemRealloc (data.cache, data.size);
        }
        strncpy (data.cache + data.pos, pachChars, cbChars);
        data.pos += cbChars;
    }

    return cbChars;
}


} /* namespace com */
