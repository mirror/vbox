/* $Id$ */
/** @file
 * MS COM / XPCOM Abstraction Layer - UTF-8 and UTF-16 string classes.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#include <iprt/log.h>

namespace com
{

// BSTR representing a null wide char with 32 bits of length prefix (0);
// this will work on Windows as well as other platforms where BSTR does
// not use length prefixes
const OLECHAR g_achEmptyBstr[3] = { 0, 0, 0 };
const BSTR g_bstrEmpty = (BSTR)&g_achEmptyBstr[2];

/* static */
const Bstr Bstr::Empty; /* default ctor is OK */

void Bstr::copyFromN(const char *a_pszSrc, size_t a_cchMax)
{
    /*
     * Initialie m_bstr first in case of throws further down in the code, then
     * check for empty input (m_bstr == NULL means empty, there are no NULL
     * strings).
     */
    m_bstr = NULL;
    if (!a_cchMax || !a_pszSrc || !*a_pszSrc)
        return;

    /*
     * Calculate the length and allocate a BSTR string buffer of the right
     * size, i.e. optimize heap usage.
     */
    size_t cwc;
    int vrc = ::RTStrCalcUtf16LenEx(a_pszSrc, a_cchMax, &cwc);
    if (RT_FAILURE(vrc))
    {
        /* ASSUME: input is valid Utf-8. Fake out of memory error. */
        AssertLogRelMsgFailed(("%Rrc %.*Rhxs\n", vrc, RTStrNLen(a_pszSrc, a_cchMax), a_pszSrc));
        throw std::bad_alloc();
    }

    m_bstr = ::SysAllocStringByteLen(NULL, cwc * sizeof(OLECHAR));
    if (RT_UNLIKELY(!m_bstr))
        throw std::bad_alloc();

    PRTUTF16 pwsz = (PRTUTF16)m_bstr;
    vrc = ::RTStrToUtf16Ex(a_pszSrc, a_cchMax, &pwsz, cwc + 1, NULL);
    if (RT_FAILURE(vrc))
    {
        /* This should not happen! */
        AssertRC(vrc);
        cleanup();
        throw std::bad_alloc();
    }
}


/* static */
const Utf8Str Utf8Str::Empty; /* default ctor is OK */

#if defined(VBOX_WITH_XPCOM)
void Utf8Str::cloneTo(char **pstr) const
{
    size_t cb = length() + 1;
    *pstr = (char*)nsMemory::Alloc(cb);
    if (RT_UNLIKELY(!*pstr))
        throw std::bad_alloc();
    memcpy(*pstr, c_str(), cb);
}

HRESULT Utf8Str::cloneToEx(char **pstr) const
{
    size_t cb = length() + 1;
    *pstr = (char*)nsMemory::Alloc(cb);
    if (RT_UNLIKELY(!*pstr))
        return E_OUTOFMEMORY;
    memcpy(*pstr, c_str(), cb);
    return S_OK;
}
#endif

Utf8Str& Utf8Str::stripTrailingSlash()
{
    if (length())
    {
        ::RTPathStripTrailingSlash(m_psz);
        jolt();
    }
    return *this;
}

Utf8Str& Utf8Str::stripFilename()
{
    if (length())
    {
        RTPathStripFilename(m_psz);
        jolt();
    }
    return *this;
}

Utf8Str& Utf8Str::stripPath()
{
    if (length())
    {
        char *pszName = ::RTPathFilename(m_psz);
        if (pszName)
        {
            size_t cchName = length() - (pszName - m_psz);
            memmove(m_psz, pszName, cchName + 1);
            jolt();
        }
        else
            cleanup();
    }
    return *this;
}

Utf8Str& Utf8Str::stripExt()
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
 * As with the RTCString::copyFrom() variants, this unconditionally sets the
 * members to a copy of the given other strings and makes no assumptions about
 * previous contents.  This can therefore be used both in copy constructors,
 * when member variables have no defined value, and in assignments after having
 * called cleanup().
 *
 * This variant converts from a UTF-16 string, most probably from
 * a Bstr assignment.
 *
 * @param   a_pbstr         The source string.  The caller guarantees that this
 *                          is valid UTF-16.
 *
 * @sa      RTCString::copyFromN
 */
