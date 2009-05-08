/** @file
 * IPRT - Types.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_types_h
#define ___iprt_types_h

#include <iprt/cdefs.h>
#include <iprt/stdint.h>

/*
 * Include standard C types.
 */
#ifndef IPRT_NO_CRT

# if defined(RT_OS_DARWIN) && defined(KERNEL)
    /*
     * Kludge for the darwin kernel:
     *  stddef.h is missing IIRC.
     */
#  ifndef _PTRDIFF_T
#  define _PTRDIFF_T
    typedef __darwin_ptrdiff_t ptrdiff_t;
#  endif
#  include <sys/types.h>

# elif defined(RT_OS_FREEBSD) && defined(_KERNEL)
    /*
     * Kludge for the FreeBSD kernel:
     *  stddef.h and sys/types.h have slightly different offsetof definitions
     *  when compiling in kernel mode. This is just to make GCC shut up.
     */
#  ifndef _STDDEF_H_
#   undef offsetof
#  endif
#  include <stddef.h>
#  ifndef _SYS_TYPES_H_
#   undef offsetof
#  endif
#  include <sys/types.h>
#  ifndef offsetof
#   error "offsetof is not defined..."
#  endif

# elif defined(RT_OS_LINUX) && defined(__KERNEL__)
    /*
     * Kludge for the linux kernel:
     *   1. sys/types.h doesn't mix with the kernel.
     *   2. Starting with 2.6.19, linux/types.h typedefs bool and linux/stddef.h
     *      declares false and true as enum values.
     *   3. Starting with 2.6.24, linux/types.h typedefs uintptr_t.
     * We work around these issues here and nowhere else.
     */
#  include <stddef.h>
#  if defined(__cplusplus)
    typedef bool _Bool;
#  endif
#  define bool linux_bool
#  define true linux_true
#  define false linux_false
#  define uintptr_t linux_uintptr_t
#  include <linux/autoconf.h>
#  include <linux/types.h>
#  include <linux/stddef.h>
#  undef uintptr_t
#  undef false
#  undef true
#  undef bool

# else
#  include <stddef.h>
#  include <sys/types.h>
# endif

/* Define any types missing from sys/types.h on windows. */
# ifdef _MSC_VER
#  undef ssize_t
   typedef intptr_t ssize_t;
# endif

#else /* no crt */
# include <iprt/nocrt/compiler/compiler.h>
#endif /* no crt */



/** @defgroup grp_rt_types  IPRT Base Types
 * @{
 */

/* define wchar_t, we don't wanna include all the wcsstuff to get this. */
#ifdef _MSC_VER
# ifndef _WCHAR_T_DEFINED
  typedef unsigned short wchar_t;
# define _WCHAR_T_DEFINED
# endif
#endif
#ifdef __GNUC__
/** @todo wchar_t on GNUC */
#endif

/*
 * C doesn't have bool.
 */
#ifndef __cplusplus
# if defined(__GNUC__)
#  if defined(RT_OS_LINUX) && __GNUC__ < 3
typedef uint8_t bool;
#  else
#   if defined(RT_OS_DARWIN) && defined(_STDBOOL_H)
#    undef bool
#   endif
typedef _Bool bool;
#  endif
# else
typedef unsigned char bool;
# endif
# ifndef true
#  define true  (1)
# endif
# ifndef false
#  define false (0)
# endif
#endif

/**
 * 128-bit unsigned integer.
 */
#if defined(__GNUC__) && defined(RT_ARCH_AMD64)
typedef __uint128_t uint128_t;
#else
typedef struct uint128_s
{
    uint64_t    Lo;
    uint64_t    Hi;
} uint128_t;
#endif


/**
 * 128-bit signed integer.
 */
#if defined(__GNUC__) && defined(RT_ARCH_AMD64)
typedef __int128_t int128_t;
#else
typedef struct int128_s
{
    uint64_t    lo;
    int64_t     hi;
} int128_t;
#endif


/**
 * 16-bit unsigned integer union.
 */
typedef union RTUINT16U
{
    /** natural view. */
    uint16_t    u;

    /** 16-bit view. */
    uint16_t    au16[1];
    /** 8-bit view. */
    uint8_t     au8[4];
    /** 16-bit hi/lo view. */
    struct
    {
        uint16_t    Lo;
        uint16_t    Hi;
    } s;
} RTUINT16U;
/** Pointer to a 16-bit unsigned integer union. */
typedef RTUINT16U *PRTUINT16U;
/** Pointer to a const 32-bit unsigned integer union. */
typedef const RTUINT16U *PCRTUINT16U;


/**
 * 32-bit unsigned integer union.
 */
typedef union RTUINT32U
{
    /** natural view. */
    uint32_t    u;
    /** Hi/Low view. */
    struct
    {
        uint16_t    Lo;
        uint16_t    Hi;
    } s;
    /** Word view. */
    struct
    {
        uint16_t    w0;
        uint16_t    w1;
    } Words;

    /** 32-bit view. */
    uint32_t    au32[1];
    /** 16-bit view. */
    uint16_t    au16[2];
    /** 8-bit view. */
    uint8_t     au8[4];
} RTUINT32U;
/** Pointer to a 32-bit unsigned integer union. */
typedef RTUINT32U *PRTUINT32U;
/** Pointer to a const 32-bit unsigned integer union. */
typedef const RTUINT32U *PCRTUINT32U;


/**
 * 64-bit unsigned integer union.
 */
typedef union RTUINT64U
{
    /** Natural view. */
    uint64_t    u;
    /** Hi/Low view. */
    struct
    {
        uint32_t    Lo;
        uint32_t    Hi;
    } s;
    /** Double-Word view. */
    struct
    {
        uint32_t    dw0;
        uint32_t    dw1;
    } DWords;
    /** Word view. */
    struct
    {
        uint16_t    w0;
        uint16_t    w1;
        uint16_t    w2;
        uint16_t    w3;
    } Words;

    /** 64-bit view. */
    uint64_t    au64[1];
    /** 32-bit view. */
    uint32_t    au32[2];
    /** 16-bit view. */
    uint16_t    au16[4];
    /** 8-bit view. */
    uint8_t     au8[8];
} RTUINT64U;
/** Pointer to a 64-bit unsigned integer union. */
typedef RTUINT64U *PRTUINT64U;
/** Pointer to a const 64-bit unsigned integer union. */
typedef const RTUINT64U *PCRTUINT64U;


