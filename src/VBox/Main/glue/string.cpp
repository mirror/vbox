/* $Id$ */

/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * UTF-8 and UTF-16 string classes
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "VBox/com/string.h"

#include <iprt/err.h>
#include <iprt/path.h>

namespace com
{

// BSTR representing a null wide char with 32 bits of length prefix (0);
// this will work on Windows as well as other platforms where BSTR does
// not use length prefixes
const OLECHAR g_achEmptyBstr[3] = { 0, 0, 0 };
const BSTR g_bstrEmpty = (BSTR)(&g_achEmptyBstr[2]);

/* static */
const Bstr Bstr::Null; /* default ctor is OK */

/* static */
const Utf8Str Utf8Str::Null; /* default ctor is OK */

#if defined (VBOX_WITH_XPCOM)
void Utf8Str::cloneTo(char **pstr) const
{
    size_t cb = length() + 1;
    *pstr = (char*)nsMemory::Alloc(cb);
    if (!*pstr)
        throw std::bad_alloc();
    memcpy(*pstr, c_str(), cb);
}
#endif

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

struct FormatData
{
    static const size_t CacheIncrement = 256;
    size_t size;
    size_t pos;
    char *cache;
};

void Utf8StrFmt::init(const char *format, va_list args)
{
    if (!format)
        return;

    // assume an extra byte for a terminating zero
    size_t fmtlen = strlen(format) + 1;

    FormatData data;
    data.size = FormatData::CacheIncrement;
    if (fmtlen >= FormatData::CacheIncrement)
        data.size += fmtlen;
    data.pos = 0;
    data.cache = (char*)::RTMemTmpAllocZ(data.size);

    size_t n = ::RTStrFormatV(strOutput, &data, NULL, NULL, format, args);

    AssertMsg(n == data.pos,
              ("The number of bytes formatted doesn't match: %d and %d!", n, data.pos));
    NOREF(n);

    // finalize formatting
    data.cache[data.pos] = 0;
    (*static_cast<Utf8Str*>(this)) = data.cache;
    ::RTMemTmpFree(data.cache);
}

// static
DECLCALLBACK(size_t) Utf8StrFmt::strOutput(void *pvArg, const char *pachChars,
                                           size_t cbChars)
{
    Assert(pvArg);
    FormatData &data = *(FormatData *) pvArg;

    if (!(pachChars == NULL && cbChars == 0))
    {
        Assert(pachChars);

        // append to cache (always assume an extra byte for a terminating zero)
        size_t needed = cbChars + 1;
        if (data.pos + needed > data.size)
        {
            data.size += FormatData::CacheIncrement;
            if (needed >= FormatData::CacheIncrement)
                data.size += needed;
            data.cache = (char*)::RTMemRealloc(data.cache, data.size);
        }
        strncpy(data.cache + data.pos, pachChars, cbChars);
        data.pos += cbChars;
    }

    return cbChars;
}


} /* namespace com */
