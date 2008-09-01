/* $Id$ */
/** @file
 * IPRT - Assertion Workers, Ring-0 Drivers, NT.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-nt-kernel.h"

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

    DbgPrint("\n!!Assertion Failed!!\n"
             "Expression: %s\n"
             "Location  : %s(%d) %s\n",
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

# ifdef IN_GUEST_R0
    va_start(va, pszFormat);
    RTLogBackdoorPrintfV(pszFormat, va);
    va_end(va);
# endif

    va_start(va, pszFormat);
    RTStrPrintfV(szMsg, sizeof(szMsg) - 1, pszFormat, va);
    szMsg[sizeof(szMsg) - 1] = '\0';
    va_end(va);
    DbgPrint("%s", szMsg);

    va_start(va, pszFormat);
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, va);
    va_end(va);
}