/**
 * 128-bit unsigned integer union.
 */
typedef union RTUINT128U
{
    /** Natural view.
     * WARNING! This member depends on the compiler supporting 128-bit stuff. */
    uint128_t   u;
    /** Hi/Low view. */
    struct
    {
        uint64_t    Lo;
        uint64_t    Hi;
    } s;
    /** Quad-Word view. */
    struct
    {
        uint64_t    qw0;
        uint64_t    qw1;
    } QWords;
    /** Double-Word view. */
    struct
    {
        uint32_t    dw0;
        uint32_t    dw1;
        uint32_t    dw2;
        uint32_t    dw3;
    } DWords;
    /** Word view. */
    struct
    {
        uint16_t    w0;
        uint16_t    w1;
        uint16_t    w2;
        uint16_t    w3;
        uint16_t    w4;
        uint16_t    w5;
        uint16_t    w6;
        uint16_t    w7;
    } Words;

    /** 64-bit view. */
    uint64_t    au64[2];
    /** 32-bit view. */
    uint32_t    au32[4];
    /** 16-bit view. */
    uint16_t    au16[8];
    /** 8-bit view. */
    uint8_t     au8[16];
} RTUINT128U;
/** Pointer to a 64-bit unsigned integer union. */
typedef RTUINT128U *PRTUINT128U;
/** Pointer to a const 64-bit unsigned integer union. */
typedef const RTUINT128U *PCRTUINT128U;


/** Generic function type.
 * @see PFNRT
 */
typedef DECLCALLBACK(void) FNRT(void);

/** Generic function pointer.
 * With -pedantic, gcc-4 complains when casting a function to a data object, for example
 *
 * @code
 *    void foo(void)
 *    {
 *    }
 *
 *    void *bar = (void *)foo;
 * @endcode
 *
 * The compiler would warn with "ISO C++ forbids casting between pointer-to-function and
 * pointer-to-object". The purpose of this warning is not to bother the programmer but to
 * point out that he is probably doing something dangerous, assigning a pointer to executable
 * code to a data object.
 */
typedef FNRT *PFNRT;


/** @defgroup grp_rt_types_both  Common Guest and Host Context Basic Types
 * @ingroup grp_rt_types
 * @{
 */

/** Signed integer which can contain both GC and HC pointers. */
#if (HC_ARCH_BITS == 32 && GC_ARCH_BITS == 32)
typedef int32_t         RTINTPTR;
#elif (HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64)
typedef int64_t         RTINTPTR;
#else
#  error Unsupported HC_ARCH_BITS and/or GC_ARCH_BITS values.
#endif
/** Pointer to signed integer which can contain both GC and HC pointers. */
typedef RTINTPTR       *PRTINTPTR;
/** Pointer const to signed integer which can contain both GC and HC pointers. */
typedef const RTINTPTR *PCRTINTPTR;

/** Unsigned integer which can contain both GC and HC pointers. */
#if (HC_ARCH_BITS == 32 && GC_ARCH_BITS == 32)
typedef uint32_t        RTUINTPTR;
#elif (HC_ARCH_BITS == 64 || GC_ARCH_BITS == 64)
typedef uint64_t        RTUINTPTR;
#else
#  error Unsupported HC_ARCH_BITS and/or GC_ARCH_BITS values.
#endif
/** Pointer to unsigned integer which can contain both GC and HC pointers. */
typedef RTUINTPTR      *PRTUINTPTR;
/** Pointer const to unsigned integer which can contain both GC and HC pointers. */
typedef const RTUINTPTR *PCRTUINTPTR;

/** Signed integer. */
typedef int32_t         RTINT;
/** Pointer to signed integer. */
typedef RTINT          *PRTINT;
/** Pointer to const signed integer. */
typedef const RTINT    *PCRTINT;

/** Unsigned integer. */
typedef uint32_t        RTUINT;
/** Pointer to unsigned integer. */
typedef RTUINT         *PRTUINT;
/** Pointer to const unsigned integer. */
typedef const RTUINT   *PCRTUINT;

/** A file offset / size (off_t). */
typedef int64_t         RTFOFF;
/** Pointer to a file offset / size. */
typedef RTFOFF         *PRTFOFF;
/** The max value for RTFOFF. */
#define RTFOFF_MAX      INT64_MAX
/** The min value for RTFOFF. */
#define RTFOFF_MIN      INT64_MIN

/** File mode (see iprt/fs.h). */
typedef uint32_t        RTFMODE;
/** Pointer to file mode. */
typedef RTFMODE        *PRTFMODE;

/** Device unix number. */
typedef uint32_t        RTDEV;
/** Pointer to a device unix number. */
typedef RTDEV          *PRTDEV;

/** i-node number. */
typedef uint64_t        RTINODE;
/** Pointer to a i-node number. */
typedef RTINODE        *PRTINODE;

/** User id. */
typedef uint32_t        RTUID;
/** Pointer to a user id. */
typedef RTUID          *PRTUID;
/** NIL user id.
 * @todo check this for portability! */
#define NIL_RTUID       (~(RTUID)0);

/** Group id. */
typedef uint32_t        RTGID;
/** Pointer to a group id. */
typedef RTGID          *PRTGID;
/** NIL group id.
 * @todo check this for portability! */
#define NIL_RTGID       (~(RTGID)0);

/** I/O Port. */
typedef uint16_t        RTIOPORT;
/** Pointer to I/O Port. */
typedef RTIOPORT       *PRTIOPORT;
/** Pointer to const I/O Port. */
typedef const RTIOPORT *PCRTIOPORT;

