/* $Id$ */
/** @file
 * innotek Portable Runtime - Assertion Workers.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#ifdef IN_RING3
# include <stdio.h>
#endif


#ifdef IN_GUEST_R0
#include <VBox/log.h>


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
    RTLogBackdoorPrintf("\n!!Assertion Failed!!\n"
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
RTDECL(void) AssertMsg2(const char *pszFormat, ...)
{   /* forwarder. */
    va_list args;
    va_start(args, pszFormat);
    RTLogBackdoorPrintfV(pszFormat, args);
    va_end(args);
}

#if defined(RT_OS_LINUX) && defined(IN_MODULE)
/*
 * When we build this in the Linux kernel module, we wish to make the
 * symbols available to other modules as well.
 */
# include "the-linux-kernel.h"
EXPORT_SYMBOL(AssertMsg1);
EXPORT_SYMBOL(AssertMsg2);
#endif

#elif defined(IN_RING0)

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
        RTLogFlush(pLog);
    }

    pLog = RTLogDefaultInstance();
    if (pLog)
    {
        RTLogPrintf("\n!!Assertion Failed!!\n"
                    "Expression: %s\n"
                    "Location  : %s(%d) %s\n",
                    pszExpr, pszFile, uLine, pszFunction);
        RTLogFlush(pLog);
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
        RTLogFlush(pLog);
    }

    pLog = RTLogDefaultInstance();
    if (pLog)
    {
        va_start(args, pszFormat);
        RTLogPrintfV(pszFormat, args);
        va_end(args);
        RTLogFlush(pLog);
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

    va_start(args, pszFormat);
    RTStrPrintfV(g_szRTAssertMsg2, sizeof(g_szRTAssertMsg2), pszFormat, args);
    va_end(args);
}

#endif /* !IN_RING0 */

