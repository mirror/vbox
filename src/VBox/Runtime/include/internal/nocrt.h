/* $Id$ */
/** @file
 * IPRT - Internal header for miscellaneous global defs and types.
 */

/*
 * Copyright (C) 2009-2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_INTERNAL_nocrt_h
#define IPRT_INCLUDED_INTERNAL_nocrt_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include "internal/iprt.h"
#include <iprt/list.h>


/**
 * No-CRT thread data.
 */
typedef struct RTNOCRTTHREADDATA
{
    /** Used by kAllocType_Heap for DLL unload cleanup. */
    RTLISTNODE      ListEntry;
    /** How this structure was allocated. */
    enum
    {
        /** Invalid zero entry. */
        kAllocType_Invalid = 0,
        /** Embedded in RTTHREADINT.   */
        kAllocType_Embedded,
        /** Preallocated static array. */
        kAllocType_Static,
        /** It's on the heap. */
        kAllocType_Heap,
        /** Cleanup dummy. */
        kAllocType_CleanupDummy,
        /** End of valid values. */
        kAllocType_End
    }               enmAllocType;

    /** errno variable.   */
    int             iErrno;
    /** strtok internal variable. */
    char           *pszStrToken;

} RTNOCRTTHREADDATA;
/** Pointer to no-CRT per-thread data. */
typedef RTNOCRTTHREADDATA *PRTNOCRTTHREADDATA;


extern RTTLS volatile       g_iTlsRtNoCrtPerThread;
extern RTNOCRTTHREADDATA    g_RtNoCrtPerThreadDummy;

PRTNOCRTTHREADDATA rtNoCrtThreadDataGet(void);

#ifdef IN_RING3
void rtNoCrtFatalWriteBegin(const char *pchMsg, size_t cchMsg);
void rtNoCrtFatalWrite(const char *pchMsg, size_t cchMsg);
void rtNoCrtFatalWriteEnd(const char *pchMsg, size_t cchMsg);
void rtNoCrtFatalWriteStr(const char *pszMsg);
void rtNoCrtFatalWritePtr(void const *pv);
void rtNoCrtFatalWriteX64(uint64_t uValue);
void rtNoCrtFatalWriteX32(uint32_t uValue);
void rtNoCrtFatalWriteRc(int rc);
void rtNoCrtFatalWriteWinRc(uint32_t rc);

void rtNoCrtFatalMsg(const char *pchMsg, size_t cchMsg);
void rtNoCrtFatalMsgWithRc(const char *pchMsg, size_t cchMsg, int rc);
#endif


#endif /* !IPRT_INCLUDED_INTERNAL_nocrt_h */

