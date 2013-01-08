/** @file
 * IPRT - Testcase Framework.
 */

/*
 * Copyright (C) 2009 Oracle Corporation
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

#ifndef ___iprt_test_h
#define ___iprt_test_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>
#include <iprt/assert.h>

RT_C_DECLS_BEGIN

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
 * Initializes IPRT and creates a test instance.
 *
 * Typical usage is:
 * @code
    int main(int argc, char **argv)
    {
        RTTEST hTest;
        int rc = RTTestInitAndCreate("tstSomething", &hTest);
        if (rc)
            return rc;
        ...
    }
   @endcode
 *
 * @returns RTEXITCODE_SUCCESS on success.  On failure an error message is
 *          printed and a suitable exit code is return.
 *
 * @param   pszTest     The test name.
 * @param   phTest      Where to store the test instance handle.
 */
RTR3DECL(RTEXITCODE) RTTestInitAndCreate(const char *pszTest, PRTTEST phTest);

/**
 * Destroys a test instance previously created by RTTestCreate.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. NIL_RTTEST is ignored.
 */
RTR3DECL(int) RTTestDestroy(RTTEST hTest);

/**
 * Changes the default test instance for the calling thread.
 *
 * @returns IPRT status code.
 *
 * @param   hNewDefaultTest The new default test. NIL_RTTEST is fine.
 * @param   phOldTest       Where to store the old test handle. Optional.
 */
RTR3DECL(int) RTTestSetDefault(RTTEST hNewDefaultTest, PRTTEST phOldTest);

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
RTR3DECL(RTEXITCODE) RTTestSummaryAndDestroy(RTTEST hTest);

/**
 * Skips the test, destroys the test instance and return an exit code.
 *
 * @returns Test program exit code.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszReasonFmt    Text explaining why, optional (NULL).
 * @param   va              Arguments for the reason format string.
 */
RTR3DECL(RTEXITCODE) RTTestSkipAndDestroyV(RTTEST hTest, const char *pszReasonFmt, va_list va);

/**
 * Skips the test, destroys the test instance and return an exit code.
 *
 * @returns Test program exit code.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszReasonFmt    Text explaining why, optional (NULL).
 * @param   ...             Arguments for the reason format string.
 */
RTR3DECL(RTEXITCODE) RTTestSkipAndDestroy(RTTEST hTest, const char *pszReasonFmt, ...);

/**
 * Starts a sub-test.
 *
 * This will perform an implicit RTTestSubDone() call if that has not been done
 * since the last RTTestSub call.
 *
 * @returns Number of chars printed.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszSubTest  The sub-test name.
 */
RTR3DECL(int) RTTestSub(RTTEST hTest, const char *pszSubTest);

/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   ...             Arguments.
 */
RTR3DECL(int) RTTestSubF(RTTEST hTest, const char *pszSubTestFmt, ...);

/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   hTest           The test handle. If NIL_RTTEST we'll use the one
 *                          associated with the calling thread.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   va              Arguments.
 */
RTR3DECL(int) RTTestSubV(RTTEST hTest, const char *pszSubTestFmt, va_list va);

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
 * Value units.
 *
 * @remarks This is an interface where we have to be binary compatible with both
 *          older versions of this header and other components using the same
 *          contant values.
 * @remarks When adding a new item:
 *              - Always add at the end of the list - do NOT group it.
 *              - Add it to rtTestUnitName in r3/test.cpp.
 *              - include/VBox/VMMDevTesting.h (VMMDEV_TESTING_UNIT_XXX).
 *              - Add it to g_aszBs2TestUnitNames in
 *                TestSuite/bootsectors/bootsector2-common-routines.mac.
 *
 */
