/** @file
 * IPRT - Assertions.
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

#ifndef ___iprt_assert_h
#define ___iprt_assert_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

/** @defgroup grp_rt_assert     Assert - Assertions
 * @ingroup grp_rt
 *
 * Assertions are generally used to check precoditions and other
 * assumptions. Sometimes it is also used to catch odd errors or errors
 * that one would like to inspect in the debugger. They should not be
 * used for errors that happen frequently.
 *
 * IPRT provides a host of assertion macros, so many that it can be a bit
 * overwhelming at first. Don't despair, there is a system (surprise).
 *
 * First there are four families of assertions:
 *      - Assert        - The normal strict build only assertions.
 *      - AssertLogRel  - Calls LogRel() in non-strict builds, otherwise like Assert.
 *      - AssertRelease - Triggers in all builds.
 *      - AssertFatal   - Triggers in all builds and cannot be continued.
 *
 * Then there are variations wrt to argument list and behavior on failure:
 *      - Msg           - Custom RTStrPrintf-like message with the assertion message.
 *      - Return        - Return the specific rc on failure.
 *      - ReturnVoid    - Return (void) on failure.
 *      - Break         - Break (out of switch/loop) on failure.
 *      - Stmt          - Execute the specified statment(s) on failure.
 *      - RC            - Assert RT_SUCCESS.
 *      - RCSuccess     - Assert VINF_SUCCESS.
 *
 * In additions there is a very special familiy AssertCompile that can be
 * used for some limited compile checking. Like structure sizes and member
 * alignment. This family doesn't have the same variations.
 *
 *
 * @remarks As you might've noticed, the macros doesn't follow the
 * coding guidelines wrt to macros supposedly being all uppercase
 * and  underscored. For various  reasons they don't, and it nobody
 * has complained yet. Wonder why... :-)
 *
 * @remarks Each project has its own specific guidelines on how to use
 * assertions, so the above is just trying to give you the general idea
 * from the IPRT point of view.
 *
 * @{
 */

__BEGIN_DECLS

/**
 * The 1st part of an assert message.
 *
 * @param   pszExpr     Expression. Can be NULL.
 * @param   uLine       Location line number.
 * @param   pszFile     Location file name.
 * @param   pszFunction Location function name.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void)    AssertMsg1(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction);

/**
 * The 2nd (optional) part of an assert message.
 * @param   pszFormat   Printf like format string.
 * @param   ...         Arguments to that string.
 * @remark  This API exists in HC Ring-3 and GC.
 */
RTDECL(void)    AssertMsg2(const char *pszFormat, ...);

/**
 * Overridable function that decides whether assertions executes the breakpoint or not.
 *
 * The generic implementation will return true.
 *
 * @returns true if the breakpoint should be hit, false if it should be ignored.
 * @remark  The RTDECL() makes this a bit difficult to override on windows. Sorry.
 */
RTDECL(bool)    RTAssertDoBreakpoint(void);

/** The last assert message, 1st part. */
extern RTDATADECL(char) g_szRTAssertMsg1[1024];
/** The last assert message, 2nd part. */
extern RTDATADECL(char) g_szRTAssertMsg2[2048];

__END_DECLS


/** @def AssertBreakpoint()
 * Assertion Breakpoint.
 *
 * @remark  In the gnu world we add a nop instruction after the int3 to
 *          force gdb to remain at the int3 source line.
 * @remark  The L4 kernel will try make sense of the breakpoint, thus the jmp.
 */
#ifdef RT_STRICT
# ifdef __GNUC__
#  ifndef __L4ENV__
#   define AssertBreakpoint()   do { if (RTAssertDoBreakpoint()) { __asm__ __volatile__ ("int3\n\tnop"); } } while (0)
#  else
#   define AssertBreakpoint()   do { if (RTAssertDoBreakpoint()) { __asm__ __volatile__ ("int3; jmp 1f; 1:"); } } while (0)
#  endif
# elif defined(_MSC_VER) || defined(DOXYGEN_RUNNING)
#  define AssertBreakpoint()    do { if (RTAssertDoBreakpoint()) { __debugbreak(); } } while (0)
# else
#  error "Unknown compiler"
# endif
#else
# define AssertBreakpoint()     do { } while (0)
#endif

/**
 * RTASSERTTYPE is the type the AssertCompile() macro redefines.
 * It has no other function and shouldn't be used.
 * Visual C++ uses this.
 */
typedef int RTASSERTTYPE[1];

/**
 * RTASSERTVAR is the type the AssertCompile() macro redefines.
 * It has no other function and shouldn't be used.
 * GCC uses this.
 */
#ifdef __GNUC__
__BEGIN_DECLS
#endif
extern int RTASSERTVAR[1];
#ifdef __GNUC__
__END_DECLS
#endif

/** @def AssertCompile
 * Asserts that a compile-time expression is true. If it's not break the build.
 * @param   expr    Expression which should be true.
 */
#ifdef __GNUC__
# define AssertCompile(expr)    extern int RTASSERTVAR[1] __attribute__((unused)), RTASSERTVAR[(expr) ? 1 : 0] __attribute__((unused))
#else
# define AssertCompile(expr)    typedef int RTASSERTTYPE[(expr) ? 1 : 0]
#endif

/** @def AssertCompileSize
 * Asserts a size at compile.
 * @param   type    The type.
 * @param   size    The expected type size.
 */
#define AssertCompileSize(type, size) \
    AssertCompile(sizeof(type) == (size))

/** @def AssertCompileSizeAlignment
 * Asserts a size alignment at compile.
 * @param   type    The type.
 * @param   align   The size alignment to assert.
 */
