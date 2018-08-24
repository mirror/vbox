/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestOutputToString implementation.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/cpp/restbase.h>

#include <iprt/err.h>
#include <iprt/string.h>


RTCRestOutputToString::RTCRestOutputToString(RTCString *a_pDst)
    : m_pDst(a_pDst)
    , m_fOutOfMemory(false)
{
}


RTCRestOutputToString::~RTCRestOutputToString()
{
    /* We don't own the string, so we don't delete it! */
    m_pDst = NULL;
}


/**
 * @callback_method_impl{FNRTSTROUTPUT}
 */
/*static*/ DECLCALLBACK(size_t)
RTCRestOutputToString::strOutput(void *pvArg, const char *pachChars, size_t cbChars)
{
    RTCRestOutputToString *pThis = (RTCRestOutputToString *)pvArg;
    if (cbChars)
    {
        RTCString *pDst = pThis->m_pDst;
        if (pDst && !pThis->m_fOutOfMemory)
        {
            /*
             * Make sure we've got sufficient space available before we append.
             */
            size_t cchCurrent = pDst->length();
            size_t cbCapacity = pDst->capacity();
            size_t cbNeeded   = cchCurrent + cbChars + 1;
            if (cbNeeded <= cbCapacity)
            { /* likely */ }
            else
            {
                /* Grow it. */
                if (cbNeeded < _16M)
                {
                    if (cbCapacity <= _1K)
                        cbCapacity = _1K;
                    else
                        cbCapacity = RT_ALIGN_Z(cbCapacity, _1K);
                    while (cbCapacity < cbNeeded)
                        cbCapacity <<= 1;
                }
                else
                {
                    cbCapacity = RT_ALIGN_Z(cbCapacity, _2M);
                    while (cbCapacity < cbNeeded)
                        cbCapacity += _2M;
                }
                int rc = pDst->reserveNoThrow(cbCapacity);
                if (RT_SUCCESS(rc))
                {
                    rc = pDst->reserveNoThrow(cbNeeded);
                    if (RT_FAILURE(rc))
                    {
                        pThis->m_fOutOfMemory = true;
                        return cbChars;
                    }
                }
            }

            /*
             * Do the appending.
             */
            pDst->append(pachChars, cbChars);
        }
    }
    return cbChars;
}


size_t RTCRestOutputToString::vprintf(const char *pszFormat, va_list va)
{
    return RTStrFormatV(strOutput, this, NULL, NULL, pszFormat, va);
}


RTCString *RTCRestOutputToString::finalize()
{
    RTCString *pRet;
    if (!m_fOutOfMemory)
        pRet = m_pDst;
    else
        pRet = NULL;
    m_pDst = NULL;
    return pRet;
}

