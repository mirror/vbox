/* $Id$ */
/** @file
 * IPRT - Testcase Framework, the implicit test handle API variation.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <iprt/test.h>
#include <iprt/stdarg.h>


RTR3DECL(int) RTTestIPrintfV(RTTESTLVL enmLevel, const char *pszFormat, va_list va)
{
    return RTTestPrintfV(NIL_RTTEST, enmLevel, pszFormat, va);
}


RTR3DECL(int) RTTestIPrintf(RTTESTLVL enmLevel, const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = RTTestPrintfV(NIL_RTTEST, enmLevel, pszFormat, va);
    va_end(va);
    return cch;
}


RTR3DECL(int) RTTestISub(const char *pszSubTest)
{
    return RTTestSub(NIL_RTTEST, pszSubTest);
}


RTR3DECL(int) RTTestISubF(const char *pszSubTestFmt, ...)
{
    va_list va;
    va_start(va, pszSubTestFmt);
    int cch = RTTestSubV(NIL_RTTEST, pszSubTestFmt, va);
    va_end(va);
    return cch;
}


RTR3DECL(int) RTTestISubV(const char *pszSubTestFmt, va_list va)
{
    return RTTestSubV(NIL_RTTEST, pszSubTestFmt, va);
}


RTR3DECL(int) RTTestISubDone(void)
{
    return RTTestSubDone(NIL_RTTEST);
}


RTR3DECL(int) RTTestIPassedV(const char *pszFormat, va_list va)
{
    return RTTestPassedV(NIL_RTTEST, pszFormat, va);
}


RTR3DECL(int) RTTestIPassed(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = RTTestPassedV(NIL_RTTEST, pszFormat, va);
    va_end(va);
    return cch;
}


RTR3DECL(int) RTTestIErrorInc(void)
{
    return RTTestErrorInc(NIL_RTTEST);
}


RTR3DECL(int) RTTestIFailedV(const char *pszFormat, va_list va)
{
    return RTTestFailedV(NIL_RTTEST, pszFormat, va);
}


RTR3DECL(int) RTTestIFailed(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = RTTestFailedV(NIL_RTTEST, pszFormat, va);
    va_end(va);
    return cch;
}


RTR3DECL(int) RTTestIFailureDetailsV(const char *pszFormat, va_list va)
{
    return RTTestFailureDetails(NIL_RTTEST, pszFormat, va);
}


RTR3DECL(int) RTTestIFailureDetails(const char *pszFormat, ...)
{
    va_list va;
    va_start(va, pszFormat);
    int cch = RTTestFailureDetailsV(NIL_RTTEST, pszFormat, va);
    va_end(va);
    return cch;
}

