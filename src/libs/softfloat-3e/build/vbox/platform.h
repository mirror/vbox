/* $Id$ */
/** @file
 * Platform Header for all VirtualBox targets.
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
 */

#ifndef VBOX_INCLUDED_SRC_vbox_platform_h
#define VBOX_INCLUDED_SRC_vbox_platform_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

/* For RT_LITTLE_ENDIAN, RT_ARCH_XXX and more: */
#include <iprt/cdefs.h>

/* Build config: */
#define SOFTFLOAT_FAST_INT64            /**< We use functions guarded by this, so must be defined regardless of truthiness. */
#define SOFTFLOAT_ROUND_ODD             /** @todo Skip this? */

/* IPRT should detect endianness correctly: */
#ifdef RT_LITTLE_ENDIAN
# define LITTLEENDIAN 1
#endif

/* Compiler/host configuration bits: */
#define SOFTFLOAT_FAST_DIV32TO16
#if ARCH_BITS > 32 || defined(RT_ARCH_X86)
# define SOFTFLOAT_FAST_DIV64TO32
#endif

/* See DECLINLINE for guidance: */
#ifdef __GNUC__
# define INLINE                         static __inline__
#elif defined(__cplusplus)
# define INLINE                         static inline
#elif defined(_MSC_VER)
# define INLINE                         static _inline
#else
# error "Port me!"
#endif

/* Generic IPRT asm.h based optimizations: */
#if !defined(__GNUC__)
# include <iprt/asm.h>
# define softfloat_countLeadingZeros16 softfloat_iprt_countLeadingZeros16
DECLINLINE(uint_fast8_t) softfloat_iprt_countLeadingZeros16(uint16_t uVal)
{
    return 16 - ASMBitLastSetU16(uVal);
}
# define softfloat_countLeadingZeros32 softfloat_iprt_countLeadingZeros32
DECLINLINE(uint_fast8_t) softfloat_iprt_countLeadingZeros32(uint32_t uVal)
{
    return 32 - ASMBitLastSetU32(uVal);
}
# define softfloat_countLeadingZeros64 softfloat_iprt_countLeadingZeros64
DECLINLINE(uint_fast8_t) softfloat_iprt_countLeadingZeros64(uint64_t uVal)
{
    return 64 - ASMBitLastSetU64(uVal);
}
#endif

/* Include GCC optimizations: */
#ifdef __GNUC__
# ifndef softfloat_countLeadingZeros16
#  define SOFTFLOAT_BUILTIN_CLZ         1
# endif
# if ARCH_BITS > 32
#  define SOFTFLOAT_INTRINSIC_INT128    1
# endif
# include "opts-GCC.h"
#endif

/* We've eliminated the global variables and need no TLS variable tricks. */
#ifndef THREAD_LOCAL
# if 1
#  define THREAD_LOCAL
# else
#  ifdef _MSC_VER
#   define THREAD_LOCAL                 __declspec(thread)
#  else
#   define THREAD_LOCAL                 __thread
#  endif
# endif
#endif

#endif /* !VBOX_INCLUDED_SRC_vbox_platform_h */
