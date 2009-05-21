/** @file
 * IPRT - Testcase Framework.
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

#ifndef ___iprt_test_h
#define ___iprt_test_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>

__BEGIN_DECLS

/** @defgroup grp_rt_test       RTTest - Testcase Framework.
 * @ingroup grp_rt
 * @{
 */

/** A test handle. */
typedef struct RTTESTINT *RTTEST;
/** A pointer to a test handle. */
typedef RTTEST *PRTTEST;
/** A const pointer to a test handle. */
typedef RTTEST const *PCRTTEST;

/** A NIL Test handle. */
#define NIL_RTTEST  ((RTTEST)0)

/**
 * Test message importance level.
 */
typedef enum RTTESTLVL
{
    /** Invalid 0. */
    RTTESTLVL_INVALID = 0,
    /** Message should always be printed. */
    RTTESTLVL_ALWAYS,
    /** Failure message. */
    RTTESTLVL_FAILURE,
    /** Sub-test banner. */
    RTTESTLVL_SUB_TEST,
    /** Info message. */
    RTTESTLVL_INFO,
    /** Debug message. */
    RTTESTLVL_DEBUG,
    /** The last (invalid). */
    RTTESTLVL_END
} RTTESTLVL;


/**
 * Creates a test instance.
 *
 * @returns IPRT status code.
 * @param   pszTest     The test name.
 * @param   phTest      Where to store the test instance handle.
 */
RTR3DECL(int) RTTestCreate(const char *pszTest, PRTTEST phTest);

/**
 * Destroys a test instance previously created by RTTestCreate.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. NIL_RTTEST is ignored.
 */
RTR3DECL(int) RTTestDestroy(RTTEST hTest);

/**
 * Allocate a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 * @param   cbAlign     The alignment of the returned block.
 * @param   fHead       Head or tail optimized guard.
 * @param   ppvUser     Where to return the pointer to the block.
 */
RTR3DECL(int) RTTestGuardedAlloc(RTTEST hTest, size_t cb, uint32_t cbAlign, bool fHead, void **ppvUser);

/**
 * Allocates a block of guarded memory where the guarded is immediately after
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocTail(RTTEST hTest, size_t cb);

/**
 * Allocates a block of guarded memory where the guarded is right in front of
 * the user memory.
 *
 * @returns Pointer to the allocated memory. NULL on failure.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   cb          The amount of memory to allocate.
 */
RTR3DECL(void *) RTTestGuardedAllocHead(RTTEST hTest, size_t cb);

/**
 * Frees a block of guarded memory.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pv          The memory. NULL is ignored.
 */
RTR3DECL(int) RTTestGuardedFree(RTTEST hTest, void *pv);

/**
 * Test vprintf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfNlV(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, va_list va);

/**
 * Test printf making sure the output starts on a new line.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintfNl(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, ...);

/**
 * Test vprintf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestPrintfV(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, va_list va);

/**
 * Test printf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestPrintf(RTTEST hTest, RTTESTLVL enmLevel, const char *pszFormat, ...);

/**
 * Prints the test banner.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestBanner(RTTEST hTest);

/**
 * Summaries the test, destroys the test instance and return an exit code.
 *
 * @returns Test program exit code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestSummaryAndDestroy(RTTEST hTest);

/**
 * Starts a sub-test.
 *
 * This will perform an implicit RTTestSubDone() call if that has not been done
 * since the last RTTestSub call.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszSubTest  The sub-test name
 */
RTR3DECL(int) RTTestSub(RTTEST hTest, const char *pszSubTest);

/**
 * Completes a sub-test.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestSubDone(RTTEST hTest);

/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestPassedV(RTTEST hTest, const char *pszFormat, va_list va);

/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestPassed(RTTEST hTest, const char *pszFormat, ...);


/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestErrorInc(RTTEST hTest);

/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestFailedV(RTTEST hTest, const char *pszFormat, va_list va);

/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestFailed(RTTEST hTest, const char *pszFormat, ...);

/**
 * Same as RTTestPrintfV with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestFailureDetailsV(RTTEST hTest, const char *pszFormat, va_list va);

/**
 * Same as RTTestPrintf with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestFailureDetails(RTTEST hTest, const char *pszFormat, ...);


/** @def RTTEST_CHECK
 * Check whether a boolean expression holds true.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest       The test handle.
 * @param   expr        The expression to evaluate.
 */
#define RTTEST_CHECK(hTest, expr) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
         } \
    } while (0)

/** @def RTTEST_CHECK_MSG
 * Check whether a boolean expression holds true.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest           The test handle.
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestFailureDetails, including
 *                          parenthesis.
 */
#define RTTEST_CHECK_MSG(hTest, expr, DetailsArgs) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
            RTTestFailureDetails DetailsArgs; \
         } \
    } while (0)

/** @def RTTEST_CHECK_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest       The test handle.
 * @param   expr        The expression to evaluate.
 * @param   rcRet       What to return on failure.
 */
#define RTTEST_CHECK_RET(hTest, expr, rcRet) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
            return (rcRet); \
         } \
    } while (0)

/** @def RTTEST_CHECK_MSG_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest           The test handle.
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestFailureDetails, including
 *                          parenthesis.
 * @param   rcRet           What to return on failure.
 */
