/** @file
 *
 * VBox frontends: Basic Frontend (BFE):
 * Logging macros and function definitions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ____H_LOGGING
#define ____H_LOGGING

/*
 * We might be including the VBox logging subsystem before
 * including this header file, so reset the logging group.
 */
#ifdef LOG_GROUP
#undef LOG_GROUP
#endif
#define LOG_GROUP LOG_GROUP_MAIN
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/thread.h>

/**
 *  Helpful macro to trace execution.
 */
#define LogTrace() \
    LogFlow((">>>>> %s (%d): %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__))

/**
 *  Helper macro to print the current reference count of the given COM object
 *  to the log file.
 *  @param obj  object in question (must be a pointer to an IUnknown subclass)
 */
#define LogComRefCnt(obj) \
    do { \
        int refc = (obj)->AddRef(); refc --; \
        LogFlow (("{%p} " #obj ".refcount=%d\n", (obj), refc)); \
        (obj)->Release(); \
    } while (0)

#if defined(DEBUG) || defined(LOG_ENABLED)
#   define V_DEBUG(m)           V_LOG(m)
#if defined(__WIN__)
#   define V_LOG(m)             \
    do { Log (("[MAIN:%05u] ", GetCurrentThreadId())); Log (m); Log (("\n")); } while (0)
#   define V_LOG2(m)            \
    do { Log2 (("[MAIN:%05u] ", GetCurrentThreadId())); Log2 (m); Log2 (("\n")); } while (0)
#   define V_LOGFLOW(m)         \
    do { LogFlow (("[MAIN:%05u] ", GetCurrentThreadId())); LogFlow (m); LogFlow (("\n")); } while (0)
#   define V_LOGFLOWMEMBER(m)   \
    do { LogFlow (("[MAIN:%05u] {%p} ", GetCurrentThreadId(), this)); LogFlow (m); LogFlow (("\n")); } while (0)
#   define V_ASSERTMSG(m, e)    \
    AssertMsg(e, m)
#   define V_ASSERTFAILED(m)    \
    AssertMsgFailed(m)
#else
#   define V_LOG(m)             \
    do { Log (("[MAIN:%RTnthrd] ", RTThreadNativeSelf())); Log (m); Log (("\n")); } while (0)
#   define V_LOG2(m)            \
    do { Log2 (("[MAIN:%RTnthrd] ", RTThreadNativeSelf())); Log2 (m); Log2 (("\n")); } while (0)
#   define V_LOGFLOW(m)         \
    do { LogFlow (("[MAIN:%RTnthrd] ", RTThreadNativeSelf())); LogFlow (m); LogFlow (("\n")); } while (0)
#   define V_LOGFLOWMEMBER(m)   \
    do { LogFlow (("[MAIN:%RTnthrd] {%p} ", RTThreadNativeSelf(), this)); LogFlow (m); LogFlow (("\n")); } while (0)
#   define V_ASSERTMSG(m, e)    \
    AssertMsg(e, m)
#   define V_ASSERTFAILED(m)    \
    AssertMsgFailed(m)
#endif
#   define LogFlowMember(m)     \
    do { LogFlow (("{%p} ", this)); LogFlow (m); } while (0)
#else // if !DEBUG
#   define V_DEBUG(m)           do {} while (0)
#   define V_LOG(m)             do {} while (0)
#   define V_LOG2(m)            do {} while (0)
#   define V_LOGFLOW(m)         do {} while (0)
#   define V_LOGFLOWMEMBER(m)   do {} while (0)
#   define V_ASSERTMSG(m, e)    do {} while (0)
#   define V_ASSERTFAILED(m)    do {} while (0)
#   define LogFlowMember(m)     do {} while (0)
#endif // !DEBUG

#endif // ____H_LOGGING
