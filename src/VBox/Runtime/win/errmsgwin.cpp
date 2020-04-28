/* $Id$ */
/** @file
 * IPRT - Status code messages, Windows.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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
#include <iprt/win/windows.h>

#include <iprt/errcore.h>
#include <iprt/asm.h>
#include <iprt/string.h>

#include <iprt/bldprog-strtab.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#if !defined(IPRT_ERRMSG_DEFINES_ONLY) && defined(IN_RT_STATIC) /* No message text in static builds to save space. */
# define IPRT_ERRMSG_DEFINES_ONLY
#endif

#ifdef IPRT_ERRMSG_DEFINES_ONLY
# define ENTRY(a_pszMsg, a_pszDefine, a_iCode) \
    { a_pszDefine, a_pszDefine, a_iCode }
#else
# define ENTRY(a_pszMsg, a_pszDefine, a_iCode) \
    { a_pszMsg, a_pszDefine, a_iCode }
#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef IPRT_ERRMSG_DEFINES_ONLY
/* Cook data just for VINF_SUCCESS so that code below compiles fine. */
static const char            g_achWinStrTabData[] = { "ERROR_SUCCESS" };
static const RTBLDPROGSTRTAB g_WinMsgStrTab = { g_achWinStrTabData, sizeof(g_achWinStrTabData) - 1, 0, NULL };
static const struct
{
    int16_t     iCode;
    uint8_t     offDefine;
    uint8_t     cchDefine;
    uint8_t     offMsgFull;
    uint8_t     cchMsgFull;
} g_aWinMsgs[] =
{
    { 0, 0, 13, 0, 13, },
};
#elif defined(IPRT_ERRMSG_DEFINES_ONLY)
# include "errmsgwindata-only-defines.h"
#else
# include "errmsgwindata-all.h"
#endif


/**
 * Looks up the message table entry for @a rc.
 *
 * @returns index into g_aWinMsgs on success, ~(size_t)0 if not found.
 * @param   rc      The status code to locate the entry for.
 */
static size_t rtErrWinLookup(long rc)
{
    /*
     * Perform binary search (duplicate code in rtErrLookup).
     */
    size_t iStart = 0;
    size_t iEnd   = RT_ELEMENTS(g_aWinMsgs);
    for (;;)
    {
        size_t i = iStart + (iEnd - iStart) / 2;
        long const iCode = g_aWinMsgs[i].iCode;
        if (rc < iCode)
        {
            if (iStart < i)
                iEnd = i;
            else
                break;
        }
        else if (rc > iCode)
        {
            i++;
            if (i < iEnd)
                iStart = i;
            else
                break;
        }
        else
            return i;
    }

#ifdef RT_STRICT
    for (size_t i = 0; i < RT_ELEMENTS(g_aWinMsgs); i++)
        Assert(g_aWinMsgs[i].iCode != rc);
#endif

    return ~(size_t)0;
}


RTDECL(bool)    RTErrWinIsKnown(long rc)
{
    if (rtErrWinLookup(rc) != ~(size_t)0)
        return true;
    if (SCODE_FACILITY(rc) == FACILITY_WIN32)
    {
        if (rtErrWinLookup(HRESULT_CODE(rc)) != ~(size_t)0)
            return true;
    }
    return false;
}


RTDECL(ssize_t) RTErrWinQueryDefine(long rc, char *pszBuf, size_t cbBuf, bool fFailIfUnknown)
{
    size_t idx = rtErrWinLookup(rc);
    if (idx != ~(size_t)0)
        return RTBldProgStrTabQueryString(&g_WinMsgStrTab,
                                          g_aWinMsgs[idx].offDefine, g_aWinMsgs[idx].cchDefine,
                                          pszBuf, cbBuf);

    /*
     * If FACILITY_WIN32 kind of status, look up the win32 code.
     */
    if (   SCODE_FACILITY(rc) == FACILITY_WIN32
        && (idx = rtErrWinLookup(HRESULT_CODE(rc))) != ~(size_t)0)
    {
        /* Append the incoming rc, so we know it's not a regular WIN32 status: */
        ssize_t cchRet = RTBldProgStrTabQueryString(&g_WinMsgStrTab,
                                                    g_aWinMsgs[idx].offDefine, g_aWinMsgs[idx].cchDefine,
                                                    pszBuf, cbBuf);
        if (cchRet > 0)
        {
            pszBuf[cchRet++] = '/';
            return RTStrFormatU32(pszBuf + cchRet, cbBuf - cchRet, rc, 16, 0, 0, RTSTR_F_SPECIAL);
        }
        return VERR_BUFFER_OVERFLOW;
    }

    if (fFailIfUnknown)
        return VERR_NOT_FOUND;
    return RTStrFormatU32(pszBuf, cbBuf, rc, 16, 0, 0, RTSTR_F_SPECIAL);
}


