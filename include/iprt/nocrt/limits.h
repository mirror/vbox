/** @file
 * innotek Portable Runtime / No-CRT - Our own limits header.
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

#ifndef ___iprt_nocrt_limits_h
#define ___iprt_nocrt_limits_h

#include <iprt/types.h>

#define CHAR_BIT        8
#define SCHAR_MAX       0x7f
#define SCHAR_MIN       (-0x7f - 1)
#define UCHAR_MAX       0xff
#if 1 /* ASSUMES: signed char */
# define CHAR_MAX       SCHAR_MAX
# define CHAR_MIN       SCHAR_MIN
#else
# define CHAR_MAX       UCHAR_MAX
# define CHAR_MIN       0
#endif

#define WORD_BIT        16
#define USHRT_MAX       0xffff
#define SHRT_MAX        0x7fff
#define SHRT_MIN        (-0x7fff - 1)

/* ASSUMES 32-bit int */
#define UINT_MAX        0xffffffffU
#define INT_MAX         0x7fffffff
#define INT_MIN         (-0x7fffffff - 1)

#if defined(__X86__) || defined(RT_OS_WINDOWS)
# define LONG_BIT       32
# define ULONG_MAX      0xffffffffU
# define LONG_MAX       0x7fffffff
# define LONG_MIN       (-0x7fffffff - 1)
#elif defined(__AMD64__)
# define LONG_BIT       64
# define ULONG_MAX      UINT64_C(0xffffffffffffffff)
# define LONG_MAX       INT64_C(0x7fffffffffffffff)
# define LONG_MIN       (INT64_C(-0x7fffffffffffffff) - 1)
#else
# error "huh?"
#endif

#define LLONG_BIT       64
#define ULLONG_MAX      UINT64_C(0xffffffffffffffff)
#define LLONG_MAX       INT64_C(0x7fffffffffffffff)
#define LLONG_MIN       (INT64_C(-0x7fffffffffffffff) - 1)

#if ARCH_BITS == 32
# define SIZE_T_MAX     0xffffffffU
# define SSIZE_MAX      0x7fffffff
#elif ARCH_BITS == 64
# define SIZE_T_MAX     UINT64_C(0xffffffffffffffff)
# define SSIZE_MAX      INT64_C(0x7fffffffffffffff)
#else
# error "huh?"
#endif

/*#define OFF_MAX         __OFF_MAX
#define OFF_MIN         __OFF_MIN*/

#endif