#define AssertCompileSizeAlignment(type, align) \
    AssertCompile(!(sizeof(type) & ((align) - 1)))

/** @def AssertCompileMemberAlignment
 * Asserts a member offset alignment at compile.
 * @param   type    The type.
 * @param   member  The member.
 * @param   align   The member offset alignment to assert.
 */
#if defined(__GNUC__) && defined(__cplusplus)
# if __GNUC__ >= 4
#  define AssertCompileMemberAlignment(type, member, align) \
    AssertCompile(!(__builtin_offsetof(type, member) & ((align) - 1)))
# else
#  define AssertCompileMemberAlignment(type, member, align) \
    AssertCompile(!(RT_OFFSETOF(type, member) & ((align) - 1)))
# endif
#else
# define AssertCompileMemberAlignment(type, member, align) \
    AssertCompile(!(RT_OFFSETOF(type, member) & ((align) - 1)))
#endif


/** @def AssertCompileMemberSize
 * Asserts a member offset alignment at compile.
 * @param   type    The type.
 * @param   member  The member.
 * @param   size    The member size to assert.
 */
#define AssertCompileMemberSize(type, member, size) \
    AssertCompile(RT_SIZEOFMEMB(type, member) == (size))

/** @def AssertCompileMemberSizeAlignment
 * Asserts a member size alignment at compile.
 * @param   type    The type.
 * @param   member  The member.
 * @param   align   The member size alignment to assert.
 */
#define AssertCompileMemberSizeAlignment(type, member, align) \
    AssertCompile(!(RT_SIZEOFMEMB(type, member) & ((align) - 1)))


/** @def Assert
 * Assert that an expression is true. If it's not hit breakpoint.
 * @param   expr    Expression which should be true.
 */
#ifdef RT_STRICT
# define Assert(expr)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertBreakpoint(); \
        } \
    } while (0)
#else
# define Assert(expr)     do { } while (0)
#endif


/** @def AssertReturn
 * Assert that an expression is true and returns if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 * @param   rc      What is to be presented to return.
 */
#ifdef RT_STRICT
# define AssertReturn(expr, rc) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertBreakpoint(); \
            return (rc); \
        } \
    } while (0)
#else
# define AssertReturn(expr, rc) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            return (rc); \
    } while (0)
#endif

/** @def AssertReturnVoid
 * Assert that an expression is true and returns if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 */
#ifdef RT_STRICT
# define AssertReturnVoid(expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertBreakpoint(); \
            return; \
        } \
    } while (0)
#else
# define AssertReturnVoid(expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            return; \
    } while (0)
#endif


/** @def AssertBreak
 * Assert that an expression is true and breaks if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 */
#ifdef RT_STRICT
# define AssertBreak(expr) \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        break; \
    } else do {} while (0)
#else
# define AssertBreak(expr) \
    if (RT_UNLIKELY(!(expr))) \
        break; \
    else do {} while (0)
#endif

/** @def AssertBreakStmt
 * Assert that an expression is true and breaks if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before doing break.
 *
 * @param   expr    Expression which should be true.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#ifdef RT_STRICT
# define AssertBreakStmt(expr, stmt) \
    if (RT_UNLIKELY(!(expr))) { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)
#else
# define AssertBreakStmt(expr, stmt) \
    if (RT_UNLIKELY(!(expr))) { \
        stmt; \
        break; \
    } else do {} while (0)
#endif


/** @def AssertMsg
 * Assert that an expression is true. If it's not print message and hit breakpoint.
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsg(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertBreakpoint(); \
        } \
    } while (0)
#else
# define AssertMsg(expr, a)  do { } while (0)
#endif

/** @def AssertMsgReturn
 * Assert that an expression is true and returns if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   rc      What is to be presented to return.
 */
#ifdef RT_STRICT
# define AssertMsgReturn(expr, a, rc)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertBreakpoint(); \
            return (rc); \
        } \
    } while (0)
#else
# define AssertMsgReturn(expr, a, rc) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            return (rc); \
    } while (0)
#endif

/** @def AssertMsgReturnVoid
 * Assert that an expression is true and returns if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsgReturnVoid(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertBreakpoint(); \
            return; \
        } \
    } while (0)
#else
# define AssertMsgReturnVoid(expr, a) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            return; \
    } while (0)
#endif


/** @def AssertMsgBreak
 * Assert that an expression is true and breaks if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before returning.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsgBreak(expr, a)  \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        break; \
    } else do {} while (0)
#else
# define AssertMsgBreak(expr, a) \
    if (RT_UNLIKELY(!(expr))) \
        break; \
    else do {} while (0)
#endif

/** @def AssertMsgBreakStmt
 * Assert that an expression is true and breaks if it isn't.
 * In RT_STRICT mode it will hit a breakpoint before doing break.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#ifdef RT_STRICT
# define AssertMsgBreakStmt(expr, a, stmt) \
    if (RT_UNLIKELY(!(expr))) { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)
#else
# define AssertMsgBreakStmt(expr, a, stmt) \
    if (RT_UNLIKELY(!(expr))) { \
        stmt; \
        break; \
    } else do {} while (0)
#endif

/** @def AssertFailed
 * An assertion failed hit breakpoint.
 */
#ifdef RT_STRICT
# define AssertFailed()  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
    } while (0)
#else
# define AssertFailed()         do { } while (0)
#endif

/** @def AssertFailedReturn
 * An assertion failed, hit breakpoint (RT_STRICT mode only) and return.
 *
 * @param   rc      The rc to return.
 */
#ifdef RT_STRICT
# define AssertFailedReturn(rc)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        return (rc); \
    } while (0)
