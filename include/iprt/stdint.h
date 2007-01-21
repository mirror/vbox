/** @file
 * InnoTek Portable Runtime - stdint.h wrapper (for backlevel compilers like MSC).
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
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
 *
 * --------------------------------------------------------------------
 *
 * This code is based on:
 *
 * Based on various FreeBSD 5.2 headers.
 *
 */

#ifndef __iprt_stdint_h__
#define __iprt_stdint_h__

#ifndef __STDC_CONSTANT_MACROS
# define __STDC_CONSTANT_MACROS
#endif
#ifndef __STDC_LIMIT_MACROS
# define __STDC_LIMIT_MACROS
#endif

#include <VBox/cdefs.h>

#if (!defined(__LINUX__) || !defined(__KERNEL__)) && !defined(_MSC_VER) && !defined(IPRT_NO_CRT)
# include <stdint.h>

#else

#if (!defined(__LINUX__) && !defined(__KERNEL__)) || defined(IPRT_NO_CRT) || defined(_MSC_VER) /** @todo remove _MSC_VER check (vcc8 merge) */
/* machine specific */
typedef signed char     __int8_t;
typedef unsigned char       __uint8_t;
typedef short           __int16_t;
typedef unsigned short      __uint16_t;
typedef int         __int32_t;
typedef unsigned int        __uint32_t;

# ifdef _MSC_VER
typedef _int64              __int64_t;
typedef unsigned _int64     __uint64_t;
# else
typedef long long       __int64_t;
typedef unsigned long long  __uint64_t;
# endif
#endif /* !linux kernel */

#if !defined(_WIN64) || defined(__i386__) || defined(__I386__) || defined(__LINUX__) /** @todo fix this, __x86__ should suffice if cdefs.h is included! */
typedef signed long             __intptr_t;
typedef unsigned long           __uintptr_t;
#else
typedef __int64_t               __intptr_t;
typedef __uint64_t              __uintptr_t;
#endif


/* the stuff we use */
#if (!defined(__LINUX__) && !defined(__KERNEL__)) || defined(IPRT_NO_CRT) || defined(_MSC_VER) /** @todo remove _MSC_VER check (vcc8 merge) */
#ifndef _INT8_T_DECLARED
typedef __int8_t        int8_t;
#define _INT8_T_DECLARED
#endif

#ifndef _INT16_T_DECLARED
typedef __int16_t       int16_t;
#define _INT16_T_DECLARED
#endif

#ifndef _INT32_T_DECLARED
typedef __int32_t       int32_t;
#define _INT32_T_DECLARED
#endif

#ifndef _INT64_T_DECLARED
typedef __int64_t       int64_t;
#define _INT64_T_DECLARED
#endif

#ifndef _UINT8_T_DECLARED
typedef __uint8_t       uint8_t;
#define _UINT8_T_DECLARED
#endif

#ifndef _UINT16_T_DECLARED
typedef __uint16_t      uint16_t;
#define _UINT16_T_DECLARED
#endif

#ifndef _UINT32_T_DECLARED
typedef __uint32_t      uint32_t;
#define _UINT32_T_DECLARED
#endif

#ifndef _UINT64_T_DECLARED
typedef __uint64_t      uint64_t;
#define _UINT64_T_DECLARED
#endif

#endif /* !linux kernel || no-crt */

#ifndef _MSC_VER
#ifndef _INTPTR_T_DECLARED
typedef __intptr_t              intptr_t;
typedef __uintptr_t             uintptr_t;
#define _INTPTR_T_DECLARED
#endif
#endif /* !_MSC_VER */

#if !defined(__cplusplus) || defined(__STDC_CONSTANT_MACROS)

#define INT8_C(c)       (c)
#define INT16_C(c)      (c)
#define INT32_C(c)      (c)
#define INT64_C(c)      (c ## LL)

#define UINT8_C(c)      (c)
#define UINT16_C(c)     (c)
#define UINT32_C(c)     (c ## U)
#define UINT64_C(c)     (c ## ULL)

#define INTMAX_C(c)     (c ## LL)
#define UINTMAX_C(c)        (c ## ULL)

#define INT8_MIN    (-0x7f-1)
#define INT16_MIN   (-0x7fff-1)
#define INT32_MIN   (-0x7fffffff-1)
#define INT64_MIN   (-0x7fffffffffffffffLL-1)

#define INT8_MAX    0x7f
#define INT16_MAX   0x7fff
#define INT32_MAX   0x7fffffff
#define INT64_MAX   0x7fffffffffffffffLL

#define UINT8_MAX   0xff
#define UINT16_MAX  0xffff
#define UINT32_MAX  0xffffffffU
#define UINT64_MAX  0xffffffffffffffffULL

#endif /* !C++ || __STDC_CONSTANT_MACROS */

#endif /* _MSC_VER */

#endif

