/* $Id$ */

/** @file
 *
 * VirtualBox COM: logging macros and function definitions
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ____H_LOGGING
#define ____H_LOGGING

/** @def LOG_GROUP_MAIN_OVERRIDE
 *  Define this macro to point to the desired log group before including
 *  the |Logging.h| header if you want to use a group other than LOG_GROUP_MAIN
 *  for logging from within Main source files.
 *
 *  @example #define LOG_GROUP_MAIN_OVERRIDE LOG_GROUP_HGCM
 */

/*
 *  We might be including the VBox logging subsystem before
 *  including this header file, so reset the logging group.
 */
#ifdef LOG_GROUP
# undef LOG_GROUP
#endif
#ifdef LOG_GROUP_MAIN_OVERRIDE
# define LOG_GROUP LOG_GROUP_MAIN_OVERRIDE
#else
# define LOG_GROUP LOG_GROUP_MAIN
#endif

/* Ensure log macros are enabled if release logging is requested */
#if defined (VBOX_MAIN_RELEASE_LOG) && !defined (DEBUG)
# ifndef LOG_ENABLED
#  define LOG_ENABLED
# endif
#endif

#include <VBox/log.h>
#include <iprt/assert.h>

/** @def MyLogIt
 * Copy of LogIt that works even when logging is completely disabled (e.g. in
 * release builds) and doesn't interefere with the default release logger
 * instance (which is already in use by the VM process).
 *
 * @warning Logging using MyLog* is intended only as a temporary mean to debug
 *          release builds (e.g. in case if the error is not reproducible with
 *          the debug builds)! Any MyLog* usage must be removed from the sources
 *          after the error has been fixed.
 */
#if defined(RT_ARCH_AMD64) || defined(LOG_USE_C99)
# define _MyLogRemoveParentheseis(...)               __VA_ARGS__
# define _MyLogIt(pvInst, fFlags, iGroup, ...)       RTLogLoggerEx((PRTLOGGER)pvInst, fFlags, iGroup, __VA_ARGS__)
# define MyLogIt(pvInst, fFlags, iGroup, fmtargs)    _MyLogIt(pvInst, fFlags, iGroup, _MyLogRemoveParentheseis fmtargs)
#else
# define MyLogIt(pvInst, fFlags, iGroup, fmtargs) \
    do \
    { \
        register PRTLOGGER LogIt_pLogger = (PRTLOGGER)(pvInst) ? (PRTLOGGER)(pvInst) : RTLogDefaultInstance(); \
        if (LogIt_pLogger) \
        { \
            register unsigned LogIt_fFlags = LogIt_pLogger->afGroups[(unsigned)(iGroup) < LogIt_pLogger->cGroups ? (unsigned)(iGroup) : 0]; \
            if ((LogIt_fFlags & ((fFlags) | RTLOGGRPFLAGS_ENABLED)) == ((fFlags) | RTLOGGRPFLAGS_ENABLED)) \
                LogIt_pLogger->pfnLogger fmtargs; \
        } \
    } while (0)
#endif

/** @def MyLog
 * Equivalent to LogFlow but uses MyLogIt instead of LogIt.
 *
 * @warning Logging using MyLog* is intended only as a temporary mean to debug
 *          release builds (e.g. in case if the error is not reproducible with
 *          the debug builds)! Any MyLog* usage must be removed from the sources
 *          after the error has been fixed.
 */
#define MyLog(a)            MyLogIt(LOG_INSTANCE, RTLOGGRPFLAGS_FLOW, LOG_GROUP, a)

#ifdef VBOX_WITH_VRDP_MEMLEAK_DETECTOR
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/critsect.h>
#include <iprt/mem.h>

void *MLDMemAllocDbg (size_t cb, bool fTmp, bool fZero, const char *pszCaller, int iLine);
void *MLDMemReallocDbg (void *pv, size_t cb, const char *pszCaller, int iLine);
void MLDMemFreeDbg (void *pv, bool fTmp);

void MLDMemInit (const char *pszPrefix);
void MLDMemUninit (void);

void MLDMemDump (void);

#define MLDMemAlloc(__cb)          MLDMemAllocDbg   (__cb, false /* fTmp */, false /* fZero */, __FILE__,  __LINE__)
#define MLDMemAllocZ(__cb)         MLDMemAllocDbg   (__cb, false /* fTmp */, true /* fZero */,  __FILE__,  __LINE__)
#define MLDMemTmpAlloc(__cb)       MLDMemAllocDbg   (__cb, true /* fTmp */,  false /* fZero */, __FILE__,  __LINE__)
#define MLDMemTmpAllocZ(__cb)      MLDMemAllocDbg   (__cb, true /* fTmp */,  true /* fZero */,  __FILE__,  __LINE__)
#define MLDMemRealloc(__pv, __cb)  MLDMemReallocDbg (__pv, __cb, __FILE__,  __LINE__)
#define MLDMemFree(__pv)           MLDMemFreeDbg    (__pv, false /* fTmp */)
#define MLDMemTmpFree(__pv)        MLDMemFreeDbg    (__pv, true /* fTmp */)

#undef RTMemAlloc
#define RTMemAlloc     MLDMemAlloc
#undef RTMemAllocZ
#define RTMemAllocZ    MLDMemAllocZ
#undef RTMemTmpAlloc
#define RTMemTmpAlloc  MLDMemTmpAlloc
#undef RTMemTmpAllocZ
#define RTMemTmpAllocZ MLDMemTmpAllocZ
#undef RTMemRealloc
#define RTMemRealloc   MLDMemRealloc
#undef RTMemFree
#define RTMemFree      MLDMemFree
#undef RTMemTmpFree
#define RTMemTmpFree   MLDMemTmpFree
#endif /* VBOX_WITH_MLD_MEMLEAK_DETECTOR */

#endif // ____H_LOGGING
/* vi: set tabstop=4 shiftwidth=4 expandtab: */
