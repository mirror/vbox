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
const Bstr Bstr::Empty; /* default ctor is OK */

/* static */
const Utf8Str Utf8Str::Empty; /* default ctor is OK */

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

Utf8Str &Utf8Str::stripTrailingSlash()
{
    if (length())
    {
        ::RTPathStripTrailingSlash(m_psz);
        jolt();
    }
    return *this;
}

Utf8Str &Utf8Str::stripFilename()
{
    if (length())
    {
        RTPathStripFilename(m_psz);
        jolt();
    }
    return *this;
}

Utf8Str &Utf8Str::stripPath()
{
    if (length())
    {
        char *pcszFilename = ::RTStrDup(::RTPathFilename(m_psz));
        cleanup();
        MiniString::copyFrom(pcszFilename);
        RTStrFree(pcszFilename);
    }
    return *this;
}

Utf8Str &Utf8Str::stripExt()
{
    if (length())
    {
        RTPathStripExt(m_psz);
        jolt();
    }
    return *this;
}

/**
 * Internal function used in Utf8Str copy constructors and assignment when
 * copying from a UTF-16 string.
 *
 * As with the iprt::ministring::copyFrom() variants, this unconditionally
 * sets the members to a copy of the given other strings and makes
 * no assumptions about previous contents. This can therefore be used
 * both in copy constructors, when member variables have no defined
 * value, and in assignments after having called cleanup().
 *
 * This variant converts from a UTF-16 string, most probably from
 * a Bstr assignment.
 *
 * @param s
 */
void Utf8Str::copyFrom(CBSTR s)
{
    if (s && *s)
    {
        int vrc = RTUtf16ToUtf8Ex((PRTUTF16)s,      // PCRTUTF16 pwszString
                                  RTSTR_MAX,        // size_t cwcString: translate entire string
                                  &m_psz,           // char **ppsz: output buffer
                                  0,                // size_t cch: if 0, func allocates buffer in *ppsz
                                  &m_cbLength);     // size_t *pcch: receives the size of the output string, excluding the terminator.
        if (RT_FAILURE(vrc))
        {
            if (    vrc == VERR_NO_STR_MEMORY
                 || vrc == VERR_NO_MEMORY
               )
                throw std::bad_alloc();

            // @todo what do we do with bad input strings? throw also? for now just keep an empty string
            m_cbLength = 0;
            m_cbAllocated = 0;
            m_psz = NULL;
        }
        else
            m_cbAllocated = m_cbLength + 1;
    }
    else
    {
        m_cbLength = 0;
        m_cbAllocated = 0;
        m_psz = NULL;
    }
}

void Utf8StrFmt::init(const char *format, va_list args)
{
    if (!format || !*format)
    {
        m_cbLength = 0;
        m_cbAllocated = 0;
        m_psz = NULL;
    }
    else
    {
        m_cbLength = RTStrAPrintfV(&m_psz, format, args);
        m_cbAllocated = m_cbLength + 1;
    }
}

} /* namespace com */