/** Selector. */
typedef uint16_t        RTSEL;
/** Pointer to selector. */
typedef RTSEL          *PRTSEL;
/** Pointer to const selector. */
typedef const RTSEL    *PCRTSEL;
/** Max selector value. */
#define RTSEL_MAX       UINT16_MAX

/** Far 16-bit pointer. */
#pragma pack(1)
typedef struct RTFAR16
{
    uint16_t        off;
    RTSEL           sel;
} RTFAR16;
#pragma pack()
/** Pointer to Far 16-bit pointer. */
typedef RTFAR16 *PRTFAR16;
/** Pointer to const Far 16-bit pointer. */
typedef const RTFAR16 *PCRTFAR16;

/** Far 32-bit pointer. */
#pragma pack(1)
typedef struct RTFAR32
{
    uint32_t        off;
    RTSEL           sel;
} RTFAR32;
#pragma pack()
/** Pointer to Far 32-bit pointer. */
typedef RTFAR32 *PRTFAR32;
/** Pointer to const Far 32-bit pointer. */
typedef const RTFAR32 *PCRTFAR32;

/** Far 64-bit pointer. */
#pragma pack(1)
typedef struct RTFAR64
{
    uint64_t        off;
    RTSEL           sel;
} RTFAR64;
#pragma pack()
/** Pointer to Far 64-bit pointer. */
typedef RTFAR64 *PRTFAR64;
/** Pointer to const Far 64-bit pointer. */
typedef const RTFAR64 *PCRTFAR64;

/** @} */


/** @defgroup grp_rt_types_hc  Host Context Basic Types
 * @ingroup grp_rt_types
 * @{
 */

/** HC Natural signed integer.
 * @deprecated silly type. */
typedef int32_t         RTHCINT;
/** Pointer to HC Natural signed integer.
 * @deprecated silly type. */
typedef RTHCINT        *PRTHCINT;
/** Pointer to const HC Natural signed integer.
 * @deprecated silly type. */
typedef const RTHCINT  *PCRTHCINT;

/** HC Natural unsigned integer.
 * @deprecated silly type. */
typedef uint32_t        RTHCUINT;
/** Pointer to HC Natural unsigned integer.
 * @deprecated silly type. */
typedef RTHCUINT       *PRTHCUINT;
/** Pointer to const HC Natural unsigned integer.
 * @deprecated silly type. */
typedef const RTHCUINT *PCRTHCUINT;


/** Signed integer which can contain a HC pointer. */
#if HC_ARCH_BITS == 32
typedef int32_t         RTHCINTPTR;
#elif HC_ARCH_BITS == 64
typedef int64_t         RTHCINTPTR;
#else
#  error Unsupported HC_ARCH_BITS value.
#endif
/** Pointer to signed integer which can contain a HC pointer. */
typedef RTHCINTPTR     *PRTHCINTPTR;
/** Pointer to const signed integer which can contain a HC pointer. */
typedef const RTHCINTPTR *PCRTHCINTPTR;

/** Signed integer which can contain a HC ring-3 pointer. */
#if R3_ARCH_BITS == 32
typedef int32_t         RTR3INTPTR;
#elif R3_ARCH_BITS == 64
typedef int64_t         RTR3INTPTR;
#else
#  error Unsupported R3_ARCH_BITS value.
#endif
/** Pointer to signed integer which can contain a HC ring-3 pointer. */
typedef RTR3INTPTR     *PRTR3INTPTR;
/** Pointer to const signed integer which can contain a HC ring-3 pointer. */
typedef const RTR3INTPTR *PCRTR3INTPTR;

/** Signed integer which can contain a HC ring-0 pointer. */
#if R0_ARCH_BITS == 32
typedef int32_t         RTR0INTPTR;
#elif R0_ARCH_BITS == 64
typedef int64_t         RTR0INTPTR;
#else
#  error Unsupported R0_ARCH_BITS value.
#endif
/** Pointer to signed integer which can contain a HC ring-0 pointer. */
typedef RTR0INTPTR     *PRTR0INTPTR;
/** Pointer to const signed integer which can contain a HC ring-0 pointer. */
typedef const RTR0INTPTR *PCRTR0INTPTR;


/** Unsigned integer which can contain a HC pointer. */
#if HC_ARCH_BITS == 32
typedef uint32_t        RTHCUINTPTR;
#elif HC_ARCH_BITS == 64
typedef uint64_t        RTHCUINTPTR;
#else
#  error Unsupported HC_ARCH_BITS value.
#endif
/** Pointer to unsigned integer which can contain a HC pointer. */
typedef RTHCUINTPTR    *PRTHCUINTPTR;
/** Pointer to unsigned integer which can contain a HC pointer. */
typedef const RTHCUINTPTR *PCRTHCUINTPTR;

/** Unsigned integer which can contain a HC ring-3 pointer. */
#if R3_ARCH_BITS == 32
typedef uint32_t        RTR3UINTPTR;
#elif R3_ARCH_BITS == 64
typedef uint64_t        RTR3UINTPTR;
#else
#  error Unsupported R3_ARCH_BITS value.
#endif
/** Pointer to unsigned integer which can contain a HC ring-3 pointer. */
typedef RTR3UINTPTR    *PRTR3UINTPTR;
/** Pointer to unsigned integer which can contain a HC ring-3 pointer. */
typedef const RTR3UINTPTR *PCRTR3UINTPTR;

/** Unsigned integer which can contain a HC ring-0 pointer. */
#if R0_ARCH_BITS == 32
typedef uint32_t        RTR0UINTPTR;
#elif R0_ARCH_BITS == 64
typedef uint64_t        RTR0UINTPTR;
#else
#  error Unsupported R0_ARCH_BITS value.
#endif
/** Pointer to unsigned integer which can contain a HC ring-0 pointer. */
typedef RTR0UINTPTR    *PRTR0UINTPTR;
/** Pointer to unsigned integer which can contain a HC ring-0 pointer. */
typedef const RTR0UINTPTR *PCRTR0UINTPTR;