typedef enum RTTESTUNIT
{
    /** The customary invalid zero value. */
    RTTESTUNIT_INVALID = 0,

    RTTESTUNIT_PCT,                             /**< Percentage (10^-2). */
    RTTESTUNIT_BYTES,                           /**< Bytes. */
    RTTESTUNIT_BYTES_PER_SEC,                   /**< Bytes per second. */
    RTTESTUNIT_KILOBYTES,                       /**< Kilobytes. */
    RTTESTUNIT_KILOBYTES_PER_SEC,               /**< Kilobytes per second. */
    RTTESTUNIT_MEGABYTES,                       /**< Megabytes. */
    RTTESTUNIT_MEGABYTES_PER_SEC,               /**< Megabytes per second. */
    RTTESTUNIT_PACKETS,                         /**< Packets. */
    RTTESTUNIT_PACKETS_PER_SEC,                 /**< Packets per second. */
    RTTESTUNIT_FRAMES,                          /**< Frames. */
    RTTESTUNIT_FRAMES_PER_SEC,                  /**< Frames per second. */
    RTTESTUNIT_OCCURRENCES,                     /**< Occurrences. */
    RTTESTUNIT_OCCURRENCES_PER_SEC,             /**< Occurrences per second. */
    RTTESTUNIT_CALLS,                           /**< Calls. */
    RTTESTUNIT_CALLS_PER_SEC,                   /**< Calls per second. */
    RTTESTUNIT_ROUND_TRIP,                      /**< Round trips. */
    RTTESTUNIT_SECS,                            /**< Seconds. */
    RTTESTUNIT_MS,                              /**< Milliseconds. */
    RTTESTUNIT_NS,                              /**< Nanoseconds. */
    RTTESTUNIT_NS_PER_CALL,                     /**< Nanoseconds per call. */
    RTTESTUNIT_NS_PER_FRAME,                    /**< Nanoseconds per frame. */
    RTTESTUNIT_NS_PER_OCCURRENCE,               /**< Nanoseconds per occurrence. */
    RTTESTUNIT_NS_PER_PACKET,                   /**< Nanoseconds per frame. */
    RTTESTUNIT_NS_PER_ROUND_TRIP,               /**< Nanoseconds per round trip. */
    RTTESTUNIT_INSTRS,                          /**< Instructions. */
    RTTESTUNIT_INSTRS_PER_SEC,                  /**< Instructions per second. */
    RTTESTUNIT_NONE,                            /**< No unit. */
    RTTESTUNIT_PP1K,                            /**< Parts per thousand (10^-3). */
    RTTESTUNIT_PP10K,                           /**< Parts per ten thousand (10^-4). */
    RTTESTUNIT_PPM,                             /**< Parts per million (10^-6). */
    RTTESTUNIT_PPB,                             /**< Parts per billion (10^-9). */

    /** The end of valid units. */
    RTTESTUNIT_END
} RTTESTUNIT;
AssertCompile(RTTESTUNIT_INSTRS == 0x19);
AssertCompile(RTTESTUNIT_NONE   == 0x1b);

/**
 * Report a named test result value.
 *
 * This is typically used for benchmarking but can be used for other purposes
 * like reporting limits of some implementation.  The value gets associated with
 * the current sub test, the name must be unique within the sub test.
 *
 * @returns IPRT status code.
 *
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   pszName     The value name.
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 */
RTR3DECL(int) RTTestValue(RTTEST hTest, const char *pszName, uint64_t u64Value, RTTESTUNIT enmUnit);

/**
 * Same as RTTestValue, except that the name is now a format string.
 *
 * @returns IPRT status code.
 *
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 * @param   pszNameFmt  The value name format string.
 * @param   ...         String arguments.
 */
RTR3DECL(int) RTTestValueF(RTTEST hTest, uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, ...);

/**
 * Same as RTTestValue, except that the name is now a format string.
 *
 * @returns IPRT status code.
 *
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 * @param   pszNameFmt  The value name format string.
 * @param   va_list     String arguments.
 */
RTR3DECL(int) RTTestValueV(RTTEST hTest, uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, va_list va);

/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(int) RTTestErrorInc(RTTEST hTest);

/**
 * Get the current error count.
 *
 * @returns The error counter, UINT32_MAX if no valid test handle.
 * @param   hTest       The test handle. If NIL_RTTEST we'll use the one
 *                      associated with the calling thread.
 */
RTR3DECL(uint32_t) RTTestErrorCount(RTTEST hTest);

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
/** @def RTTEST_CHECK_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and
 * expression, then return @a rcRet.
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
/** @def RTTEST_CHECK_RETV
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestFailed giving the line number and
 * expression, then return void.
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
/** @def RTTEST_CHECK_BREAK
 * Check whether a boolean expression holds true.
 *
 * If the expression is false, call RTTestFailed giving the line number and
 * expression, then break.
 *
 * @param   hTest       The test handle.
 * @param   expr        The expression to evaluate.
 */
#define RTTEST_CHECK_BREAK(hTest, expr) \
    if (!(expr)) { \
        RTTestFailed((hTest), "line %u: %s", __LINE__, #expr); \
        break; \
    } else do {} while (0)


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
 * @param   rcExpr          The expression resulting in an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTEST_CHECK_RC(hTest, rcExpr, rcExpect) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestFailed((hTest), "line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
        } \
    } while (0)