#define RTTEST_CHECK_MSG_RET(hTest, expr, DetailsArgs, rcRet) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
            RTTestFailureDetails DetailsArgs; \
            return (rcRet); \
         } \
    } while (0)

/** @def RTTEST_CHECK_RETV
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest       The test handle.
 * @param   expr        The expression to evaluate.
 */
#define RTTEST_CHECK_RETV(hTest, expr) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
            return; \
         } \
    } while (0)

/** @def RTTEST_CHECK_MSG_RET
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and expression.
 *
 * @param   hTest           The test handle.
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestFailureDetails, including
 *                          parenthesis.
 */
#define RTTEST_CHECK_MSG_RETV(hTest, expr, DetailsArgs) \
    do { if (!(expr)) { \
            RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
            RTTestFailureDetails DetailsArgs; \
            return; \
         } \
    } while (0)

/** @def RTTEST_CHECK_RC
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestFailed giving the line
 * number, expression, actual and expected status codes.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTEST_CHECK_RC(rcExpr, rcExpect) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestFailed((hTest), "line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
        } \
    } while (0)

/** @def RTTEST_CHECK_RC_OK
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestFailed giving the line number,
 * expression and status code.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting an IPRT status code.
 */
#define RTTEST_CHECK_RC_OK(hTest, rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestFailed((hTest), "line %u: %s: %Rrc", __LINE__, #rcExpr, rc); \
        } \
    } while (0)




/** @name Implicit Test Handle API Variation
 * The test handle is retrieved from the test TLS entry of the calling thread.
 * @{
 */

/**
 * Test vprintf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestIPrintfV(RTTESTLVL enmLevel, const char *pszFormat, va_list va);

/**
 * Test printf, makes sure lines are prefixed and so forth.
 *
 * @returns Number of chars printed.
 * @param   enmLevel    Message importance level.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestIPrintf(RTTESTLVL enmLevel, const char *pszFormat, ...);

/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestIPassedV(const char *pszFormat, va_list va);

/**
 * Prints an extended PASSED message, optional.
 *
 * This does not conclude the sub-test, it could be used to report the passing
 * of a sub-sub-to-the-power-of-N-test.
 *
 * @returns IPRT status code.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestIPassed(const char *pszFormat, ...);

/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 */
RTR3DECL(int) RTTestIErrorInc(void);

/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestIFailedV(const char *pszFormat, va_list va);

/**
 * Increments the error counter and prints a failure message.
 *
 * @returns IPRT status code.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestIFailed(const char *pszFormat, ...);

/**
 * Same as RTTestIPrintfV with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   pszFormat   The message.
 * @param   va          Arguments.
 */
RTR3DECL(int) RTTestIFailureDetailsV(const char *pszFormat, va_list va);

/**
 * Same as RTTestIPrintf with RTTESTLVL_FAILURE.
 *
 * @returns Number of chars printed.
 * @param   pszFormat   The message.
 * @param   ...         Arguments.
 */
RTR3DECL(int) RTTestIFailureDetails(const char *pszFormat, ...);


/** @def RTTESTI_CHECK
 * Check whether a boolean expression holds true.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr        The expression to evaluate.
 */
#define RTTESTI_CHECK(expr) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
         } \
    } while (0)

/** @def RTTESTI_CHECK_MSG
 * Check whether a boolean expression holds true.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestIFailureDetails, including
 *                          parenthesis.
 */
#define RTTESTI_CHECK_MSG(expr, DetailsArgs) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            RTTestIFailureDetails DetailsArgs; \
         } \
    } while (0)

/** @def RTTESTI_CHECK_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr        The expression to evaluate.
 * @param   rcRet       What to return on failure.
 */
#define RTTESTI_CHECK_RET(expr, rcRet) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            return (rcRet); \
         } \
    } while (0)

/** @def RTTESTI_CHECK_MSG_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestIFailureDetails, including
 *                          parenthesis.
 * @param   rcRet           What to return on failure.
 */
#define RTTESTI_CHECK_MSG_RET(expr, DetailsArgs, rcRet) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            RTTestIFailureDetails DetailsArgs; \
            return (rcRet); \
         } \
    } while (0)

/** @def RTTESTI_CHECK_RETV
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr        The expression to evaluate.
 */
#define RTTESTI_CHECK_RETV(expr) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            return; \
         } \
    } while (0)

/** @def RTTESTI_CHECK_MSG_RET
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression.
 *
 * @param   expr            The expression to evaluate.
 * @param   DetailsArgs     Argument list for RTTestIFailureDetails, including
 *                          parenthesis.
 */
#define RTTESTI_CHECK_MSG_RETV(expr, DetailsArgs) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            RTTestIFailureDetails DetailsArgs; \
            return; \
         } \
    } while (0)

/** @def RTTESTI_CHECK_RC
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestIFailed giving the line
 * number, expression, actual and expected status codes.
 *
 * @param   rcExpr          The expression resulting an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTESTI_CHECK_RC(rcExpr, rcExpect) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestIFailed("line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
        } \
    } while (0)

/** @def RTTESTI_CHECK_RC_OK
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestIFailed giving the line number,
 * expression and status code.
 *
 * @param   rcExpr          The expression resulting an IPRT status code.
 */
#define RTTESTI_CHECK_RC_OK(rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestIFailed("line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
        } \
    } while (0)


/** @} */


/** @}  */

__END_DECLS

#endif