/** Host Physical Memory Address. */
typedef uint64_t        RTHCPHYS;
/** Pointer to Host Physical Memory Address. */
typedef RTHCPHYS       *PRTHCPHYS;
/** Pointer to const Host Physical Memory Address. */
typedef const RTHCPHYS *PCRTHCPHYS;
/** @def NIL_RTHCPHYS
 * NIL HC Physical Address.
 * NIL_RTHCPHYS is used to signal an invalid physical address, similar
 * to the NULL pointer.
 */
#define NIL_RTHCPHYS     (~(RTHCPHYS)0)


/** HC pointer. */
#ifndef IN_RC
typedef void *          RTHCPTR;
#else
typedef RTHCUINTPTR     RTHCPTR;
#endif
/** Pointer to HC pointer. */
typedef RTHCPTR        *PRTHCPTR;
/** Pointer to const HC pointer. */
typedef const RTHCPTR  *PCRTHCPTR;
/** @def NIL_RTHCPTR
 * NIL HC pointer.
 */
#define NIL_RTHCPTR     ((RTHCPTR)0)

/** HC ring-3 pointer. */
#ifdef  IN_RING3
typedef void *          RTR3PTR;
#else
typedef RTR3UINTPTR     RTR3PTR;
#endif
/** Pointer to HC ring-3 pointer. */
typedef RTR3PTR        *PRTR3PTR;
/** Pointer to const HC ring-3 pointer. */
typedef const RTR3PTR  *PCRTR3PTR;
/** @def NIL_RTR3PTR
 * NIL HC ring-3 pointer.
 */
#define NIL_RTR3PTR     ((RTR3PTR)0)

/** HC ring-0 pointer. */
#ifdef  IN_RING0
typedef void *          RTR0PTR;
#else
typedef RTR0UINTPTR     RTR0PTR;
#endif
/** Pointer to HC ring-0 pointer. */
typedef RTR0PTR        *PRTR0PTR;
/** Pointer to const HC ring-0 pointer. */
typedef const RTR0PTR  *PCRTR0PTR;
/** @def NIL_RTR0PTR
 * NIL HC ring-0 pointer.
 */
#define NIL_RTR0PTR     ((RTR0PTR)0)


/** Unsigned integer register in the host context. */
#if HC_ARCH_BITS == 32
typedef uint32_t            RTHCUINTREG;
#elif HC_ARCH_BITS == 64
typedef uint64_t            RTHCUINTREG;
#else
# error "Unsupported HC_ARCH_BITS!"
#endif
/** Pointer to an unsigned integer register in the host context. */
typedef RTHCUINTREG        *PRTHCUINTREG;
/** Pointer to a const unsigned integer register in the host context. */
typedef const RTHCUINTREG  *PCRTHCUINTREG;

/** Unsigned integer register in the host ring-3 context. */
#if R3_ARCH_BITS == 32
typedef uint32_t            RTR3UINTREG;
#elif R3_ARCH_BITS == 64
typedef uint64_t            RTR3UINTREG;
#else
# error "Unsupported R3_ARCH_BITS!"
#endif
/** Pointer to an unsigned integer register in the host ring-3 context. */
typedef RTR3UINTREG        *PRTR3UINTREG;
/** Pointer to a const unsigned integer register in the host ring-3 context. */
typedef const RTR3UINTREG  *PCRTR3UINTREG;

/** Unsigned integer register in the host ring-3 context. */
#if R0_ARCH_BITS == 32
typedef uint32_t            RTR0UINTREG;
#elif R0_ARCH_BITS == 64
typedef uint64_t            RTR0UINTREG;
#else
# error "Unsupported R3_ARCH_BITS!"
#endif
/** Pointer to an unsigned integer register in the host ring-3 context. */
typedef RTR0UINTREG        *PRTR0UINTREG;
/** Pointer to a const unsigned integer register in the host ring-3 context. */
typedef const RTR0UINTREG  *PCRTR0UINTREG;

/** @} */


/** @defgroup grp_rt_types_gc  Guest Context Basic Types
 * @ingroup grp_rt_types
 * @{
 */

/** Natural signed integer in the GC.
 * @deprecated silly type. */
#if GC_ARCH_BITS == 32
typedef int32_t         RTGCINT;
#elif GC_ARCH_BITS == 64 /** @todo this isn't right, natural int is 32-bit, see RTHCINT. */
typedef int64_t         RTGCINT;
#endif
/** Pointer to natural signed integer in GC.
 * @deprecated silly type. */
typedef RTGCINT        *PRTGCINT;
/** Pointer to const natural signed integer in GC.
 * @deprecated silly type. */
typedef const RTGCINT  *PCRTGCINT;

/** Natural unsigned integer in the GC.
 * @deprecated silly type. */
#if GC_ARCH_BITS == 32
typedef uint32_t        RTGCUINT;
#elif GC_ARCH_BITS == 64 /** @todo this isn't right, natural int is 32-bit, see RTHCUINT. */
typedef uint64_t        RTGCUINT;
#endif
/** Pointer to natural unsigned integer in GC.
 * @deprecated silly type. */
typedef RTGCUINT       *PRTGCUINT;
/** Pointer to const natural unsigned integer in GC.
 * @deprecated silly type. */
typedef const RTGCUINT *PCRTGCUINT;

/** Signed integer which can contain a GC pointer. */
#if GC_ARCH_BITS == 32
typedef int32_t         RTGCINTPTR;
#elif GC_ARCH_BITS == 64
typedef int64_t         RTGCINTPTR;
#endif
/** Pointer to signed integer which can contain a GC pointer. */
typedef RTGCINTPTR     *PRTGCINTPTR;
/** Pointer to const signed integer which can contain a GC pointer. */
typedef const RTGCINTPTR *PCRTGCINTPTR;