void Utf8Str::copyFrom(CBSTR a_pbstr)
{
    if (a_pbstr && *a_pbstr)
    {
        int vrc = RTUtf16ToUtf8Ex((PCRTUTF16)a_pbstr,
                                  RTSTR_MAX,        // size_t cwcString: translate entire string
                                  &m_psz,           // char **ppsz: output buffer
                                  0,                // size_t cch: if 0, func allocates buffer in *ppsz
                                  &m_cch);          // size_t *pcch: receives the size of the output string, excluding the terminator.
        if (RT_SUCCESS(vrc))
            m_cbAllocated = m_cch + 1;
        else
        {
            if (   vrc != VERR_NO_STR_MEMORY
                && vrc != VERR_NO_MEMORY)
            {
                /* ASSUME: input is valid Utf-16. Fake out of memory error. */
                AssertLogRelMsgFailed(("%Rrc %.*Rhxs\n", vrc, RTUtf16Len(a_pbstr) * sizeof(RTUTF16), a_pbstr));
            }

            m_cch = 0;
            m_cbAllocated = 0;
            m_psz = NULL;

            throw std::bad_alloc();
        }
    }
    else
    {
        m_cch = 0;
        m_cbAllocated = 0;
        m_psz = NULL;
    }
}

/**
 * A variant of Utf8Str::copyFrom that does not throw any exceptions but returns
 * E_OUTOFMEMORY instead.
 *
 * @param   a_pbstr         The source string.
 * @returns S_OK or E_OUTOFMEMORY.
 */
HRESULT Utf8Str::copyFromEx(CBSTR a_pbstr)
{
    if (a_pbstr && *a_pbstr)
    {
        int vrc = RTUtf16ToUtf8Ex((PCRTUTF16)a_pbstr,
                                  RTSTR_MAX,        // size_t cwcString: translate entire string
                                  &m_psz,           // char **ppsz: output buffer
                                  0,                // size_t cch: if 0, func allocates buffer in *ppsz
                                  &m_cch);          // size_t *pcch: receives the size of the output string, excluding the terminator.
        if (RT_SUCCESS(vrc))
            m_cbAllocated = m_cch + 1;
        else
        {
            if (   vrc != VERR_NO_STR_MEMORY
                && vrc != VERR_NO_MEMORY)
            {
                /* ASSUME: input is valid Utf-16. Fake out of memory error. */
                AssertLogRelMsgFailed(("%Rrc %.*Rhxs\n", vrc, RTUtf16Len(a_pbstr) * sizeof(RTUTF16), a_pbstr));
            }

            m_cch = 0;
            m_cbAllocated = 0;
            m_psz = NULL;

            return E_OUTOFMEMORY;
        }
    }
    else
    {
        m_cch = 0;
        m_cbAllocated = 0;
        m_psz = NULL;
    }
    return S_OK;
}


/**
 * A variant of Utf8Str::copyFromN that does not throw any exceptions but
 * returns E_OUTOFMEMORY instead.
 *
 * @param   a_pcszSrc   The source string.
 * @param   a_cchSrc    The source string.
 * @returns S_OK or E_OUTOFMEMORY.
 *
 * @remarks This calls cleanup() first, so the caller doesn't have to. (Saves
 *          code space.)
 */
HRESULT Utf8Str::copyFromExNComRC(const char *a_pcszSrc, size_t a_cchSrc)
{
    cleanup();
    if (a_cchSrc)
    {
        m_psz = RTStrAlloc(a_cchSrc + 1);
        if (RT_LIKELY(m_psz))
        {
            m_cch = a_cchSrc;
            m_cbAllocated = a_cchSrc + 1;
            memcpy(m_psz, a_pcszSrc, a_cchSrc);
            m_psz[a_cchSrc] = '\0';
        }
        else
        {
            m_cch = 0;
            m_cbAllocated = 0;
            return E_OUTOFMEMORY;
        }
    }
    else
    {
        m_cch = 0;
        m_cbAllocated = 0;
        m_psz = NULL;
    }
    return S_OK;
}

} /* namespace com */
