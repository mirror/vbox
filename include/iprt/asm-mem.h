/** @file
 * IPRT - Assembly Memory Functions.
 */

/*
 * Copyright (C) 2006-2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef IPRT_INCLUDED_asm_mem_h
#define IPRT_INCLUDED_asm_mem_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>

#if defined(_MSC_VER) && RT_INLINE_ASM_USES_INTRIN
/* Emit the intrinsics at all optimization levels. */
# include <iprt/sanitized/intrin.h>
# pragma intrinsic(__cpuid)
# pragma intrinsic(__stosd)
# pragma intrinsic(__stosw)
# pragma intrinsic(__stosb)
# ifdef RT_ARCH_AMD64
#  pragma intrinsic(__stosq)
# endif
#endif


/*
 * Undefine all symbols we have Watcom C/C++ #pragma aux'es for.
 */
#if defined(__WATCOMC__) && ARCH_BITS == 16 && defined(RT_ARCH_X86)
# include "asm-mem-watcom-x86-16.h"
#elif defined(__WATCOMC__) && ARCH_BITS == 32 && defined(RT_ARCH_X86)
# include "asm-mem-watcom-x86-32.h"
#endif



/** @defgroup grp_rt_asm_mem    ASM - Memory Assembly Routines
 * @ingroup grp_rt_asm
 * @{
 */

#if defined(DOXYGEN_RUNNING) || defined(RT_ASM_INCLUDE_PAGE_SIZE)
/** @def RT_ASM_PAGE_SIZE
 * We try avoid dragging in iprt/param.h here.
 * @internal
 */
# if defined(RT_ARCH_SPARC64)
#  define RT_ASM_PAGE_SIZE   0x2000
#  if defined(PAGE_SIZE) && !defined(NT_INCLUDED)
#   if PAGE_SIZE != 0x2000
#    error "PAGE_SIZE is not 0x2000!"
#   endif
#  endif
# elif defined(RT_ARCH_ARM64) && defined(RT_OS_DARWIN)
#  define RT_ASM_PAGE_SIZE   0x4000
#  if defined(PAGE_SIZE) && !defined(NT_INCLUDED) && !defined(_MACH_ARM_VM_PARAM_H_)
#   if PAGE_SIZE != 0x4000
#    error "PAGE_SIZE is not 0x4000!"
#   endif
#  endif
# else
#  define RT_ASM_PAGE_SIZE   0x1000
#  if defined(PAGE_SIZE) && !defined(NT_INCLUDED) && !defined(RT_OS_LINUX) && !defined(RT_ARCH_ARM64)
#   if PAGE_SIZE != 0x1000
#    error "PAGE_SIZE is not 0x1000!"
#   endif
#  endif
# endif
#endif


#ifdef RT_ASM_PAGE_SIZE
/**
 * Zeros a 4K memory page.
 *
 * @param   pv  Pointer to the memory block. This must be page aligned.
 */
# if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) || (!defined(RT_ARCH_AMD64) && !defined(RT_ARCH_X86))
RT_ASM_DECL_PRAGMA_WATCOM(void) ASMMemZeroPage(volatile void RT_FAR *pv) RT_NOTHROW_PROTO;
#  else
DECLINLINE(void) ASMMemZeroPage(volatile void RT_FAR *pv) RT_NOTHROW_DEF
{
#   if RT_INLINE_ASM_USES_INTRIN
#    ifdef RT_ARCH_AMD64
    __stosq((unsigned __int64 *)pv, 0, RT_ASM_PAGE_SIZE / 8);
#    else
    __stosd((unsigned long *)pv, 0, RT_ASM_PAGE_SIZE / 4);
#    endif

#   elif RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG uDummy;
#    ifdef RT_ARCH_AMD64
    __asm__ __volatile__("rep stosq"
                         : "=D" (pv),
                           "=c" (uDummy)
                         : "0" (pv),
                           "c" (RT_ASM_PAGE_SIZE >> 3),
                           "a" (0)
                         : "memory");
#    else
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (uDummy)
                         : "0" (pv),
                           "c" (RT_ASM_PAGE_SIZE >> 2),
                           "a" (0)
                         : "memory");
#    endif
#   else
    __asm
    {
#    ifdef RT_ARCH_AMD64
        xor     rax, rax
        mov     ecx, 0200h
        mov     rdi, [pv]
        rep     stosq
#    else
        xor     eax, eax
        mov     ecx, 0400h
        mov     edi, [pv]
        rep     stosd
#    endif
    }