/** Unsigned integer which can contain a GC pointer. */
#if GC_ARCH_BITS == 32
typedef uint32_t        RTGCUINTPTR;
#elif GC_ARCH_BITS == 64
typedef uint64_t        RTGCUINTPTR;
#else
#  error Unsupported GC_ARCH_BITS value.
#endif
/** Pointer to unsigned integer which can contain a GC pointer. */
typedef RTGCUINTPTR     *PRTGCUINTPTR;
/** Pointer to unsigned integer which can contain a GC pointer. */
typedef const RTGCUINTPTR *PCRTGCUINTPTR;

/** Unsigned integer which can contain a 32 bits GC pointer. */
typedef uint32_t        RTGCUINTPTR32;
/** Pointer to unsigned integer which can contain a 32 bits GC pointer. */
typedef RTGCUINTPTR32   *PRTGCUINTPTR32;
/** Pointer to unsigned integer which can contain a 32 bits GC pointer. */
typedef const RTGCUINTPTR32 *PCRTGCUINTPTR32;

/** Unsigned integer which can contain a 64 bits GC pointer. */
typedef uint64_t        RTGCUINTPTR64;
/** Pointer to unsigned integer which can contain a 32 bits GC pointer. */
typedef RTGCUINTPTR64   *PRTGCUINTPTR64;
/** Pointer to unsigned integer which can contain a 32 bits GC pointer. */
typedef const RTGCUINTPTR64 *PCRTGCUINTPTR64;

/** Guest Physical Memory Address.*/
typedef uint64_t        RTGCPHYS;
/** Pointer to Guest Physical Memory Address. */
typedef RTGCPHYS       *PRTGCPHYS;
/** Pointer to const Guest Physical Memory Address. */
typedef const RTGCPHYS *PCRTGCPHYS;
/** @def NIL_RTGCPHYS
 * NIL GC Physical Address.
 * NIL_RTGCPHYS is used to signal an invalid physical address, similar
 * to the NULL pointer. Note that this value may actually be valid in
 * some contexts.
 */
#define NIL_RTGCPHYS     (~(RTGCPHYS)0U)


/** Guest Physical Memory Address; limited to 32 bits.*/
typedef uint32_t        RTGCPHYS32;
/** Pointer to Guest Physical Memory Address. */
typedef RTGCPHYS32     *PRTGCPHYS32;
/** Pointer to const Guest Physical Memory Address. */
typedef const RTGCPHYS32 *PCRTGCPHYS32;
/** @def NIL_RTGCPHYS32
 * NIL GC Physical Address.
 * NIL_RTGCPHYS32 is used to signal an invalid physical address, similar
 * to the NULL pointer. Note that this value may actually be valid in
 * some contexts.
 */
#define NIL_RTGCPHYS32     (~(RTGCPHYS32)0)


/** Guest Physical Memory Address; limited to 64 bits.*/
typedef uint64_t        RTGCPHYS64;
/** Pointer to Guest Physical Memory Address. */
typedef RTGCPHYS64     *PRTGCPHYS64;
/** Pointer to const Guest Physical Memory Address. */
typedef const RTGCPHYS64 *PCRTGCPHYS64;
/** @def NIL_RTGCPHYS64
 * NIL GC Physical Address.
 * NIL_RTGCPHYS64 is used to signal an invalid physical address, similar
 * to the NULL pointer. Note that this value may actually be valid in
 * some contexts.
 */
#define NIL_RTGCPHYS64     (~(RTGCPHYS64)0)

/** Guest context pointer, 32 bits.
 * Keep in mind that this type is an unsigned integer in
 * HC and void pointer in GC.
 */
typedef RTGCUINTPTR32   RTGCPTR32;
/** Pointer to a guest context pointer. */
typedef RTGCPTR32      *PRTGCPTR32;
/** Pointer to a const guest context pointer. */
typedef const RTGCPTR32 *PCRTGCPTR32;
/** @def NIL_RTGCPTR32
 * NIL GC pointer.
 */
#define NIL_RTGCPTR32   ((RTGCPTR32)0)

/** Guest context pointer, 64 bits.
 */
typedef RTGCUINTPTR64   RTGCPTR64;
/** Pointer to a guest context pointer. */
typedef RTGCPTR64      *PRTGCPTR64;
/** Pointer to a const guest context pointer. */
typedef const RTGCPTR64 *PCRTGCPTR64;
/** @def NIL_RTGCPTR64
 * NIL GC pointer.
 */
#define NIL_RTGCPTR64   ((RTGCPTR64)0)

/** Guest context pointer.
 * Keep in mind that this type is an unsigned integer in
 * HC and void pointer in GC.
 */
#if GC_ARCH_BITS == 64
typedef RTGCPTR64       RTGCPTR;
/** Pointer to a guest context pointer. */
typedef PRTGCPTR64      PRTGCPTR;
/** Pointer to a const guest context pointer. */
typedef PCRTGCPTR64    PCRTGCPTR;
/** @def NIL_RTGCPTR
 * NIL GC pointer.
 */
#define NIL_RTGCPTR     NIL_RTGCPTR64
#elif GC_ARCH_BITS == 32
typedef RTGCPTR32       RTGCPTR;
/** Pointer to a guest context pointer. */
typedef PRTGCPTR32      PRTGCPTR;
/** Pointer to a const guest context pointer. */
typedef PCRTGCPTR32    PCRTGCPTR;
/** @def NIL_RTGCPTR
 * NIL GC pointer.
 */
#define NIL_RTGCPTR     NIL_RTGCPTR32
#else
# error "Unsupported GC_ARCH_BITS!"
#endif

/** Unsigned integer register in the guest context. */
typedef uint32_t              RTGCUINTREG32;
/** Pointer to an unsigned integer register in the guest context. */
typedef RTGCUINTREG32        *PRTGCUINTREG32;
/** Pointer to a const unsigned integer register in the guest context. */
typedef const RTGCUINTREG32  *PCRTGCUINTREG32;

typedef uint64_t              RTGCUINTREG64;
/** Pointer to an unsigned integer register in the guest context. */
typedef RTGCUINTREG64        *PRTGCUINTREG64;
/** Pointer to a const unsigned integer register in the guest context. */
typedef const RTGCUINTREG64  *PCRTGCUINTREG64;

