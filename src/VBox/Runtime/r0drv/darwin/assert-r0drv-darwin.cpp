/* $Id$ */
/** @file
 * innotek Portable Runtime -  Assertion Workers, Ring-0 Drivers, Darwin.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"

#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The last assert message, 1st part. */
RTDATADECL(char) g_szRTAssertMsg1[1024];
/** The last assert message, 2nd part. */
RTDATADECL(char) g_szRTAssertMsg2[2048];


RTDECL(void) AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
#ifdef IN_GUEST_R0
    RTLogBackdoorPrintf("\n!!Assertion Failed!!\n"
                        "Expression: %s\n"
                        "Location  : %s(%d) %s\n",
                        pszExpr, pszFile, uLine, pszFunction);
#endif

    printf("\r\n!!Assertion Failed!!\r\n"
           "Expression: %s\r\n"
           "Location  : %s(%d) %s\r\n",
           pszExpr, pszFile, uLine, pszFunction);

    RTStrPrintf(g_szRTAssertMsg1, sizeof(g_szRTAssertMsg1),
                "\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
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
    printf("%s", szMsg);

    va_start(va, pszFormat);
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, va);
    va_end(va);

    panic("%s%s", g_szRTAssertMsg1, g_szRTAssertMsg2);
}