#   endif
}
# endif
#endif /* RT_ASM_PAGE_SIZE */


/**
 * Zeros a memory block with a 32-bit aligned size.
 *
 * @param   pv  Pointer to the memory block.
 * @param   cb  Number of bytes in the block. This MUST be aligned on 32-bit!
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) || (!defined(RT_ARCH_AMD64) && !defined(RT_ARCH_X86))
RT_ASM_DECL_PRAGMA_WATCOM(void) ASMMemZero32(volatile void RT_FAR *pv, size_t cb) RT_NOTHROW_PROTO;
#else
DECLINLINE(void) ASMMemZero32(volatile void RT_FAR *pv, size_t cb) RT_NOTHROW_DEF
{
# if RT_INLINE_ASM_USES_INTRIN
#  ifdef RT_ARCH_AMD64
    if (!(cb & 7))
        __stosq((unsigned __int64 RT_FAR *)pv, 0, cb / 8);
    else
#  endif
        __stosd((unsigned long RT_FAR *)pv, 0, cb / 4);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (cb)
                         : "0" (pv),
                           "1" (cb >> 2),
                           "a" (0)
                         : "memory");
# else
    __asm
    {
        xor     eax, eax
#  ifdef RT_ARCH_AMD64
        mov     rcx, [cb]
        shr     rcx, 2
        mov     rdi, [pv]
#  else
        mov     ecx, [cb]
        shr     ecx, 2
        mov     edi, [pv]
#  endif
        rep stosd
    }
# endif
}
#endif


/**
 * Fills a memory block with a 32-bit aligned size.
 *
 * @param   pv  Pointer to the memory block.
 * @param   cb  Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u32 The value to fill with.
 */
#if (RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN) || (!defined(RT_ARCH_AMD64) && !defined(RT_ARCH_X86))
RT_ASM_DECL_PRAGMA_WATCOM(void) ASMMemFill32(volatile void RT_FAR *pv, size_t cb, uint32_t u32) RT_NOTHROW_PROTO;
#else
DECLINLINE(void) ASMMemFill32(volatile void RT_FAR *pv, size_t cb, uint32_t u32) RT_NOTHROW_DEF
{
# if RT_INLINE_ASM_USES_INTRIN
#  ifdef RT_ARCH_AMD64
    if (!(cb & 7))
        __stosq((unsigned __int64 RT_FAR *)pv, RT_MAKE_U64(u32, u32), cb / 8);
    else
#  endif
        __stosd((unsigned long RT_FAR *)pv, u32, cb / 4);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep stosl"
                         : "=D" (pv),
                           "=c" (cb)
                         : "0" (pv),
                           "1" (cb >> 2),
                           "a" (u32)
                         : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rcx, [cb]
        shr     rcx, 2
        mov     rdi, [pv]
#  else
        mov     ecx, [cb]
        shr     ecx, 2
        mov     edi, [pv]
#  endif
        mov     eax, [u32]
        rep stosd
    }
# endif
}
#endif


/**
 * Checks if a memory block is all zeros.
 *
 * @returns Pointer to the first non-zero byte.
 * @returns NULL if all zero.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block.
 */
#if !defined(RDESKTOP) && (!defined(RT_OS_LINUX) || !defined(__KERNEL__))
DECLASM(void RT_FAR *) ASMMemFirstNonZero(void const RT_FAR *pv, size_t cb) RT_NOTHROW_PROTO;
#else
DECLINLINE(void RT_FAR *) ASMMemFirstNonZero(void const RT_FAR *pv, size_t cb) RT_NOTHROW_DEF
{
/** @todo replace with ASMMemFirstNonZero-generic.cpp in kernel modules. */
    uint8_t const *pb = (uint8_t const RT_FAR *)pv;
    for (; cb; cb--, pb++)
        if (RT_LIKELY(*pb == 0))
        { /* likely */ }
        else
            return (void RT_FAR *)pb;
    return NULL;
}
#endif


/**
 * Checks if a memory block is all zeros.
 *
 * @returns true if zero, false if not.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block.
 *
 * @sa      ASMMemFirstNonZero
 */