#else
# define AssertFailedReturn(rc)  \
    do { \
        return (rc); \
    } while (0)
#endif

/** @def AssertFailedReturnVoid
 * An assertion failed, hit breakpoint (RT_STRICT mode only) and return.
 */
#ifdef RT_STRICT
# define AssertFailedReturnVoid()  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        return; \
    } while (0)
#else
# define AssertFailedReturnVoid()  \
    do { \
        return; \
    } while (0)
#endif


/** @def AssertFailedBreak
 * An assertion failed, hit breakpoint (RT_STRICT mode only) and break.
 * @todo Rename to AssertFailedBreak.
 */
#ifdef RT_STRICT
# define AssertFailedBreak()  \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        break; \
    } else do {} while (0)
#else
# define AssertFailedBreak()  \
    if (1) \
        break; \
    else do {} while (0)
#endif

/** @def AssertFailedBreakStmt
 * An assertion failed, hit breakpoint (RT_STRICT mode only), execute
 * the given statement and break.
 *
 * @param   stmt    Statement to execute before break.
 */
#ifdef RT_STRICT
# define AssertFailedBreakStmt(stmt) \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)
#else
# define AssertFailedBreakStmt(stmt) \
    if (1) { \
        stmt; \
        break; \
    } else do {} while (0)
#endif


/** @def AssertMsgFailed
 * An assertion failed print a message and a hit breakpoint.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsgFailed(a)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
    } while (0)
#else
# define AssertMsgFailed(a)     do { } while (0)
#endif

/** @def AssertMsgFailedReturn
 * An assertion failed, hit breakpoint with message (RT_STRICT mode only) and return.
 *
 * @param   a       printf argument list (in parenthesis).
 * @param   rc      What is to be presented to return.
 */
#ifdef RT_STRICT
# define AssertMsgFailedReturn(a, rc)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        return (rc); \
    } while (0)
#else
# define AssertMsgFailedReturn(a, rc)  \
    do { \
        return (rc); \
    } while (0)
#endif

/** @def AssertMsgFailedReturnVoid
 * An assertion failed, hit breakpoint with message (RT_STRICT mode only) and return.
 *
 * @param   a       printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsgFailedReturnVoid(a)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        return; \
    } while (0)
#else
# define AssertMsgFailedReturnVoid(a)  \
    do { \
        return; \
    } while (0)
#endif


/** @def AssertMsgFailedBreak
 * An assertion failed, hit breakpoint with message (RT_STRICT mode only) and break.
 *
 * @param   a       printf argument list (in parenthesis).
 */
#ifdef RT_STRICT
# define AssertMsgFailedBreak(a)  \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        break; \
    } else do {} while (0)
#else
# define AssertMsgFailedBreak(a)  \
    if (1) \
        break; \
    else do {} while (0)
#endif

/** @def AssertMsgFailedBreakStmt
 * An assertion failed, hit breakpoint (RT_STRICT mode only), execute
 * the given statement and break.
 *
 * @param   a       printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break.
 */
#ifdef RT_STRICT
# define AssertMsgFailedBreakStmt(a, stmt) \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)
#else
# define AssertMsgFailedBreakStmt(a, stmt) \
    if (1) { \
        stmt; \
        break; \
    } else do {} while (0)
#endif



/** @def AssertLogRelBreakpoint()
 * Assertion LogRel Breakpoint.
 *
 * NOP in non-strict (release) builds, hardware breakpoint in strict builds,
 *
 * @remark  In the gnu world we add a nop instruction after the int3 to
 *          force gdb to remain at the int3 source line.
 * @remark  The L4 kernel will try make sense of the breakpoint, thus the jmp.
 */
#ifdef RT_STRICT
# ifdef __GNUC__
#  ifndef __L4ENV__
#   define AssertLogRelBreakpoint()     do { RTAssertDoBreakpoint(); __asm__ __volatile__ ("int3\n\tnop"); } while (0)
#  else
#   define AssertLogRelBreakpoint()     do { RTAssertDoBreakpoint(); __asm__ __volatile__ ("int3; jmp 1f; 1:"); } while (0)
#  endif
# elif defined(_MSC_VER) || defined(DOXYGEN_RUNNING)
#  define AssertLogRelBreakpoint()      do { RTAssertDoBreakpoint(); __debugbreak(); } while (0)
# else
#  error "Unknown compiler"
# endif
#else   /* !RT_STRICT */
# define AssertLogRelBreakpoint()       do { } while (0)
#endif  /* !RT_STRICT */


/** @def AssertLogRelMsg1
 * AssertMsg1 (strict builds) / LogRel wrapper (non-strict).
 */
#ifdef RT_STRICT
# define AssertLogRelMsg1(pszExpr, iLine, pszFile, pszFunction) \
    AssertMsg1(pszExpr, iLine, pszFile, pszFunction)
#else
# define AssertLogRelMsg1(pszExpr, iLine, pszFile, pszFunction) \
    LogRel(("AssertLogRel %s(%d): %s\n",\
            (pszFile), (iLine), (pszFile), (pszFunction), (pszExpr) ))
#endif

/** @def AssertLogRelMsg2
 * AssertMsg2 (strict builds) / LogRel wrapper (non-strict).
 */
#ifdef RT_STRICT
# define AssertLogRelMsg2(a) AssertMsg2 a
#else
# define AssertLogRelMsg2(a) LogRel(a)
#endif

/** @def AssertLogRel
 * Assert that an expression is true.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertLogRel(expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelBreakpoint(); \
        } \
    } while (0)

/** @def AssertLogRelReturn
 * Assert that an expression is true, return \a rc if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   rc      What is to be presented to return.
 */
