/** @file
 * IPRT / No-CRT - Our own sys/types header.
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

#ifndef IPRT_INCLUDED_nocrt_sys_types_h
#define IPRT_INCLUDED_nocrt_sys_types_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#if defined(IPRT_INCLUDED_types_h) && !defined(IPRT_COMPLETED_types_h)
# error "Can't include nocrt/sys/types.h from iprt/types.h"
#endif

#include <iprt/types.h>

/* #if !defined(MSC-define) && !defined(GNU/LINUX-define) */
#if !defined(_DEV_T_DEFINED) && !defined(__dev_t_defined)
typedef RTDEV       dev_t;
#endif
#if !defined(_UCRT_RESTORE_CLANG_WARNINGS) /* MSC specific type */
typedef int         errno_t;
#endif
#if !defined(_INO_T_DEFINED) && !defined(__ino_t_defined)
typedef RTINODE     ino_t;
#endif
#if !defined(_OFF_T_DEFINED) && !defined(__off_t_defined)
typedef RTFOFF      off_t;
#endif
#if !defined(_PID_T_DEFINED) && !defined(__pid_t_defined)
typedef RTPROCESS   pid_t;
#endif

#endif /* !IPRT_INCLUDED_nocrt_sys_types_h */