/** @def RTTEST_CHECK_RC_RET
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestFailed giving the line
 * number, expression, actual and expected status codes, then return.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 *                          This will be assigned to a local rcCheck variable
 *                          that can be used as return value.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 * @param   rcRet           The return code.
 */
#define RTTEST_CHECK_RC_RET(hTest, rcExpr, rcExpect, rcRet) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestFailed((hTest), "line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            return (rcRet); \
        } \
    } while (0)
/** @def RTTEST_CHECK_RC_RETV
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestFailed giving the line
 * number, expression, actual and expected status codes, then return.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTEST_CHECK_RC_RETV(hTest, rcExpr, rcExpect) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestFailed((hTest), "line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            return; \
        } \
    } while (0)
/** @def RTTEST_CHECK_RC_BREAK
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestFailed giving the line
 * number, expression, actual and expected status codes, then break.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTEST_CHECK_RC_BREAK(hTest, rcExpr, rcExpect) \
    if (1) { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestFailed((hTest), "line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            break; \
        } \
    } else do {} while (0)


/** @def RTTEST_CHECK_RC_OK
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestFailed giving the line number,
 * expression and status code.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 */
#define RTTEST_CHECK_RC_OK(hTest, rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestFailed((hTest), "line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
        } \
    } while (0)
/** @def RTTEST_CHECK_RC_OK_RET
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestFailed giving the line number,
 * expression and status code, then return with the specified value.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 *                          This will be assigned to a local rcCheck variable
 *                          that can be used as return value.
 * @param   rcRet           The return code.
 */
#define RTTEST_CHECK_RC_OK_RET(hTest, rcExpr, rcRet) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestFailed((hTest), "line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
            return (rcRet); \
        } \
    } while (0)
/** @def RTTEST_CHECK_RC_OK_RETV
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestFailed giving the line number,
 * expression and status code, then return.
 *
 * @param   hTest           The test handle.
 * @param   rcExpr          The expression resulting in an IPRT status code.
 */
#define RTTEST_CHECK_RC_OK_RETV(hTest, rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestFailed((hTest), "line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
            return; \
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
 * Starts a sub-test.
 *
 * This will perform an implicit RTTestSubDone() call if that has not been done
 * since the last RTTestSub call.
 *
 * @returns Number of chars printed.
 * @param   pszSubTest  The sub-test name.
 */
RTR3DECL(int) RTTestISub(const char *pszSubTest);

/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   ...             Arguments.
 */
RTR3DECL(int) RTTestISubF(const char *pszSubTestFmt, ...);

/**
 * Format string version of RTTestSub.
 *
 * See RTTestSub for details.
 *
 * @returns Number of chars printed.
 * @param   pszSubTestFmt   The sub-test name format string.
 * @param   va              Arguments.
 */
RTR3DECL(int) RTTestISubV(const char *pszSubTestFmt, va_list va);

/**
 * Completes a sub-test.
 *
 * @returns Number of chars printed.
 */
RTR3DECL(int) RTTestISubDone(void);

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
 * Report a named test result value.
 *
 * This is typically used for benchmarking but can be used for other purposes
 * like reporting limits of some implementation.  The value gets associated with
 * the current sub test, the name must be unique within the sub test.
 *
 * @returns IPRT status code.
 *
 * @param   pszName     The value name.
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 */
RTR3DECL(int) RTTestIValue(const char *pszName, uint64_t u64Value, RTTESTUNIT enmUnit);

/**
 * Same as RTTestValue, except that the name is now a format string.
 *
 * @returns IPRT status code.
 *
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 * @param   pszNameFmt  The value name format string.
 * @param   ...         String arguments.
 */
RTR3DECL(int) RTTestIValueF(uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, ...);

/**
 * Same as RTTestValue, except that the name is now a format string.
 *
 * @returns IPRT status code.
 *
 * @param   u64Value    The value.
 * @param   enmUnit     The value unit.
 * @param   pszNameFmt  The value name format string.
 * @param   va_list     String arguments.
 */
RTR3DECL(int) RTTestIValueV(uint64_t u64Value, RTTESTUNIT enmUnit, const char *pszNameFmt, va_list va);

/**
 * Increments the error counter.
 *
 * @returns IPRT status code.
 */
RTR3DECL(int) RTTestIErrorInc(void);

/**
 * Get the current error count.
 *
 * @returns The error counter, UINT32_MAX if no valid test handle.
 */
RTR3DECL(uint32_t) RTTestIErrorCount(void);

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
 * Increments the error counter, prints a failure message and returns the
 * specified status code.
 *
 * This is mainly a convenience method for saving vertical space in the source
 * code.
 *
 * @returns @a rcRet
 * @param   rcRet       The IPRT status code to return.
 * @param   pszFormat   The message. No trailing newline.
 * @param   va          The arguments.
 */
