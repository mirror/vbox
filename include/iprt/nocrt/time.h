/** @file
 * IPRT / No-CRT - Our minimal time.h.
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_time_h
#define IPRT_INCLUDED_nocrt_time_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>
#include <iprt/nocrt/sys/types.h>

#if defined(RT_OS_WINDOWS) && defined(_USE_32BIT_TIME_T) && ARCH_BITS == 32
typedef long time_t;
#else
typedef int64_t time_t;
#endif
#ifdef _MSC_VER
typedef int64_t __time64_t;
#endif

struct timespec
{
    time_t tv_sec;
    long   tv_nsec;
};

struct tm
{
    int tm_sec, tm_min, tm_hour, tm_mday, tm_mon, tm_year;
    int tm_wday, tm_yday, tm_isdst, tm_gmtoff;
    const char *tm_zone;
};

RT_C_DECLS_BEGIN

errno_t    RT_NOCRT(localtime_s)(struct tm *, const time_t *); /* The Microsoft version, not the C11 one. */
struct tm *RT_NOCRT(localtime_r)(const time_t *, struct tm *);

errno_t    RT_NOCRT(_localtime_s)(struct tm *, const time_t *);
struct tm *RT_NOCRT(_localtime_r)(const time_t *, struct tm *);

# if !defined(RT_WITHOUT_NOCRT_WRAPPERS) && !defined(RT_WITHOUT_NOCRT_WRAPPER_ALIASES)
#  define localtime_s       RT_NOCRT(localtime_s)
#  define localtime_r       RT_NOCRT(localtime_r)

#  define _localtime_s      RT_NOCRT(localtime_s)
#  define _localtime_r      RT_NOCRT(localtime_r)
# endif

RT_C_DECLS_END


#endif /* !IPRT_INCLUDED_nocrt_time_h */