DECLINLINE(bool) ASMMemIsZero(void const RT_FAR *pv, size_t cb) RT_NOTHROW_DEF
{
    return ASMMemFirstNonZero(pv, cb) == NULL;
}


#ifdef RT_ASM_PAGE_SIZE
/**
 * Checks if a memory page is all zeros.
 *
 * @returns true / false.
 *
 * @param   pvPage      Pointer to the page.  Must be aligned on 16 byte
 *                      boundary
 */
DECLINLINE(bool) ASMMemIsZeroPage(void const RT_FAR *pvPage) RT_NOTHROW_DEF
{
# if 0 /*RT_INLINE_ASM_GNU_STYLE - this is actually slower... */
    union { RTCCUINTREG r; bool f; } uAX;
    RTCCUINTREG xCX, xDI;
   Assert(!((uintptr_t)pvPage & 15));
    __asm__ __volatile__("repe; "
#  ifdef RT_ARCH_AMD64
                         "scasq\n\t"
#  else
                         "scasl\n\t"
#  endif
                         "setnc %%al\n\t"
                         : "=&c" (xCX)
                         , "=&D" (xDI)
                         , "=&a" (uAX.r)
                         : "mr" (pvPage)
#  ifdef RT_ARCH_AMD64
                         , "0" (RT_ASM_PAGE_SIZE/8)
#  else
                         , "0" (RT_ASM_PAGE_SIZE/4)
#  endif
                         , "1" (pvPage)
                         , "2" (0)
                         : "cc");
    return uAX.f;
# else
   uintptr_t const RT_FAR *puPtr = (uintptr_t const RT_FAR *)pvPage;
   size_t                  cLeft = RT_ASM_PAGE_SIZE / sizeof(uintptr_t) / 8;
   Assert(!((uintptr_t)pvPage & 15));
   for (;;)
   {
       if (puPtr[0])        return false;
       if (puPtr[4])        return false;

       if (puPtr[2])        return false;
       if (puPtr[6])        return false;

       if (puPtr[1])        return false;
       if (puPtr[5])        return false;

       if (puPtr[3])        return false;
       if (puPtr[7])        return false;

       if (!--cLeft)
           return true;
       puPtr += 8;
   }
# endif
}
#endif /* RT_ASM_PAGE_SIZE */


/**
 * Checks if a memory block is filled with the specified byte, returning the
 * first mismatch.
 *
 * This is sort of an inverted memchr.
 *
 * @returns Pointer to the byte which doesn't equal u8.
 * @returns NULL if all equal to u8.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block.
 * @param   u8      The value it's supposed to be filled with.
 *
 * @remarks No alignment requirements.
 */
#if    (!defined(RT_OS_LINUX) || !defined(__KERNEL__)) \
    && (!defined(RT_OS_FREEBSD) || !defined(_KERNEL))
DECLASM(void *) ASMMemFirstMismatchingU8(void const RT_FAR *pv, size_t cb, uint8_t u8) RT_NOTHROW_PROTO;
#else
DECLINLINE(void *) ASMMemFirstMismatchingU8(void const RT_FAR *pv, size_t cb, uint8_t u8) RT_NOTHROW_DEF
{
/** @todo replace with ASMMemFirstMismatchingU8-generic.cpp in kernel modules. */
    uint8_t const *pb = (uint8_t const RT_FAR *)pv;
    for (; cb; cb--, pb++)
        if (RT_LIKELY(*pb == u8))
        { /* likely */ }
        else
            return (void *)pb;
    return NULL;
}
#endif


/**
 * Checks if a memory block is filled with the specified byte.
 *
 * @returns true if all matching, false if not.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block.
 * @param   u8      The value it's supposed to be filled with.
 *
 * @remarks No alignment requirements.
 */
DECLINLINE(bool) ASMMemIsAllU8(void const RT_FAR *pv, size_t cb, uint8_t u8) RT_NOTHROW_DEF
{
    return ASMMemFirstMismatchingU8(pv, cb, u8) == NULL;
}


/**
 * Checks if a memory block is filled with the specified 32-bit value.
 *
 * This is a sort of inverted memchr.
 *
 * @returns Pointer to the first value which doesn't equal u32.
 * @returns NULL if all equal to u32.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u32     The value it's supposed to be filled with.
 */
