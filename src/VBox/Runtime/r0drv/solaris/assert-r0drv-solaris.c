/* $Id$ */
/** @file
 * IPRT - Assertion Workers, Ring-0 Drivers, Solaris.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"

#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include <iprt/asm.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The last assert message, 1st part. */
RTDATADECL(char)                    g_szRTAssertMsg1[1024];
/** The last assert message, 2nd part. */
RTDATADECL(char)                    g_szRTAssertMsg2[2048];
/** The last assert message, file name. */
RTDATADECL(const char *) volatile   g_pszRTAssertFile;
/** The last assert message, line number. */
RTDATADECL(uint32_t) volatile       g_u32RTAssertLine;
/** The last assert message, function name. */
RTDATADECL(const char *) volatile   g_pszRTAssertFunction;


RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{

#ifdef IN_GUEST_R0
    RTLogBackdoorPrintf("\n!!Assertion Failed!!\n"
                        "Expression: %s\n"
                        "Location  : %s(%d) %s\n",
                        pszExpr, pszFile, uLine, pszFunction);
#endif

    uprintf("\r\n!!Assertion Failed!!\r\n"
            "Expression: %s\r\n"
            "Location  : %s(%d) %s\r\n",
            pszExpr, pszFile, uLine, pszFunction);

    RTStrPrintf(g_szRTAssertMsg1, sizeof(g_szRTAssertMsg1),
                "\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
    ASMAtomicUoWritePtr(&g_pszRTAssertFile, pszFile);
    ASMAtomicUoWriteU32(&g_u32RTAssertLine, uLine);
    ASMAtomicUoWritePtr(&g_pszRTAssertFunction, pszFunction);
}


RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{
    va_list va;
    char    szMsg[256];

#ifdef IN_GUEST_R0
    va_start(va, pszFormat);
    RTLogBackdoorPrintfV(pszFormat, va);
    va_end(va);
#endif

    va_start(va, pszFormat);
    RTStrPrintfV(szMsg, sizeof(szMsg) - 1, pszFormat, va);
    szMsg[sizeof(szMsg) - 1] = '\0';
    va_end(va);
    uprintf("%s", szMsg);

    va_start(va, pszFormat);
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, va);
    va_end(va);
}


RTR0DECL(void) RTR0AssertPanicSystem(void)
{
    const char *psz    = &g_szRTAssertMsg2[0];
    const char *pszEnd = &g_szRTAssertMsg2[sizeof(g_szRTAssertMsg2)];
    while (psz < pszEnd && (*psz == ' ' || *psz == '\t' || *psz == '\n' || *psz == '\r'))
        psz++;

    if (psz >= pszEnd || *psz)
        assfail(psz, g_pszRTAssertFile, g_u32RTAssertLine);
    else
        assfail(g_szRTAssertMsg1, g_pszRTAssertFile, g_u32RTAssertLine);
    g_szRTAssertMsg2[0] = '\0';
}

