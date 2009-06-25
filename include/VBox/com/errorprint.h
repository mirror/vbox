/** @file
 * MS COM / XPCOM Abstraction Layer:
 * Error printing macros using shared functions defined in shared glue code.
 * Use these CHECK_* macros for efficient error checking around calling COM methods.
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

#ifndef ___VBox_com_errorprint_h
#define ___VBox_com_errorprint_h

namespace com
{

// shared prototypes; these are defined in shared glue code and are
// compiled only once for all front-ends
void GluePrintErrorInfo(com::ErrorInfo &info);
void GluePrintErrorContext(const char *pcszContext, const char *pcszSourceFile, uint32_t ulLine);
void GluePrintRCMessage(HRESULT rc);
void GlueHandleComError(ComPtr<IUnknown> iface, const char *pcszContext, HRESULT rc, const char *pcszSourceFile, uint32_t ulLine);

/**
 *  Calls the given method of the given interface and then checks if the return
 *  value (COM result code) indicates a failure. If so, prints the failed
 *  function/line/file, the description of the result code and attempts to
 *  query the extended error information on the current thread (using
 *  com::ErrorInfo) if the interface reports that it supports error information.
 *
 *  Used by command line tools or for debugging and assumes the |HRESULT rc|
 *  variable is accessible for assigning in the current scope.
 */
#define CHECK_ERROR(iface, method) \
    do { \
        rc = iface->method; \
        if (FAILED(rc)) \
            com::GlueHandleComError(iface, #method, rc, __FILE__, __LINE__); \
    } while (0)

/**
 *  Does the same as CHECK_ERROR(), but executes the |break| statement on
 *  failure.
 */
#ifdef __GNUC__
# define CHECK_ERROR_BREAK(iface, method) \
    ({ \
        rc = iface->method; \
        if (FAILED(rc)) \
        { \
            com::GlueHandleComError(iface, #method, rc, __FILE__, __LINE__); \
            break; \
        } \
    })
#else
# define CHECK_ERROR_BREAK(iface, method) \
    if (1) \
    { \
        rc = iface->method; \
        if (FAILED(rc)) \
        { \
            com::GlueHandleComError(iface, #method, rc, __FILE__, __LINE__); \
            break; \
        } \
    } \
    else do {} while (0)
#endif

/**
 *  Does the same as CHECK_ERROR(), but executes the |return ret| statement on
 *  failure.
 */
#define CHECK_ERROR_RET(iface, method, ret) \
    do { \
        rc = iface->method; \
        if (FAILED(rc)) \
        { \
            com::GlueHandleComError(iface, #method, rc, __FILE__, __LINE__); \
            return (ret); \
        } \
    } while (0)

/**
 *  Asserts the given expression is true. When the expression is false, prints
 *  a line containing the failed function/line/file; otherwise does nothing.
 */
#define ASSERT(expr) \
    do { \
        if (!(expr)) \
        { \
            RTPrintf ("[!] ASSERTION FAILED at line %d: %s\n", __LINE__, #expr); \
            Log (("[!] ASSERTION FAILED at line %d: %s\n", __LINE__, #expr)); \
        } \
    } while (0)

#endif

/**
 *  Does the same as ASSERT(), but executes the |return ret| statement if the
 *  expression to assert is false;
 */
#define ASSERT_RET(expr, ret) \
    do { ASSERT(expr); if (!(expr)) return (ret); } while (0)

/**
 *  Does the same as ASSERT(), but executes the |break| statement if the
 *  expression to assert is false;
 */
#define ASSERT_BREAK(expr, ret) \
    if (1) { ASSERT(expr); if (!(expr)) break; } else do {} while (0)

} /* namespace com */