DECLINLINE(uint32_t RT_FAR *) ASMMemFirstMismatchingU32(void const RT_FAR *pv, size_t cb, uint32_t u32) RT_NOTHROW_DEF
{
/** @todo rewrite this in inline assembly? */
    uint32_t const RT_FAR *pu32 = (uint32_t const RT_FAR *)pv;
    for (; cb; cb -= 4, pu32++)
        if (RT_LIKELY(*pu32 == u32))
        { /* likely */ }
        else
            return (uint32_t RT_FAR *)pu32;
    return NULL;
}


/**
 * Probes a byte pointer for read access.
 *
 * While the function will not fault if the byte is not read accessible,
 * the idea is to do this in a safe place like before acquiring locks
 * and such like.
 *
 * Also, this functions guarantees that an eager compiler is not going
 * to optimize the probing away.
 *
 * @param   pvByte      Pointer to the byte.
 */
#if RT_INLINE_ASM_EXTERNAL_TMP_ARM
RT_ASM_DECL_PRAGMA_WATCOM(uint8_t) ASMProbeReadByte(const void RT_FAR *pvByte) RT_NOTHROW_PROTO;
#else
DECLINLINE(uint8_t) ASMProbeReadByte(const void RT_FAR *pvByte) RT_NOTHROW_DEF
{
# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    uint8_t u8;
#  if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movb %1, %0\n\t"
                         : "=q" (u8)
                         : "m" (*(const uint8_t *)pvByte));
#  else
    __asm
    {
#   ifdef RT_ARCH_AMD64
        mov     rax, [pvByte]
        mov     al, [rax]
#   else
        mov     eax, [pvByte]
        mov     al, [eax]
#   endif
        mov     [u8], al
    }
#  endif
    return u8;

# elif defined(RT_ARCH_ARM64) || defined(RT_ARCH_ARM32)
    uint32_t u32;
    __asm__ __volatile__("Lstart_ASMProbeReadByte_%=:\n\t"
#  if defined(RT_ARCH_ARM64)
                         "ldxrb     %w[uDst], %[pMem]\n\t"
#  else
                         "ldrexb    %[uDst], %[pMem]\n\t"
#  endif
                         : [uDst] "=&r" (u32)
                         : [pMem] "Q" (*(uint8_t const *)pvByte));
    return (uint8_t)u32;

# else
#  error "Port me"
# endif
}
#endif

#ifdef RT_ASM_PAGE_SIZE
/**
 * Probes a buffer for read access page by page.
 *
 * While the function will fault if the buffer is not fully read
 * accessible, the idea is to do this in a safe place like before
 * acquiring locks and such like.
 *
 * Also, this functions guarantees that an eager compiler is not going
 * to optimize the probing away.
 *
 * @param   pvBuf       Pointer to the buffer.
 * @param   cbBuf       The size of the buffer in bytes. Must be >= 1.
 */
DECLINLINE(void) ASMProbeReadBuffer(const void RT_FAR *pvBuf, size_t cbBuf) RT_NOTHROW_DEF
{
    /** @todo verify that the compiler actually doesn't optimize this away. (intel & gcc) */
    /* the first byte */
    const uint8_t RT_FAR *pu8 = (const uint8_t RT_FAR *)pvBuf;
    ASMProbeReadByte(pu8);

    /* the pages in between pages. */
    while (cbBuf > RT_ASM_PAGE_SIZE)
    {
        ASMProbeReadByte(pu8);
        cbBuf -= RT_ASM_PAGE_SIZE;
        pu8   += RT_ASM_PAGE_SIZE;
    }

    /* the last byte */
    ASMProbeReadByte(pu8 + cbBuf - 1);
}
#endif

/** @} */

/*
 * Include #pragma aux definitions for Watcom C/C++.
 */
#if defined(__WATCOMC__) && ARCH_BITS == 16 && defined(RT_ARCH_X86)
# define IPRT_ASM_WATCOM_X86_16_WITH_PRAGMAS
# undef IPRT_INCLUDED_asm_mem_watcom_x86_16_h
# include "asm-mem-watcom-x86-16.h"
#elif defined(__WATCOMC__) && ARCH_BITS == 32 && defined(RT_ARCH_X86)
# define IPRT_ASM_WATCOM_X86_32_WITH_PRAGMAS
# undef IPRT_INCLUDED_asm_mem_watcom_x86_32_h
# include "asm-mem-watcom-x86-32.h"
#endif

#endif /* !IPRT_INCLUDED_asm_mem_h */