#define AssertLogRelReturn(expr, rc) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelBreakpoint(); \
            return (rc); \
        } \
    } while (0)

/** @def AssertLogRelReturnVoid
 * Assert that an expression is true, return void if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertLogRelReturnVoid(expr) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelBreakpoint(); \
            return; \
        } \
    } while (0)

/** @def AssertLogRelBreak
 * Assert that an expression is true, break if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertLogRelBreak(expr) \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        break; \
    } \
    else do {} while (0)

/** @def AssertLogRelBreakStmt
 * Assert that an expression is true, execute \a stmt and break if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertLogRelBreakStmt(expr, stmt) \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)

/** @def AssertLogRelMsg
 * Assert that an expression is true.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertLogRelMsg(expr, a) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelMsg2(a); \
            AssertLogRelBreakpoint(); \
        } \
    } while (0)

/** @def AssertLogRelMsgReturn
 * Assert that an expression is true, return \a rc if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   rc      What is to be presented to return.
 */
#define AssertLogRelMsgReturn(expr, a, rc) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelMsg2(a); \
            AssertLogRelBreakpoint(); \
            return (rc); \
        } \
    } while (0)

/** @def AssertLogRelMsgReturnVoid
 * Assert that an expression is true, return (void) if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertLogRelMsgReturnVoid(expr, a) \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertLogRelMsg2(a); \
            AssertLogRelBreakpoint(); \
            return; \
        } \
    } while (0)

/** @def AssertLogRelMsgBreak
 * Assert that an expression is true, break if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertLogRelMsgBreak(expr, a) \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        break; \
    } \
    else do {} while (0)

/** @def AssertLogRelMsgBreakStmt
 * Assert that an expression is true, execute \a stmt and break if it isn't.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertLogRelMsgBreakStmt(expr, a, stmt) \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertLogRelMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)

/** @def AssertLogRelFailed
 * An assertion failed.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 */
#define AssertLogRelFailed() \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
    } while (0)

/** @def AssertLogRelFailedReturn
 * An assertion failed.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   rc      What is to be presented to return.
 */
#define AssertLogRelFailedReturn(rc) \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        return (rc); \
    } while (0)

/** @def AssertLogRelFailedReturnVoid
 * An assertion failed, hit a breakpoint and return.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 */
#define AssertLogRelFailedReturnVoid() \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        return; \
    } while (0)

/** @def AssertLogRelFailedBreak
 * An assertion failed, break.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 */
#define AssertLogRelFailedBreak() \
    if (1) \
    { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        break; \
    } else do {} while (0)

/** @def AssertLogRelFailedBreakStmt
 * An assertion failed, execute \a stmt and break.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   stmt    Statement to execute before break.
 */
#define AssertLogRelFailedBreakStmt(stmt) \
    if (1) \
    { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)

/** @def AssertLogRelMsgFailed
 * An assertion failed.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertLogRelMsgFailed(a) \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
    } while (0)

/** @def AssertLogRelMsgFailedReturn
 * An assertion failed, return \a rc.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   a   printf argument list (in parenthesis).
 * @param   rc  What is to be presented to return.
 */
#define AssertLogRelMsgFailedReturn(a, rc) \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        return (rc); \
    } while (0)

/** @def AssertLogRelMsgFailedReturnVoid
 * An assertion failed, return void.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertLogRelMsgFailedReturnVoid(a) \
    do { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        return; \
    } while (0)

/** @def AssertLogRelMsgFailedBreak
 * An assertion failed, break.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertLogRelMsgFailedBreak(a) \
    if (1)\
    { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        break; \
    } else do {} while (0)

/** @def AssertLogRelMsgFailedBreakStmt
 * An assertion failed, execute \a stmt and break.
 * Strict builds will hit a breakpoint, non-strict will only do LogRel.
 *
 * @param   a   printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break.
 */
#define AssertLogRelMsgFailedBreakStmt(a, stmt) \
    if (1) \
    { \
        AssertLogRelMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertLogRelMsg2(a); \
        AssertLogRelBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)



/** @def AssertReleaseBreakpoint()
 * Assertion Breakpoint.
 *
 * @remark  In the gnu world we add a nop instruction after the int3 to
 *          force gdb to remain at the int3 source line.
 * @remark  The L4 kernel will try make sense of the breakpoint, thus the jmp.
 */
#ifdef __GNUC__
# ifndef __L4ENV__
#  define AssertReleaseBreakpoint()     do { RTAssertDoBreakpoint(); __asm__ __volatile__ ("int3\n\tnop"); } while (0)
# else
#  define AssertReleaseBreakpoint()     do { RTAssertDoBreakpoint(); __asm__ __volatile__ ("int3; jmp 1f; 1:"); } while (0)
# endif
#elif defined(_MSC_VER) || defined(DOXYGEN_RUNNING)
# define AssertReleaseBreakpoint()      do { RTAssertDoBreakpoint(); __debugbreak(); } while (0)
#else
# error "Unknown compiler"
#endif


/** @def AssertRelease
 * Assert that an expression is true. If it's not hit a breakpoint.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertRelease(expr)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertReleaseBreakpoint(); \
        } \
    } while (0)

/** @def AssertReleaseReturn
 * Assert that an expression is true, hit a breakpoing and return if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   rc      What is to be presented to return.
 */
#define AssertReleaseReturn(expr, rc)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertReleaseBreakpoint(); \
            return (rc); \
        } \
    } while (0)