RTDECL(size_t)  RTErrWinFormatDefine(long rc, PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, char *pszTmp, size_t cbTmp)
{
    RT_NOREF(pszTmp, cbTmp);
    size_t idx = rtErrWinLookup(rc);
    if (idx != ~(size_t)0)
        return RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                          g_aWinMsgs[idx].offDefine, g_aWinMsgs[idx].cchDefine,
                                          pfnOutput, pvArgOutput);

    /*
     * If FACILITY_WIN32 kind of status, look up the win32 code.
     */
    size_t cchRet = 0;
    if (   SCODE_FACILITY(rc) == FACILITY_WIN32
        && (idx = rtErrWinLookup(HRESULT_CODE(rc))) != ~(size_t)0)
    {
        /* Append the incoming rc, so we know it's not a regular WIN32 status: */
        cchRet  = RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                             g_aWinMsgs[idx].offDefine, g_aWinMsgs[idx].cchDefine,
                                             pfnOutput, pvArgOutput);
        cchRet += pfnOutput(pvArgOutput, RT_STR_TUPLE("/"));
    }

    ssize_t cchValue = RTStrFormatU32(pszTmp, cbTmp, rc, 16, 0, 0, RTSTR_F_SPECIAL);
    Assert(cchValue > 0);
    cchRet += pfnOutput(pvArgOutput, pszTmp, cchValue);
    return cchRet;
}


RTDECL(size_t)  RTErrWinFormatMsg(long rc, PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, char *pszTmp, size_t cbTmp)
{
#ifdef IPRT_ERRMSG_DEFINES_ONLY
    return RTErrWinFormatDefine(rc, pfnOutput, pvArgOutput, pszTmp, cbTmp);
#else
    size_t idx = rtErrWinLookup(rc);
    if (idx != ~(size_t)0)
        return RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                          g_aWinMsgs[idx].offMsgFull, g_aWinMsgs[idx].cchMsgFull,
                                          pfnOutput, pvArgOutput);

    /*
     * If FACILITY_WIN32 kind of status, look up the win32 code.
     */
    const char *pszTail = NULL;
    size_t cchRet;
    if (   SCODE_FACILITY(rc) == FACILITY_WIN32
        && (idx = rtErrWinLookup(HRESULT_CODE(rc))) != ~(size_t)0)
    {
        /* Append the incoming rc, so we know it's not a regular WIN32 status: */
        cchRet  = RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                             g_aWinMsgs[idx].offMsgFull, g_aWinMsgs[idx].cchMsgFull,
                                             pfnOutput, pvArgOutput);
        cchRet += pfnOutput(pvArgOutput, RT_STR_TUPLE(" ("));
        pszTail = ")";
    }
    else
        cchRet  = pfnOutput(pvArgOutput, RT_STR_TUPLE("Unknown Status "));

    ssize_t cchValue = RTStrFormatU32(pszTmp, cbTmp, rc, 16, 0, 0, RTSTR_F_SPECIAL);
    Assert(cchValue > 0);
    cchRet += pfnOutput(pvArgOutput, pszTmp, cchValue);

    if (pszTail)
        cchRet += pfnOutput(pvArgOutput, pszTail, 1);
    return cchRet;
#endif
}


RTDECL(size_t)  RTErrWinFormatMsgAll(long rc, PFNRTSTROUTPUT pfnOutput, void *pvArgOutput, char *pszTmp, size_t cbTmp)
{
    RT_NOREF(pszTmp, cbTmp);
    size_t cchRet;
    size_t idx = rtErrWinLookup(rc);
    if (   idx != ~(size_t)0
        || (   SCODE_FACILITY(rc) == FACILITY_WIN32
            && (idx = rtErrWinLookup(HRESULT_CODE(rc))) != ~(size_t)0))
    {
        cchRet  = RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                             g_aWinMsgs[idx].offDefine, g_aWinMsgs[idx].cchDefine,
                                             pfnOutput, pvArgOutput);
        cchRet += pfnOutput(pvArgOutput, RT_STR_TUPLE(" ("));
    }
    else
        cchRet  = pfnOutput(pvArgOutput, RT_STR_TUPLE("Unknown Status "));

    ssize_t cchValue = RTStrFormatU32(pszTmp, cbTmp, rc, 16, 0, 0, RTSTR_F_SPECIAL);
    Assert(cchValue > 0);
    cchRet += pfnOutput(pvArgOutput, pszTmp, cchValue);

#ifndef IPRT_ERRMSG_DEFINES_ONLY
    if (idx != ~(size_t)0)
    {
        cchRet += pfnOutput(pvArgOutput, RT_STR_TUPLE(" - "));
        cchRet += RTBldProgStrTabQueryOutput(&g_WinMsgStrTab,
                                             g_aWinMsgs[idx].offMsgFull, g_aWinMsgs[idx].cchMsgFull,
                                             pfnOutput, pvArgOutput);
    }
#endif
    return cchRet;
}

