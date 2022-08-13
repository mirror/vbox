/* $Id$ */
/** @file
 * IPRT Testcase - Testcase for the No-CRT math bits.
 */

/*
 * Copyright (C) 2008-2022 Oracle Corporation
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
#if defined(RT_WITHOUT_NOCRT_WRAPPERS) || !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
# error "Build config error."
#endif

#include <math.h>
#include <float.h>

#define IPRT_NOCRT_WITHOUT_MATH_CONSTANTS /* so we can include both the CRT one and our no-CRT header */
#include <iprt/nocrt/math.h>

#include <iprt/string.h>
#include <iprt/test.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define CHECK_DBL(a_Expr, a_rdExpect) do { \
        RTFLOAT64U uRet; \
        uRet.rd = (a_Expr); \
        RTFLOAT64U uExpect; \
        uExpect.rd = a_rdExpect; \
        if (!RTFLOAT64U_ARE_IDENTICAL(&uRet, &uExpect)) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uExpect, 0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s -> %s, expected %s (%s)", \
                         __LINE__, #a_Expr, g_szFloat[0], g_szFloat[1], #a_rdExpect); \
        } \
    } while (0)

#define CHECK_DBL_SAME(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.rd = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.rd   =          a_Fn  a_Args; \
        if (!RTFLOAT64U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet)) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

#define CHECK_DBL_APPROX_SAME(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.rd = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.rd   =          a_Fn  a_Args; \
        if (   !RTFLOAT64U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (  (uNoCrtRet.u >= uCrtRet.u ? uNoCrtRet.u - uCrtRet.u : uCrtRet.u - uNoCrtRet.u) > 1 /* off by one is okay */ \
                || RTFLOAT64U_IS_NAN(&uNoCrtRet) \
                || RTFLOAT64U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RTTEST  g_hTest;
char    g_szFloat[2][128];




void testExp2()
{
    RTTestSub(g_hTest, "exp2");

    CHECK_DBL(RT_NOCRT(exp2)(1.0), 2.0);
    CHECK_DBL(RT_NOCRT(exp2)(2.0), 4.0);
    CHECK_DBL(RT_NOCRT(exp2)(32.0), 4294967296.0);
    CHECK_DBL(RT_NOCRT(exp2)(-1.0), 0.5);
    CHECK_DBL(RT_NOCRT(exp2)(-3.0), 0.125);
    CHECK_DBL_SAME(exp2, (0.0));
    CHECK_DBL_SAME(exp2, (+INFINITY));
    CHECK_DBL_SAME(exp2, (-INFINITY));
    CHECK_DBL_SAME(exp2, (nan("1")));
    CHECK_DBL_SAME(exp2, (RTStrNanDouble("ab305f", true)));
    CHECK_DBL_SAME(exp2, (RTStrNanDouble("fffffffff_signaling", true)));
    CHECK_DBL_SAME(exp2, (RTStrNanDouble("7777777777778_sig", false)));
    CHECK_DBL_SAME(exp2, (1.0));
    CHECK_DBL_SAME(exp2, (2.0));
    CHECK_DBL_SAME(exp2, (-1.0));
    CHECK_DBL_APPROX_SAME(exp2, (+0.5));
    CHECK_DBL_APPROX_SAME(exp2, (-0.5));
    CHECK_DBL_APPROX_SAME(exp2, (+1.5));
    CHECK_DBL_APPROX_SAME(exp2, (-1.5));
    CHECK_DBL_APPROX_SAME(exp2, (+3.25));
    CHECK_DBL_APPROX_SAME(exp2, (99.2559430));
    CHECK_DBL_APPROX_SAME(exp2, (-99.2559430));
    CHECK_DBL_APPROX_SAME(exp2, (+305.2559430));
    CHECK_DBL_APPROX_SAME(exp2, (-305.2559430));
    CHECK_DBL_APPROX_SAME(exp2, (+309.99884));
    CHECK_DBL_APPROX_SAME(exp2, (-309.111048));
    CHECK_DBL_APPROX_SAME(exp2, (+999.864597634));
    CHECK_DBL_APPROX_SAME(exp2, (-999.098234837));


    RTTestSub(g_hTest, "exp2f");
    CHECK_DBL(RT_NOCRT(exp2f)(1.0f), 2.0f);
    CHECK_DBL(RT_NOCRT(exp2f)(2.0f), 4.0f);
    CHECK_DBL(RT_NOCRT(exp2f)(32.0f), 4294967296.0f);
    CHECK_DBL(RT_NOCRT(exp2f)(-1.0f), 0.5f);
    CHECK_DBL(RT_NOCRT(exp2f)(-3.0f), 0.125f);
    CHECK_DBL_SAME(exp2f, (0.0f));
    CHECK_DBL_SAME(exp2f, (+INFINITY));
    CHECK_DBL_SAME(exp2f, (-INFINITY));
    CHECK_DBL_SAME(exp2f, (nan("1")));
    CHECK_DBL_SAME(exp2f, (RTStrNanFloat("ab305f", true)));
    CHECK_DBL_SAME(exp2f, (RTStrNanFloat("3fffff_signaling", true)));
    CHECK_DBL_SAME(exp2f, (RTStrNanFloat("79778_sig", false)));
    CHECK_DBL_SAME(exp2f, (1.0f));
    CHECK_DBL_SAME(exp2f, (2.0f));
    CHECK_DBL_SAME(exp2f, (-1.0f));
    CHECK_DBL_APPROX_SAME(exp2f, (+0.5f));
    CHECK_DBL_APPROX_SAME(exp2f, (-0.5f));
    CHECK_DBL_APPROX_SAME(exp2f, (+1.5f));
    CHECK_DBL_APPROX_SAME(exp2f, (-1.5f));
    CHECK_DBL_APPROX_SAME(exp2f, (+3.25f));
    CHECK_DBL_APPROX_SAME(exp2f, (99.25594f));
    CHECK_DBL_APPROX_SAME(exp2f, (-99.25594f));
    CHECK_DBL_APPROX_SAME(exp2f, (+305.25594f));
    CHECK_DBL_APPROX_SAME(exp2f, (-305.25594f));
    CHECK_DBL_APPROX_SAME(exp2f, (+309.99884f));
    CHECK_DBL_APPROX_SAME(exp2f, (-309.111048f));
    CHECK_DBL_APPROX_SAME(exp2f, (+999.86459f));
    CHECK_DBL_APPROX_SAME(exp2f, (-999.09823f));
}

int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTNoCrt-2", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    testExp2();



    return RTTestSummaryAndDestroy(g_hTest);
}