#if GC_ARCH_BITS == 64
typedef RTGCUINTREG64       RTGCUINTREG;
#elif GC_ARCH_BITS == 32
typedef RTGCUINTREG32       RTGCUINTREG;
#else
# error "Unsupported GC_ARCH_BITS!"
#endif
/** Pointer to an unsigned integer register in the guest context. */
typedef RTGCUINTREG        *PRTGCUINTREG;
/** Pointer to a const unsigned integer register in the guest context. */
typedef const RTGCUINTREG  *PCRTGCUINTREG;

/** @} */

/** @defgroup grp_rt_types_rc  Raw mode Context Basic Types
 * @ingroup grp_rt_types
 * @{
 */

/** Raw mode context pointer; a 32 bits guest context pointer.
 * Keep in mind that this type is an unsigned integer in
 * HC and void pointer in RC.
 */
#ifdef IN_RC
typedef void *          RTRCPTR;
#else
typedef uint32_t        RTRCPTR;
#endif
/** Pointer to a raw mode context pointer. */
typedef RTRCPTR        *PRTRCPTR;
/** Pointer to a const raw mode context pointer. */
typedef const RTRCPTR  *PCRTRCPTR;
/** @def NIL_RTGCPTR
 * NIL RC pointer.
 */
#define NIL_RTRCPTR    ((RTRCPTR)0)
/** @def RTRCPTR_MAX
 * The maximum value a RTRCPTR can have. Mostly used as INVALID value.
 */
#define RTRCPTR_MAX    ((RTRCPTR)UINT32_MAX)

/** Raw mode context pointer, unsigned integer variant. */
typedef int32_t         RTRCINTPTR;
/** @def RTRCUINPTR_MAX
 * The maximum value a RTRCUINPTR can have.
 */
#define RTRCUINTPTR_MAX ((RTRCUINTPTR)UINT32_MAX)

/** Raw mode context pointer, signed integer variant. */
typedef uint32_t        RTRCUINTPTR;
/** @def RTRCINPTR_MIN
 * The minimum value a RTRCINPTR can have.
 */
#define RTRCINTPTR_MIN ((RTRCINTPTR)INT32_MIN)
/** @def RTRCINPTR_MAX
 * The maximum value a RTRCINPTR can have.
 */
#define RTRCINTPTR_MAX ((RTRCINTPTR)INT32_MAX)

/** @} */


/** @defgroup grp_rt_types_cc  Current Context Basic Types
 * @ingroup grp_rt_types
 * @{
 */

/** Current Context Physical Memory Address.*/
#ifdef IN_RC
typedef RTGCPHYS RTCCPHYS;
#else
typedef RTHCPHYS RTCCPHYS;
#endif
/** Pointer to Current Context Physical Memory Address. */
typedef RTCCPHYS       *PRTCCPHYS;
/** Pointer to const Current Context Physical Memory Address. */
typedef const RTCCPHYS *PCRTCCPHYS;
/** @def NIL_RTCCPHYS
 * NIL CC Physical Address.
 * NIL_RTCCPHYS is used to signal an invalid physical address, similar
 * to the NULL pointer.
 */
#ifdef IN_RC
# define NIL_RTCCPHYS   NIL_RTGCPHYS
#else
# define NIL_RTCCPHYS   NIL_RTHCPHYS
#endif

/** Unsigned integer register in the current context. */
#if ARCH_BITS == 32
typedef uint32_t                RTCCUINTREG;
#elif ARCH_BITS == 64
typedef uint64_t                RTCCUINTREG;
#else
# error "Unsupported ARCH_BITS!"
#endif
/** Pointer to an unsigned integer register in the current context. */
typedef RTCCUINTREG            *PRTCCUINTREG;
/** Pointer to a const unsigned integer register in the current context. */
typedef RTCCUINTREG const      *PCRTCCUINTREG;

/** Signed integer register in the current context. */
#if ARCH_BITS == 32
typedef int32_t                 RTCCINTREG;
#elif ARCH_BITS == 64
typedef int64_t                 RTCCINTREG;
#endif
/** Pointer to a signed integer register in the current context. */
typedef RTCCINTREG             *PRTCCINTREG;
/** Pointer to a const signed integer register in the current context. */
typedef RTCCINTREG const       *PCRTCCINTREG;

/** @} */


/** File handle. */
typedef RTUINT                                      RTFILE;
/** Pointer to file handle. */
typedef RTFILE                                     *PRTFILE;
/** Nil file handle. */
#define NIL_RTFILE                                  (~(RTFILE)0)

/** Async I/O request handle. */
typedef R3PTRTYPE(struct RTFILEAIOREQINTERNAL *)    RTFILEAIOREQ;
/** Pointer to a async I/O request handle. */
typedef RTFILEAIOREQ                               *PRTFILEAIOREQ;
/** Nil request handle. */
#define NIL_RTFILEAIOREQ                            0

/** Async I/O completion context handle. */
typedef R3PTRTYPE(struct RTFILEAIOCTXINTERNAL *)    RTFILEAIOCTX;
/** Pointer to a async I/O completion context handle. */
typedef RTFILEAIOCTX                               *PRTFILEAIOCTX;
/** Nil context handle. */
#define NIL_RTFILEAIOCTX                            0

/** Loader module handle. */
typedef R3PTRTYPE(struct RTLDRMODINTERNAL *)        RTLDRMOD;
/** Pointer to a loader module handle. */
typedef RTLDRMOD                                   *PRTLDRMOD;
/** Nil loader module handle. */
#define NIL_RTLDRMOD                                0

/** Ring-0 memory object handle. */
typedef R0PTRTYPE(struct RTR0MEMOBJINTERNAL *)      RTR0MEMOBJ;
/** Pointer to a Ring-0 memory object handle. */
typedef RTR0MEMOBJ                                 *PRTR0MEMOBJ;
/** Nil ring-0 memory object handle. */
#define NIL_RTR0MEMOBJ                              0