RTR3DECL(int) RTTestIFailedRcV(int rcRet, const char *pszFormat, va_list va);

/**
 * Increments the error counter, prints a failure message and returns the
 * specified status code.
 *
 * This is mainly a convenience method for saving vertical space in the source
 * code.
 *
 * @returns @a rcRet
 * @param   rcRet       The IPRT status code to return.
 * @param   pszFormat   The message. No trailing newline.
 * @param   ...         The arguments.
 */
RTR3DECL(int) RTTestIFailedRc(int rcRet, const char *pszFormat, ...);

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
/** @def RTTESTI_CHECK_RET
 * Check whether a boolean expression holds true, returns on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression, then return @a rcRet.
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
/** @def RTTESTI_CHECK_RETV
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression, then return void.
 *
 * @param   expr        The expression to evaluate.
 */
#define RTTESTI_CHECK_RETV(expr) \
    do { if (!(expr)) { \
            RTTestIFailed("line %u: %s", __LINE__, #expr); \
            return; \
         } \
    } while (0)
/** @def RTTESTI_CHECK_RETV
 * Check whether a boolean expression holds true, returns void on false.
 *
 * If the expression is false, call RTTestIFailed giving the line number and
 * expression, then break.
 *
 * @param   expr        The expression to evaluate.
 */
#define RTTESTI_CHECK_BREAK(expr) \
    if (!(expr)) { \
        RTTestIFailed("line %u: %s", __LINE__, #expr); \
        break; \
    } do {} while (0)


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
 * @param   rcExpr          The expression resulting in an IPRT status code.
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
/** @def RTTESTI_CHECK_RC_RET
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestIFailed giving the line
 * number, expression, actual and expected status codes, then return.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 *                          This will be assigned to a local rcCheck variable
 *                          that can be used as return value.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 * @param   rcRet           The return code.
 */
#define RTTESTI_CHECK_RC_RET(rcExpr, rcExpect, rcRet) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestIFailed("line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            return (rcRet); \
        } \
    } while (0)
/** @def RTTESTI_CHECK_RC_RETV
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestIFailed giving the line
 * number, expression, actual and expected status codes, then return.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTESTI_CHECK_RC_RETV(rcExpr, rcExpect) \
    do { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestIFailed("line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            return; \
        } \
    } while (0)
/** @def RTTESTI_CHECK_RC_BREAK
 * Check whether an expression returns a specific IPRT style status code.
 *
 * If a different status code is return, call RTTestIFailed giving the line
 * number, expression, actual and expected status codes, then break.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 * @param   rcExpect        The expected return code. This may be referenced
 *                          more than once by the macro.
 */
#define RTTESTI_CHECK_RC_BREAK(rcExpr, rcExpect) \
    if (1) { \
        int rcCheck = (rcExpr); \
        if (rcCheck != (rcExpect)) { \
            RTTestIFailed("line %u: %s: expected %Rrc, got %Rrc", __LINE__, #rcExpr, (rcExpect), rcCheck); \
            break; \
        } \
    } else do {} while (0)


/** @def RTTESTI_CHECK_RC_OK
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestIFailed giving the line number,
 * expression and status code.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 */
#define RTTESTI_CHECK_RC_OK(rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestIFailed("line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
        } \
    } while (0)
/** @def RTTESTI_CHECK_RC_OK_RET
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestIFailed giving the line number,
 * expression and status code, then return with the specified value.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 *                          This will be assigned to a local rcCheck variable
 *                          that can be used as return value.
 * @param   rcRet           The return code.
 */
#define RTTESTI_CHECK_RC_OK_RET(rcExpr, rcRet) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestIFailed("line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
            return (rcRet); \
        } \
    } while (0)
/** @def RTTESTI_CHECK_RC_OK_RETV
 * Check whether a IPRT style status code indicates success.
 *
 * If the status indicates failure, call RTTestIFailed giving the line number,
 * expression and status code, then return.
 *
 * @param   rcExpr          The expression resulting in an IPRT status code.
 */
#define RTTESTI_CHECK_RC_OK_RETV(rcExpr) \
    do { \
        int rcCheck = (rcExpr); \
        if (RT_FAILURE(rcCheck)) { \
            RTTestIFailed("line %u: %s: %Rrc", __LINE__, #rcExpr, rcCheck); \
            return; \
        } \
    } while (0)

/** @} */


/** @}  */

RT_C_DECLS_END

#endif

