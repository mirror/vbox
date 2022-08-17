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

#include <float.h>
#include <limits.h>
#include <math.h>

#define IPRT_NO_CRT_FOR_3RD_PARTY
#define IPRT_NOCRT_WITHOUT_CONFLICTING_CONSTANTS /* so we can include both the CRT one and our no-CRT header */
#define IPRT_NOCRT_WITHOUT_CONFLICTING_TYPES     /* so we can include both the CRT one and our no-CRT header */
#include <iprt/nocrt/math.h>
#define IPRT_INCLUDED_nocrt_limits_h /* prevent our limits from being included */
#include <iprt/nocrt/stdlib.h>
#include <iprt/nocrt/fenv.h>        /* Need to test fegetround and stuff. */

#include <iprt/string.h>
#include <iprt/test.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/x86.h>
#endif

/* Stuff we provide in our math, but UCRT apparently doesn't: */
#ifndef  M_E
# define M_E     2.7182818284590452354   /* e */
#endif
#ifndef  M_LOG2E
# define M_LOG2E     1.4426950408889634074   /* log 2e */
#endif
#ifndef  M_LOG10E
# define M_LOG10E    0.43429448190325182765  /* log 10e */
#endif
#ifndef  M_LN2
# define M_LN2       0.69314718055994530942  /* log e2 */
#endif
#ifndef  M_LN10
# define M_LN10      2.30258509299404568402  /* log e10 */
#endif
#ifndef  M_PI
# define M_PI        3.14159265358979323846  /* pi */
#endif
#ifndef  M_PI_2
# define M_PI_2      1.57079632679489661923  /* pi/2 */
#endif
#ifndef  M_PI_4
# define M_PI_4      0.78539816339744830962  /* pi/4 */
#endif
#ifndef  M_1_PI
# define M_1_PI      0.31830988618379067154  /* 1/pi */
#endif
#ifndef  M_2_PI
# define M_2_PI      0.63661977236758134308  /* 2/pi */
#endif
#ifndef  M_2_SQRTPI
# define M_2_SQRTPI  1.12837916709551257390  /* 2/sqrt(pi) */
#endif
#ifndef  M_SQRT2
# define M_SQRT2     1.41421356237309504880  /* sqrt(2) */
#endif
#ifndef  M_SQRT1_2
# define M_SQRT1_2   0.70710678118654752440  /* 1/sqrt(2) */
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/*
 * Macros checking  i n t e g e r  returns.
 */
#define CHECK_INT(a_Expr, a_rcExpect) do { \
        int const rcActual = (a_Expr); \
        if (rcActual != (a_rcExpect)) \
            RTTestFailed(g_hTest, "line %u: %s -> %d, expected %d", __LINE__, #a_Expr, rcActual, (a_rcExpect)); \
    } while (0)

#define CHECK_INT_SAME(a_Fn, a_Args) do { \
        int const rcNoCrt = RT_NOCRT(a_Fn) a_Args; \
        int const rcCrt   =          a_Fn  a_Args; \
        if (rcNoCrt != rcCrt) \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %d; CRT => %d", __LINE__, #a_Fn, #a_Args, rcNoCrt, rcCrt); \
    } while (0)


/*
 * Macros checking  l o n g  returns.
 */
#define CHECK_LONG(a_Expr, a_rcExpect) do { \
        long const rcActual = (a_Expr); \
        long const rcExpect = (a_rcExpect); \
        if (rcActual != rcExpect) \
            RTTestFailed(g_hTest, "line %u: %s -> %ld, expected %ld", __LINE__, #a_Expr, rcActual, rcExpect); \
    } while (0)

#define CHECK_LONG_SAME(a_Fn, a_Args) do { \
        long const rcNoCrt = RT_NOCRT(a_Fn) a_Args; \
        long const rcCrt   =          a_Fn  a_Args; \
        if (rcNoCrt != rcCrt) \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %ld; CRT => %ld", __LINE__, #a_Fn, #a_Args, rcNoCrt, rcCrt); \
    } while (0)


/*
 * Macros checking  l o n g  l o n g  returns.
 */
#define CHECK_LLONG(a_Expr, a_rcExpect) do { \
        long long const rcActual = (a_Expr); \
        long long const rcExpect = (a_rcExpect); \
        if (rcActual != rcExpect) \
            RTTestFailed(g_hTest, "line %u: %s -> %lld, expected %lld", __LINE__, #a_Expr, rcActual, rcExpect); \
    } while (0)

#define CHECK_LLONG_SAME(a_Fn, a_Args) do { \
        long long const rcNoCrt = RT_NOCRT(a_Fn) a_Args; \
        long long const rcCrt   =          a_Fn  a_Args; \
        if (rcNoCrt != rcCrt) \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %lld; CRT => %lld", __LINE__, #a_Fn, #a_Args, rcNoCrt, rcCrt); \
    } while (0)


/*
 * Macros checking  l o n g   d o u b l e  returns.
 */