/** @def AssertReleaseReturnVoid
 * Assert that an expression is true, hit a breakpoing and return if it isn't.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertReleaseReturnVoid(expr)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertReleaseBreakpoint(); \
            return; \
        } \
    } while (0)


/** @def AssertReleaseBreak
 * Assert that an expression is true, hit a breakpoing and break if it isn't.
 *
 * @param   expr    Expression which should be true.
 */
#define AssertReleaseBreak(expr)  \
    if { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertReleaseBreakpoint(); \
            break; \
        } \
    } else do {} while (0)

/** @def AssertReleaseBreakStmt
 * Assert that an expression is true, hit a breakpoing and break if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertReleaseBreakStmt(expr, stmt)  \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)


/** @def AssertReleaseMsg
 * Assert that an expression is true, print the message and hit a breakpoint if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertReleaseMsg(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertReleaseBreakpoint(); \
        } \
    } while (0)

/** @def AssertReleaseMsgReturn
 * Assert that an expression is true, print the message and hit a breakpoint and return if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   rc      What is to be presented to return.
 */
#define AssertReleaseMsgReturn(expr, a, rc)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertReleaseBreakpoint(); \
            return (rc); \
        } \
    } while (0)

/** @def AssertReleaseMsgReturnVoid
 * Assert that an expression is true, print the message and hit a breakpoint and return if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertReleaseMsgReturnVoid(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
        { \
            AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertReleaseBreakpoint(); \
            return; \
        } \
    } while (0)


/** @def AssertReleaseMsgBreak
 * Assert that an expression is true, print the message and hit a breakpoint and break if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertReleaseMsgBreak(expr, a)  \
    if (RT_UNLIKELY(!(expr))) \
    { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        break; \
    } else do {} while (0)

/** @def AssertReleaseMsgBreakStmt
 * Assert that an expression is true, print the message and hit a breakpoing and break if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertReleaseMsgBreakStmt(expr, a, stmt)  \
    if (RT_UNLIKELY(!(expr))) { \
        AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)


/** @def AssertReleaseFailed
 * An assertion failed, hit a breakpoint.
 */
#define AssertReleaseFailed()  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
    } while (0)

/** @def AssertReleaseFailedReturn
 * An assertion failed, hit a breakpoint and return.
 *
 * @param   rc      What is to be presented to return.
 */
#define AssertReleaseFailedReturn(rc)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
        return (rc); \
    } while (0)

/** @def AssertReleaseFailedReturnVoid
 * An assertion failed, hit a breakpoint and return.
 */
#define AssertReleaseFailedReturnVoid()  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
        return; \
    } while (0)


/** @def AssertReleaseFailedBreak
 * An assertion failed, hit a breakpoint and break.
 */
#define AssertReleaseFailedBreak()  \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
        break; \
    } else do {} while (0)

/** @def AssertReleaseFailedBreakStmt
 * An assertion failed, hit a breakpoint and break.
 *
 * @param   stmt    Statement to execute before break.
 */
#define AssertReleaseFailedBreakStmt(stmt)  \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertReleaseBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)


/** @def AssertReleaseMsgFailed
 * An assertion failed, print a message and hit a breakpoint.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertReleaseMsgFailed(a)  \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
    } while (0)

/** @def AssertReleaseMsgFailedReturn
 * An assertion failed, print a message, hit a breakpoint and return.
 *
 * @param   a   printf argument list (in parenthesis).
 * @param   rc      What is to be presented to return.
 */
#define AssertReleaseMsgFailedReturn(a, rc) \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        return (rc); \
    } while (0)

/** @def AssertReleaseMsgFailedReturnVoid
 * An assertion failed, print a message, hit a breakpoint and return.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertReleaseMsgFailedReturnVoid(a) \
    do { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        return; \
    } while (0)


/** @def AssertReleaseMsgFailedBreak
 * An assertion failed, print a message, hit a breakpoint and break.
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertReleaseMsgFailedBreak(a) \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        break; \
    } else do {} while (0)

/** @def AssertReleaseMsgFailedBreakStmt
 * An assertion failed, print a message, hit a breakpoint and break.
 *
 * @param   a   printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break.
 */
#define AssertReleaseMsgFailedBreakStmt(a, stmt) \
    if (1) { \
        AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
        AssertMsg2 a; \
        AssertReleaseBreakpoint(); \
        stmt; \
        break; \
    } else do {} while (0)


/** @def AssertFatal
 * Assert that an expression is true. If it's not hit a breakpoint (for ever).
 *
 * @param   expr    Expression which should be true.
 */
#define AssertFatal(expr)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            for (;;) \
            { \
                AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
                AssertReleaseBreakpoint(); \
            } \
    } while (0)

/** @def AssertFatalMsg
 * Assert that an expression is true, print the message and hit a breakpoint (for ever) if it isn't.
 *
 * @param   expr    Expression which should be true.
 * @param   a       printf argument list (in parenthesis).
 */
