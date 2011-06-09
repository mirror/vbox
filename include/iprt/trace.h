/** @file
 * IPRT - Tracing.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___iprt_trace_h
#define ___iprt_trace_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/stdarg.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_trace      RTTrace - Tracing
 * @ingroup grp_rt
 *
 * The tracing facility is somewhat similar to a stripped down logger that
 * outputs to a circular buffer.  Part of the idea here is that it can the
 * overhead is much smaller and that it can be done without involving any
 * locking or other thing that could throw off timing.
 *
 * @{
 */


#ifdef DOXYGEN_RUNNING
# define RTTRACE_DISABLED
# define RTTRACE_ENABLED
#endif

/** @def RTTRACE_DISABLED
 * Use this compile time define to disable all tracing macros.  This trumps
 * RTTRACE_ENABLED.
 */

/** @def RTTRACE_ENABLED
 * Use this compile time define to enable tracing when not in debug mode
 */

/*
 * Determine whether tracing is enabled and forcefully normalize the indicators.
 */
#if (defined(DEBUG) || defined(RTTRACE_ENABLED)) && !defined(RTTRACE_DISABLED)
# undef  RTTRACE_DISABLED
# undef  RTTRACE_ENABLED
# define RTTRACE_ENABLED
#else
# undef  RTTRACE_DISABLED
# undef  RTTRACE_ENABLED
# define RTTRACE_DISABLED
#endif


/** @name RTTRACEBUF_FLAGS_XXX - RTTraceBufCarve and RTTraceBufCreate flags.
 * @{ */
/** Free the memory block on release using RTMemFree(). */
#define RTTRACEBUF_FLAGS_FREE_ME    RT_BIT_32(0)
/** Mask of the valid flags. */
#define RTTRACEBUF_FLAGS_MASK       UINT32_C(0x00000001)
/** @}  */


RTDECL(int)         RTTraceBufCreate(PRTTRACEBUF hTraceBuf, uint32_t cEntries, uint32_t cbEntry, uint32_t fFlags);
RTDECL(int)         RTTraceBufCarve(PRTTRACEBUF hTraceBuf, uint32_t cEntries, uint32_t cbEntry, uint32_t fFlags,
                                    void *pvBlock, size_t *pcbBlock);
RTDECL(uint32_t)    RTTraceBufRetain(RTTRACEBUF hTraceBuf);
RTDECL(uint32_t)    RTTraceBufRelease(RTTRACEBUF hTraceBuf);
RTDECL(int)         RTTraceBufDumpToLog(RTTRACEBUF hTraceBuf);
RTDECL(int)         RTTraceBufDumpToAssert(RTTRACEBUF hTraceBuf);

RTDECL(int)         RTTraceBufAddMsg(      RTTRACEBUF hTraceBuf, const char *pszMsg);
RTDECL(int)         RTTraceBufAddMsgF(     RTTRACEBUF hTraceBuf, const char *pszMsgFmt, ...);
RTDECL(int)         RTTraceBufAddMsgV(     RTTRACEBUF hTraceBuf, const char *pszMsgFmt, va_list va);
RTDECL(int)         RTTraceBufAddMsgEx(    RTTRACEBUF hTraceBuf, const char *pszMsg, size_t cbMaxMsg);

RTDECL(int)         RTTraceBufAddPos(      RTTRACEBUF hTraceBuf, RT_SRC_POS_DECL);
RTDECL(int)         RTTraceBufAddPosMsg(   RTTRACEBUF hTraceBuf, RT_SRC_POS_DECL, const char *pszMsg);
RTDECL(int)         RTTraceBufAddPosMsgEx( RTTRACEBUF hTraceBuf, RT_SRC_POS_DECL, const char *pszMsg, size_t cbMaxMsg);
RTDECL(int)         RTTraceBufAddPosMsgF(  RTTRACEBUF hTraceBuf, RT_SRC_POS_DECL, const char *pszMsgFmt, ...);
RTDECL(int)         RTTraceBufAddPosMsgV(  RTTRACEBUF hTraceBuf, RT_SRC_POS_DECL, const char *pszMsgFmt, va_list va);


RTDECL(int)         RTTraceSetDefaultBuf(RTTRACEBUF hTraceBuf);
RTDECL(RTTRACEBUF)  RTTraceGetDefaultBuf(void);


/** @def RTTRACE_BUF
 * The trace buffer used by the macros.
 */
#ifndef RTTRACE_BUF
# define RTTRACE_BUF        NULL
#endif

/**
 * Record the current source position.
 */
#ifdef RTTRACE_ENABLED
# define RTTRACE_POS()              do { RTTraceBufAddPos(RTTRACE_BUF, RT_SRC_POS); } while (0)
#else
# define RTTRACE_POS()              do { } while (0)
#endif


/** @} */

RT_C_DECLS_END

#endif