#ifdef RT_COMPILER_WITH_80BIT_LONG_DOUBLE
# define CHECK_LDBL(a_Expr, a_lrdExpect) do { \
        RTFLOAT80U2 uRet; \
        uRet.r    = (a_Expr); \
        RTFLOAT80U2 uExpect; \
        uExpect.r = a_lrdExpect; \
        if (!RTFLOAT80U_ARE_IDENTICAL(&uRet, &uExpect)) \
        { \
            RTStrFormatR80u2(g_szFloat[0], sizeof(g_szFloat[0]), &uRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR80u2(g_szFloat[1], sizeof(g_szFloat[0]), &uExpect, 0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s -> %s, expected %s (%s)", \
                         __LINE__, #a_Expr, g_szFloat[0], g_szFloat[1], #a_lrdExpect); \
        } \
    } while (0)

# define CHECK_LDBL_SAME(a_Fn, a_Args) do { \
        RTFLOAT80U2 uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (!RTFLOAT80U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet)) \
        { \
            RTStrFormatR80u2(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR80u2(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

# define CHECK_LDBL_APPROX_SAME(a_Fn, a_Args) do { \
        RTFLOAT80U2 uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (   !RTFLOAT80U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (  (uNoCrtRet.u >= uCrtRet.u ? uNoCrtRet.u - uCrtRet.u : uCrtRet.u - uNoCrtRet.u) > 1 /* off by one is okay */ \
                || RTFLOAT80U_IS_NAN(&uNoCrtRet) \
                || RTFLOAT80U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR80u2(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR80u2(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

# define CHECK_LDBL_SAME_RELAXED_NAN(a_Fn, a_Args) do { \
        RTFLOAT80U2 uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (   !RTFLOAT80U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (   !RTFLOAT80U_IS_NAN(&uNoCrtRet) \
                || !RTFLOAT80U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR80u2(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR80u2(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

#elif defined(RT_COMPILER_WITH_128BIT_LONG_DOUBLE)
# error todo

#else
# define CHECK_LDBL(a_Expr, a_lrdExpect) do { \
        RTFLOAT64U uRet; \
        uRet.lrd    = (a_Expr); \
        RTFLOAT64U uExpect; \
        uExpect.lrd = a_lrdExpect; \
        if (!RTFLOAT64U_ARE_IDENTICAL(&uRet, &uExpect)) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uExpect, 0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s -> %s, expected %s (%s)", \
                         __LINE__, #a_Expr, g_szFloat[0], g_szFloat[1], #a_lrdExpect); \
        } \
    } while (0)

# define CHECK_LDBL_SAME(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.lrd = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.lrd   =          a_Fn  a_Args; \
        if (!RTFLOAT64U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet)) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

# define CHECK_LDBL_APPROX_SAME(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.lrd = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.lrd   =          a_Fn  a_Args; \
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

# define CHECK_LDBL_SAME_RELAXED_NAN(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.lrd = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.lrd   =          a_Fn  a_Args; \
        if (   !RTFLOAT64U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (   !RTFLOAT64U_IS_NAN(&uNoCrtRet) \
                || !RTFLOAT64U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)
#endif


/*
 * Macros checking  d o u b l e  returns.
 */
#define CHECK_DBL(a_Expr, a_rdExpect) do { \
        RTFLOAT64U uRet; \
        uRet.r    = (a_Expr); \
        RTFLOAT64U uExpect; \
        uExpect.r = a_rdExpect; \
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
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
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
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
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

#define CHECK_DBL_RANGE(a_Expr, a_rdExpect, a_rdPlusMin) do { \
        RTFLOAT64U uRet; \
        uRet.r = a_Expr; \
        RTFLOAT64U uExpectMin; \
        uExpectMin.r = (a_rdExpect) - (a_rdPlusMin); \
        RTFLOAT64U uExpectMax; \
        uExpectMax.r = (a_rdExpect) + (a_rdPlusMin); \
        if (   !(RTFLOAT64U_IS_NORMAL(&uRet) || RTFLOAT64U_IS_ZERO(&uRet))\
            || uRet.r < uExpectMin.r \
            || uRet.r > uExpectMax.r ) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uRet,       0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[1]), &uExpectMin, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[2], sizeof(g_szFloat[2]), &uExpectMax, 0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s -> %s, expected [%s,%s] (%s +/- %s)", \
                         __LINE__, #a_Expr, g_szFloat[0], g_szFloat[1], #a_rdExpect, #a_rdPlusMin); \
        } \
    } while (0)

#define CHECK_DBL_SAME_RELAXED_NAN(a_Fn, a_Args) do { \
        RTFLOAT64U uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (   !RTFLOAT64U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (   !RTFLOAT64U_IS_NAN(&uNoCrtRet) \
                || !RTFLOAT64U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR64(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR64(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

/*
 * Macros checking  f l o a t  returns.
 */
#define CHECK_FLT(a_Expr, a_rExpect) do { \
        RTFLOAT32U uRet; \
        uRet.r    = (a_Expr); \
        RTFLOAT32U uExpect; \
        uExpect.r = a_rExpect; \
        if (!RTFLOAT32U_ARE_IDENTICAL(&uRet, &uExpect)) \
        { \
            RTStrFormatR32(g_szFloat[0], sizeof(g_szFloat[0]), &uRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR32(g_szFloat[1], sizeof(g_szFloat[0]), &uExpect, 0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s -> %s, expected %s (%s)", \
                         __LINE__, #a_Expr, g_szFloat[0], g_szFloat[1], #a_rExpect); \
        } \
    } while (0)

#define CHECK_FLT_SAME(a_Fn, a_Args) do { \
        RTFLOAT32U uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (!RTFLOAT32U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet)) \
        { \
            RTStrFormatR32(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR32(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

#define CHECK_FLT_APPROX_SAME(a_Fn, a_Args) do { \
        RTFLOAT32U uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (   !RTFLOAT32U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (  (uNoCrtRet.u >= uCrtRet.u ? uNoCrtRet.u - uCrtRet.u : uCrtRet.u - uNoCrtRet.u) > 1 /* off by one is okay */ \
                || RTFLOAT32U_IS_NAN(&uNoCrtRet) \
                || RTFLOAT32U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR32(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR32(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)

#define CHECK_FLT_SAME_RELAXED_NAN(a_Fn, a_Args) do { \
        RTFLOAT32U uNoCrtRet, uCrtRet; \
        uNoCrtRet.r = RT_NOCRT(a_Fn) a_Args; \
        uCrtRet.r   =          a_Fn  a_Args; \
        if (   !RTFLOAT32U_ARE_IDENTICAL(&uNoCrtRet, &uCrtRet) \
            && (   !RTFLOAT32U_IS_NAN(&uNoCrtRet) \
                || !RTFLOAT32U_IS_NAN(&uCrtRet) ) ) \
        { \
            RTStrFormatR32(g_szFloat[0], sizeof(g_szFloat[0]), &uNoCrtRet, 0, 0, RTSTR_F_SPECIAL); \
            RTStrFormatR32(g_szFloat[1], sizeof(g_szFloat[0]), &uCrtRet,   0, 0, RTSTR_F_SPECIAL); \
            RTTestFailed(g_hTest, "line %u: %s%s: noCRT => %s; CRT => %s", \
                         __LINE__, #a_Fn, #a_Args, g_szFloat[0], g_szFloat[1]); \
        } \
    } while (0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
RTTEST  g_hTest;
char    g_szFloat[4][128];


#ifdef _MSC_VER
# pragma fenv_access(on)
#endif


void testAbs()
{
    RTTestSub(g_hTest, "abs,labs,llabs");
    CHECK_INT(RT_NOCRT(abs)(1),  1);
    CHECK_INT(RT_NOCRT(abs)(-1), 1);
    CHECK_INT(RT_NOCRT(abs)(9685), 9685);
    CHECK_INT(RT_NOCRT(abs)(-9685), 9685);
    CHECK_INT(RT_NOCRT(abs)(589685), 589685);
    CHECK_INT(RT_NOCRT(abs)(-589685), 589685);
    CHECK_INT(RT_NOCRT(abs)(INT_MAX), INT_MAX);
    CHECK_INT(RT_NOCRT(abs)(INT_MIN + 1), INT_MAX);
    CHECK_INT(RT_NOCRT(abs)(INT_MIN), INT_MIN); /* oddity */
    CHECK_INT_SAME(abs,(INT_MIN));
    CHECK_INT_SAME(abs,(INT_MAX));

    CHECK_LONG(RT_NOCRT(labs)(1),  1);
    CHECK_LONG(RT_NOCRT(labs)(-1), 1);
    CHECK_LONG(RT_NOCRT(labs)(9685), 9685);
    CHECK_LONG(RT_NOCRT(labs)(-9685), 9685);
    CHECK_LONG(RT_NOCRT(labs)(589685), 589685);
    CHECK_LONG(RT_NOCRT(labs)(-589685), 589685);
    CHECK_LONG(RT_NOCRT(labs)(LONG_MAX),     LONG_MAX);
    CHECK_LONG(RT_NOCRT(labs)(LONG_MIN + 1), LONG_MAX);
    CHECK_LONG(RT_NOCRT(labs)(LONG_MIN),     LONG_MIN); /* oddity */
    CHECK_LONG_SAME(labs,(LONG_MIN));
    CHECK_LONG_SAME(labs,(LONG_MAX));

    CHECK_LONG(RT_NOCRT(llabs)(1),  1);
    CHECK_LONG(RT_NOCRT(llabs)(-1), 1);
    CHECK_LONG(RT_NOCRT(llabs)(9685), 9685);
    CHECK_LONG(RT_NOCRT(llabs)(-9685), 9685);
    CHECK_LONG(RT_NOCRT(llabs)(589685), 589685);
    CHECK_LONG(RT_NOCRT(llabs)(-589685), 589685);
    CHECK_LONG(RT_NOCRT(llabs)(LONG_MAX),     LONG_MAX);
    CHECK_LONG(RT_NOCRT(llabs)(LONG_MIN + 1), LONG_MAX);
    CHECK_LONG(RT_NOCRT(llabs)(LONG_MIN),     LONG_MIN); /* oddity */
    CHECK_LONG_SAME(llabs,(LONG_MIN));
    CHECK_LONG_SAME(llabs,(LONG_MAX));
}


void testFAbs()
{
    RTTestSub(g_hTest, "fabs[fl]");

    CHECK_DBL(RT_NOCRT(fabs)(              +0.0),               +0.0);
    CHECK_DBL(RT_NOCRT(fabs)(              -0.0),               +0.0);
    CHECK_DBL(RT_NOCRT(fabs)(             -42.5),              +42.5);
    CHECK_DBL(RT_NOCRT(fabs)(             +42.5),              +42.5);
    CHECK_DBL(RT_NOCRT(fabs)(+1234.60958634e+20), +1234.60958634e+20);
    CHECK_DBL(RT_NOCRT(fabs)(-1234.60958634e+20), +1234.60958634e+20);
    CHECK_DBL(RT_NOCRT(fabs)(      +2.1984e-310),       +2.1984e-310); /* subnormal */
    CHECK_DBL(RT_NOCRT(fabs)(      -2.1984e-310),       +2.1984e-310); /* subnormal */
    CHECK_DBL(RT_NOCRT(fabs)(-INFINITY),                   +INFINITY);
    CHECK_DBL(RT_NOCRT(fabs)(+INFINITY),                   +INFINITY);
    CHECK_DBL(RT_NOCRT(fabs)(RTStrNanDouble(NULL, true)), RTStrNanDouble(NULL, true));
    CHECK_DBL(RT_NOCRT(fabs)(RTStrNanDouble("s", false)), RTStrNanDouble("s", true));
    CHECK_DBL_SAME(fabs,(              -0.0));
    CHECK_DBL_SAME(fabs,(              +0.0));
    CHECK_DBL_SAME(fabs,(             +22.5));
    CHECK_DBL_SAME(fabs,(             -22.5));
    CHECK_DBL_SAME(fabs,(      +2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(fabs,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(fabs,(+1234.60958634e+20));
    CHECK_DBL_SAME(fabs,(-1234.60958634e+20));
    CHECK_DBL_SAME(fabs,(-INFINITY));
    CHECK_DBL_SAME(fabs,(+INFINITY));
    CHECK_DBL_SAME(fabs,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(fabs,(RTStrNanDouble("s", false)));

    CHECK_FLT(RT_NOCRT(fabsf)(              +0.0f),               +0.0f);
    CHECK_FLT(RT_NOCRT(fabsf)(              -0.0f),               +0.0f);
    CHECK_FLT(RT_NOCRT(fabsf)(             -42.5f),              +42.5f);
    CHECK_FLT(RT_NOCRT(fabsf)(             +42.5f),              +42.5f);
    CHECK_FLT(RT_NOCRT(fabsf)(+1234.60958634e+20f), +1234.60958634e+20f);
    CHECK_FLT(RT_NOCRT(fabsf)(-1234.60958634e+20f), +1234.60958634e+20f);
    CHECK_FLT(RT_NOCRT(fabsf)(      +2.1984e-310f),       +2.1984e-310f); /* subnormal */
    CHECK_FLT(RT_NOCRT(fabsf)(      -2.1984e-310f),       +2.1984e-310f); /* subnormal */
    CHECK_FLT(RT_NOCRT(fabsf)(-INFINITY),                     +INFINITY);
    CHECK_FLT(RT_NOCRT(fabsf)(+INFINITY),                     +INFINITY);
    CHECK_FLT(RT_NOCRT(fabsf)(RTStrNanFloat(NULL, true)), RTStrNanFloat(NULL, true));
    CHECK_FLT(RT_NOCRT(fabsf)(RTStrNanFloat("s", false)), RTStrNanFloat("s", true));
    CHECK_FLT_SAME(fabsf,(              -0.0f));
    CHECK_FLT_SAME(fabsf,(              +0.0f));
    CHECK_FLT_SAME(fabsf,(             +22.5f));
    CHECK_FLT_SAME(fabsf,(             -22.5f));
    CHECK_FLT_SAME(fabsf,(      +2.1984e-310f)); /* subnormal */
    CHECK_FLT_SAME(fabsf,(      -2.1984e-310f)); /* subnormal */
    CHECK_FLT_SAME(fabsf,(+1234.60958634e+20f));
    CHECK_FLT_SAME(fabsf,(-1234.60958634e+20f));
    CHECK_FLT_SAME(fabsf,(-INFINITY));
    CHECK_FLT_SAME(fabsf,(+INFINITY));
    CHECK_FLT_SAME(fabsf,(RTStrNanFloat(NULL, true)));
#if 0 /* UCRT on windows converts this to a quiet NaN, so skip it. */
    CHECK_FLT_SAME(fabsf,(RTStrNanFloat("s", false)));
#endif
}


void testCopySign()
{
    RTTestSub(g_hTest, "copysign[fl]");

    CHECK_DBL(RT_NOCRT(copysign)(1.0, 2.0), 1.0);
    CHECK_DBL(RT_NOCRT(copysign)(-1.0, 2.0), 1.0);
    CHECK_DBL(RT_NOCRT(copysign)(-1.0, -2.0), -1.0);
    CHECK_DBL(RT_NOCRT(copysign)(1.0, -2.0), -1.0);
    CHECK_DBL(RT_NOCRT(copysign)(42.24, -INFINITY), -42.24);
    CHECK_DBL(RT_NOCRT(copysign)(-42.24, +INFINITY), +42.24);
    CHECK_DBL(RT_NOCRT(copysign)(-999888777.666, RTStrNanDouble(NULL, true)),  +999888777.666);
    CHECK_DBL(RT_NOCRT(copysign)(-999888777.666, RTStrNanDouble("sig", true)),  +999888777.666);
    CHECK_DBL(RT_NOCRT(copysign)(+999888777.666, RTStrNanDouble(NULL, false)), -999888777.666);
    CHECK_DBL_SAME(copysign,(1.0, 2.0));
    CHECK_DBL_SAME(copysign,(-1.0, 2.0));
    CHECK_DBL_SAME(copysign,(-1.0, -2.0));
    CHECK_DBL_SAME(copysign,(1.0, -2.0));
    CHECK_DBL_SAME(copysign,(42.24, -INFINITY));
    CHECK_DBL_SAME(copysign,(-42.24, +INFINITY));
    CHECK_DBL_SAME(copysign,(-999888777.666, RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(copysign,(+999888777.666, RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(copysign,(+999888777.666, RTStrNanDouble("sig", false)));

    CHECK_FLT(RT_NOCRT(copysignf)(1.0f, 2.0f), 1.0f);
    CHECK_FLT(RT_NOCRT(copysignf)(-1.0f, 2.0f), 1.0f);
    CHECK_FLT(RT_NOCRT(copysignf)(-1.0f, -2.0f), -1.0f);
    CHECK_FLT(RT_NOCRT(copysignf)(1.0f, -2.0f), -1.0f);
    CHECK_FLT(RT_NOCRT(copysignf)(42.24f, -INFINITY), -42.24f);
    CHECK_FLT(RT_NOCRT(copysignf)(-42.24f, +INFINITY), +42.24f);
    CHECK_FLT(RT_NOCRT(copysignf)(-999888777.666f, RTStrNanFloat(NULL, true)),  +999888777.666f);
    CHECK_FLT(RT_NOCRT(copysignf)(+999888777.666f, RTStrNanFloat(NULL, false)), -999888777.666f);
    CHECK_FLT_SAME(copysignf,(1.0f, 2.0f));
    CHECK_FLT_SAME(copysignf,(-3.0f, 2.0f));
    CHECK_FLT_SAME(copysignf,(-5.0e3f, -2.0f));
    CHECK_FLT_SAME(copysignf,(6.0e-3f, -2.0f));
    CHECK_FLT_SAME(copysignf,(434.24f, -INFINITY));
    CHECK_FLT_SAME(copysignf,(-42.24f, +INFINITY));
    CHECK_FLT_SAME(copysignf,(-39480.6e+33f, RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME(copysignf,(+39480.6e-32f, RTStrNanFloat(NULL, false)));

    CHECK_LDBL(RT_NOCRT(copysignl)(1.0L, 2.0L), 1.0L);
    CHECK_LDBL(RT_NOCRT(copysignl)(-1.0L, 2.0L), 1.0L);
    CHECK_LDBL(RT_NOCRT(copysignl)(-1.0L, -2.0L), -1.0L);
    CHECK_LDBL(RT_NOCRT(copysignl)(1.0L, -2.0L), -1.0L);
    CHECK_LDBL(RT_NOCRT(copysignl)(42.24L, -INFINITY), -42.24L);
    CHECK_LDBL(RT_NOCRT(copysignl)(-42.24L, +INFINITY), +42.24L);
    CHECK_LDBL(RT_NOCRT(copysignl)(-999888777.666L, RTStrNanLongDouble(NULL, true)),  +999888777.666L);
    CHECK_LDBL(RT_NOCRT(copysignl)(+999888777.666L, RTStrNanLongDouble("2343f_sig", false)), -999888777.666L);
    CHECK_LDBL_SAME(copysignl,(1.0L, 2.0L));
    CHECK_LDBL_SAME(copysignl,(-3.0L, 2.0L));
    CHECK_LDBL_SAME(copysignl,(-5.0e3L, -2.0L));
    CHECK_LDBL_SAME(copysignl,(6.0e-3L, -2.0L));
    CHECK_LDBL_SAME(copysignl,(434.24L, -INFINITY));
    CHECK_LDBL_SAME(copysignl,(-42.24L, +INFINITY));
    CHECK_LDBL_SAME(copysignl,(-39480.6e+33L, RTStrNanLongDouble("8888_s", true)));
    CHECK_LDBL_SAME(copysignl,(+39480.6e-32L, RTStrNanLongDouble(NULL, false)));
}


void testFmax()
{
    RTTestSub(g_hTest, "fmax[fl]");

    CHECK_DBL(RT_NOCRT(fmax)( 1.0,      1.0),      1.0);
    CHECK_DBL(RT_NOCRT(fmax)( 4.0,      2.0),      4.0);
    CHECK_DBL(RT_NOCRT(fmax)( 2.0,      4.0),      4.0);
    CHECK_DBL(RT_NOCRT(fmax)(-2.0,     -4.0),     -2.0);
    CHECK_DBL(RT_NOCRT(fmax)(-2.0, -4.0e-10), -4.0e-10);
    CHECK_DBL(RT_NOCRT(fmax)(+INFINITY, +INFINITY), +INFINITY);
    CHECK_DBL(RT_NOCRT(fmax)(-INFINITY, -INFINITY), -INFINITY);
    CHECK_DBL(RT_NOCRT(fmax)(+INFINITY, -INFINITY), +INFINITY);
    CHECK_DBL(RT_NOCRT(fmax)(-INFINITY, +INFINITY), +INFINITY);
    CHECK_DBL_SAME(fmax, (   99.99,    99.87));
    CHECK_DBL_SAME(fmax, (  -99.99,   -99.87));
    CHECK_DBL_SAME(fmax, (-987.453, 34599.87));
    CHECK_DBL_SAME(fmax, (34599.87, -987.453));
    CHECK_DBL_SAME(fmax, (    +0.0,     -0.0));
    CHECK_DBL_SAME(fmax, (    -0.0,     +0.0));
    CHECK_DBL_SAME(fmax, (    -0.0,     -0.0));
    CHECK_DBL_SAME(fmax, (+INFINITY, +INFINITY));
    CHECK_DBL_SAME(fmax, (-INFINITY, -INFINITY));
    CHECK_DBL_SAME(fmax, (+INFINITY, -INFINITY));
    CHECK_DBL_SAME(fmax, (-INFINITY, +INFINITY));
    CHECK_DBL_SAME(fmax, (RTStrNanDouble(NULL, true),  -42.4242424242e222));
    CHECK_DBL_SAME(fmax, (RTStrNanDouble(NULL, false), -42.4242424242e222));
    CHECK_DBL_SAME(fmax, (-42.4242424242e-222, RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(fmax, (-42.4242424242e-222, RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmax, (RTStrNanDouble("2", false),   RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmax, (RTStrNanDouble("3", true),    RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmax, (RTStrNanDouble("4sig", true), RTStrNanDouble(NULL, false)));

    CHECK_FLT(RT_NOCRT(fmaxf)( 1.0f,      1.0f),      1.0f);
    CHECK_FLT(RT_NOCRT(fmaxf)( 4.0f,      2.0f),      4.0f);
    CHECK_FLT(RT_NOCRT(fmaxf)( 2.0f,      4.0f),      4.0f);
    CHECK_FLT(RT_NOCRT(fmaxf)(-2.0f,     -4.0f),     -2.0f);
    CHECK_FLT(RT_NOCRT(fmaxf)(-2.0f, -4.0e-10f), -4.0e-10f);
    CHECK_FLT(RT_NOCRT(fmaxf)(+INFINITY, +INFINITY), +INFINITY);
    CHECK_FLT(RT_NOCRT(fmaxf)(-INFINITY, -INFINITY), -INFINITY);
    CHECK_FLT(RT_NOCRT(fmaxf)(+INFINITY, -INFINITY), +INFINITY);
    CHECK_FLT(RT_NOCRT(fmaxf)(-INFINITY, +INFINITY), +INFINITY);
    CHECK_FLT_SAME(fmaxf, (   99.99f,    99.87f));
    CHECK_FLT_SAME(fmaxf, (  -99.99f,   -99.87f));
    CHECK_FLT_SAME(fmaxf, (-987.453f, 34599.87f));
    CHECK_FLT_SAME(fmaxf, (34599.87f, -987.453f));
    CHECK_FLT_SAME(fmaxf, (    +0.0f,     -0.0f));
    CHECK_FLT_SAME(fmaxf, (    -0.0f,     +0.0f));
    CHECK_FLT_SAME(fmaxf, (    -0.0f,     -0.0f));
    CHECK_FLT_SAME(fmaxf, (+INFINITY, +INFINITY));
    CHECK_FLT_SAME(fmaxf, (-INFINITY, -INFINITY));
    CHECK_FLT_SAME(fmaxf, (+INFINITY, -INFINITY));
    CHECK_FLT_SAME(fmaxf, (-INFINITY, +INFINITY));
    CHECK_FLT_SAME(fmaxf, (RTStrNanFloat(NULL, true),  -42.4242424242e22f));
    CHECK_FLT_SAME(fmaxf, (RTStrNanFloat(NULL, false), -42.4242424242e22f));
    CHECK_FLT_SAME(fmaxf, (-42.42424242e-22f, RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME(fmaxf, (-42.42424242e-22f, RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fmaxf, (RTStrNanFloat("2", false),   RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fmaxf, (RTStrNanFloat("3", true),    RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fmaxf, (RTStrNanFloat("4sig", true), RTStrNanFloat(NULL, false)));

    CHECK_LDBL(RT_NOCRT(fmaxl)( 1.0L,      1.0L),      1.0L);
    CHECK_LDBL(RT_NOCRT(fmaxl)( 4.0L,      2.0L),      4.0L);
    CHECK_LDBL(RT_NOCRT(fmaxl)( 2.0L,      4.0L),      4.0L);
    CHECK_LDBL(RT_NOCRT(fmaxl)(-2.0L,     -4.0L),     -2.0L);
    CHECK_LDBL(RT_NOCRT(fmaxl)(-2.0L, -4.0e-10L), -4.0e-10L);
    CHECK_LDBL(RT_NOCRT(fmaxl)(+INFINITY, +INFINITY), +INFINITY);
    CHECK_LDBL(RT_NOCRT(fmaxl)(-INFINITY, -INFINITY), -INFINITY);
    CHECK_LDBL(RT_NOCRT(fmaxl)(+INFINITY, -INFINITY), +INFINITY);
    CHECK_LDBL(RT_NOCRT(fmaxl)(-INFINITY, +INFINITY), +INFINITY);
    CHECK_LDBL_SAME(fmaxl, (   99.99L,    99.87L));
    CHECK_LDBL_SAME(fmaxl, (  -99.99L,   -99.87L));
    CHECK_LDBL_SAME(fmaxl, (-987.453L, 34599.87L));
    CHECK_LDBL_SAME(fmaxl, (34599.87L, -987.453L));
    CHECK_LDBL_SAME(fmaxl, (    +0.0L,     -0.0L));
    CHECK_LDBL_SAME(fmaxl, (    -0.0L,     +0.0L));
    CHECK_LDBL_SAME(fmaxl, (    -0.0L,     -0.0L));
    CHECK_LDBL_SAME(fmaxl, (+INFINITY, +INFINITY));
    CHECK_LDBL_SAME(fmaxl, (-INFINITY, -INFINITY));
    CHECK_LDBL_SAME(fmaxl, (+INFINITY, -INFINITY));
    CHECK_LDBL_SAME(fmaxl, (-INFINITY, +INFINITY));
    CHECK_LDBL_SAME(fmaxl, (RTStrNanLongDouble(NULL, true),  -42.4242424242e222L));
    CHECK_LDBL_SAME(fmaxl, (RTStrNanLongDouble(NULL, false), -42.4242424242e222L));
    CHECK_LDBL_SAME(fmaxl, (-42.4242424242e-222L, RTStrNanLongDouble(NULL, true)));
    CHECK_LDBL_SAME(fmaxl, (-42.4242424242e-222L, RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fmaxl, (RTStrNanLongDouble("2", false),   RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fmaxl, (RTStrNanLongDouble("3", true),    RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fmaxl, (RTStrNanLongDouble("4sig", true), RTStrNanLongDouble(NULL, false)));
}


void testFmin()
{
    RTTestSub(g_hTest, "fmin[fl]");

    CHECK_DBL(RT_NOCRT(fmin)( 1.0,            1.0),       1.0);
    CHECK_DBL(RT_NOCRT(fmin)( 4.0,            2.0),       2.0);
    CHECK_DBL(RT_NOCRT(fmin)( 2.0,            4.0),       2.0);
    CHECK_DBL(RT_NOCRT(fmin)(-2.0,           -4.0),      -4.0);
    CHECK_DBL(RT_NOCRT(fmin)(-2.0,       -4.0e+10),  -4.0e+10);
    CHECK_DBL(RT_NOCRT(fmin)(+INFINITY, +INFINITY), +INFINITY);
    CHECK_DBL(RT_NOCRT(fmin)(-INFINITY, -INFINITY), -INFINITY);
    CHECK_DBL(RT_NOCRT(fmin)(+INFINITY, -INFINITY), -INFINITY);
    CHECK_DBL(RT_NOCRT(fmin)(-INFINITY, +INFINITY), -INFINITY);
    CHECK_DBL_SAME(fmin, (   99.99,    99.87));
    CHECK_DBL_SAME(fmin, (  -99.99,   -99.87));
    CHECK_DBL_SAME(fmin, (-987.453, 34599.87));
    CHECK_DBL_SAME(fmin, (34599.87, -987.453));
    CHECK_DBL_SAME(fmin, (    +0.0,     -0.0));
    CHECK_DBL_SAME(fmin, (    -0.0,     +0.0));
    CHECK_DBL_SAME(fmin, (    -0.0,     -0.0));
    CHECK_DBL_SAME(fmin, (+INFINITY, +INFINITY));
    CHECK_DBL_SAME(fmin, (-INFINITY, -INFINITY));
    CHECK_DBL_SAME(fmin, (+INFINITY, -INFINITY));
    CHECK_DBL_SAME(fmin, (-INFINITY, +INFINITY));
    CHECK_DBL_SAME(fmin, (RTStrNanDouble(NULL, true),  -42.4242424242e222));
    CHECK_DBL_SAME(fmin, (RTStrNanDouble(NULL, false), -42.4242424242e222));
    CHECK_DBL_SAME(fmin, (-42.4242424242e-222, RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(fmin, (-42.4242424242e-222, RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmin, (RTStrNanDouble("2", false),   RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmin, (RTStrNanDouble("3", true),    RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(fmin, (RTStrNanDouble("4sig", true), RTStrNanDouble(NULL, false)));

    CHECK_FLT(RT_NOCRT(fmin)( 1.0f,          1.0f),       1.0f);
    CHECK_FLT(RT_NOCRT(fmin)( 4.0f,          2.0f),       2.0f);
    CHECK_FLT(RT_NOCRT(fmin)( 2.0f,          4.0f),       2.0f);
    CHECK_FLT(RT_NOCRT(fmin)(-2.0f,         -4.0f),      -4.0f);
    CHECK_FLT(RT_NOCRT(fmin)(-2.0f,     -4.0e+10f),  -4.0e+10f);
    CHECK_FLT(RT_NOCRT(fmin)(+INFINITY, +INFINITY),  +INFINITY);
    CHECK_FLT(RT_NOCRT(fmin)(-INFINITY, -INFINITY),  -INFINITY);
    CHECK_FLT(RT_NOCRT(fmin)(+INFINITY, -INFINITY),  -INFINITY);
    CHECK_FLT(RT_NOCRT(fmin)(-INFINITY, +INFINITY),  -INFINITY);
    CHECK_FLT_SAME(fminf, (   99.99f,    99.87f));
    CHECK_FLT_SAME(fminf, (  -99.99f,   -99.87f));
    CHECK_FLT_SAME(fminf, (-987.453f, 34599.87f));
    CHECK_FLT_SAME(fminf, (34599.87f, -987.453f));
    CHECK_FLT_SAME(fminf, (    +0.0f,     -0.0f));
    CHECK_FLT_SAME(fminf, (    -0.0f,     +0.0f));
    CHECK_FLT_SAME(fminf, (    -0.0f,     -0.0f));
    CHECK_FLT_SAME(fminf, (+INFINITY, +INFINITY));
    CHECK_FLT_SAME(fminf, (-INFINITY, -INFINITY));
    CHECK_FLT_SAME(fminf, (+INFINITY, -INFINITY));
    CHECK_FLT_SAME(fminf, (-INFINITY, +INFINITY));
    CHECK_FLT_SAME(fminf, (RTStrNanFloat(NULL, true),  -42.4242424242e22f));
    CHECK_FLT_SAME(fminf, (RTStrNanFloat(NULL, false), -42.4242424242e22f));
    CHECK_FLT_SAME(fminf, (-42.42424242e-22f, RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME(fminf, (-42.42424242e-22f, RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fminf, (RTStrNanFloat("2", false),   RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fminf, (RTStrNanFloat("3", true),    RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(fminf, (RTStrNanFloat("4sig", true), RTStrNanFloat(NULL, false)));

    CHECK_LDBL(RT_NOCRT(fmin)( 1.0L,          1.0L),       1.0L);
    CHECK_LDBL(RT_NOCRT(fmin)( 4.0L,          2.0L),       2.0L);
    CHECK_LDBL(RT_NOCRT(fmin)( 2.0L,          4.0L),       2.0L);
    CHECK_LDBL(RT_NOCRT(fmin)(-2.0L,         -4.0L),      -4.0L);
    CHECK_LDBL(RT_NOCRT(fmin)(-2.0L,     -4.0e+10L),  -4.0e+10L);
    CHECK_LDBL(RT_NOCRT(fmin)(+INFINITY, +INFINITY),  +INFINITY);
    CHECK_LDBL(RT_NOCRT(fmin)(-INFINITY, -INFINITY),  -INFINITY);
    CHECK_LDBL(RT_NOCRT(fmin)(+INFINITY, -INFINITY),  -INFINITY);
    CHECK_LDBL(RT_NOCRT(fmin)(-INFINITY, +INFINITY),  -INFINITY);
    CHECK_LDBL_SAME(fminl, (   99.99L,    99.87L));
    CHECK_LDBL_SAME(fminl, (  -99.99L,   -99.87L));
    CHECK_LDBL_SAME(fminl, (-987.453L, 34599.87L));
    CHECK_LDBL_SAME(fminl, (34599.87L, -987.453L));
    CHECK_LDBL_SAME(fminl, (    +0.0L,     -0.0L));
    CHECK_LDBL_SAME(fminl, (    -0.0L,     +0.0L));
    CHECK_LDBL_SAME(fminl, (    -0.0L,     -0.0L));
    CHECK_LDBL_SAME(fminl, (+INFINITY, +INFINITY));
    CHECK_LDBL_SAME(fminl, (-INFINITY, -INFINITY));
    CHECK_LDBL_SAME(fminl, (+INFINITY, -INFINITY));
    CHECK_LDBL_SAME(fminl, (-INFINITY, +INFINITY));
    CHECK_LDBL_SAME(fminl, (RTStrNanLongDouble(NULL, true),  -42.4242424242e222L));
    CHECK_LDBL_SAME(fminl, (RTStrNanLongDouble(NULL, false), -42.4242424242e222L));
    CHECK_LDBL_SAME(fminl, (-42.4242424242e-222L, RTStrNanLongDouble(NULL, true)));
    CHECK_LDBL_SAME(fminl, (-42.4242424242e-222L, RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fminl, (RTStrNanLongDouble("2", false),   RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fminl, (RTStrNanLongDouble("3", true),    RTStrNanLongDouble(NULL, false)));
    CHECK_LDBL_SAME(fminl, (RTStrNanLongDouble("4sig", true), RTStrNanLongDouble(NULL, false)));
}


void testIsInf()
{
    RTTestSub(g_hTest, "isinf,__isinf[fl]");
#undef isinf
    CHECK_INT(RT_NOCRT(isinf)(           1.0), 0);
    CHECK_INT(RT_NOCRT(isinf)( 2394.2340e200), 0);
    CHECK_INT(RT_NOCRT(isinf)(-2394.2340e200), 0);
    CHECK_INT(RT_NOCRT(isinf)(-INFINITY), 1);
    CHECK_INT(RT_NOCRT(isinf)(+INFINITY), 1);
    CHECK_INT(RT_NOCRT(isinf)(RTStrNanDouble(NULL, true)), 0);
    CHECK_INT(RT_NOCRT(isinf)(RTStrNanDouble("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isinff)(          1.0f), 0);
    CHECK_INT(RT_NOCRT(__isinff)( 2394.2340e20f), 0);
    CHECK_INT(RT_NOCRT(__isinff)(-2394.2340e20f), 0);
    CHECK_INT(RT_NOCRT(__isinff)(-INFINITY), 1);
    CHECK_INT(RT_NOCRT(__isinff)(+INFINITY), 1);
    CHECK_INT(RT_NOCRT(__isinff)(RTStrNanFloat(NULL, true)), 0);
    CHECK_INT(RT_NOCRT(__isinff)(RTStrNanFloat("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isinfl)(           1.0L), 0);
    CHECK_INT(RT_NOCRT(__isinfl)( 2394.2340e200L), 0);
    CHECK_INT(RT_NOCRT(__isinfl)(-2394.2340e200L), 0);
    CHECK_INT(RT_NOCRT(__isinfl)(-INFINITY), 1);
    CHECK_INT(RT_NOCRT(__isinfl)(+INFINITY), 1);
    CHECK_INT(RT_NOCRT(__isinfl)(RTStrNanLongDouble(NULL, true)), 0);
    CHECK_INT(RT_NOCRT(__isinfl)(RTStrNanLongDouble("4sig", false)), 0);
}


void testIsNan()
{
    RTTestSub(g_hTest, "isnan[f],__isnanl");
#undef isnan
    CHECK_INT(RT_NOCRT(isnan)(           0.0), 0);
    CHECK_INT(RT_NOCRT(isnan)(           1.0), 0);
    CHECK_INT(RT_NOCRT(isnan)( 2394.2340e200), 0);
    CHECK_INT(RT_NOCRT(isnan)(-2394.2340e200), 0);
    CHECK_INT(RT_NOCRT(isnan)(-INFINITY), 0);
    CHECK_INT(RT_NOCRT(isnan)(+INFINITY), 0);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble(NULL,          true)),  1);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble(NULL,          false)), 1);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble("435876quiet", false)), 1);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble("435876quiet", true)),  1);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble("678sig",      false)), 1);
    CHECK_INT(RT_NOCRT(isnan)(RTStrNanDouble("45547absig",  true)),  1);

    CHECK_INT(RT_NOCRT(isnanf)(          0.0f), 0);
    CHECK_INT(RT_NOCRT(isnanf)(          1.0f), 0);
    CHECK_INT(RT_NOCRT(isnanf)( 2394.2340e20f), 0);
    CHECK_INT(RT_NOCRT(isnanf)(-2394.2340e20f), 0);
    CHECK_INT(RT_NOCRT(isnanf)(-INFINITY), 0);
    CHECK_INT(RT_NOCRT(isnanf)(+INFINITY), 0);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat(NULL,        true)),  1);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat(NULL,        false)), 1);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat("9560q",     false)), 1);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat("aaaaq",     true)),  1);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat("4sig",      false)), 1);
    CHECK_INT(RT_NOCRT(isnanf)(RTStrNanFloat("69504sig",  true)),  1);

    CHECK_INT(RT_NOCRT(__isnanl)(           0.0L), 0);
    CHECK_INT(RT_NOCRT(__isnanl)(           1.0L), 0);
    CHECK_INT(RT_NOCRT(__isnanl)( 2394.2340e200L), 0);
    CHECK_INT(RT_NOCRT(__isnanl)(-2394.2340e200L), 0);
    CHECK_INT(RT_NOCRT(__isnanl)(-INFINITY), 0);
    CHECK_INT(RT_NOCRT(__isnanl)(+INFINITY), 0);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble(NULL,           true)),  1);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble(NULL,           false)), 1);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble("bbbbq",        false)), 1);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble("11122q",       true)),  1);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble("4sig",         false)), 1);
    CHECK_INT(RT_NOCRT(__isnanl)(RTStrNanLongDouble("23423406sig",  true)),  1);
}


void testIsFinite()
{
    RTTestSub(g_hTest, "__isfinite[fl]");
    CHECK_INT(RT_NOCRT(__isfinite)(           1.0),  1);
    CHECK_INT(RT_NOCRT(__isfinite)( 2394.2340e200),  1);
    CHECK_INT(RT_NOCRT(__isfinite)(-2394.2340e200),  1);
    CHECK_INT(RT_NOCRT(__isfinite)(-2.1984e-310),    1); /* subnormal */
    CHECK_INT(RT_NOCRT(__isfinite)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinite)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinite)(RTStrNanDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isfinite)(RTStrNanDouble("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isfinitef)(          1.0f),  1);
    CHECK_INT(RT_NOCRT(__isfinitef)( 2394.2340e20f),  1);
    CHECK_INT(RT_NOCRT(__isfinitef)(-2394.2340e20f),  1);
    CHECK_INT(RT_NOCRT(__isfinitef)(-2.1984e-40f),    1); /* subnormal */
    CHECK_INT(RT_NOCRT(__isfinitef)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinitef)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinitef)(RTStrNanFloat(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isfinitef)(RTStrNanFloat("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isfinitel)(           1.0L), 1);
    CHECK_INT(RT_NOCRT(__isfinitel)( 2394.2340e200L), 1);
    CHECK_INT(RT_NOCRT(__isfinitel)(-2394.2340e200L), 1);
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_INT(RT_NOCRT(__isfinitel)(-2.1984e-310L),   1); /* subnormal */
#else
    CHECK_INT(RT_NOCRT(__isfinitel)(-2.1984e-4935L),  1); /* subnormal */
#endif
    CHECK_INT(RT_NOCRT(__isfinitel)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinitel)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isfinitel)(RTStrNanLongDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isfinitel)(RTStrNanLongDouble("4sig", false)), 0);
}


void testIsNormal()
{
    RTTestSub(g_hTest, "__isnormal[fl]");
    CHECK_INT(RT_NOCRT(__isnormal)(           1.0),  1);
    CHECK_INT(RT_NOCRT(__isnormal)( 2394.2340e200),  1);
    CHECK_INT(RT_NOCRT(__isnormal)(-2394.2340e200),  1);
    CHECK_INT(RT_NOCRT(__isnormal)(-2.1984e-310),    0); /* subnormal */
    CHECK_INT(RT_NOCRT(__isnormal)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormal)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormal)(RTStrNanDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isnormal)(RTStrNanDouble("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isnormalf)(          1.0f),  1);
    CHECK_INT(RT_NOCRT(__isnormalf)( 2394.2340e20f),  1);
    CHECK_INT(RT_NOCRT(__isnormalf)(-2394.2340e20f),  1);
    CHECK_INT(RT_NOCRT(__isnormalf)(-2.1984e-40f),    0); /* subnormal */
    CHECK_INT(RT_NOCRT(__isnormalf)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormalf)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormalf)(RTStrNanFloat(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isnormalf)(RTStrNanFloat("4sig", false)), 0);

    CHECK_INT(RT_NOCRT(__isnormall)(           1.0L), 1);
    CHECK_INT(RT_NOCRT(__isnormall)( 2394.2340e200L), 1);
    CHECK_INT(RT_NOCRT(__isnormall)(-2394.2340e200L), 1);
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_INT(RT_NOCRT(__isnormall)(-2.1984e-310L),   0); /* subnormal */
#else
    CHECK_INT(RT_NOCRT(__isnormall)(-2.1984e-4935L),  0); /* subnormal */
#endif
    CHECK_INT(RT_NOCRT(__isnormall)(-INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormall)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__isnormall)(RTStrNanLongDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__isnormall)(RTStrNanLongDouble("4sig", false)), 0);
}


void testFpClassify()
{
    RTTestSub(g_hTest, "__fpclassify[dfl]");
    CHECK_INT(RT_NOCRT(__fpclassifyd)(          +0.0),  RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(          -0.0),  RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(           1.0),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyd)( 2394.2340e200),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(-2394.2340e200),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(-2.1984e-310),    RT_NOCRT_FP_SUBNORMAL); /* subnormal */
    CHECK_INT(RT_NOCRT(__fpclassifyd)(-INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(+INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(RTStrNanDouble(NULL,   true)),  RT_NOCRT_FP_NAN);
    CHECK_INT(RT_NOCRT(__fpclassifyd)(RTStrNanDouble("4sig", false)), RT_NOCRT_FP_NAN);

    CHECK_INT(RT_NOCRT(__fpclassifyf)(         +0.0f),  RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(         -0.0f),  RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(          1.0f),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyf)( 2394.2340e20f),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(-2394.2340e20f),  RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(-2.1984e-40f),    RT_NOCRT_FP_SUBNORMAL); /* subnormal */
    CHECK_INT(RT_NOCRT(__fpclassifyf)(-INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(+INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(RTStrNanFloat(NULL,   true)),  RT_NOCRT_FP_NAN);
    CHECK_INT(RT_NOCRT(__fpclassifyf)(RTStrNanFloat("4sig", false)), RT_NOCRT_FP_NAN);

    CHECK_INT(RT_NOCRT(__fpclassifyl)(          +0.0L), RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(          -0.0L), RT_NOCRT_FP_ZERO);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(           1.0L), RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyl)( 2394.2340e200L), RT_NOCRT_FP_NORMAL);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(-2394.2340e200L), RT_NOCRT_FP_NORMAL);
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_INT(RT_NOCRT(__fpclassifyl)(-2.1984e-310L),   RT_NOCRT_FP_SUBNORMAL); /* subnormal */
#else
    CHECK_INT(RT_NOCRT(__fpclassifyl)(-2.1984e-4935L),  RT_NOCRT_FP_SUBNORMAL); /* subnormal */
#endif
    CHECK_INT(RT_NOCRT(__fpclassifyl)(-INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(+INFINITY),       RT_NOCRT_FP_INFINITE);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(RTStrNanLongDouble(NULL,   true)),  RT_NOCRT_FP_NAN);
    CHECK_INT(RT_NOCRT(__fpclassifyl)(RTStrNanLongDouble("4sig", false)), RT_NOCRT_FP_NAN);
}


void testSignBit()
{
    RTTestSub(g_hTest, "__signbit[fl]");
    CHECK_INT(RT_NOCRT(__signbit)(          +0.0),  0);
    CHECK_INT(RT_NOCRT(__signbit)(          -0.0),  1);
    CHECK_INT(RT_NOCRT(__signbit)(           1.0),  0);
    CHECK_INT(RT_NOCRT(__signbit)( 2394.2340e200),  0);
    CHECK_INT(RT_NOCRT(__signbit)(-2394.2340e200),  1);
    CHECK_INT(RT_NOCRT(__signbit)(-2.1984e-310),    1); /* subnormal */
    CHECK_INT(RT_NOCRT(__signbit)(-INFINITY),       1);
    CHECK_INT(RT_NOCRT(__signbit)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__signbit)(RTStrNanDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__signbit)(RTStrNanDouble("4sig", false)), 1);

    CHECK_INT(RT_NOCRT(__signbitf)(         +0.0f),  0);
    CHECK_INT(RT_NOCRT(__signbitf)(         -0.0f),  1);
    CHECK_INT(RT_NOCRT(__signbitf)(          1.0f),  0);
    CHECK_INT(RT_NOCRT(__signbitf)( 2394.2340e20f),  0);
    CHECK_INT(RT_NOCRT(__signbitf)(-2394.2340e20f),  1);
    CHECK_INT(RT_NOCRT(__signbitf)(-2.1984e-40f),    1); /* subnormal */
    CHECK_INT(RT_NOCRT(__signbitf)(-INFINITY),       1);
    CHECK_INT(RT_NOCRT(__signbitf)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__signbitf)(RTStrNanFloat(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__signbitf)(RTStrNanFloat("4sig", false)), 1);

    CHECK_INT(RT_NOCRT(__signbitl)(          +0.0L), 0);
    CHECK_INT(RT_NOCRT(__signbitl)(          -0.0L), 1);
    CHECK_INT(RT_NOCRT(__signbitl)(           1.0L), 0);
    CHECK_INT(RT_NOCRT(__signbitl)( 2394.2340e200L), 0);
    CHECK_INT(RT_NOCRT(__signbitl)(-2394.2340e200L), 1);
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_INT(RT_NOCRT(__signbitl)(-2.1984e-310L),   1); /* subnormal */
#else
    CHECK_INT(RT_NOCRT(__signbitl)(-2.1984e-4935L),  1); /* subnormal */
#endif
    CHECK_INT(RT_NOCRT(__signbitl)(-INFINITY),       1);
    CHECK_INT(RT_NOCRT(__signbitl)(+INFINITY),       0);
    CHECK_INT(RT_NOCRT(__signbitl)(RTStrNanLongDouble(NULL,   true)),  0);
    CHECK_INT(RT_NOCRT(__signbitl)(RTStrNanLongDouble("4sig", false)), 1);
}


void testCeil()
{
    RTTestSub(g_hTest, "ceil[f]");
    CHECK_DBL(RT_NOCRT(ceil)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(ceil)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(ceil)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(ceil)( -42.5), -42.0);
    CHECK_DBL(RT_NOCRT(ceil)( +42.5), +43.0);
    CHECK_DBL(RT_NOCRT(ceil)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(ceil)(+42.25), +43.0);
    CHECK_DBL_SAME(ceil,(              -0.0));
    CHECK_DBL_SAME(ceil,(              +0.0));
    CHECK_DBL_SAME(ceil,(            +42.25));
    CHECK_DBL_SAME(ceil,(+1234.60958634e+10));
    CHECK_DBL_SAME(ceil,(-1234.60958634e+10));
    CHECK_DBL_SAME(ceil,(  -1234.499999e+10));
    CHECK_DBL_SAME(ceil,(  -1234.499999e-10));
    CHECK_DBL_SAME(ceil,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(ceil,(-INFINITY));
    CHECK_DBL_SAME(ceil,(+INFINITY));
    CHECK_DBL_SAME(ceil,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(ceil,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(ceilf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(ceilf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(ceilf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(ceilf)( -42.5f), -42.0f);
    CHECK_DBL(RT_NOCRT(ceilf)( +42.5f), +43.0f);
    CHECK_DBL(RT_NOCRT(ceilf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(ceilf)(+42.25f), +43.0f);
    CHECK_DBL_SAME(ceilf,(              -0.0f));
    CHECK_DBL_SAME(ceilf,(              +0.0f));
    CHECK_DBL_SAME(ceilf,(            +42.25f));
    CHECK_DBL_SAME(ceilf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(ceilf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(ceilf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(ceilf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(ceilf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(ceilf,(-INFINITY));
    CHECK_DBL_SAME(ceilf,(+INFINITY));
    CHECK_DBL_SAME(ceilf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(ceilf,(RTStrNanFloat("s", false)));
}


void testFloor()
{
    RTTestSub(g_hTest, "floor[f]");
    CHECK_DBL(RT_NOCRT(floor)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(floor)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(floor)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(floor)( -42.5), -43.0);
    CHECK_DBL(RT_NOCRT(floor)( +42.5), +42.0);
    CHECK_DBL(RT_NOCRT(floor)(-42.25), -43.0);
    CHECK_DBL(RT_NOCRT(floor)(+42.25), +42.0);
    CHECK_DBL_SAME(floor,(              -0.0));
    CHECK_DBL_SAME(floor,(              +0.0));
    CHECK_DBL_SAME(floor,(            +42.25));
    CHECK_DBL_SAME(floor,(+1234.60958634e+10));
    CHECK_DBL_SAME(floor,(-1234.60958634e+10));
    CHECK_DBL_SAME(floor,(  -1234.499999e+10));
    CHECK_DBL_SAME(floor,(  -1234.499999e-10));
    CHECK_DBL_SAME(floor,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(floor,(-INFINITY));
    CHECK_DBL_SAME(floor,(+INFINITY));
    CHECK_DBL_SAME(floor,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(floor,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(floorf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(floorf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(floorf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(floorf)( -42.5f), -43.0f);
    CHECK_DBL(RT_NOCRT(floorf)( +42.5f), +42.0f);
    CHECK_DBL(RT_NOCRT(floorf)(-42.25f), -43.0f);
    CHECK_DBL(RT_NOCRT(floorf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(floorf,(              -0.0f));
    CHECK_DBL_SAME(floorf,(              +0.0f));
    CHECK_DBL_SAME(floorf,(            +42.25f));
    CHECK_DBL_SAME(floorf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(floorf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(floorf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(floorf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(floorf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(floorf,(-INFINITY));
    CHECK_DBL_SAME(floorf,(+INFINITY));
    CHECK_DBL_SAME(floorf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(floorf,(RTStrNanFloat("s", false)));
}


void testTrunc()
{
    RTTestSub(g_hTest, "trunc[f]");
    CHECK_DBL(RT_NOCRT(trunc)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(trunc)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(trunc)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(trunc)( -42.5), -42.0);
    CHECK_DBL(RT_NOCRT(trunc)( +42.5), +42.0);
    CHECK_DBL(RT_NOCRT(trunc)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(trunc)(+42.25), +42.0);
    CHECK_DBL_SAME(trunc,(              -0.0));
    CHECK_DBL_SAME(trunc,(              +0.0));
    CHECK_DBL_SAME(trunc,(            +42.25));
    CHECK_DBL_SAME(trunc,(+1234.60958634e+10));
    CHECK_DBL_SAME(trunc,(-1234.60958634e+10));
    CHECK_DBL_SAME(trunc,(  -1234.499999e+10));
    CHECK_DBL_SAME(trunc,(  -1234.499999e-10));
    CHECK_DBL_SAME(trunc,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(trunc,(-INFINITY));
    CHECK_DBL_SAME(trunc,(+INFINITY));
    CHECK_DBL_SAME(trunc,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(trunc,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(truncf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(truncf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(truncf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(truncf)( -42.5f), -42.0f);
    CHECK_DBL(RT_NOCRT(truncf)( +42.5f), +42.0f);
    CHECK_DBL(RT_NOCRT(truncf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(truncf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(truncf,(              -0.0f));
    CHECK_DBL_SAME(truncf,(              +0.0f));
    CHECK_DBL_SAME(truncf,(            +42.25f));
    CHECK_DBL_SAME(truncf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(truncf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(truncf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(truncf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(truncf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(truncf,(-INFINITY));
    CHECK_DBL_SAME(truncf,(+INFINITY));
    CHECK_DBL_SAME(truncf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(truncf,(RTStrNanFloat("s", false)));
}


void testRound()
{
    RTTestSub(g_hTest, "round[f]");
    CHECK_DBL(RT_NOCRT(round)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(round)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(round)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(round)( -42.5), -43.0);
    CHECK_DBL(RT_NOCRT(round)( +42.5), +43.0);
    CHECK_DBL(RT_NOCRT(round)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(round)(+42.25), +42.0);
    CHECK_DBL_SAME(round,(              -0.0));
    CHECK_DBL_SAME(round,(              +0.0));
    CHECK_DBL_SAME(round,(            +42.25));
    CHECK_DBL_SAME(round,(+1234.60958634e+10));
    CHECK_DBL_SAME(round,(-1234.60958634e+10));
    CHECK_DBL_SAME(round,(  -1234.499999e+10));
    CHECK_DBL_SAME(round,(  -1234.499999e-10));
    CHECK_DBL_SAME(round,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(round,(-INFINITY));
    CHECK_DBL_SAME(round,(+INFINITY));
    CHECK_DBL_SAME(round,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(round,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(roundf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(roundf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(roundf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(roundf)( -42.5f), -43.0f);
    CHECK_DBL(RT_NOCRT(roundf)( +42.5f), +43.0f);
    CHECK_DBL(RT_NOCRT(roundf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(roundf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(roundf,(              -0.0f));
    CHECK_DBL_SAME(roundf,(              +0.0f));
    CHECK_DBL_SAME(roundf,(            +42.25f));
    CHECK_DBL_SAME(roundf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(roundf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(roundf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(roundf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(roundf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(roundf,(-INFINITY));
    CHECK_DBL_SAME(roundf,(+INFINITY));
    CHECK_DBL_SAME(roundf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(roundf,(RTStrNanFloat("s", false)));
}


void testRInt()
{
    RTTestSub(g_hTest, "rint[f]");

    /*
     * Round nearest.
     */
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    AssertCompile(RT_NOCRT_FE_TONEAREST  == X86_FCW_RC_NEAREST);
    AssertCompile(RT_NOCRT_FE_DOWNWARD   == X86_FCW_RC_DOWN);
    AssertCompile(RT_NOCRT_FE_UPWARD     == X86_FCW_RC_UP);
    AssertCompile(RT_NOCRT_FE_TOWARDZERO == X86_FCW_RC_ZERO);
    AssertCompile(RT_NOCRT_FE_ROUND_MASK == X86_FCW_RC_MASK);
#endif
    int const iSavedMode = RT_NOCRT(fegetround)();
    if (iSavedMode != FE_TONEAREST)
        RTTestFailed(g_hTest, "expected FE_TONEAREST as default rounding mode, not %#x (%d)", iSavedMode, iSavedMode);
    RT_NOCRT(fesetround)(FE_TONEAREST);

    CHECK_DBL(RT_NOCRT(rint)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(rint)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.5), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( +42.5), +42.0);
    CHECK_DBL(RT_NOCRT(rint)( -43.5), -44.0);
    CHECK_DBL(RT_NOCRT(rint)( +43.5), +44.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.25), +42.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.75), -43.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.75), +43.0);
    CHECK_DBL_SAME(rint,(              -0.0));
    CHECK_DBL_SAME(rint,(              +0.0));
    CHECK_DBL_SAME(rint,(            +42.25));
    CHECK_DBL_SAME(rint,(            +42.50));
    CHECK_DBL_SAME(rint,(            +42.75));
    CHECK_DBL_SAME(rint,(            -42.25));
    CHECK_DBL_SAME(rint,(            -42.50));
    CHECK_DBL_SAME(rint,(            -42.75));
    CHECK_DBL_SAME(rint,(+1234.60958634e+10));
    CHECK_DBL_SAME(rint,(-1234.60958634e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e-10));
    CHECK_DBL_SAME(rint,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(rint,(-INFINITY));
    CHECK_DBL_SAME(rint,(+INFINITY));
    CHECK_DBL_SAME(rint,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(rint,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(rintf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(rintf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.5f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +42.5f), +42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -43.5f), -44.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +43.5f), +44.0f);
    CHECK_DBL(RT_NOCRT(rintf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(rintf,(              -0.0f));
    CHECK_DBL_SAME(rintf,(              +0.0f));
    CHECK_DBL_SAME(rintf,(            +42.25f));
    CHECK_DBL_SAME(rintf,(            +42.50f));
    CHECK_DBL_SAME(rintf,(            +42.75f));
    CHECK_DBL_SAME(rintf,(            -42.25f));
    CHECK_DBL_SAME(rintf,(            -42.50f));
    CHECK_DBL_SAME(rintf,(            -42.75f));
    CHECK_DBL_SAME(rintf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(rintf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(rintf,(-INFINITY));
    CHECK_DBL_SAME(rintf,(+INFINITY));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat("s", false)));

    /*
     * Round UP.
     */
    RT_NOCRT(fesetround)(FE_UPWARD);

    CHECK_DBL(RT_NOCRT(rint)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(rint)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.5), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( +42.5), +43.0);
    CHECK_DBL(RT_NOCRT(rint)( -43.5), -43.0);
    CHECK_DBL(RT_NOCRT(rint)( +43.5), +44.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.25), +43.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.75), -42.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.75), +43.0);
    CHECK_DBL_SAME(rint,(              -0.0));
    CHECK_DBL_SAME(rint,(              +0.0));
    CHECK_DBL_SAME(rint,(            +42.25));
    CHECK_DBL_SAME(rint,(            +42.50));
    CHECK_DBL_SAME(rint,(            +42.75));
    CHECK_DBL_SAME(rint,(            -42.25));
    CHECK_DBL_SAME(rint,(            -42.50));
    CHECK_DBL_SAME(rint,(            -42.75));
    CHECK_DBL_SAME(rint,(+1234.60958634e+10));
    CHECK_DBL_SAME(rint,(-1234.60958634e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e-10));
    CHECK_DBL_SAME(rint,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(rint,(-INFINITY));
    CHECK_DBL_SAME(rint,(+INFINITY));
    CHECK_DBL_SAME(rint,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(rint,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(rintf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(rintf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.5f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +42.5f), +43.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -43.5f), -43.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +43.5f), +44.0f);
    CHECK_DBL(RT_NOCRT(rintf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)(+42.25f), +43.0f);
    CHECK_DBL_SAME(rintf,(              -0.0f));
    CHECK_DBL_SAME(rintf,(              +0.0f));
    CHECK_DBL_SAME(rintf,(            +42.25f));
    CHECK_DBL_SAME(rintf,(            +42.50f));
    CHECK_DBL_SAME(rintf,(            +42.75f));
    CHECK_DBL_SAME(rintf,(            -42.25f));
    CHECK_DBL_SAME(rintf,(            -42.50f));
    CHECK_DBL_SAME(rintf,(            -42.75f));
    CHECK_DBL_SAME(rintf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(rintf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(rintf,(-INFINITY));
    CHECK_DBL_SAME(rintf,(+INFINITY));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat("s", false)));

    /*
     * Round DOWN.
     */
    RT_NOCRT(fesetround)(FE_DOWNWARD);

    CHECK_DBL(RT_NOCRT(rint)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(rint)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.5), -43.0);
    CHECK_DBL(RT_NOCRT(rint)( +42.5), +42.0);
    CHECK_DBL(RT_NOCRT(rint)( -43.5), -44.0);
    CHECK_DBL(RT_NOCRT(rint)( +43.5), +43.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.25), -43.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.25), +42.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.75), -43.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.75), +42.0);
    CHECK_DBL_SAME(rint,(              -0.0));
    CHECK_DBL_SAME(rint,(              +0.0));
    CHECK_DBL_SAME(rint,(            +42.25));
    CHECK_DBL_SAME(rint,(            +42.50));
    CHECK_DBL_SAME(rint,(            +42.75));
    CHECK_DBL_SAME(rint,(            -42.25));
    CHECK_DBL_SAME(rint,(            -42.50));
    CHECK_DBL_SAME(rint,(            -42.75));
    CHECK_DBL_SAME(rint,(+1234.60958634e+10));
    CHECK_DBL_SAME(rint,(-1234.60958634e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e-10));
    CHECK_DBL_SAME(rint,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(rint,(-INFINITY));
    CHECK_DBL_SAME(rint,(+INFINITY));
    CHECK_DBL_SAME(rint,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(rint,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(rintf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(rintf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.5f), -43.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +42.5f), +42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -43.5f), -44.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +43.5f), +43.0f);
    CHECK_DBL(RT_NOCRT(rintf)(-42.25f), -43.0f);
    CHECK_DBL(RT_NOCRT(rintf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(rintf,(              -0.0f));
    CHECK_DBL_SAME(rintf,(              +0.0f));
    CHECK_DBL_SAME(rintf,(            +42.25f));
    CHECK_DBL_SAME(rintf,(            +42.50f));
    CHECK_DBL_SAME(rintf,(            +42.75f));
    CHECK_DBL_SAME(rintf,(            -42.25f));
    CHECK_DBL_SAME(rintf,(            -42.50f));
    CHECK_DBL_SAME(rintf,(            -42.75f));
    CHECK_DBL_SAME(rintf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(rintf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(rintf,(-INFINITY));
    CHECK_DBL_SAME(rintf,(+INFINITY));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat("s", false)));

    /*
     * Round towards ZERO.
     */
    RT_NOCRT(fesetround)(FE_TOWARDZERO);

    CHECK_DBL(RT_NOCRT(rint)(  +0.0),  +0.0);
    CHECK_DBL(RT_NOCRT(rint)(  -0.0),  -0.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.0), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( -42.5), -42.0);
    CHECK_DBL(RT_NOCRT(rint)( +42.5), +42.0);
    CHECK_DBL(RT_NOCRT(rint)( -43.5), -43.0);
    CHECK_DBL(RT_NOCRT(rint)( +43.5), +43.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.25), -42.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.25), +42.0);
    CHECK_DBL(RT_NOCRT(rint)(-42.75), -42.0);
    CHECK_DBL(RT_NOCRT(rint)(+42.75), +42.0);
    CHECK_DBL_SAME(rint,(              -0.0));
    CHECK_DBL_SAME(rint,(              +0.0));
    CHECK_DBL_SAME(rint,(            +42.25));
    CHECK_DBL_SAME(rint,(            +42.50));
    CHECK_DBL_SAME(rint,(            +42.75));
    CHECK_DBL_SAME(rint,(            -42.25));
    CHECK_DBL_SAME(rint,(            -42.50));
    CHECK_DBL_SAME(rint,(            -42.75));
    CHECK_DBL_SAME(rint,(+1234.60958634e+10));
    CHECK_DBL_SAME(rint,(-1234.60958634e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e+10));
    CHECK_DBL_SAME(rint,(  -1234.499999e-10));
    CHECK_DBL_SAME(rint,(      -2.1984e-310)); /* subnormal */
    CHECK_DBL_SAME(rint,(-INFINITY));
    CHECK_DBL_SAME(rint,(+INFINITY));
    CHECK_DBL_SAME(rint,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(rint,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(rintf)(  +0.0f),  +0.0f);
    CHECK_DBL(RT_NOCRT(rintf)(  -0.0f),  -0.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.0f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -42.5f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +42.5f), +42.0f);
    CHECK_DBL(RT_NOCRT(rintf)( -43.5f), -43.0f);
    CHECK_DBL(RT_NOCRT(rintf)( +43.5f), +43.0f);
    CHECK_DBL(RT_NOCRT(rintf)(-42.25f), -42.0f);
    CHECK_DBL(RT_NOCRT(rintf)(+42.25f), +42.0f);
    CHECK_DBL_SAME(rintf,(              -0.0f));
    CHECK_DBL_SAME(rintf,(              +0.0f));
    CHECK_DBL_SAME(rintf,(            +42.25f));
    CHECK_DBL_SAME(rintf,(            +42.50f));
    CHECK_DBL_SAME(rintf,(            +42.75f));
    CHECK_DBL_SAME(rintf,(            -42.25f));
    CHECK_DBL_SAME(rintf,(            -42.50f));
    CHECK_DBL_SAME(rintf,(            -42.75f));
    CHECK_DBL_SAME(rintf,(+1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(-1234.60958634e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e+10f));
    CHECK_DBL_SAME(rintf,(  -1234.499999e-10f));
    CHECK_DBL_SAME(rintf,(       -2.1984e-40f)); /* subnormal */
    CHECK_DBL_SAME(rintf,(-INFINITY));
    CHECK_DBL_SAME(rintf,(+INFINITY));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(rintf,(RTStrNanFloat("s", false)));

    RT_NOCRT(fesetround)(iSavedMode);
}


void testLRound()
{
    RTTestSub(g_hTest, "lround[f]");
    CHECK_LONG(RT_NOCRT(lround)(              +0.0),                0);
    CHECK_LONG(RT_NOCRT(lround)(              -0.0),                0);
    CHECK_LONG(RT_NOCRT(lround)(             -42.0),              -42);
    CHECK_LONG(RT_NOCRT(lround)(             -42.5),              -43);
    CHECK_LONG(RT_NOCRT(lround)(             +42.5),              +43);
    CHECK_LONG(RT_NOCRT(lround)(            -42.25),              -42);
    CHECK_LONG(RT_NOCRT(lround)(            +42.25),              +42);
    CHECK_LONG(RT_NOCRT(lround)(+1234.60958634e+20),         LONG_MAX);
    CHECK_LONG(RT_NOCRT(lround)(-1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lround)(  -1234.499999e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lround)(  -1234.499999e-10),                0);
    CHECK_LONG(RT_NOCRT(lround)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lround)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lround)(+INFINITY),                  LONG_MAX);
    CHECK_LONG(RT_NOCRT(lround)(RTStrNanDouble(NULL, true)), LONG_MAX);
    CHECK_LONG(RT_NOCRT(lround)(RTStrNanDouble("s", false)), LONG_MAX);
    CHECK_LONG_SAME(lround,(              -0.0));
    CHECK_LONG_SAME(lround,(              +0.0));
    CHECK_LONG_SAME(lround,(            +42.25));
    CHECK_LONG_SAME(lround,(         +42.25e+6));
    CHECK_LONG_SAME(lround,(         -42.25e+6));
    CHECK_LONG_SAME(lround,(  -1234.499999e-10));
    CHECK_LONG_SAME(lround,(      -2.1984e-310)); /* subnormal */
#if 0 /* unspecified, we test our expected behavior above */
    CHECK_LONG_SAME(lround,(+1234.60958634e+20));
    CHECK_LONG_SAME(lround,(-1234.60958634e+20));
    CHECK_LONG_SAME(lround,(  -1234.499999e+20));
    CHECK_LONG_SAME(lround,(-INFINITY));
    CHECK_LONG_SAME(lround,(+INFINITY));
    CHECK_LONG_SAME(lround,(RTStrNanDouble(NULL, true)));
    CHECK_LONG_SAME(lround,(RTStrNanDouble("s", false)));
#endif

    CHECK_LONG(RT_NOCRT(lroundf)(              +0.0f),               0);
    CHECK_LONG(RT_NOCRT(lroundf)(              -0.0f),               0);
    CHECK_LONG(RT_NOCRT(lroundf)(             -42.0f),             -42);
    CHECK_LONG(RT_NOCRT(lroundf)(             -42.5f),             -43);
    CHECK_LONG(RT_NOCRT(lroundf)(             +42.5f),             +43);
    CHECK_LONG(RT_NOCRT(lroundf)(            -42.25f),             -42);
    CHECK_LONG(RT_NOCRT(lroundf)(            +42.25f),             +42);
    CHECK_LONG(RT_NOCRT(lroundf)(+1234.60958634e+20f),        LONG_MAX);
    CHECK_LONG(RT_NOCRT(lroundf)(-1234.60958634e+20f),        LONG_MIN);
    CHECK_LONG(RT_NOCRT(lroundf)(  -1234.499999e+20f),        LONG_MIN);
    CHECK_LONG(RT_NOCRT(lroundf)(  -1234.499999e-10f),               0);
    CHECK_LONG(RT_NOCRT(lroundf)(       -2.1984e-40f),               0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lroundf)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lroundf)(+INFINITY),                  LONG_MAX);
    CHECK_LONG(RT_NOCRT(lroundf)(RTStrNanFloat(NULL, true)),  LONG_MAX);
    CHECK_LONG(RT_NOCRT(lroundf)(RTStrNanFloat("s", false)),  LONG_MAX);
    CHECK_LONG_SAME(lroundf,(              -0.0f));
    CHECK_LONG_SAME(lroundf,(              +0.0f));
    CHECK_LONG_SAME(lroundf,(            +42.25f));
    CHECK_LONG_SAME(lroundf,(         +42.25e+6f));
    CHECK_LONG_SAME(lroundf,(         -42.25e+6f));
    CHECK_LONG_SAME(lroundf,(  -1234.499999e-10f));
    CHECK_LONG_SAME(lroundf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* unspecified, we test our expected behavior above */
    CHECK_LONG_SAME(lroundf,(+1234.60958634e+20f));
    CHECK_LONG_SAME(lroundf,(-1234.60958634e+20f));
    CHECK_LONG_SAME(lroundf,(  -1234.499999e+20f));
    CHECK_LONG_SAME(lroundf,(-INFINITY));
    CHECK_LONG_SAME(lroundf,(+INFINITY));
    CHECK_LONG_SAME(lroundf,(RTStrNanFloat(NULL, true)));
    CHECK_LONG_SAME(lroundf,(RTStrNanFloat("s", false)));
#endif
}


void testLLRound()
{
    RTTestSub(g_hTest, "llround[f]");
    CHECK_LLONG(RT_NOCRT(llround)(  +0.0),                             0);
    CHECK_LLONG(RT_NOCRT(llround)(  -0.0),                             0);
    CHECK_LLONG(RT_NOCRT(llround)( -42.0),                           -42);
    CHECK_LLONG(RT_NOCRT(llround)( -42.5),                           -43);
    CHECK_LLONG(RT_NOCRT(llround)( +42.5),                           +43);
    CHECK_LLONG(RT_NOCRT(llround)(-42.25),                           -42);
    CHECK_LLONG(RT_NOCRT(llround)(+42.25),                           +42);
    CHECK_LLONG(RT_NOCRT(llround)(+42.25e4),                     +422500);
    CHECK_LLONG(RT_NOCRT(llround)(+42.25e12),          +42250000000000LL);
    CHECK_LLONG(RT_NOCRT(llround)(+1234.60958634e+20),         LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llround)(-1234.60958634e+20),         LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llround)(  -1234.499999e+20),         LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llround)(  -1234.499999e-10),                 0);
    CHECK_LLONG(RT_NOCRT(llround)(      -2.1984e-310),                 0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llround)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llround)(+INFINITY),                  LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llround)(RTStrNanDouble(NULL, true)), LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llround)(RTStrNanDouble("s", false)), LLONG_MAX);
    CHECK_LLONG_SAME(llround,(              -0.0));
    CHECK_LLONG_SAME(llround,(              +0.0));
    CHECK_LLONG_SAME(llround,(            +42.25));
    CHECK_LLONG_SAME(llround,(         +42.25e+6));
    CHECK_LLONG_SAME(llround,(         -42.25e+6));
    CHECK_LLONG_SAME(llround,(        -42.25e+12));
    CHECK_LLONG_SAME(llround,(    +42.265785e+13));
    CHECK_LLONG_SAME(llround,(  -1234.499999e-10));
    CHECK_LLONG_SAME(llround,(      -2.1984e-310)); /* subnormal */
#if 0 /* unspecified, we test our expected behavior above */
    CHECK_LLONG_SAME(llround,(+1234.60958634e+20));
    CHECK_LLONG_SAME(llround,(-1234.60958634e+20));
    CHECK_LLONG_SAME(llround,(  -1234.499999e+20));
    CHECK_LLONG_SAME(llround,(-INFINITY));
    CHECK_LLONG_SAME(llround,(+INFINITY));
    CHECK_LLONG_SAME(llround,(RTStrNanDouble(NULL, true)));
    CHECK_LLONG_SAME(llround,(RTStrNanDouble("s", false)));
#endif

    CHECK_LLONG(RT_NOCRT(llroundf)(  +0.0f),                            0);
    CHECK_LLONG(RT_NOCRT(llroundf)(  -0.0f),                            0);
    CHECK_LLONG(RT_NOCRT(llroundf)( -42.0f),                          -42);
    CHECK_LLONG(RT_NOCRT(llroundf)( -42.5f),                          -43);
    CHECK_LLONG(RT_NOCRT(llroundf)( +42.5f),                          +43);
    CHECK_LLONG(RT_NOCRT(llroundf)(-42.25f),                          -42);
    CHECK_LLONG(RT_NOCRT(llroundf)(+42.25f),                          +42);
    CHECK_LLONG(RT_NOCRT(llroundf)(+42.25e4f),                    +422500);
    CHECK_LLONG(RT_NOCRT(llroundf)(+42.24e10f),           +422400000000LL);
    CHECK_LLONG(RT_NOCRT(llroundf)(+1234.60958634e+20f),        LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundf)(-1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundf)(  -1234.499999e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundf)(  -1234.499999e-10f),                0);
    CHECK_LLONG(RT_NOCRT(llroundf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llroundf)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundf)(+INFINITY),                  LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundf)(RTStrNanFloat(NULL, true)),  LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundf)(RTStrNanFloat("s", false)),  LLONG_MAX);
    CHECK_LLONG_SAME(llroundf,(              -0.0f));
    CHECK_LLONG_SAME(llroundf,(              +0.0f));
    CHECK_LLONG_SAME(llroundf,(            +42.25f));
    CHECK_LLONG_SAME(llroundf,(         +42.25e+6f));
    CHECK_LLONG_SAME(llroundf,(         -42.25e+6f));
    CHECK_LLONG_SAME(llroundf,(        -42.25e+12f));
    CHECK_LLONG_SAME(llroundf,(    +42.265785e+13f));
    CHECK_LLONG_SAME(llroundf,(  -1234.499999e-10f));
    CHECK_LLONG_SAME(llroundf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* unspecified, we test our expected behavior above */
    CHECK_LLONG_SAME(llroundf,(+1234.60958634e+20f));
    CHECK_LLONG_SAME(llroundf,(-1234.60958634e+20f));
    CHECK_LLONG_SAME(llroundf,(  -1234.499999e+20f));
    CHECK_LLONG_SAME(llroundf,(-INFINITY));
    CHECK_LLONG_SAME(llroundf,(+INFINITY));
    CHECK_LLONG_SAME(llroundf,(RTStrNanFloat(NULL, true)));
    CHECK_LLONG_SAME(llroundf,(RTStrNanFloat("s", false)));
#endif

#if 0
    CHECK_LLONG(RT_NOCRT(llroundl)(  +0.0L),                                 0);
    CHECK_LLONG(RT_NOCRT(llroundl)(  -0.0L),                                 0);
    CHECK_LLONG(RT_NOCRT(llroundl)( -42.0L),                               -42);
    CHECK_LLONG(RT_NOCRT(llroundl)( -42.5L),                               -43);
    CHECK_LLONG(RT_NOCRT(llroundl)( +42.5L),                               +43);
    CHECK_LLONG(RT_NOCRT(llroundl)(-42.25L),                               -42);
    CHECK_LLONG(RT_NOCRT(llroundl)(+42.25L),                               +42);
    CHECK_LLONG(RT_NOCRT(llroundl)(+42.25e4L),                         +422500);
    CHECK_LLONG(RT_NOCRT(llroundl)(+42.24e12L),              +42240000000000LL);
    CHECK_LLONG(RT_NOCRT(llroundl)(+1234.60958634e+20L),             LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundl)(-1234.60958634e+20L),             LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundl)(  -1234.499999e+20L),             LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundl)(  -1234.499999e-10L),                     0);
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_LLONG(RT_NOCRT(llroundl)(      -2.1984e-310L),                     0); /* subnormal */
#else
    CHECK_LLONG(RT_NOCRT(llroundl)(     -2.1984e-4935L),                     0); /* subnormal */
#endif
    CHECK_LLONG(RT_NOCRT(llroundl)(-INFINITY),                       LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llroundl)(+INFINITY),                       LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundl)(RTStrNanLongDouble(NULL, true)),  LLONG_MAX);
    CHECK_LLONG(RT_NOCRT(llroundl)(RTStrNanLongDouble("s", false)),  LLONG_MAX);
    CHECK_LLONG_SAME(llroundl,(              -0.0));
    CHECK_LLONG_SAME(llroundl,(              +0.0));
    CHECK_LLONG_SAME(llroundl,(            +42.25));
    CHECK_LLONG_SAME(llroundl,(         +42.25e+6));
    CHECK_LLONG_SAME(llroundl,(         -42.25e+6));
    CHECK_LLONG_SAME(llroundl,(        -42.25e+12));
    CHECK_LLONG_SAME(llroundl,(    +42.265785e+13));
    CHECK_LLONG_SAME(llroundl,(  -1234.499999e-10L));
# ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    CHECK_LLONG_SAME(llroundl,(      -2.1984e-310L)); /* subnormal */
# else
    CHECK_LLONG_SAME(llroundl,(     -2.1984e-4935L)); /* subnormal */
# endif
#if 0 /* unspecified, we test our expected behavior above */
    CHECK_LLONG_SAME(llroundl,(+1234.60958634e+20L));
    CHECK_LLONG_SAME(llroundl,(-1234.60958634e+20L));
    CHECK_LLONG_SAME(llroundl,(  -1234.499999e+20L));
    CHECK_LLONG_SAME(llroundl,(-INFINITY));
    CHECK_LLONG_SAME(llroundl,(+INFINITY));
    CHECK_LLONG_SAME(llroundl,(RTStrNanLongDouble(NULL, true)));
    CHECK_LLONG_SAME(llroundl,(RTStrNanLongDouble("s", false)));
#endif
#endif
}


void testLRInt()
{
    RTTestSub(g_hTest, "lrint[f]");

    /*
     * Round nearest.
     */
    int const iSavedMode = RT_NOCRT(fegetround)();
    if (iSavedMode != FE_TONEAREST)
        RTTestFailed(g_hTest, "expected FE_TONEAREST as default rounding mode, not %#x (%d)", iSavedMode, iSavedMode);
    RT_NOCRT(fesetround)(FE_TONEAREST);

    CHECK_LONG(RT_NOCRT(lrint)(              +0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(              -0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.0),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.5),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             +42.5),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(             -43.5),              -44);
    CHECK_LONG(RT_NOCRT(lrint)(             +43.5),              +44);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.25),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.25),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.75),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.75),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(+1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(-1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e-10),                0);
    CHECK_LONG(RT_NOCRT(lrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrint)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(+INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble(NULL, true)), LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble("s", false)), LONG_MIN);
    CHECK_LONG_SAME(lrint,(              -0.0));
    CHECK_LONG_SAME(lrint,(              +0.0));
    CHECK_LONG_SAME(lrint,(            +42.25));
    CHECK_LONG_SAME(lrint,(            -42.25));
    CHECK_LONG_SAME(lrint,(            +42.75));
    CHECK_LONG_SAME(lrint,(            -42.75));
    CHECK_LONG_SAME(lrint,(             +22.5));
    CHECK_LONG_SAME(lrint,(             -22.5));
    CHECK_LONG_SAME(lrint,(             +23.5));
    CHECK_LONG_SAME(lrint,(             -23.5));
    CHECK_LONG_SAME(lrint,(         +42.25e+6));
    CHECK_LONG_SAME(lrint,(         -42.25e+6));
    CHECK_LONG_SAME(lrint,(  -1234.499999e-10));
    CHECK_LONG_SAME(lrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrint,(+1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(-1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(  -1234.499999e+20));
    CHECK_LONG_SAME(lrint,(-INFINITY));
    CHECK_LONG_SAME(lrint,(+INFINITY));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble(NULL, true)));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LONG(RT_NOCRT(lrintf)(              +0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(              -0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.0f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.5f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             +42.5f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -43.5f),              -44);
    CHECK_LONG(RT_NOCRT(lrintf)(             +43.5f),              +44);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.25f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.25f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.75f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.75f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(+1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(-1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e-10f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrintf)(-INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(+INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble(NULL, true)),  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble("s", false)),  LONG_MIN);
    CHECK_LONG_SAME(lrintf,(              -0.0f));
    CHECK_LONG_SAME(lrintf,(              +0.0f));
    CHECK_LONG_SAME(lrintf,(            +42.25f));
    CHECK_LONG_SAME(lrintf,(            -42.25f));
    CHECK_LONG_SAME(lrintf,(            +42.75f));
    CHECK_LONG_SAME(lrintf,(            -42.75f));
    CHECK_LONG_SAME(lrintf,(             +22.5f));
    CHECK_LONG_SAME(lrintf,(             -22.5f));
    CHECK_LONG_SAME(lrintf,(             +23.5f));
    CHECK_LONG_SAME(lrintf,(             -23.5f));
    CHECK_LONG_SAME(lrintf,(         +42.25e+6f));
    CHECK_LONG_SAME(lrintf,(         -42.25e+6f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e-10f));
    CHECK_LONG_SAME(lrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrintf,(+1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(-1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e+20f));
    CHECK_LONG_SAME(lrintf,(-INFINITY));
    CHECK_LONG_SAME(lrintf,(+INFINITY));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round UP.
     */
    RT_NOCRT(fesetround)(FE_UPWARD);

    CHECK_LONG(RT_NOCRT(lrint)(              +0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(              -0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.0),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.5),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             +42.5),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(             -43.5),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(             +43.5),              +44);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.25),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.25),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.75),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.75),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(+1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(-1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e-10),                0);
    CHECK_LONG(RT_NOCRT(lrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrint)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(+INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble(NULL, true)), LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble("s", false)), LONG_MIN);
    CHECK_LONG_SAME(lrint,(              -0.0));
    CHECK_LONG_SAME(lrint,(              +0.0));
    CHECK_LONG_SAME(lrint,(            +42.25));
    CHECK_LONG_SAME(lrint,(            -42.25));
    CHECK_LONG_SAME(lrint,(            +42.75));
    CHECK_LONG_SAME(lrint,(            -42.75));
    CHECK_LONG_SAME(lrint,(             +22.5));
    CHECK_LONG_SAME(lrint,(             -22.5));
    CHECK_LONG_SAME(lrint,(             +23.5));
    CHECK_LONG_SAME(lrint,(             -23.5));
    CHECK_LONG_SAME(lrint,(         +42.25e+6));
    CHECK_LONG_SAME(lrint,(         -42.25e+6));
    CHECK_LONG_SAME(lrint,(  -1234.499999e-10));
    CHECK_LONG_SAME(lrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrint,(+1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(-1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(  -1234.499999e+20));
    CHECK_LONG_SAME(lrint,(-INFINITY));
    CHECK_LONG_SAME(lrint,(+INFINITY));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble(NULL, true)));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LONG(RT_NOCRT(lrintf)(              +0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(              -0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.0f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.5f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             +42.5f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(             -43.5f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(             +43.5f),              +44);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.25f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.25f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.75f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.75f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(+1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(-1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e-10f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrintf)(-INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(+INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble(NULL, true)),  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble("s", false)),  LONG_MIN);
    CHECK_LONG_SAME(lrintf,(              -0.0f));
    CHECK_LONG_SAME(lrintf,(              +0.0f));
    CHECK_LONG_SAME(lrintf,(            +42.25f));
    CHECK_LONG_SAME(lrintf,(            -42.25f));
    CHECK_LONG_SAME(lrintf,(            +42.75f));
    CHECK_LONG_SAME(lrintf,(            -42.75f));
    CHECK_LONG_SAME(lrintf,(             +22.5f));
    CHECK_LONG_SAME(lrintf,(             -22.5f));
    CHECK_LONG_SAME(lrintf,(             +23.5f));
    CHECK_LONG_SAME(lrintf,(             -23.5f));
    CHECK_LONG_SAME(lrintf,(         +42.25e+6f));
    CHECK_LONG_SAME(lrintf,(         -42.25e+6f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e-10f));
    CHECK_LONG_SAME(lrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrintf,(+1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(-1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e+20f));
    CHECK_LONG_SAME(lrintf,(-INFINITY));
    CHECK_LONG_SAME(lrintf,(+INFINITY));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round DOWN.
     */
    RT_NOCRT(fesetround)(FE_DOWNWARD);

    CHECK_LONG(RT_NOCRT(lrint)(              +0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(              -0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.0),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.5),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(             +42.5),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(             -43.5),              -44);
    CHECK_LONG(RT_NOCRT(lrint)(             +43.5),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.25),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.25),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.75),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.75),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(+1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(-1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e-10),               -1);
    CHECK_LONG(RT_NOCRT(lrint)(      -2.1984e-310),               -1); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrint)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(+INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble(NULL, true)), LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble("s", false)), LONG_MIN);
    CHECK_LONG_SAME(lrint,(              -0.0));
    CHECK_LONG_SAME(lrint,(              +0.0));
    CHECK_LONG_SAME(lrint,(            +42.25));
    CHECK_LONG_SAME(lrint,(            -42.25));
    CHECK_LONG_SAME(lrint,(            +42.75));
    CHECK_LONG_SAME(lrint,(            -42.75));
    CHECK_LONG_SAME(lrint,(             +22.5));
    CHECK_LONG_SAME(lrint,(             -22.5));
    CHECK_LONG_SAME(lrint,(             +23.5));
    CHECK_LONG_SAME(lrint,(             -23.5));
    CHECK_LONG_SAME(lrint,(         +42.25e+6));
    CHECK_LONG_SAME(lrint,(         -42.25e+6));
    CHECK_LONG_SAME(lrint,(  -1234.499999e-10));
    CHECK_LONG_SAME(lrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrint,(+1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(-1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(  -1234.499999e+20));
    CHECK_LONG_SAME(lrint,(-INFINITY));
    CHECK_LONG_SAME(lrint,(+INFINITY));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble(NULL, true)));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LONG(RT_NOCRT(lrintf)(              +0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(              -0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.0f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.5f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(             +42.5f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -43.5f),              -44);
    CHECK_LONG(RT_NOCRT(lrintf)(             +43.5f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.25f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.25f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.75f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.75f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(+1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(-1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e-10f),               -1);
    CHECK_LONG(RT_NOCRT(lrintf)(       -2.1984e-40f),               -1); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrintf)(-INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(+INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble(NULL, true)),  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble("s", false)),  LONG_MIN);
    CHECK_LONG_SAME(lrintf,(              -0.0f));
    CHECK_LONG_SAME(lrintf,(              +0.0f));
    CHECK_LONG_SAME(lrintf,(            +42.25f));
    CHECK_LONG_SAME(lrintf,(            -42.25f));
    CHECK_LONG_SAME(lrintf,(            +42.75f));
    CHECK_LONG_SAME(lrintf,(            -42.75f));
    CHECK_LONG_SAME(lrintf,(             +22.5f));
    CHECK_LONG_SAME(lrintf,(             -22.5f));
    CHECK_LONG_SAME(lrintf,(             +23.5f));
    CHECK_LONG_SAME(lrintf,(             -23.5f));
    CHECK_LONG_SAME(lrintf,(         +42.25e+6f));
    CHECK_LONG_SAME(lrintf,(         -42.25e+6f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e-10f));
    CHECK_LONG_SAME(lrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrintf,(+1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(-1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e+20f));
    CHECK_LONG_SAME(lrintf,(-INFINITY));
    CHECK_LONG_SAME(lrintf,(+INFINITY));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round towards ZERO.
     */
    RT_NOCRT(fesetround)(FE_TOWARDZERO);

    CHECK_LONG(RT_NOCRT(lrint)(              +0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(              -0.0),                0);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.0),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             -42.5),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(             +42.5),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(             -43.5),              -43);
    CHECK_LONG(RT_NOCRT(lrint)(             +43.5),              +43);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.25),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.25),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(            -42.75),              -42);
    CHECK_LONG(RT_NOCRT(lrint)(            +42.75),              +42);
    CHECK_LONG(RT_NOCRT(lrint)(+1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(-1234.60958634e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e+20),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(  -1234.499999e-10),                0);
    CHECK_LONG(RT_NOCRT(lrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrint)(-INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(+INFINITY),                  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble(NULL, true)), LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrint)(RTStrNanDouble("s", false)), LONG_MIN);
    CHECK_LONG_SAME(lrint,(              -0.0));
    CHECK_LONG_SAME(lrint,(              +0.0));
    CHECK_LONG_SAME(lrint,(            +42.25));
    CHECK_LONG_SAME(lrint,(            -42.25));
    CHECK_LONG_SAME(lrint,(            +42.75));
    CHECK_LONG_SAME(lrint,(            -42.75));
    CHECK_LONG_SAME(lrint,(             +22.5));
    CHECK_LONG_SAME(lrint,(             -22.5));
    CHECK_LONG_SAME(lrint,(             +23.5));
    CHECK_LONG_SAME(lrint,(             -23.5));
    CHECK_LONG_SAME(lrint,(         +42.25e+6));
    CHECK_LONG_SAME(lrint,(         -42.25e+6));
    CHECK_LONG_SAME(lrint,(  -1234.499999e-10));
    CHECK_LONG_SAME(lrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrint,(+1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(-1234.60958634e+20));
    CHECK_LONG_SAME(lrint,(  -1234.499999e+20));
    CHECK_LONG_SAME(lrint,(-INFINITY));
    CHECK_LONG_SAME(lrint,(+INFINITY));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble(NULL, true)));
    CHECK_LONG_SAME(lrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LONG(RT_NOCRT(lrintf)(              +0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(              -0.0f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.0f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -42.5f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(             +42.5f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(             -43.5f),              -43);
    CHECK_LONG(RT_NOCRT(lrintf)(             +43.5f),              +43);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.25f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.25f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(            -42.75f),              -42);
    CHECK_LONG(RT_NOCRT(lrintf)(            +42.75f),              +42);
    CHECK_LONG(RT_NOCRT(lrintf)(+1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(-1234.60958634e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e+20f),         LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(  -1234.499999e-10f),                0);
    CHECK_LONG(RT_NOCRT(lrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LONG(RT_NOCRT(lrintf)(-INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(+INFINITY),                   LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble(NULL, true)),  LONG_MIN);
    CHECK_LONG(RT_NOCRT(lrintf)(RTStrNanDouble("s", false)),  LONG_MIN);
    CHECK_LONG_SAME(lrintf,(              -0.0f));
    CHECK_LONG_SAME(lrintf,(              +0.0f));
    CHECK_LONG_SAME(lrintf,(            +42.25f));
    CHECK_LONG_SAME(lrintf,(            -42.25f));
    CHECK_LONG_SAME(lrintf,(            +42.75f));
    CHECK_LONG_SAME(lrintf,(            -42.75f));
    CHECK_LONG_SAME(lrintf,(             +22.5f));
    CHECK_LONG_SAME(lrintf,(             -22.5f));
    CHECK_LONG_SAME(lrintf,(             +23.5f));
    CHECK_LONG_SAME(lrintf,(             -23.5f));
    CHECK_LONG_SAME(lrintf,(         +42.25e+6f));
    CHECK_LONG_SAME(lrintf,(         -42.25e+6f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e-10f));
    CHECK_LONG_SAME(lrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LONG_SAME(lrintf,(+1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(-1234.60958634e+20f));
    CHECK_LONG_SAME(lrintf,(  -1234.499999e+20f));
    CHECK_LONG_SAME(lrintf,(-INFINITY));
    CHECK_LONG_SAME(lrintf,(+INFINITY));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LONG_SAME(lrintf,(RTStrNanFloat("s", false)));
#endif

    RT_NOCRT(fesetround)(iSavedMode);
}


void testLLRInt()
{
    RTTestSub(g_hTest, "llrint[f]");

    /*
     * Round nearest.
     */
    int const iSavedMode = RT_NOCRT(fegetround)();
    if (iSavedMode != FE_TONEAREST)
        RTTestFailed(g_hTest, "expected FE_TONEAREST as default rounding mode, not %#x (%d)", iSavedMode, iSavedMode);
    RT_NOCRT(fesetround)(FE_TONEAREST);

    CHECK_LLONG(RT_NOCRT(llrint)(              +0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(              -0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.0),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.5),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             +42.5),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -43.5),              -44);
    CHECK_LLONG(RT_NOCRT(llrint)(             +43.5),              +44);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.25),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.25),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.75),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.75),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(+1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(-1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e-10),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrint)(-INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(+INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble(NULL, true)),LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble("s", false)),LLONG_MIN);
    CHECK_LLONG_SAME(llrint,(              -0.0));
    CHECK_LLONG_SAME(llrint,(              +0.0));
    CHECK_LLONG_SAME(llrint,(            +42.25));
    CHECK_LLONG_SAME(llrint,(            -42.25));
    CHECK_LLONG_SAME(llrint,(            +42.75));
    CHECK_LLONG_SAME(llrint,(            -42.75));
    CHECK_LLONG_SAME(llrint,(             +22.5));
    CHECK_LLONG_SAME(llrint,(             -22.5));
    CHECK_LLONG_SAME(llrint,(             +23.5));
    CHECK_LLONG_SAME(llrint,(             -23.5));
    CHECK_LLONG_SAME(llrint,(         +42.25e+6));
    CHECK_LLONG_SAME(llrint,(         -42.25e+6));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e-10));
    CHECK_LLONG_SAME(llrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrint,(+1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(-1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e+20));
    CHECK_LLONG_SAME(llrint,(-INFINITY));
    CHECK_LLONG_SAME(llrint,(+INFINITY));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble(NULL, true)));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LLONG(RT_NOCRT(llrintf)(              +0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(              -0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.0f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.5f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +42.5f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -43.5f),              -44);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +43.5f),              +44);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.25f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.25f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.75f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.75f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(+1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(-1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e-10f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrintf)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(+INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble(NULL, true)), LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble("s", false)), LLONG_MIN);
    CHECK_LLONG_SAME(llrintf,(              -0.0f));
    CHECK_LLONG_SAME(llrintf,(              +0.0f));
    CHECK_LLONG_SAME(llrintf,(            +42.25f));
    CHECK_LLONG_SAME(llrintf,(            -42.25f));
    CHECK_LLONG_SAME(llrintf,(            +42.75f));
    CHECK_LLONG_SAME(llrintf,(            -42.75f));
    CHECK_LLONG_SAME(llrintf,(             +22.5f));
    CHECK_LLONG_SAME(llrintf,(             -22.5f));
    CHECK_LLONG_SAME(llrintf,(             +23.5f));
    CHECK_LLONG_SAME(llrintf,(             -23.5f));
    CHECK_LLONG_SAME(llrintf,(         +42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(         -42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e-10f));
    CHECK_LLONG_SAME(llrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrintf,(+1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(-1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e+20f));
    CHECK_LLONG_SAME(llrintf,(-INFINITY));
    CHECK_LLONG_SAME(llrintf,(+INFINITY));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round UP.
     */
    RT_NOCRT(fesetround)(FE_UPWARD);

    CHECK_LLONG(RT_NOCRT(llrint)(              +0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(              -0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.0),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.5),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             +42.5),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(             -43.5),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(             +43.5),              +44);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.25),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.25),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.75),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.75),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(+1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(-1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e-10),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrint)(-INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(+INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble(NULL, true)),LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble("s", false)),LLONG_MIN);
    CHECK_LLONG_SAME(llrint,(              -0.0));
    CHECK_LLONG_SAME(llrint,(              +0.0));
    CHECK_LLONG_SAME(llrint,(            +42.25));
    CHECK_LLONG_SAME(llrint,(            -42.25));
    CHECK_LLONG_SAME(llrint,(            +42.75));
    CHECK_LLONG_SAME(llrint,(            -42.75));
    CHECK_LLONG_SAME(llrint,(             +22.5));
    CHECK_LLONG_SAME(llrint,(             -22.5));
    CHECK_LLONG_SAME(llrint,(             +23.5));
    CHECK_LLONG_SAME(llrint,(             -23.5));
    CHECK_LLONG_SAME(llrint,(         +42.25e+6));
    CHECK_LLONG_SAME(llrint,(         -42.25e+6));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e-10));
    CHECK_LLONG_SAME(llrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrint,(+1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(-1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e+20));
    CHECK_LLONG_SAME(llrint,(-INFINITY));
    CHECK_LLONG_SAME(llrint,(+INFINITY));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble(NULL, true)));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LLONG(RT_NOCRT(llrintf)(              +0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(              -0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.0f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.5f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +42.5f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -43.5f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +43.5f),              +44);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.25f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.25f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.75f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.75f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(+1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(-1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e-10f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrintf)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(+INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble(NULL, true)), LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble("s", false)), LLONG_MIN);
    CHECK_LLONG_SAME(llrintf,(              -0.0f));
    CHECK_LLONG_SAME(llrintf,(              +0.0f));
    CHECK_LLONG_SAME(llrintf,(            +42.25f));
    CHECK_LLONG_SAME(llrintf,(            -42.25f));
    CHECK_LLONG_SAME(llrintf,(            +42.75f));
    CHECK_LLONG_SAME(llrintf,(            -42.75f));
    CHECK_LLONG_SAME(llrintf,(             +22.5f));
    CHECK_LLONG_SAME(llrintf,(             -22.5f));
    CHECK_LLONG_SAME(llrintf,(             +23.5f));
    CHECK_LLONG_SAME(llrintf,(             -23.5f));
    CHECK_LLONG_SAME(llrintf,(         +42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(         -42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e-10f));
    CHECK_LLONG_SAME(llrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrintf,(+1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(-1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e+20f));
    CHECK_LLONG_SAME(llrintf,(-INFINITY));
    CHECK_LLONG_SAME(llrintf,(+INFINITY));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round DOWN.
     */
    RT_NOCRT(fesetround)(FE_DOWNWARD);

    CHECK_LLONG(RT_NOCRT(llrint)(              +0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(              -0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.0),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.5),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(             +42.5),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -43.5),              -44);
    CHECK_LLONG(RT_NOCRT(llrint)(             +43.5),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.25),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.25),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.75),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.75),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(+1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(-1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e-10),               -1);
    CHECK_LLONG(RT_NOCRT(llrint)(      -2.1984e-310),               -1); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrint)(-INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(+INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble(NULL, true)),LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble("s", false)),LLONG_MIN);
    CHECK_LLONG_SAME(llrint,(              -0.0));
    CHECK_LLONG_SAME(llrint,(              +0.0));
    CHECK_LLONG_SAME(llrint,(            +42.25));
    CHECK_LLONG_SAME(llrint,(            -42.25));
    CHECK_LLONG_SAME(llrint,(            +42.75));
    CHECK_LLONG_SAME(llrint,(            -42.75));
    CHECK_LLONG_SAME(llrint,(             +22.5));
    CHECK_LLONG_SAME(llrint,(             -22.5));
    CHECK_LLONG_SAME(llrint,(             +23.5));
    CHECK_LLONG_SAME(llrint,(             -23.5));
    CHECK_LLONG_SAME(llrint,(         +42.25e+6));
    CHECK_LLONG_SAME(llrint,(         -42.25e+6));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e-10));
    CHECK_LLONG_SAME(llrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrint,(+1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(-1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e+20));
    CHECK_LLONG_SAME(llrint,(-INFINITY));
    CHECK_LLONG_SAME(llrint,(+INFINITY));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble(NULL, true)));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LLONG(RT_NOCRT(llrintf)(              +0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(              -0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.0f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.5f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +42.5f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -43.5f),              -44);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +43.5f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.25f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.25f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.75f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.75f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(+1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(-1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e-10f),               -1);
    CHECK_LLONG(RT_NOCRT(llrintf)(       -2.1984e-40f),               -1); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrintf)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(+INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble(NULL, true)), LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble("s", false)), LLONG_MIN);
    CHECK_LLONG_SAME(llrintf,(              -0.0f));
    CHECK_LLONG_SAME(llrintf,(              +0.0f));
    CHECK_LLONG_SAME(llrintf,(            +42.25f));
    CHECK_LLONG_SAME(llrintf,(            -42.25f));
    CHECK_LLONG_SAME(llrintf,(            +42.75f));
    CHECK_LLONG_SAME(llrintf,(            -42.75f));
    CHECK_LLONG_SAME(llrintf,(             +22.5f));
    CHECK_LLONG_SAME(llrintf,(             -22.5f));
    CHECK_LLONG_SAME(llrintf,(             +23.5f));
    CHECK_LLONG_SAME(llrintf,(             -23.5f));
    CHECK_LLONG_SAME(llrintf,(         +42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(         -42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e-10f));
    CHECK_LLONG_SAME(llrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrintf,(+1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(-1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e+20f));
    CHECK_LLONG_SAME(llrintf,(-INFINITY));
    CHECK_LLONG_SAME(llrintf,(+INFINITY));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat("s", false)));
#endif

    /*
     * Round towards ZERO.
     */
    RT_NOCRT(fesetround)(FE_TOWARDZERO);

    CHECK_LLONG(RT_NOCRT(llrint)(              +0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(              -0.0),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.0),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -42.5),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(             +42.5),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(             -43.5),              -43);
    CHECK_LLONG(RT_NOCRT(llrint)(             +43.5),              +43);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.25),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.25),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(            -42.75),              -42);
    CHECK_LLONG(RT_NOCRT(llrint)(            +42.75),              +42);
    CHECK_LLONG(RT_NOCRT(llrint)(+1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(-1234.60958634e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e+20),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(  -1234.499999e-10),                0);
    CHECK_LLONG(RT_NOCRT(llrint)(      -2.1984e-310),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrint)(-INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(+INFINITY),                 LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble(NULL, true)),LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrint)(RTStrNanDouble("s", false)),LLONG_MIN);
    CHECK_LLONG_SAME(llrint,(              -0.0));
    CHECK_LLONG_SAME(llrint,(              +0.0));
    CHECK_LLONG_SAME(llrint,(            +42.25));
    CHECK_LLONG_SAME(llrint,(            -42.25));
    CHECK_LLONG_SAME(llrint,(            +42.75));
    CHECK_LLONG_SAME(llrint,(            -42.75));
    CHECK_LLONG_SAME(llrint,(             +22.5));
    CHECK_LLONG_SAME(llrint,(             -22.5));
    CHECK_LLONG_SAME(llrint,(             +23.5));
    CHECK_LLONG_SAME(llrint,(             -23.5));
    CHECK_LLONG_SAME(llrint,(         +42.25e+6));
    CHECK_LLONG_SAME(llrint,(         -42.25e+6));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e-10));
    CHECK_LLONG_SAME(llrint,(      -2.1984e-310)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrint,(+1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(-1234.60958634e+20));
    CHECK_LLONG_SAME(llrint,(  -1234.499999e+20));
    CHECK_LLONG_SAME(llrint,(-INFINITY));
    CHECK_LLONG_SAME(llrint,(+INFINITY));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble(NULL, true)));
    CHECK_LLONG_SAME(llrint,(RTStrNanDouble("s", false)));
#endif

    CHECK_LLONG(RT_NOCRT(llrintf)(              +0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(              -0.0f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.0f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -42.5f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +42.5f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(             -43.5f),              -43);
    CHECK_LLONG(RT_NOCRT(llrintf)(             +43.5f),              +43);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.25f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.25f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            -42.75f),              -42);
    CHECK_LLONG(RT_NOCRT(llrintf)(            +42.75f),              +42);
    CHECK_LLONG(RT_NOCRT(llrintf)(+1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(-1234.60958634e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e+20f),        LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(  -1234.499999e-10f),                0);
    CHECK_LLONG(RT_NOCRT(llrintf)(       -2.1984e-40f),                0); /* subnormal */
    CHECK_LLONG(RT_NOCRT(llrintf)(-INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(+INFINITY),                  LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble(NULL, true)), LLONG_MIN);
    CHECK_LLONG(RT_NOCRT(llrintf)(RTStrNanDouble("s", false)), LLONG_MIN);
    CHECK_LLONG_SAME(llrintf,(              -0.0f));
    CHECK_LLONG_SAME(llrintf,(              +0.0f));
    CHECK_LLONG_SAME(llrintf,(            +42.25f));
    CHECK_LLONG_SAME(llrintf,(            -42.25f));
    CHECK_LLONG_SAME(llrintf,(            +42.75f));
    CHECK_LLONG_SAME(llrintf,(            -42.75f));
    CHECK_LLONG_SAME(llrintf,(             +22.5f));
    CHECK_LLONG_SAME(llrintf,(             -22.5f));
    CHECK_LLONG_SAME(llrintf,(             +23.5f));
    CHECK_LLONG_SAME(llrintf,(             -23.5f));
    CHECK_LLONG_SAME(llrintf,(         +42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(         -42.25e+6f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e-10f));
    CHECK_LLONG_SAME(llrintf,(       -2.1984e-40f)); /* subnormal */
#if 0 /* Undefined, we disagree with UCRT on windows. */
    CHECK_LLONG_SAME(llrintf,(+1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(-1234.60958634e+20f));
    CHECK_LLONG_SAME(llrintf,(  -1234.499999e+20f));
    CHECK_LLONG_SAME(llrintf,(-INFINITY));
    CHECK_LLONG_SAME(llrintf,(+INFINITY));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat(NULL, true)));
    CHECK_LLONG_SAME(llrintf,(RTStrNanFloat("s", false)));
#endif

    RT_NOCRT(fesetround)(iSavedMode);
}


void testExp2()
{
    RTTestSub(g_hTest, "exp2[f]");

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


    CHECK_FLT(RT_NOCRT(exp2f)(1.0f), 2.0f);
    CHECK_FLT(RT_NOCRT(exp2f)(2.0f), 4.0f);
    CHECK_FLT(RT_NOCRT(exp2f)(32.0f), 4294967296.0f);
    CHECK_FLT(RT_NOCRT(exp2f)(-1.0f), 0.5f);
    CHECK_FLT(RT_NOCRT(exp2f)(-3.0f), 0.125f);
    CHECK_FLT_SAME(exp2f, (0.0f));
    CHECK_FLT_SAME(exp2f, (+INFINITY));
    CHECK_FLT_SAME(exp2f, (-INFINITY));
    CHECK_FLT_SAME(exp2f, (nan("1")));
    CHECK_FLT_SAME(exp2f, (RTStrNanFloat("ab305f", true)));
    CHECK_FLT_SAME(exp2f, (RTStrNanFloat("3fffff_signaling", true)));
    CHECK_FLT_SAME(exp2f, (RTStrNanFloat("79778_sig", false)));
    CHECK_FLT_SAME(exp2f, (1.0f));
    CHECK_FLT_SAME(exp2f, (2.0f));
    CHECK_FLT_SAME(exp2f, (-1.0f));
    CHECK_FLT_APPROX_SAME(exp2f, (+0.5f));
    CHECK_FLT_APPROX_SAME(exp2f, (-0.5f));
    CHECK_FLT_APPROX_SAME(exp2f, (+1.5f));
    CHECK_FLT_APPROX_SAME(exp2f, (-1.5f));
    CHECK_FLT_APPROX_SAME(exp2f, (+3.25f));
    CHECK_FLT_APPROX_SAME(exp2f, (99.25594f));
    CHECK_FLT_APPROX_SAME(exp2f, (-99.25594f));
    CHECK_FLT_APPROX_SAME(exp2f, (+305.25594f));
    CHECK_FLT_APPROX_SAME(exp2f, (-305.25594f));
    CHECK_FLT_APPROX_SAME(exp2f, (+309.99884f));
    CHECK_FLT_APPROX_SAME(exp2f, (-309.111048f));
    CHECK_FLT_APPROX_SAME(exp2f, (+999.86459f));
    CHECK_FLT_APPROX_SAME(exp2f, (-999.09823f));
}


void testLdExp()
{
    RTTestSub(g_hTest, "ldexp[f]");

    CHECK_DBL(RT_NOCRT(ldexp)(1.0,  1),          2.0);
    CHECK_DBL(RT_NOCRT(ldexp)(1.0,  2),          4.0);
    CHECK_DBL(RT_NOCRT(ldexp)(1.0, 32), 4294967296.0);
    CHECK_DBL(RT_NOCRT(ldexp)(2.0, 31), 4294967296.0);
    CHECK_DBL(RT_NOCRT(ldexp)(0.5, 33), 4294967296.0);
    CHECK_DBL(RT_NOCRT(ldexp)(1.0, -1),          0.5);
    CHECK_DBL(RT_NOCRT(ldexp)(1.0, -3),        0.125);
    CHECK_DBL_SAME(ldexp, (0.0, 0));
    CHECK_DBL_SAME(ldexp, (+INFINITY, 1));
    CHECK_DBL_SAME(ldexp, (+INFINITY, 2));
    CHECK_DBL_SAME(ldexp, (-INFINITY, 1));
    CHECK_DBL_SAME(ldexp, (-INFINITY, 2));
    CHECK_DBL_SAME(ldexp, (nan("1"), 1));
    CHECK_DBL_SAME(ldexp, (RTStrNanDouble("ab305f", true), 2));
    CHECK_DBL_SAME(ldexp, (RTStrNanDouble("fffffffff_signaling", true), 3));
    CHECK_DBL_SAME(ldexp, (RTStrNanDouble("7777777777778_sig", false), -4));
    CHECK_DBL_SAME(ldexp, (           1.0, 1));
    CHECK_DBL_SAME(ldexp, (           2.0, 2));
    CHECK_DBL_SAME(ldexp, (          -1.0, -1));
    CHECK_DBL_SAME(ldexp, (          +0.5, 63));
    CHECK_DBL_SAME(ldexp, (          -0.5, -73));
    CHECK_DBL_SAME(ldexp, (          +1.5, -88));
    CHECK_DBL_SAME(ldexp, (          -1.5, 99));
    CHECK_DBL_SAME(ldexp, (         +3.25, -102));
    CHECK_DBL_SAME(ldexp, (    99.2559430, -256));
    CHECK_DBL_SAME(ldexp, (   -99.2559430, 256));
    CHECK_DBL_SAME(ldexp, (  +305.2559430, 34));
    CHECK_DBL_SAME(ldexp, (  -305.2559430, 79));
    CHECK_DBL_SAME(ldexp, (    +309.99884, -99));
    CHECK_DBL_SAME(ldexp, (   -309.111048, -38));
    CHECK_DBL_SAME(ldexp, (+999.864597634, -21));
    CHECK_DBL_SAME(ldexp, (-999.098234837, 21));

    CHECK_FLT(RT_NOCRT(ldexpf)(1.0f,  1),          2.0f);
    CHECK_FLT(RT_NOCRT(ldexpf)(1.0f,  2),          4.0f);
    CHECK_FLT(RT_NOCRT(ldexpf)(1.0f, 32), 4294967296.0f);
    CHECK_FLT(RT_NOCRT(ldexpf)(2.0f, 31), 4294967296.0f);
    CHECK_FLT(RT_NOCRT(ldexpf)(0.5f, 33), 4294967296.0f);
    CHECK_FLT(RT_NOCRT(ldexpf)(1.0f, -1),          0.5f);
    CHECK_FLT(RT_NOCRT(ldexpf)(1.0f, -3),        0.125f);
    CHECK_FLT_SAME(ldexpf, (0.0f, 0));
    CHECK_FLT_SAME(ldexpf, (+INFINITY, 1));
    CHECK_FLT_SAME(ldexpf, (+INFINITY, 2));
    CHECK_FLT_SAME(ldexpf, (-INFINITY, 1));
    CHECK_FLT_SAME(ldexpf, (-INFINITY, 2));
    CHECK_FLT_SAME(ldexpf, (nan("1"), 1));
    CHECK_FLT_SAME(ldexpf, (RTStrNanDouble("ab305f", true), 2));
    CHECK_FLT_SAME(ldexpf, (RTStrNanDouble("fffffffff_signaling", true), 3));
    CHECK_FLT_SAME(ldexpf, (RTStrNanDouble("7777777777778_sig", false), -4));
    CHECK_FLT_SAME(ldexpf, (           1.0f, 1));
    CHECK_FLT_SAME(ldexpf, (           2.0f, 2));
    CHECK_FLT_SAME(ldexpf, (          -1.0f, -1));
    CHECK_FLT_SAME(ldexpf, (          +0.5f, 63));
    CHECK_FLT_SAME(ldexpf, (          -0.5f, -73));
    CHECK_FLT_SAME(ldexpf, (          +1.5f, -88));
    CHECK_FLT_SAME(ldexpf, (          -1.5f, 99));
    CHECK_FLT_SAME(ldexpf, (         +3.25f, -102));
    CHECK_FLT_SAME(ldexpf, (    99.2559430f, -256));
    CHECK_FLT_SAME(ldexpf, (   -99.2559430f, 256));
    CHECK_FLT_SAME(ldexpf, (  +305.2559430f, 34));
    CHECK_FLT_SAME(ldexpf, (  -305.2559430f, 79));
    CHECK_FLT_SAME(ldexpf, (    +309.99884f, -99));
    CHECK_FLT_SAME(ldexpf, (   -309.111048f, -38));
    CHECK_FLT_SAME(ldexpf, (+999.864597634f, -21));
    CHECK_FLT_SAME(ldexpf, (-999.098234837f, 21));

}


void testFma()
{
    RTTestSub(g_hTest, "fma[f]");

    CHECK_DBL(RT_NOCRT(fma)(1.0, 1.0,  1.0), 2.0);
    CHECK_DBL(RT_NOCRT(fma)(4.0, 2.0,  1.0), 9.0);
    CHECK_DBL(RT_NOCRT(fma)(4.0, 2.0, -1.0), 7.0);
    CHECK_DBL_SAME(fma, (0.0, 0.0, 0.0));
    CHECK_DBL_SAME(fma, (999999.0,            33334.0,       29345.0));
    CHECK_DBL_SAME(fma, (39560.32334,       9605.5546, -59079.345069));
    CHECK_DBL_SAME(fma, (39560.32334,   -59079.345069,     9605.5546));
    CHECK_DBL_SAME(fma, (-59079.345069,   39560.32334,     9605.5546));
    CHECK_DBL_SAME(fma, (+INFINITY, +INFINITY, -INFINITY));
    CHECK_DBL_SAME(fma, (4.0, +INFINITY, 2.0));
    CHECK_DBL_SAME(fma, (4.0, 4.0, +INFINITY));
    CHECK_DBL_SAME(fma, (-INFINITY, 4.0, 4.0));
    CHECK_DBL_SAME(fma, (2.34960584706e100, 7.6050698459e-13, 9.99996777e77));

    CHECK_FLT(RT_NOCRT(fmaf)(1.0f, 1.0f, 1.0), 2.0);
    CHECK_FLT(RT_NOCRT(fmaf)(4.0f, 2.0f, 1.0), 9.0);
    CHECK_FLT(RT_NOCRT(fmaf)(4.0f, 2.0f, -1.0), 7.0);
    CHECK_FLT_SAME(fmaf, (0.0f, 0.0f, 0.0f));
    CHECK_FLT_SAME(fmaf, (999999.0f,            33334.0f,       29345.0f));
    CHECK_FLT_SAME(fmaf, (39560.32334f,       9605.5546f, -59079.345069f));
    CHECK_FLT_SAME(fmaf, (39560.32334f,   -59079.345069f,     9605.5546f));
    CHECK_FLT_SAME(fmaf, (-59079.345069f,   39560.32334f,     9605.5546f));
    CHECK_FLT_SAME(fmaf, (+INFINITY, +INFINITY, -INFINITY));
    CHECK_FLT_SAME(fmaf, (4.0f, +INFINITY, 2.0f));
    CHECK_FLT_SAME(fmaf, (4.0f, 4.0f, +INFINITY));
    CHECK_FLT_SAME(fmaf, (-INFINITY, 4.0f, 4.0f));
    CHECK_FLT_SAME(fmaf, (2.34960584706e22f, 7.6050698459e-13f, 9.99996777e27f));
}


void testRemainder()
{
    RTTestSub(g_hTest, "remainder[f]");

    /* The UCRT and x87 FPU generally disagree on the sign of the NaN, so don't be too picky here for now. */

    CHECK_DBL(        RT_NOCRT(remainder)(              1.0,                    1.0), +0.0);
    CHECK_DBL(        RT_NOCRT(remainder)(              1.5,                    1.0), -0.5);
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(              1.0,                    1.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(              1.5,                    1.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(             +0.0,                   +0.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(             +0.0,                   -0.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(             -0.0,                   -0.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(             -0.0,                   +0.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(         999999.0,                33334.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(        -999999.0,                33334.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(        -999999.0,               -33334.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(         999999.0,               -33334.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(      39560.32334,              9605.5546));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(      39560.32334,          -59079.345069));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(        +INFINITY,              +INFINITY));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(              2.4,              +INFINITY));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(        +INFINITY,                    2.4));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(2.34960584706e100,       7.6050698459e+13));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(2.34960584706e300,      -7.6050698459e-13));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(2.34960584706e300, RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(RTStrNanDouble(NULL, true),           2.0));
    CHECK_DBL_SAME_RELAXED_NAN(remainder,(RTStrNanDouble(NULL, true), RTStrNanDouble("s", false)));

    CHECK_FLT(        RT_NOCRT(remainderf)(              1.0f,                   1.0f), +0.0f);
    CHECK_FLT(        RT_NOCRT(remainderf)(              1.5f,                   1.0f), -0.5f);
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(              1.0f,                   1.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(              1.5f,                   1.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(             +0.0f,                  +0.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(             +0.0f,                  -0.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(             -0.0f,                  -0.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(             -0.0f,                  +0.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(         999999.0f,               33334.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(        -999999.0f,               33334.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(        -999999.0f,              -33334.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(         999999.0f,              -33334.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(      39560.32334f,             9605.5546f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(      39560.32334f,         -59079.345069f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(         +INFINITY,              +INFINITY));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(              2.4f,              +INFINITY));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(         +INFINITY,                   2.4f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(-2.34960584706e+35f,     7.6050698459e-23f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(2.34960584706e+35f,      7.6050698459e-13f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(2.34960584706e+30f, RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(RTStrNanFloat(NULL, true),           2.0f));
    CHECK_FLT_SAME_RELAXED_NAN(remainderf,(RTStrNanFloat(NULL, true), RTStrNanFloat("s", false)));
}


void testLog()
{
    RTTestSub(g_hTest, "log[f]");

    CHECK_DBL(RT_NOCRT(log)(                  1.0), +0.0);
    CHECK_DBL(RT_NOCRT(log)(2.7182818284590452354), +1.0);
    CHECK_DBL(RT_NOCRT(log)(2.0), 0.69314718055994530942);
    CHECK_DBL_SAME(log,(              1.0));
    CHECK_DBL_SAME(log,(              1.5));
    CHECK_DBL_SAME(log,(             +0.0));
    CHECK_DBL_SAME(log,(             +0.0));
    CHECK_DBL_SAME(log,(             -0.0));
    CHECK_DBL_SAME(log,(             -0.0));
    CHECK_DBL_SAME(log,(         999999.0));
    CHECK_DBL_SAME(log,(        -999999.0));
    CHECK_DBL_SAME(log,(        -999999.0));
    CHECK_DBL_SAME(log,(         999999.0));
    CHECK_DBL_SAME(log,(      39560.32334));
    CHECK_DBL_SAME(log,(      39560.32334));
    CHECK_DBL_SAME(log,(        +INFINITY));
    CHECK_DBL_SAME(log,(        -INFINITY));
    CHECK_DBL_SAME(log,(         +DBL_MAX));
    CHECK_DBL_SAME(log,(         -DBL_MAX));
    CHECK_DBL_SAME(log,(2.34960584706e100));
    CHECK_DBL_SAME(log,(2.34960584706e300));
    CHECK_DBL_SAME(log,(2.34960584706e300));
    CHECK_DBL_SAME(log,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(log,(RTStrNanDouble("s",  true)));
    CHECK_DBL_SAME(log,(RTStrNanDouble("s", false)));

    CHECK_FLT(RT_NOCRT(logf)(                  1.0f), +0.0f);
    CHECK_FLT(RT_NOCRT(logf)((float)2.7182818284590452354), +0.99999995f); /* floating point is fun */
    CHECK_FLT(RT_NOCRT(logf)(2.0f), (float)0.69314718055994530942);
    CHECK_FLT_SAME(logf,((float)2.7182818284590452354));
    CHECK_FLT_SAME(logf,(              1.0f));
    CHECK_FLT_SAME(logf,(              1.5f));
    CHECK_FLT_SAME(logf,(             +0.0f));
    CHECK_FLT_SAME(logf,(             +0.0f));
    CHECK_FLT_SAME(logf,(             -0.0f));
    CHECK_FLT_SAME(logf,(             -0.0f));
    CHECK_FLT_SAME(logf,(         999999.0f));
    CHECK_FLT_SAME(logf,(        -999999.0f));
    CHECK_FLT_SAME(logf,(        -999999.0f));
    CHECK_FLT_SAME(logf,(         999999.0f));
    CHECK_FLT_SAME(logf,(      39560.32334f));
    CHECK_FLT_SAME(logf,(      39560.32334f));
    CHECK_FLT_SAME(logf,(        +INFINITY));
    CHECK_FLT_SAME(logf,(        -INFINITY));
    CHECK_FLT_SAME(logf,(         +FLT_MAX));
    CHECK_FLT_SAME(logf,(         -FLT_MAX));
    CHECK_FLT_SAME(logf,(2.34960584706e+10f));
    CHECK_FLT_SAME(logf,(2.34960584706e+30f));
    CHECK_FLT_SAME(logf,(2.34960584706e+30f));
    CHECK_FLT_SAME(logf,(RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME(logf,(RTStrNanFloat("s",  true)));
    CHECK_FLT_SAME(logf,(RTStrNanFloat("s", false)));
}


void testSqRt()
{
    RTTestSub(g_hTest, "sqrt[f]");

    CHECK_DBL(RT_NOCRT(sqrt)(              1.0),  +1.0);
    CHECK_DBL(RT_NOCRT(sqrt)(              4.0),  +2.0);
    CHECK_DBL(RT_NOCRT(sqrt)(            144.0), +12.0);
    CHECK_DBL(RT_NOCRT(sqrt)(             -1.0),  RTStrNanDouble(NULL, false));
    CHECK_DBL(RT_NOCRT(sqrt)(        -995.4547),  RTStrNanDouble(NULL, false));
    CHECK_DBL_SAME(    sqrt,(              1.0));
    CHECK_DBL_SAME(    sqrt,(              1.5));
    CHECK_DBL_SAME(    sqrt,(             +0.0));
    CHECK_DBL_SAME(    sqrt,(             +0.0));
    CHECK_DBL_SAME(    sqrt,(             -0.0));
    CHECK_DBL_SAME(    sqrt,(             -0.0));
    CHECK_DBL_SAME(    sqrt,(         999999.0));
    CHECK_DBL_SAME(    sqrt,(        -999999.0));
    CHECK_DBL_SAME(    sqrt,(        -999999.0));
    CHECK_DBL_SAME(    sqrt,(         999999.0));
    CHECK_DBL_SAME(    sqrt,(      39560.32334));
    CHECK_DBL_SAME(    sqrt,(      39560.32334));
    CHECK_DBL_SAME(    sqrt,(        +INFINITY));
    CHECK_DBL_SAME(    sqrt,(        -INFINITY));
    CHECK_DBL_SAME(    sqrt,(         +DBL_MAX));
    CHECK_DBL_SAME(    sqrt,(         -DBL_MAX));
    CHECK_DBL_SAME(    sqrt,(2.34960584706e100));
    CHECK_DBL_SAME(    sqrt,(2.34960584706e300));
    CHECK_DBL_SAME(    sqrt,(2.34960584706e300));
    CHECK_DBL_SAME(    sqrt,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(    sqrt,(RTStrNanDouble("s",  true)));
    CHECK_DBL_SAME(    sqrt,(RTStrNanDouble("s", false)));

    CHECK_FLT(RT_NOCRT(sqrtf)(              1.0f),  +1.0f);
    CHECK_FLT(RT_NOCRT(sqrtf)(              4.0f),  +2.0f);
    CHECK_FLT(RT_NOCRT(sqrtf)(            144.0f), +12.0f);
    CHECK_FLT(RT_NOCRT(sqrtf)(             -1.0f),  RTStrNanDouble(NULL, false));
    CHECK_FLT(RT_NOCRT(sqrtf)(        -995.4547f),  RTStrNanDouble(NULL, false));
    CHECK_FLT_SAME(    sqrtf,(              1.0f));
    CHECK_FLT_SAME(    sqrtf,(              1.5f));
    CHECK_FLT_SAME(    sqrtf,(             +0.0f));
    CHECK_FLT_SAME(    sqrtf,(             +0.0f));
    CHECK_FLT_SAME(    sqrtf,(             -0.0f));
    CHECK_FLT_SAME(    sqrtf,(             -0.0f));
    CHECK_FLT_SAME(    sqrtf,(         999999.0f));
    CHECK_FLT_SAME(    sqrtf,(        -999999.0f));
    CHECK_FLT_SAME(    sqrtf,(        -999999.0f));
    CHECK_FLT_SAME(    sqrtf,(         999999.0f));
    CHECK_FLT_SAME(    sqrtf,(      39560.32334f));
    CHECK_FLT_SAME(    sqrtf,(      39560.32334f));
    CHECK_FLT_SAME(    sqrtf,(        +INFINITY));
    CHECK_FLT_SAME(    sqrtf,(        -INFINITY));
    CHECK_FLT_SAME(    sqrtf,(         +FLT_MAX));
    CHECK_FLT_SAME(    sqrtf,(         -FLT_MAX));
    CHECK_FLT_SAME(    sqrtf,(2.34960584706e+10f));
    CHECK_FLT_SAME(    sqrtf,(2.34960584706e+30f));
    CHECK_FLT_SAME(    sqrtf,(2.34960584706e+30f));
    CHECK_FLT_SAME(    sqrtf,(RTStrNanDouble(NULL, true)));
    CHECK_FLT_SAME(    sqrtf,(RTStrNanDouble("s",  true)));
    CHECK_FLT_SAME(    sqrtf,(RTStrNanDouble("s", false)));
}


void testATan()
{
    RTTestSub(g_hTest, "atan[f]");

    CHECK_DBL(RT_NOCRT(atan)(             +1.0), +M_PI_4);
    CHECK_DBL(RT_NOCRT(atan)(             -1.0), -M_PI_4);
    CHECK_DBL(RT_NOCRT(atan)(        +INFINITY), +M_PI_2);
    CHECK_DBL(RT_NOCRT(atan)(        -INFINITY), -M_PI_2);
    CHECK_DBL_SAME(    atan,(              1.0));
    CHECK_DBL_SAME(    atan,(              1.5));
    CHECK_DBL_SAME(    atan,(             +0.0));
    CHECK_DBL_SAME(    atan,(             +0.0));
    CHECK_DBL_SAME(    atan,(             -0.0));
    CHECK_DBL_SAME(    atan,(             -0.0));
    CHECK_DBL_SAME(    atan,(      238.6634566));
    CHECK_DBL_SAME(    atan,(      -49.4578999));
    CHECK_DBL_SAME(    atan,(         999999.0));
    CHECK_DBL_SAME(    atan,(        -999999.0));
    CHECK_DBL_SAME(    atan,(        -999999.0));
    CHECK_DBL_SAME(    atan,(         999999.0));
    CHECK_DBL_SAME(    atan,(      39560.32334));
    CHECK_DBL_SAME(    atan,(      39560.32334));
    CHECK_DBL_SAME(    atan,(        +INFINITY));
    CHECK_DBL_SAME(    atan,(        -INFINITY));
    CHECK_DBL_SAME(    atan,(         +DBL_MAX));
    CHECK_DBL_SAME(    atan,(         -DBL_MAX));
    CHECK_DBL_SAME(    atan,(2.34960584706e100));
    CHECK_DBL_SAME(    atan,(2.34960584706e300));
    CHECK_DBL_SAME(    atan,(2.34960584706e300));
    CHECK_DBL_SAME(    atan,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(    atan,(RTStrNanDouble("s",  true)));
    CHECK_DBL_SAME(    atan,(RTStrNanDouble("s", false)));

    CHECK_DBL(RT_NOCRT(atanf)(             +1.0f), (float)+M_PI_4);
    CHECK_DBL(RT_NOCRT(atanf)(             -1.0f), (float)-M_PI_4);
    CHECK_DBL(RT_NOCRT(atanf)(         +INFINITY), (float)+M_PI_2);
    CHECK_DBL(RT_NOCRT(atanf)(         -INFINITY), (float)-M_PI_2);
    CHECK_DBL_SAME(    atanf,(              1.0f));
    CHECK_DBL_SAME(    atanf,(              1.5f));
    CHECK_DBL_SAME(    atanf,(             +0.0f));
    CHECK_DBL_SAME(    atanf,(             +0.0f));
    CHECK_DBL_SAME(    atanf,(             -0.0f));
    CHECK_DBL_SAME(    atanf,(             -0.0f));
    CHECK_DBL_SAME(    atanf,(      238.6634566f));
    CHECK_DBL_SAME(    atanf,(      -49.4578999f));
    CHECK_DBL_SAME(    atanf,(         999999.0f));
    CHECK_DBL_SAME(    atanf,(        -999999.0f));
    CHECK_DBL_SAME(    atanf,(        -999999.0f));
    CHECK_DBL_SAME(    atanf,(         999999.0f));
    CHECK_DBL_SAME(    atanf,(      39560.32334f));
    CHECK_DBL_SAME(    atanf,(      39560.32334f));
    CHECK_DBL_SAME(    atanf,(         +INFINITY));
    CHECK_DBL_SAME(    atanf,(         -INFINITY));
    CHECK_DBL_SAME(    atanf,(          +FLT_MAX));
    CHECK_DBL_SAME(    atanf,(          -FLT_MAX));
    CHECK_DBL_SAME(    atanf,(2.34960584706e+10f));
    CHECK_DBL_SAME(    atanf,(2.34960584706e+30f));
    CHECK_DBL_SAME(    atanf,(2.34960584706e+30f));
    CHECK_DBL_SAME(    atanf,(RTStrNanFloat(NULL, true)));
    CHECK_DBL_SAME(    atanf,(RTStrNanFloat("s",  true)));
    CHECK_DBL_SAME(    atanf,(RTStrNanFloat("s", false)));
}


void testATan2()
{
    RTTestSub(g_hTest, "atan2[f]");

    CHECK_DBL(RT_NOCRT(atan2)(             +1.0,            0.0), +M_PI_2);
    CHECK_DBL(RT_NOCRT(atan2)(             -1.0,            0.0), -M_PI_2);
    CHECK_DBL(RT_NOCRT(atan2)(             +1.0,           +1.0), +M_PI_4);
    CHECK_DBL(RT_NOCRT(atan2)(             -1.0,           -1.0), -M_PI_2 - M_PI_4);
    CHECK_DBL_SAME(    atan2,(             +1.0,            0.0));
    CHECK_DBL_SAME(    atan2,(             +1.0,           -0.0));
    CHECK_DBL_SAME(    atan2,(             -1.0,            0.0));
    CHECK_DBL_SAME(    atan2,(             -1.0,           -0.0));
    CHECK_DBL_SAME(    atan2,(             +1.0,           +1.0));
    CHECK_DBL_SAME(    atan2,(             -1.0,           +1.0));
    CHECK_DBL_SAME(    atan2,(             +1.0,           -1.0));
    CHECK_DBL_SAME(    atan2,(             -1.0,           -1.0));
    CHECK_DBL_SAME(    atan2,(      238.6634566,      -999999.0));
    CHECK_DBL_SAME(    atan2,(     -905698045.1,       490876.0));
    CHECK_DBL_SAME(    atan2,(     1.333334e-10,   -1.9993e+200));
    CHECK_DBL_SAME(    atan2,(    1.333334e+168,   -1.9993e+299));
    CHECK_DBL_SAME(    atan2,(         +DBL_MAX,       +DBL_MAX));
    CHECK_DBL_SAME(    atan2,(         -DBL_MAX,       +DBL_MAX));
    CHECK_DBL_SAME(    atan2,(        +INFINITY,      +INFINITY));
    CHECK_DBL_SAME(    atan2,(        -INFINITY,      +INFINITY));
    CHECK_DBL_SAME(    atan2,(        -INFINITY,      42.242424));
    CHECK_DBL_SAME(    atan2,(RTStrNanDouble(NULL, true), RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(    atan2,(RTStrNanDouble(NULL, false), RTStrNanDouble(NULL, false)));
    CHECK_DBL_SAME(    atan2,(RTStrNanDouble(NULL, false), RTStrNanDouble(NULL, true)));
    //CHECK_DBL_SAME(    atan2,(RTStrNanDouble(NULL, true), RTStrNanDouble(NULL, false))); - UCRT returns -QNaN, we +QNaN
    CHECK_DBL_SAME(    atan2,(RTStrNanDouble(NULL, true), RTStrNanDouble("s", false)));

    CHECK_FLT(RT_NOCRT(atan2f)(             +1.0f,            0.0f), (float)+M_PI_2);
    CHECK_FLT(RT_NOCRT(atan2f)(             -1.0f,            0.0f), (float)-M_PI_2);
    CHECK_FLT(RT_NOCRT(atan2f)(             +1.0f,           +1.0f), (float)+M_PI_4);
    CHECK_FLT(RT_NOCRT(atan2f)(             -1.0f,           -1.0f), (float)(-M_PI_2 - M_PI_4));
    CHECK_FLT_SAME(    atan2f,(             +1.0f,            0.0f));
    CHECK_FLT_SAME(    atan2f,(             +1.0f,           -0.0f));
    CHECK_FLT_SAME(    atan2f,(             -1.0f,            0.0f));
    CHECK_FLT_SAME(    atan2f,(             -1.0f,           -0.0f));
    CHECK_FLT_SAME(    atan2f,(             +1.0f,           +1.0f));
    CHECK_FLT_SAME(    atan2f,(             -1.0f,           +1.0f));
    CHECK_FLT_SAME(    atan2f,(             +1.0f,           -1.0f));
    CHECK_FLT_SAME(    atan2f,(             -1.0f,           -1.0f));
    CHECK_FLT_SAME(    atan2f,(      238.6634566f,      -999999.0f));
    CHECK_FLT_SAME(    atan2f,(     -905698045.1f,       490876.0f));
    CHECK_FLT_SAME(    atan2f,(     1.333334e-10f,    -1.9993e+20f));
    CHECK_FLT_SAME(    atan2f,(     1.333334e+35f,    -1.9993e+29f));
    CHECK_FLT_SAME(    atan2f,(          +FLT_MAX,        +FLT_MAX));
    CHECK_FLT_SAME(    atan2f,(          -FLT_MAX,        +FLT_MAX));
    CHECK_FLT_SAME(    atan2f,(         +INFINITY,       +INFINITY));
    CHECK_FLT_SAME(    atan2f,(         -INFINITY,       +INFINITY));
    CHECK_FLT_SAME(    atan2f,(         -INFINITY,      42.242424f));
    CHECK_FLT_SAME(    atan2f,(RTStrNanFloat(NULL, true), RTStrNanFloat(NULL, true)));
    CHECK_FLT_SAME(    atan2f,(RTStrNanFloat(NULL, false), RTStrNanFloat(NULL, false)));
    CHECK_FLT_SAME(    atan2f,(RTStrNanFloat(NULL, false), RTStrNanFloat(NULL, true)));
    //CHECK_FLT_SAME(    atan2f,(RTStrNanFloat(NULL, true), RTStrNanFloat(NULL, false))); - UCRT returns -QNaN, we +QNaN
    CHECK_FLT_SAME(    atan2f,(RTStrNanFloat(NULL, true), RTStrNanFloat("s", false)));
}


void testSin()
{
    RTTestSub(g_hTest, "sin[f]");

    /*
     * Note! sin, cos and friends are complicated the results may differ between
     *       implementations.  The numbers below was computed using amd64 glibc
     *       (2.27-3ubuntu1.4) sinl() and a %.33Lf printf.
     *
     *       Our code is based on the x87 CPU and does not have the best
     *       reduction code is inaccurate, so accuracy drops. Also, with the
     *       input accuracy difference we must expect differences too.
     */
    CHECK_DBL(      RT_NOCRT(sin)(                          +0.0),                           +0.0);
    CHECK_DBL(      RT_NOCRT(sin)(                          -0.0),                           -0.0);
    CHECK_DBL(      RT_NOCRT(sin)(                         +M_PI),                           +0.0);
    CHECK_DBL(      RT_NOCRT(sin)(                         -M_PI),                           +0.0);
    CHECK_DBL(      RT_NOCRT(sin)(                       +M_PI_2),                           +1.0);
    CHECK_DBL(      RT_NOCRT(sin)(                       -M_PI_2),                           -1.0);
    CHECK_DBL(      RT_NOCRT(sin)(              +M_PI_2 + M_PI*4),                           +1.0);
    CHECK_DBL(      RT_NOCRT(sin)(              -M_PI_2 - M_PI*4),                           -1.0);

    CHECK_DBL(      RT_NOCRT(sin)(              +M_PI_2 + M_PI*2),                           +1.0);
    CHECK_DBL(      RT_NOCRT(sin)(              -M_PI_2 - M_PI*2),                           -1.0);
    CHECK_DBL(      RT_NOCRT(sin)(                          +1.0),        +0.84147098480789650488);
    CHECK_DBL(      RT_NOCRT(sin)(                          +2.0),        +0.90929742682568170942);
    CHECK_DBL(      RT_NOCRT(sin)(                          +3.0),        +0.14112000805986721352);
    CHECK_DBL(      RT_NOCRT(sin)(                          +4.0),        -0.75680249530792820245);
    CHECK_DBL(      RT_NOCRT(sin)(                          +5.0),        -0.95892427466313845397);
    CHECK_DBL(      RT_NOCRT(sin)(                          +6.0),        -0.27941549819892586015);
    CHECK_DBL(      RT_NOCRT(sin)(                          +7.0),        +0.65698659871878906102);
    CHECK_DBL(      RT_NOCRT(sin)(                          +8.0),        +0.98935824662338178737);
    CHECK_DBL(      RT_NOCRT(sin)(                          +9.0),        +0.41211848524175659358);
    CHECK_DBL(      RT_NOCRT(sin)(                         +10.0),        -0.54402111088936977445);
    CHECK_DBL(      RT_NOCRT(sin)(                        +100.0),        -0.50636564110975879061);
    CHECK_DBL(      RT_NOCRT(sin)(                +654.216812456),        +0.69292681127157818022);
    CHECK_DBL(      RT_NOCRT(sin)(     10.1010101010101010101010),        -0.62585878258501614901);
    CHECK_DBL(      RT_NOCRT(sin)(    +25.2525252525252525252525),        +0.11949778146891366915);
    CHECK_DBL(      RT_NOCRT(sin)(   +252.2525252525252525252525),        +0.79868874455343841223);
    CHECK_DBL(      RT_NOCRT(sin)(  +2525.2525252525252525252525),        -0.55467159842968405403);
    CHECK_DBL_RANGE(RT_NOCRT(sin)( +25252.2525252525252525252525),        +0.13040325588994761130, 0.0000000000000010000);
    CHECK_DBL_RANGE(RT_NOCRT(sin)(+252525.2525252525252525252525),        -0.77923047482990159818, 0.0000000000000100000);

    CHECK_DBL(      RT_NOCRT(sin)(                          -1.0),        -0.84147098480789650488);
    CHECK_DBL(      RT_NOCRT(sin)(                          -2.0),        -0.90929742682568170942);
    CHECK_DBL(      RT_NOCRT(sin)(                          -3.0),        -0.14112000805986721352);
    CHECK_DBL(      RT_NOCRT(sin)(                          -4.0),        +0.75680249530792820245);
    CHECK_DBL(      RT_NOCRT(sin)(                          -5.0),        +0.95892427466313845397);
    CHECK_DBL(      RT_NOCRT(sin)(                          -6.0),        +0.27941549819892586015);
    CHECK_DBL(      RT_NOCRT(sin)(                          -7.0),        -0.65698659871878906102);
    CHECK_DBL(      RT_NOCRT(sin)(                          -8.0),        -0.98935824662338178737);
    CHECK_DBL(      RT_NOCRT(sin)(                          -9.0),        -0.41211848524175659358);
    CHECK_DBL(      RT_NOCRT(sin)(                         -10.0),        +0.54402111088936977445);
    CHECK_DBL(      RT_NOCRT(sin)(                        -100.0),        +0.50636564110975879061);
    CHECK_DBL(      RT_NOCRT(sin)(                -654.216812456),        -0.69292681127157818022);
    CHECK_DBL(      RT_NOCRT(sin)(    -10.1010101010101010101010),        +0.62585878258501614901);
    CHECK_DBL(      RT_NOCRT(sin)(    -25.2525252525252525252525),        -0.11949778146891366915);
    CHECK_DBL(      RT_NOCRT(sin)(   -252.2525252525252525252525),        -0.79868874455343841223);
    CHECK_DBL(      RT_NOCRT(sin)(  -2525.2525252525252525252525),        +0.55467159842968405403);
    CHECK_DBL_RANGE(RT_NOCRT(sin)( -25252.2525252525252525252525),        -0.13040325588994761130, 0.0000000000000010000);
    CHECK_DBL_RANGE(RT_NOCRT(sin)(-252525.2525252525252525252525),        +0.77923047482990159818, 0.0000000000000100000);
    CHECK_DBL(      RT_NOCRT(sin)(     RTStrNanDouble("s", true)),       RTStrNanDouble("s", true));
    CHECK_DBL(      RT_NOCRT(sin)(RTStrNanDouble("9999s", false)),  RTStrNanDouble("9999s", false));

    CHECK_DBL_SAME(    sin,(              1.0));
    CHECK_DBL_SAME(    sin,(              1.5));
    CHECK_DBL_SAME(    sin,(             +0.0));
    CHECK_DBL_SAME(    sin,(             +0.0));
    CHECK_DBL_SAME(    sin,(             -0.0));
    CHECK_DBL_SAME(    sin,(             -0.0));
    CHECK_DBL_SAME(    sin,(            -10.0));
#if 0 /* UCRT returns tiny fractions for these in the 2**-53 range, we return 0.0 */
    CHECK_DBL_SAME(    sin,(            +M_PI));
    CHECK_DBL_SAME(    sin,(            -M_PI));
#endif
    CHECK_DBL_SAME(    sin,(          +M_PI_2));
    CHECK_DBL_SAME(    sin,(          -M_PI_2));
    CHECK_DBL_SAME(    sin,(        +INFINITY));
    CHECK_DBL_SAME(    sin,(        -INFINITY));
    CHECK_DBL_SAME(    sin,(RTStrNanDouble(NULL, true)));
#if 0 /*UCRT converts these to quiet ones, we check above */
    //CHECK_DBL_SAME(    sin,(RTStrNanDouble("s",  true)));
    //CHECK_DBL_SAME(    sin,(RTStrNanDouble("s", false)));
#endif



}


void testCos()
{
    RTTestSub(g_hTest, "cos[f]");

    CHECK_DBL(RT_NOCRT(cos)(             +0.0),     1.0);
    CHECK_DBL(          cos(             +0.0),     1.0);
    CHECK_DBL(RT_NOCRT(cos)(            +M_PI),    -1.0);
    CHECK_DBL(          cos(            +M_PI),    -1.0);
    CHECK_DBL(RT_NOCRT(cos)(            -M_PI),    -1.0);
    CHECK_DBL(          cos(            -M_PI),    -1.0);
    CHECK_DBL(RT_NOCRT(cos)(          +M_PI_2),     0.0);
    CHECK_DBL(          cos(          +M_PI_2),     0.0);
    CHECK_DBL(RT_NOCRT(cos)(          -M_PI_2),     0.0);
    CHECK_DBL(          cos(          -M_PI_2),     0.0);
    CHECK_DBL(RT_NOCRT(cos)(             +1.0), +M_PI_4);
    CHECK_DBL(          cos(             +1.0), +M_PI_4);
    CHECK_DBL(RT_NOCRT(cos)(             -1.0), -M_PI_4);
    CHECK_DBL(          cos(             -1.0), -M_PI_4);
    CHECK_DBL_SAME(    cos,(              1.0));
    CHECK_DBL_SAME(    cos,(              1.5));
    CHECK_DBL_SAME(    cos,(             +0.0));
    CHECK_DBL_SAME(    cos,(             +0.0));
    CHECK_DBL_SAME(    cos,(             -0.0));
    CHECK_DBL_SAME(    cos,(             -0.0));
    CHECK_DBL_SAME(    cos,(            +M_PI));
    CHECK_DBL_SAME(    cos,(            -M_PI));
    CHECK_DBL_SAME(    cos,(          +M_PI_2));
    CHECK_DBL_SAME(    cos,(          -M_PI_2));
#if 0
    CHECK_DBL_SAME(    cos,(      238.6634566));
    CHECK_DBL_SAME(    cos,(      -49.4578999));
    CHECK_DBL_SAME(    cos,(         999999.0));
    CHECK_DBL_SAME(    cos,(        -999999.0));
    CHECK_DBL_SAME(    cos,(        -999999.0));
    CHECK_DBL_SAME(    cos,(         999999.0));
    CHECK_DBL_SAME(    cos,(      39560.32334));
    CHECK_DBL_SAME(    cos,(      39560.32334));
    CHECK_DBL_SAME(    cos,(        +INFINITY));
    CHECK_DBL_SAME(    cos,(        -INFINITY));
    CHECK_DBL_SAME(    cos,(         +DBL_MAX));
    CHECK_DBL_SAME(    cos,(         -DBL_MAX));
    CHECK_DBL_SAME(    cos,(2.34960584706e100));
    CHECK_DBL_SAME(    cos,(2.34960584706e300));
    CHECK_DBL_SAME(    cos,(2.34960584706e300));
    CHECK_DBL_SAME(    cos,(RTStrNanDouble(NULL, true)));
    CHECK_DBL_SAME(    cos,(RTStrNanDouble("s",  true)));
    CHECK_DBL_SAME(    cos,(RTStrNanDouble("s", false)));
#endif
}



int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTNoCrt-2", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    /* Some preconditions: */
    RTFLOAT32U r32;
    r32.r = RTStrNanFloat("s", false);
    RTTEST_CHECK(g_hTest, RTFLOAT32U_IS_SIGNALLING_NAN(&r32));
    r32.r = RTStrNanFloat("q", false);
    RTTEST_CHECK(g_hTest, RTFLOAT32U_IS_QUIET_NAN(&r32));
    r32.r = RTStrNanFloat(NULL, false);
    RTTEST_CHECK(g_hTest, RTFLOAT32U_IS_QUIET_NAN(&r32));

    RTFLOAT64U r64;
    r64.r = RTStrNanDouble("s", false);
    RTTEST_CHECK(g_hTest, RTFLOAT64U_IS_SIGNALLING_NAN(&r64));
    r64.r = RTStrNanDouble("q", false);
    RTTEST_CHECK(g_hTest, RTFLOAT64U_IS_QUIET_NAN(&r64));
    r64.r = RTStrNanDouble(NULL, false);
    RTTEST_CHECK(g_hTest, RTFLOAT64U_IS_QUIET_NAN(&r64));

    /* stdlib.h (integer) */
    testAbs();

    /* math.h */
    testFAbs();
    testCopySign();
    testFmax();
    testFmin();
    testIsInf();
    testIsNan();
    testIsFinite();
    testIsNormal();
    testFpClassify();
    testSignBit();
    testCeil();
    testFloor();
    testTrunc();
    testRound();
    testRInt();
    testLRound();
    testLLRound();
    testLRInt();
    testLLRInt();

    testExp2();
    testLdExp();
    testFma();
    testRemainder();
    testLog();
    testSqRt();

    testATan();
    testATan2();
    testSin();
    //testCos();

#if 0
    ../common/math/cos.asm \
    ../common/math/cosf.asm \
    ../common/math/cosl.asm \
    ../common/math/sin.asm \
    ../common/math/sinf.asm \
    ../common/math/tan.asm \
    ../common/math/tanf.asm \

#endif

    return RTTestSummaryAndDestroy(g_hTest);
}

