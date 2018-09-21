/* $Id$ */
/** @file
 * IPRT - C++ REST, RTCRestOutputPrettyBase implementation.
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
#define LOG_GROUP RTLOGGROUP_REST
#include <iprt/cpp/restoutput.h>

#include <iprt/err.h>
#include <iprt/string.h>


RTCRestOutputPrettyBase::RTCRestOutputPrettyBase()
    : RTCRestOutputBase()
{
}


RTCRestOutputPrettyBase::~RTCRestOutputPrettyBase()
{
}


uint32_t RTCRestOutputPrettyBase::beginArray()
{
    output(RT_STR_TUPLE("["));
    uint32_t const uOldState = m_uState;
    m_uState = (uOldState & 0xffff) + 1;
    return uOldState;
}


void RTCRestOutputPrettyBase::endArray(uint32_t a_uOldState)
{
    m_uState = a_uOldState;
    output(RT_STR_TUPLE("\n"));
    outputIndentation();
    output(RT_STR_TUPLE("]"));
}


uint32_t RTCRestOutputPrettyBase::beginObject()
{
    output(RT_STR_TUPLE("{"));
    uint32_t const uOldState = m_uState;
    m_uState = (uOldState & 0xffff) + 1;
    return uOldState;
}


void RTCRestOutputPrettyBase::endObject(uint32_t a_uOldState)
{
    m_uState = a_uOldState;
    output(RT_STR_TUPLE("\n"));
    outputIndentation();
    output(RT_STR_TUPLE("}"));
}


void RTCRestOutputPrettyBase::valueSeparator()
{
    if (m_uState & RT_BIT_32(31))
        output(RT_STR_TUPLE(",\n"));
    else
    {
        m_uState |= RT_BIT_32(31);
        output(RT_STR_TUPLE("\n"));
    }
    outputIndentation();
}


void RTCRestOutputPrettyBase::valueSeparatorAndName(const char *a_pszName, size_t a_cchName)
{
    RT_NOREF(a_cchName);
    if (m_uState & RT_BIT_32(31))
        output(RT_STR_TUPLE(",\n"));
    else
    {
        m_uState |= RT_BIT_32(31);
        output(RT_STR_TUPLE("\n"));
    }
    outputIndentation();
    printf("%RMjs: ", a_pszName);
}


void RTCRestOutputPrettyBase::outputIndentation()
{
    static char const s_szSpaces[] = "                                                                                         ";
    size_t cchIndent = (m_uState & 0xffff) << 1;
    while (cchIndent > 0)
    {
        size_t cbToWrite = RT_MIN(cchIndent, sizeof(s_szSpaces) - 1);
        output(s_szSpaces, cbToWrite);
        cchIndent -= cbToWrite;
    }
}

