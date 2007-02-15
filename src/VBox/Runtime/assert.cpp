/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Assertion Workers.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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


#elif defined(IN_RING0)


#if 0 /* this code is totally unused */

#include <VBox/sup.h>


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
    SUPR0Printf("\n!!Assertion Failed!!\n"
                "Expression: %s\n"
                "Location  : %s(%d) %s\n",
                pszExpr, pszFile, uLine, pszFunction);
#if !defined(IN_RING3) && !defined(LOG_NO_COM)
    RTLogComPrintf("\n!!Assertion Failed!!\n"
                   "Expression: %s\n"
                   "Location  : %s(%d) %s\n",
                   pszExpr, pszFile, uLine, pszFunction);
#endif
}


/**
 * The 2nd (optional) part of an assert message.
 *
 * @param   pszFormat   Printf like format string.
 * @param   ...         Arguments to that string.
 * @remark  This API exists in HC Ring-3 and GC.
 */
#ifdef __GNUC__
/* asm (".globl AssertMsg2; AssertMsg2: jmp *SUPR0Printf"); - DEADLY! */
#else
__declspec(naked) void AssertMsg2(const char *pszFormat, ...)
{   /* forwarder. */
    __asm jmp dword ptr [SUPR0Printf];
}
#endif

#endif /* dead code */

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
            pszExpr, pszFile, uLine, pszFunction);
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