#define AssertFatalMsg(expr, a)  \
    do { \
        if (RT_UNLIKELY(!(expr))) \
            for (;;) \
            { \
                AssertMsg1(#expr, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
                AssertMsg2 a; \
                AssertReleaseBreakpoint(); \
            } \
    } while (0)

/** @def AssertFatalFailed
 * An assertion failed, hit a breakpoint (for ever).
 */
#define AssertFatalFailed()  \
    do { \
        for (;;) \
        { \
            AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertReleaseBreakpoint(); \
        } \
    } while (0)

/** @def AssertFatalMsgFailed
 * An assertion failed, print a message and hit a breakpoint (for ever).
 *
 * @param   a   printf argument list (in parenthesis).
 */
#define AssertFatalMsgFailed(a)  \
    do { \
        for (;;) \
        { \
            AssertMsg1((const char *)0, __LINE__, __FILE__, __PRETTY_FUNCTION__); \
            AssertMsg2 a; \
            AssertReleaseBreakpoint(); \
        } \
    } while (0)


/** @def AssertRC
 * Asserts a iprt status code successful.
 *
 * On failure it will print info about the rc and hit a breakpoint.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRC(rc)                AssertMsgRC(rc, ("%Vra\n", (rc)))

/** @def AssertRCReturn
 * Asserts a iprt status code successful, bitch (RT_STRICT mode only) and return if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCReturn(rc, rcRet)   AssertMsgRCReturn(rc, ("%Vra\n", (rc)), rcRet)

/** @def AssertRCReturnVoid
 * Asserts a iprt status code successful, bitch (RT_STRICT mode only) and return if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCReturnVoid(rc)      AssertMsgRCReturnVoid(rc, ("%Vra\n", (rc)))

/** @def AssertRCBreak
 * Asserts a iprt status code successful, bitch (RT_STRICT mode only) and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCBreak(rc)           AssertMsgRCBreak(rc, ("%Vra\n", (rc)))

/** @def AssertRCBreakStmt
 * Asserts a iprt status code successful, bitch (RT_STRICT mode only) and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCBreakStmt(rc, stmt) AssertMsgRCBreakStmt(rc, ("%Vra\n", (rc)), stmt)

/** @def AssertMsgRC
 * Asserts a iprt status code successful.
 *
 * It prints a custom message and hits a breakpoint on FAILURE.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertMsgRC(rc, msg) \
    do { AssertMsg(RT_SUCCESS_NP(rc), msg); NOREF(rc); } while (0)

/** @def AssertMsgRCReturn
 * Asserts a iprt status code successful and if it's not return the specified status code.
 *
 * If RT_STRICT is defined the message will be printed and a breakpoint hit before it returns
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertMsgRCReturn(rc, msg, rcRet) \
    do { AssertMsgReturn(RT_SUCCESS_NP(rc), msg, rcRet); NOREF(rc); } while (0)

/** @def AssertMsgRCReturnVoid
 * Asserts a iprt status code successful and if it's not return.
 *
 * If RT_STRICT is defined the message will be printed and a breakpoint hit before it returns
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertMsgRCReturnVoid(rc, msg) \
    do { AssertMsgReturnVoid(RT_SUCCESS_NP(rc), msg); NOREF(rc); } while (0)

/** @def AssertMsgRCBreak
 * Asserts a iprt status code successful and if it's not break.
 *
 * If RT_STRICT is defined the message will be printed and a breakpoint hit before it breaks
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertMsgRCBreak(rc, msg) \
    if (1) { AssertMsgBreak(RT_SUCCESS(rc), msg); NOREF(rc); } else do {} while (0)

/** @def AssertMsgRCBreakStmt
 * Asserts a iprt status code successful and break if it's not.
 *
 * If RT_STRICT is defined the message will be printed and a breakpoint hit before it returns
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertMsgRCBreakStmt(rc, msg, stmt) \
    if (1) { AssertMsgBreakStmt(RT_SUCCESS_NP(rc), msg, stmt); NOREF(rc); } else do {} while (0)

/** @def AssertRCSuccess
 * Asserts an iprt status code equals VINF_SUCCESS.
 *
 * On failure it will print info about the rc and hit a breakpoint.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCSuccess(rc)                 AssertMsg((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertRCSuccessReturn
 * Asserts that an iprt status code equals VINF_SUCCESS, bitch (RT_STRICT mode only) and return if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCSuccessReturn(rc, rcRet)    AssertMsgReturn((rc) == VINF_SUCCESS, ("%Vra\n", (rc)), rcRet)

/** @def AssertRCSuccessReturnVoid
 * Asserts that an iprt status code equals VINF_SUCCESS, bitch (RT_STRICT mode only) and return if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCSuccessReturnVoid(rc)       AssertMsgReturnVoid((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertRCSuccessBreak
 * Asserts that an iprt status code equals VINF_SUCCESS, bitch (RT_STRICT mode only) and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCSuccessBreak(rc)            AssertMsgBreak((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertRCSuccessBreakStmt
 * Asserts that an iprt status code equals VINF_SUCCESS, bitch (RT_STRICT mode only) and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times. In release mode is NOREF()'ed.
 */
#define AssertRCSuccessBreakStmt(rc, stmt)  AssertMsgBreakStmt((rc) == VINF_SUCCESS, ("%Vra\n", (rc)), stmt)


/** @def AssertLogRelRC
 * Asserts a iprt status code successful.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRC(rc)                      AssertLogRelMsgRC(rc, ("%Rra\n", (rc)))

/** @def AssertLogRelRCReturn
 * Asserts a iprt status code successful, returning \a rc if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCReturn(rc, rcRet)         AssertLogRelMsgRCReturn(rc, ("%Rra\n", (rc)), rcRet)

/** @def AssertLogRelRCReturnVoid
 * Asserts a iprt status code successful, returning (void) if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCReturnVoid(rc)            AssertLogRelMsgRCReturnVoid(rc, ("%Rra\n", (rc)))

/** @def AssertLogRelRCBreak
 * Asserts a iprt status code successful, breaking if it isn't.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCBreak(rc)                 AssertLogRelMsgRCBreak(rc, ("%Rra\n", (rc)))

/** @def AssertLogRelRCBreakStmt
 * Asserts a iprt status code successful, execute \a statement and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCBreakStmt(rc, stmt)       AssertLogRelMsgRCBreakStmt(rc, ("%Rra\n", (rc)), stmt)

/** @def AssertLogRelMsgRC
 * Asserts a iprt status code successful.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertLogRelMsgRC(rc, msg)              AssertLogRelMsg(RT_SUCCESS_NP(rc), msg)

/** @def AssertLogRelMsgRCReturn
 * Asserts a iprt status code successful.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelMsgRCReturn(rc, msg, rcRet) AssertLogRelMsgReturn(RT_SUCCESS_NP(rc), msg, rcRet)

/** @def AssertLogRelMsgRCReturnVoid
 * Asserts a iprt status code successful.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertLogRelMsgRCReturnVoid(rc, msg)    AssertLogRelMsgReturnVoid(RT_SUCCESS_NP(rc), msg)

/** @def AssertLogRelMsgRCBreak
 * Asserts a iprt status code successful.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertLogRelMsgRCBreak(rc, msg)         AssertLogRelMsgBreak(RT_SUCCESS(rc), msg)

/** @def AssertLogRelMsgRCBreakStmt
 * Asserts a iprt status code successful, execute \a stmt and break if it isn't.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelMsgRCBreakStmt(rc, msg, stmt) AssertLogRelMsgBreakStmt(RT_SUCCESS_NP(rc), msg, stmt)

/** @def AssertLogRelRCSuccess
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCSuccess(rc)               AssertLogRelMsg((rc) == VINF_SUCCESS, ("%Rra\n", (rc)))

/** @def AssertLogRelRCSuccessReturn
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCSuccessReturn(rc, rcRet)  AssertLogRelMsgReturn((rc) == VINF_SUCCESS, ("%Rra\n", (rc)), rcRet)

/** @def AssertLogRelRCSuccessReturnVoid
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCSuccessReturnVoid(rc)     AssertLogRelMsgReturnVoid((rc) == VINF_SUCCESS, ("%Rra\n", (rc)))

/** @def AssertLogRelRCSuccessBreak
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCSuccessBreak(rc)          AssertLogRelMsgBreak((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertLogRelRCSuccessBreakStmt
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertLogRelRCSuccessBreakStmt(rc, stmt) AssertLogRelMsgBreakStmt((rc) == VINF_SUCCESS, ("%Vra\n", (rc)), stmt)


/** @def AssertReleaseRC
 * Asserts a iprt status code successful.
 *
 * On failure information about the error will be printed and a breakpoint hit.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRC(rc)             AssertReleaseMsgRC(rc, ("%Vra\n", (rc)))

/** @def AssertReleaseRCReturn
 * Asserts a iprt status code successful, returning if it isn't.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally returning from the function if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCReturn(rc, rcRet) AssertReleaseMsgRCReturn(rc, ("%Vra\n", (rc)), rcRet)

/** @def AssertReleaseRCReturnVoid
 * Asserts a iprt status code successful, returning if it isn't.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally returning from the function if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCReturnVoid(rc) AssertReleaseMsgRCReturnVoid(rc, ("%Vra\n", (rc)))

/** @def AssertReleaseRCBreakStmt
 * Asserts a iprt status code successful, break if it isn't.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally the break statement will be issued if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCBreakStmt(rc, stmt)  AssertReleaseMsgRCBreakStmt(rc, ("%Vra\n", (rc)), stmt)

/** @def AssertReleaseRCBreakVoid
 * Asserts a iprt status code successful, breaking if it isn't.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally breaking the current statement if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 * @todo Rename to AssertReleaseRCBreak.
 */
#define AssertReleaseRCBreakVoid(rc) AssertReleaseMsgRCBreakVoid(rc, ("%Vra\n", (rc)))

/** @def AssertReleaseMsgRC
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed and a breakpoint is hit.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertReleaseMsgRC(rc, msg)    AssertReleaseMsg(RT_SUCCESS_NP(rc), msg)

/** @def AssertReleaseMsgRCReturn
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed, a breakpoint is hit, and finally
 * returning from the function if the breakpoint is showhow ignored.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseMsgRCReturn(rc, msg, rcRet)    AssertReleaseMsgReturn(RT_SUCCESS_NP(rc), msg, rcRet)

/** @def AssertReleaseMsgRCReturnVoid
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed, a breakpoint is hit, and finally
 * returning from the function if the breakpoint is showhow ignored.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertReleaseMsgRCReturnVoid(rc, msg)    AssertReleaseMsgReturnVoid(RT_SUCCESS_NP(rc), msg)

/** @def AssertReleaseMsgRCBreakStmt
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed, a breakpoint is hit, and finally
 * the brean statement is issued if the breakpoint is showhow ignored.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseMsgRCBreakStmt(rc, msg, stmt)  AssertReleaseMsgBreakStmt(RT_SUCCESS_NP(rc), msg, stmt)

/** @def AssertReleaseMsgRCBreakVoid
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed, a breakpoint is hit, and finally
 * breaking the current status if the breakpoint is showhow ignored.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 * @todo Rename to AssertReleaseMsgRCBreak.
 */
#define AssertReleaseMsgRCBreakVoid(rc, msg)    AssertReleaseMsgBreak(RT_SUCCESS(rc), msg)

/** @def AssertReleaseRCSuccess
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed and a breakpoint hit.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCSuccess(rc)                  AssertReleaseMsg((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertReleaseRCSuccessReturn
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally returning from the function if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @param   rcRet   What is to be presented to return.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCSuccessReturn(rc, rcRet)     AssertReleaseMsgReturn((rc) == VINF_SUCCESS, ("%Vra\n", (rc)), rcRet)

/** @def AssertReleaseRCSuccessReturnVoid
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally returning from the function if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCSuccessReturnVoid(rc)     AssertReleaseMsgReturnVoid((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))

/** @def AssertReleaseRCSuccessBreakStmt
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally the break statement will be issued if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 * @remark  rc is references multiple times.
 */
#define AssertReleaseRCSuccessBreakStmt(rc, stmt)   AssertReleaseMsgBreakStmt((rc) == VINF_SUCCESS, ("%Vra\n", (rc)), stmt)

/** @def AssertReleaseRCSuccessBreakVoid
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed, a breakpoint hit
 * and finally breaking the current statement if the breakpoint is somehow ignored.
 *
 * @param   rc      iprt status code.
 * @remark  rc is references multiple times.
 * @todo Rename to AssertReleaseRCSuccessBreak.
 */
#define AssertReleaseRCSuccessBreakVoid(rc)     AssertReleaseMsgBreak((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))


/** @def AssertFatalRC
 * Asserts a iprt status code successful.
 *
 * On failure information about the error will be printed and a breakpoint hit.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertFatalRC(rc)           AssertFatalMsgRC(rc, ("%Vra\n", (rc)))

/** @def AssertReleaseMsgRC
 * Asserts a iprt status code successful.
 *
 * On failure a custom message is printed and a breakpoint is hit.
 *
 * @param   rc      iprt status code.
 * @param   msg     printf argument list (in parenthesis).
 * @remark  rc is references multiple times.
 */
#define AssertFatalMsgRC(rc, msg)   AssertFatalMsg(RT_SUCCESS_NP(rc), msg)

/** @def AssertFatalRCSuccess
 * Asserts that an iprt status code equals VINF_SUCCESS.
 *
 * On failure information about the error will be printed and a breakpoint hit.
 *
 * @param   rc  iprt status code.
 * @remark  rc is references multiple times.
 */
#define AssertFatalRCSuccess(rc)    AssertFatalMsg((rc) == VINF_SUCCESS, ("%Vra\n", (rc)))


/** @def AssertPtr
 * Asserts that a pointer is valid.
 *
 * @param   pv      The pointer.
 */
#define AssertPtr(pv)               AssertMsg(VALID_PTR(pv), ("%p\n", (pv)))

/** @def AssertPtrReturn
 * Asserts that a pointer is valid.
 *
 * @param   pv      The pointer.
 * @param   rcRet   What is to be presented to return.
 */
#define AssertPtrReturn(pv, rcRet)  AssertMsgReturn(VALID_PTR(pv), ("%p\n", (pv)), rcRet)

/** @def AssertPtrReturnVoid
 * Asserts that a pointer is valid.
 *
 * @param   pv      The pointer.
 */
#define AssertPtrReturnVoid(pv)     AssertMsgReturnVoid(VALID_PTR(pv), ("%p\n", (pv)))

/** @def AssertPtrBreakStmt
 * Asserts that a pointer is valid.
 *
 * @param   pv      The pointer.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertPtrBreakStmt(pv, stmt)    AssertMsgBreakStmt(VALID_PTR(pv), ("%p\n", (pv)), stmt)

/** @def AssertPtrBreakVoid
 * Asserts that a pointer is valid.
 *
 * @param   pv      The pointer.
 * @todo Rename to AssertPtrBreak.
 */
#define AssertPtrBreakVoid(pv)  AssertMsgBreak(VALID_PTR(pv), ("%p\n", (pv)))

/** @def AssertPtrNull
 * Asserts that a pointer is valid or NULL.
 *
 * @param   pv      The pointer.
 */
#define AssertPtrNull(pv)               AssertMsg(VALID_PTR(pv) || (pv) == NULL, ("%p\n", (pv)))

/** @def AssertPtrNullReturn
 * Asserts that a pointer is valid or NULL.
 *
 * @param   pv      The pointer.
 * @param   rcRet   What is to be presented to return.
 */
#define AssertPtrNullReturn(pv, rcRet)  AssertMsgReturn(VALID_PTR(pv) || (pv) == NULL, ("%p\n", (pv)), rcRet)

/** @def AssertPtrNullReturnVoid
 * Asserts that a pointer is valid or NULL.
 *
 * @param   pv      The pointer.
 */
#define AssertPtrNullReturnVoid(pv)  AssertMsgReturnVoid(VALID_PTR(pv) || (pv) == NULL, ("%p\n", (pv)))

/** @def AssertPtrNullBreakStmt
 * Asserts that a pointer is valid or NULL.
 *
 * @param   pv      The pointer.
 * @param   stmt    Statement to execute before break in case of a failed assertion.
 */
#define AssertPtrNullBreakStmt(pv, stmt)    AssertMsgBreakStmt(VALID_PTR(pv) || (pv) == NULL, ("%p\n", (pv)), stmt)

/** @def AssertPtrNullBreakVoid
 * Asserts that a pointer is valid or NULL.
 *
 * @param   pv      The pointer.
 * @todo Rename to AssertPtrNullBreak.
 */
#define AssertPtrNullBreakVoid(pv)  AssertMsgBreak(VALID_PTR(pv) || (pv) == NULL, ("%p\n", (pv)))

/** @def AssertGCPhys32
 * Asserts that the high dword of a physical address is zero
 *
 * @param   GCPhys      The address (RTGCPHYS).
 */
#define AssertGCPhys32(GCPhys)      AssertMsg(VALID_PHYS32(GCPhys), ("%RGp\n", (RTGCPHYS)(GCPhys)))


/** @} */

#endif

