/** @file
 *
 * MS COM / XPCOM Abstraction Layer:
 * Smart string classes definition
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

namespace com
{

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

