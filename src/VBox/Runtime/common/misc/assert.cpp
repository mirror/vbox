/* $Id$ */
/** @file
 * IPRT - Assertion Workers.
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
#include <iprt/assert.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#ifdef IN_RING3
# include <stdio.h>
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Set if assertions are quiet. */
static bool volatile g_fQuiet = false;
/** Set if assertions may panic. */
static bool volatile g_fMayPanic = true;


RTDECL(bool) RTAssertSetQuiet(bool fQuiet)
{
    return ASMAtomicXchgBool(&g_fQuiet, fQuiet);
}


RTDECL(bool) RTAssertAreQuiet(void)
{
    return ASMAtomicUoReadBool(&g_fQuiet);
}


RTDECL(bool) RTAssertSetMayPanic(bool fMayPanic)
{
    return ASMAtomicXchgBool(&g_fMayPanic, fMayPanic);
}


RTDECL(bool) RTAssertMayPanic(void)
{
    return ASMAtomicUoReadBool(&g_fMayPanic);
}


#ifdef IN_RING0

/* OS specific.  */

#else /* !IN_RING0 */


/** The last assert message, 1st part. */
RTDATADECL(char) g_szRTAssertMsg1[1024];
/** The last assert message, 2nd part. */
RTDATADECL(char) g_szRTAssertMsg2[2048];

/**
 * The 1st part of an assert message.
 *
 * @param   pszExpr     Expression. Can be NULL.
 * @param   uLine       Location line number.
 * @param   pszFile     Location file name.
 * @param   pszFunction Location function name.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void)    AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    if (!RTAssertAreQuiet())
    {
#if !defined(IN_RING3) && !defined(LOG_NO_COM)
        RTLogComPrintf("\n!!Assertion Failed!!\n"
                       "Expression: %s\n"
                       "Location  : %s(%d) %s\n",
                       pszExpr, pszFile, uLine, pszFunction);
#endif

        PRTLOGGER pLog = RTLogRelDefaultInstance();
        if (pLog)
        {
            RTLogRelPrintf("\n!!Assertion Failed!!\n"
                           "Expression: %s\n"
                           "Location  : %s(%d) %s\n",
                           pszExpr, pszFile, uLine, pszFunction);
#ifndef IN_RC /* flushing is done automatically in RC */
            RTLogFlush(pLog);
#endif
        }

#ifndef LOG_ENABLED
        if (!pLog)
#endif
        {
            pLog = RTLogDefaultInstance();
            if (pLog)
            {
                RTLogPrintf("\n!!Assertion Failed!!\n"
                            "Expression: %s\n"
                            "Location  : %s(%d) %s\n",
                            pszExpr, pszFile, uLine, pszFunction);
#ifndef IN_RC /* flushing is done automatically in RC */
                RTLogFlush(pLog);
#endif
            }
        }

#ifdef IN_RING3
        /* print to stderr, helps user and gdb debugging. */
        fprintf(stderr,
                "\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                VALID_PTR(pszExpr) ? pszExpr : "<none>",
                VALID_PTR(pszFile) ? pszFile : "<none>",
                uLine,
                VALID_PTR(pszFunction) ? pszFunction : "");
        fflush(stderr);
#endif
    }

    RTStrPrintf(g_szRTAssertMsg1, sizeof(g_szRTAssertMsg1),
                "\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
}


/**
 * The 2nd (optional) part of an assert message.
 *
 * @param   pszFormat   Printf like format string.
 * @param   ...         Arguments to that string.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void)    AssertMsg2(const char *pszFormat, ...)
{
    va_list args;

    if (!RTAssertAreQuiet())
    {
#if !defined(IN_RING3) && !defined(LOG_NO_COM)
        va_start(args, pszFormat);
        RTLogComPrintfV(pszFormat, args);
        va_end(args);
#endif

        PRTLOGGER pLog = RTLogRelDefaultInstance();
        if (pLog)
        {
            va_start(args, pszFormat);
            RTLogRelPrintfV(pszFormat, args);
            va_end(args);
#ifndef IN_RC /* flushing is done automatically in RC */
            RTLogFlush(pLog);
#endif
        }

        pLog = RTLogDefaultInstance();
        if (pLog)
        {
            va_start(args, pszFormat);
            RTLogPrintfV(pszFormat, args);
            va_end(args);
#ifndef IN_RC /* flushing is done automatically in RC */
            RTLogFlush(pLog);
#endif
        }

#ifdef IN_RING3
        /* print to stderr, helps user and gdb debugging. */
        char szMsg[1024];
        va_start(args, pszFormat);
        RTStrPrintfV(szMsg, sizeof(szMsg), pszFormat, args);
        va_end(args);
        fprintf(stderr, "%s", szMsg);
        fflush(stderr);
#endif
    }

    va_start(args, pszFormat);
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, args);
    va_end(args);
}

#endif /* !IN_RING0 */