/** Native thread handle. */
typedef RTHCUINTPTR                                 RTNATIVETHREAD;
/** Pointer to an native thread handle. */
typedef RTNATIVETHREAD                             *PRTNATIVETHREAD;
/** Nil native thread handle. */
#define NIL_RTNATIVETHREAD                          (~(RTNATIVETHREAD)0)

/** Process identifier. */
typedef uint32_t                                    RTPROCESS;
/** Pointer to a process identifier. */
typedef RTPROCESS                                  *PRTPROCESS;
/** Nil process identifier. */
#define NIL_RTPROCESS                               (~(RTPROCESS)0)

/** Process ring-0 handle. */
typedef RTR0UINTPTR                                 RTR0PROCESS;
/** Pointer to a ring-0 process handle. */
typedef RTR0PROCESS                                *PRTR0PROCESS;
/** Nil ring-0 process handle. */
#define NIL_RTR0PROCESS                             (~(RTR0PROCESS)0)

/** @typedef RTSEMEVENT
 * Event Semaphore handle. */
typedef R3R0PTRTYPE(struct RTSEMEVENTINTERNAL *)    RTSEMEVENT;
/** Pointer to an event semaphore handle. */
typedef RTSEMEVENT                                 *PRTSEMEVENT;
/** Nil event semaphore handle. */
#define NIL_RTSEMEVENT                              0

/** @typedef RTSEMEVENTMULTI
 * Event Multiple Release Semaphore handle. */
typedef R3R0PTRTYPE(struct RTSEMEVENTMULTIINTERNAL *) RTSEMEVENTMULTI;
/** Pointer to an event multiple release semaphore handle. */
typedef RTSEMEVENTMULTI                            *PRTSEMEVENTMULTI;
/** Nil multiple release event semaphore handle. */
#define NIL_RTSEMEVENTMULTI                         0

/** @typedef RTSEMFASTMUTEX
 * Fast mutex Semaphore handle. */
typedef R3R0PTRTYPE(struct RTSEMFASTMUTEXINTERNAL *) RTSEMFASTMUTEX;
/** Pointer to a mutex semaphore handle. */
typedef RTSEMFASTMUTEX                             *PRTSEMFASTMUTEX;
/** Nil fast mutex semaphore handle. */
#define NIL_RTSEMFASTMUTEX                          0

/** @typedef RTSEMMUTEX
 * Mutex Semaphore handle. */
typedef R3R0PTRTYPE(struct RTSEMMUTEXINTERNAL *)    RTSEMMUTEX;
/** Pointer to a mutex semaphore handle. */
typedef RTSEMMUTEX                                 *PRTSEMMUTEX;
/** Nil mutex semaphore handle. */
#define NIL_RTSEMMUTEX                              0

/** @typedef RTSEMRW
 * Read/Write Semaphore handle. */
typedef R3R0PTRTYPE(struct RTSEMRWINTERNAL *)       RTSEMRW;
/** Pointer to a read/write semaphore handle. */
typedef RTSEMRW                                    *PRTSEMRW;
/** Nil read/write semaphore handle. */
#define NIL_RTSEMRW                                 0

/** Spinlock handle. */
typedef R3R0PTRTYPE(struct RTSPINLOCKINTERNAL *)    RTSPINLOCK;
/** Pointer to a spinlock handle. */
typedef RTSPINLOCK                                 *PRTSPINLOCK;
/** Nil spinlock handle. */
#define NIL_RTSPINLOCK                              0

/** Socket handle. */
typedef int RTSOCKET;
/** Pointer to socket handle. */
typedef RTSOCKET *PRTSOCKET;
/** Nil socket handle. */
#define NIL_RTSOCKET                                (~(RTSOCKET)0)

/** Thread handle.*/
typedef R3R0PTRTYPE(struct RTTHREADINT *)           RTTHREAD;
/** Pointer to thread handle. */
typedef RTTHREAD                                   *PRTTHREAD;
/** Nil thread handle. */
#define NIL_RTTHREAD                                0

/** A TLS index. */
typedef RTHCINTPTR                                  RTTLS;
/** Pointer to a TLS index. */
typedef RTTLS                                      *PRTTLS;
/** Pointer to a const TLS index. */
typedef RTTLS const                                *PCRTTLS;
/** NIL TLS index value. */
#define NIL_RTTLS                                   (-1)

/** Handle to a simple heap. */
typedef R3R0PTRTYPE(struct RTHEAPSIMPLEINTERNAL *)  RTHEAPSIMPLE;
/** Pointer to a handle to a simple heap. */
typedef RTHEAPSIMPLE                               *PRTHEAPSIMPLE;
/** NIL simple heap handle. */
#define NIL_RTHEAPSIMPLE                            ((RTHEAPSIMPLE)0)

/** Handle to an environment block. */
typedef R3PTRTYPE(struct RTENVINTERNAL *)           RTENV;
/** Pointer to a handle to an environment block. */
typedef RTENV                                      *PRTENV;
/** NIL simple heap handle. */
#define NIL_RTENV                                   ((RTENV)0)

/** A CPU identifier.
 * @remarks This doesn't have to correspond to the APIC ID (intel/amd). Nor
 *          does it have to correspond to the bits in the affinity mask, at
 *          least not until we've sorted out Windows NT. */
typedef uint32_t                                    RTCPUID;
/** Pointer to a CPU identifier. */
typedef RTCPUID                                    *PRTCPUID;
/** Pointer to a const CPU identifier. */
typedef RTCPUID const                              *PCRTCPUID;
/** Nil CPU Id. */
#define NIL_RTCPUID                                 ((RTCPUID)~0)

/** A CPU set.
 * Treat this as an opaque type and always use RTCpuSet* for manupulating it.
 * @remarks Subject to change. */
typedef uint64_t                                    RTCPUSET;
/** Pointer to a CPU set. */
typedef RTCPUSET                                   *PRTCPUSET;
/** Pointer to a const CPU set. */
typedef RTCPUSET const                             *PCRTCPUSET;

