/** @file
 * IPRT / No-CRT - Our minimal float.h.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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

#ifndef IPRT_INCLUDED_nocrt_float_h
#define IPRT_INCLUDED_nocrt_float_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/types.h>

/*
 * Common.
 */
#define FLT_RADIX       2


/*
 * float
 */
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64)

# define FLT_MAX        (3.40282347E+38F)
# define FLT_MIN        (1.17549435E-38F)
# define FLT_MAX_EXP    (128)
# define FLT_MIN_EXP    (-125)
# define FLT_EPSILON    (1.192092896E-07F)

#endif

/*
 * double
 */
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64)

# define DBL_MAX        (1.7976931348623157E+308)
# define DBL_MIN        (2.2250738585072014E-308)
# define DBL_MAX_EXP    (1024)
# define DBL_MIN_EXP    (-1021)
# define DBL_EPSILON    (2.2204460492503131E-16)

#endif

/*
 * long double
 */
#if (defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)) && defined(RT_OS_WINDOWS)
# define LDBL_MAX        DBL_MAX
# define LDBL_MIN        DBL_MIN
# define LDBL_MAX_EXP    DBL_MAX_EXP
# define LDBL_MIN_EXP    DBL_MIN_EXP
# define LDBL_EPSIOLON   DBL_EPSIOLON
#endif


#endif /* !IPRT_INCLUDED_nocrt_float_h */