/** A handle table handle. */
typedef R3R0PTRTYPE(struct RTHANDLETABLEINT *)      RTHANDLETABLE;
/** A pointer to a handle table handle. */
typedef RTHANDLETABLE                              *PRTHANDLETABLE;
/** @def NIL_RTHANDLETABLE
 * NIL handle table handle. */
#define NIL_RTHANDLETABLE                           ((RTHANDLETABLE)0)

/** A handle to a low resolution timer. */
typedef R3R0PTRTYPE(struct RTTIMERLRINT *)          RTTIMERLR;
/** A pointer to a low resolution timer handle. */
typedef RTTIMERLR                                  *PRTTIMERLR;
/** @def NIL_RTTIMERLR
 * NIL low resolution timer handle value. */
#define NIL_RTTIMERLR                               ((RTTIMERLR)0)

/** Handle to a random number generator. */
typedef R3R0PTRTYPE(struct RTRANDINT *)             RTRAND;
/** Pointer to a random number generator handle. */
typedef RTRAND                                     *PRTRAND;
/** NIL random number genrator handle value. */
#define NIL_RTRAND                                  ((RTRAND)0)

/** Debug address space handle. */
typedef R3R0PTRTYPE(struct RTDBGASINT *)            RTDBGAS;
/** Pointer to a debug address space handle. */
typedef RTDBGAS                                    *PRTDBGAS;
/** NIL debug address space handle. */
#define NIL_RTDBGAS                                 ((PRTDBGAS)0)

/** Debug module handle. */
typedef R3R0PTRTYPE(struct RTDBGMODINT *)           RTDBGMOD;
/** Pointer to a debug module handle. */
typedef RTDBGMOD                                   *PRTDBGMOD;
/** NIL debug module handle. */
#define NIL_RTDBGMOD                                ((PRTDBGMOD)0)


/**
 * UUID data type.
 *
 * @note IPRT defines that the first three integers in the @c Gen struct
 * interpretation are in little endian representation. This is different to
 * many other UUID implementation, and requires conversion if you need to
 * achieve consistent results.
 */
typedef union RTUUID
{
    /** 8-bit view. */
    uint8_t     au8[16];
    /** 16-bit view. */
    uint16_t    au16[8];
    /** 32-bit view. */
    uint32_t    au32[4];
    /** 64-bit view. */
    uint64_t    au64[2];
    /** The way the UUID is declared by the DCE specification. */
    struct
    {
        uint32_t    u32TimeLow;
        uint16_t    u16TimeMid;
        uint16_t    u16TimeHiAndVersion;
        uint8_t     u8ClockSeqHiAndReserved;
        uint8_t     u8ClockSeqLow;
        uint8_t     au8Node[6];
    } Gen;
} RTUUID;
/** Pointer to UUID data. */
typedef RTUUID *PRTUUID;
/** Pointer to readonly UUID data. */
typedef const RTUUID *PCRTUUID;

/**
 * UUID string maximum length.
 */
#define RTUUID_STR_LENGTH       37


/** Compression handle. */
typedef struct RTZIPCOMP   *PRTZIPCOMP;

/** Decompressor handle. */
typedef struct RTZIPDECOMP *PRTZIPDECOMP;


/**
 * Unicode Code Point.
 */
typedef uint32_t        RTUNICP;
/** Pointer to an Unicode Code Point. */
typedef RTUNICP        *PRTUNICP;
/** Pointer to an Unicode Code Point. */
typedef const RTUNICP  *PCRTUNICP;


/**
 * UTF-16 character.
 * @remark  wchar_t is not usable since it's compiler defined.
 * @remark  When we use the term character we're not talking about unicode code point, but
 *          the basic unit of the string encoding. Thus cwc - count of wide chars - means
 *          count of RTUTF16; cuc - count of unicode chars - means count of RTUNICP;
 *          and cch means count of the typedef 'char', which is assumed to be an octet.
 */
typedef uint16_t        RTUTF16;
/** Pointer to a UTF-16 character. */
typedef RTUTF16        *PRTUTF16;
/** Pointer to a const UTF-16 character. */
typedef const RTUTF16  *PCRTUTF16;


/**
 * Wait for ever if we have to.
 */
#define RT_INDEFINITE_WAIT      (~0U)


/**
 * Generic process callback.
 *
 * @returns VBox status code. Failure will cancel the operation.
 * @param   uPercentage     The percentage of the operation which has been completed.
 * @param   pvUser          The user specified argument.
 */
typedef DECLCALLBACK(int) FNRTPROGRESS(unsigned uPrecentage, void *pvUser);
/** Pointer to a generic progress callback function, FNRTPROCESS(). */
typedef FNRTPROGRESS *PFNRTPROGRESS;


/**
 * Rectangle data type.
 */
typedef struct RTRECT
{
    /** left X coordinate. */
    int32_t     xLeft;
    /** top Y coordinate. */
    int32_t     yTop;
    /** right X coordinate. (exclusive) */
    int32_t     xRight;
    /** bottom Y coordinate. (exclusive) */
    int32_t     yBottom;
} RTRECT;
/** Pointer to a rectangle. */
typedef RTRECT *PRTRECT;
/** Pointer to a const rectangle. */
typedef const RTRECT *PCRTRECT;


/**
 * Ethernet MAC address.
 *
 * The first 24 bits make up the Organisationally Unique Identifier (OUI),
 * where the first bit (little endian) indicates multicast (set) / unicast,
 * and the second bit indicates locally (set) / global administered. If all
 * bits are set, it's a broadcast.
 */
typedef union RTMAC
{
    /** @todo add a bitfield view of this stuff. */
    /** 8-bit view. */
    uint8_t     au8[6];
    /** 16-bit view. */
    uint16_t    au16[3];
} RTMAC;
/** Pointer to a MAC address. */
typedef RTMAC *PRTMAC;
/** Pointer to a readonly MAC address. */
typedef const RTMAC *PCRTMAC;

/** @} */

#endif

