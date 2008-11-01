/*
 *  dyngen defines for micro operation code
 *
 *  Copyright (c) 2003 Fabrice Bellard
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/*
 * Sun LGPL Disclaimer: For the avoidance of doubt, except that if any license choice
 * other than GPL or LGPL is available it will apply instead, Sun elects to use only
 * the Lesser General Public License version 2.1 (LGPLv2) at this time for any software where
 * a choice of LGPL license versions is made available with the language indicating
 * that LGPLv2 or any later version may be used, or where a choice of which version
 * of the LGPL is applied is otherwise unspecified.
 */
#if !defined(__DYNGEN_EXEC_H__)
#define __DYNGEN_EXEC_H__

#ifndef VBOX
/* prevent Solaris from trying to typedef FILE in gcc's
   include/floatingpoint.h which will conflict with the
   definition down below */
#ifdef __sun__
#define _FILEDEFED
#endif
#endif /* !VBOX */

/* NOTE: standard headers should be used with special care at this
   point because host CPU registers are used as global variables. Some
   host headers do not allow that. */
#include <stddef.h>

#ifndef VBOX

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
/* Linux/Sparc64 defines uint64_t */
#if !(defined (__sparc_v9__) && defined(__linux__))
/* XXX may be done for all 64 bits targets ? */
#if defined (__x86_64__) || defined(__ia64)
typedef unsigned long uint64_t;
#else
typedef unsigned long long uint64_t;
#endif
#endif

/* if Solaris/__sun__, don't typedef int8_t, as it will be typedef'd
   prior to this and will cause an error in compliation, conflicting
   with /usr/include/sys/int_types.h, line 75 */
#ifndef __sun__
typedef signed char int8_t;
#endif
typedef signed short int16_t;
typedef signed int int32_t;
// Linux/Sparc64 defines int64_t
#if !(defined (__sparc_v9__) && defined(__linux__))
#if defined (__x86_64__) || defined(__ia64)
typedef signed long int64_t;
#else
typedef signed long long int64_t;
#endif
#endif

/* XXX: This may be wrong for 64-bit ILP32 hosts.  */
typedef void * host_reg_t;

#define INT8_MIN		(-128)
#define INT16_MIN		(-32767-1)
#define INT32_MIN		(-2147483647-1)
#define INT64_MIN		(-(int64_t)(9223372036854775807)-1)
#define INT8_MAX		(127)
#define INT16_MAX		(32767)
#define INT32_MAX		(2147483647)
#define INT64_MAX		((int64_t)(9223372036854775807))
#define UINT8_MAX		(255)
#define UINT16_MAX		(65535)
#define UINT32_MAX		(4294967295U)
#define UINT64_MAX		((uint64_t)(18446744073709551615))

#ifdef _BSD
typedef struct __sFILE FILE;
#else
typedef struct FILE FILE;
#endif
extern int fprintf(FILE *, const char *, ...);
extern int fputs(const char *, FILE *);
extern int printf(const char *, ...);
#undef NULL
#define NULL 0

#else  /* VBOX */

/* XXX: This may be wrong for 64-bit ILP32 hosts.  */
typedef void * host_reg_t;

#include <iprt/stdint.h>
#include <stdio.h>

#endif /* VBOX */

#ifdef __i386__
#ifndef VBOX
#define AREG0 "ebp"
#define AREG1 "ebx"
#define AREG2 "esi"
#define AREG3 "edi"
#else
#define AREG0 "esi"
#define AREG1 "edi"
#endif
#endif
#ifdef __x86_64__
#if defined(VBOX) 
/* Must be in sync with TCG register notion, see tcg-target.h */
#endif
#define AREG0 "r14"
#define AREG1 "r15"
#define AREG2 "r12"
#define AREG3 "r13"
#endif
#ifdef __powerpc__
#define AREG0 "r27"
#define AREG1 "r24"
#define AREG2 "r25"
#define AREG3 "r26"
/* XXX: suppress this hack */
#if defined(CONFIG_USER_ONLY)
#define AREG4 "r16"
#define AREG5 "r17"
#define AREG6 "r18"
#define AREG7 "r19"
#define AREG8 "r20"
#define AREG9 "r21"
#define AREG10 "r22"
#define AREG11 "r23"
#endif
#define USE_INT_TO_FLOAT_HELPERS
#define BUGGY_GCC_DIV64
#endif
#ifdef __arm__
#define AREG0 "r7"
#define AREG1 "r4"
#define AREG2 "r5"
#define AREG3 "r6"
#endif
#ifdef __mips__
#define AREG0 "s3"
#define AREG1 "s0"
#define AREG2 "s1"
#define AREG3 "s2"
#endif
#ifdef __sparc__
#ifdef HOST_SOLARIS
#define AREG0 "g2"
#define AREG1 "g3"
#define AREG2 "g4"
#define AREG3 "g5"
#define AREG4 "g6"
#else
#ifdef __sparc_v9__
#define AREG0 "g1"
#define AREG1 "g4"
#define AREG2 "g5"
#define AREG3 "g7"
#else
#define AREG0 "g6"
#define AREG1 "g1"
#define AREG2 "g2"
#define AREG3 "g3"
#define AREG4 "l0"
#define AREG5 "l1"
#define AREG6 "l2"
#define AREG7 "l3"
#define AREG8 "l4"
#define AREG9 "l5"
#define AREG10 "l6"
#define AREG11 "l7"
#endif
#endif
#define USE_FP_CONVERT
#endif
#ifdef __s390__
#define AREG0 "r10"
#define AREG1 "r7"
#define AREG2 "r8"
#define AREG3 "r9"
#endif
#ifdef __alpha__
/* Note $15 is the frame pointer, so anything in op-i386.c that would
   require a frame pointer, like alloca, would probably loose.  */
#define AREG0 "$15"
#define AREG1 "$9"
#define AREG2 "$10"
#define AREG3 "$11"
#define AREG4 "$12"
#define AREG5 "$13"
#define AREG6 "$14"
#endif
#ifdef __mc68000
#define AREG0 "%a5"
#define AREG1 "%a4"
#define AREG2 "%d7"
#define AREG3 "%d6"
#define AREG4 "%d5"
#endif
#ifdef __ia64__
#define AREG0 "r7"
#define AREG1 "r4"
#define AREG2 "r5"
#define AREG3 "r6"
#endif

#ifndef VBOX
/* force GCC to generate only one epilog at the end of the function */
#define FORCE_RET() __asm__ __volatile__("" : : : "memory");
#endif

#ifndef OPPROTO
#define OPPROTO
#endif

#define xglue(x, y) x ## y
#define glue(x, y) xglue(x, y)
#define stringify(s)	tostring(s)
#define tostring(s)	#s

#if defined(__alpha__) || defined(__s390__)
/* the symbols are considered non exported so a br immediate is generated */
#define __hidden __attribute__((visibility("hidden")))
#else
#define __hidden 
#endif

#if defined(__alpha__)
/* Suggested by Richard Henderson. This will result in code like
        ldah $0,__op_param1($29)        !gprelhigh
        lda $0,__op_param1($0)          !gprellow
   We can then conveniently change $29 to $31 and adapt the offsets to
   emit the appropriate constant.  */
extern int __op_param1 __hidden;
extern int __op_param2 __hidden;
extern int __op_param3 __hidden;
#define PARAM1 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param1)); _r; })
#define PARAM2 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param2)); _r; })
#define PARAM3 ({ int _r; asm("" : "=r"(_r) : "0" (&__op_param3)); _r; })
#else
#if defined(__APPLE__)
static int __op_param1, __op_param2, __op_param3;
#else
extern int __op_param1, __op_param2, __op_param3;
#endif
#define PARAM1 ((long)(&__op_param1))
#define PARAM2 ((long)(&__op_param2))
#define PARAM3 ((long)(&__op_param3))
#endif /* !defined(__alpha__) */

extern int __op_jmp0, __op_jmp1, __op_jmp2, __op_jmp3;

#if defined(_WIN32) || defined(__APPLE__) || defined(__OS2__)
#define ASM_NAME(x) "_" #x
#else
#define ASM_NAME(x) #x
#endif

#ifdef VBOX
#define GETPC() ASMReturnAddress() 
#elif defined(__s390__)
/* The return address may point to the start of the next instruction.
   Subtracting one gets us the call instruction itself.  */
# define GETPC() ((void*)(((unsigned long)__builtin_return_address(0) & 0x7fffffffUL) - 1))
#elif defined(__arm__)
/* Thumb return addresses have the low bit set, so we need to subtract two.
   This is still safe in ARM mode because instructions are 4 bytes.  */
# define GETPC() ((void *)((unsigned long)__builtin_return_address(0) - 2))
#else
# define GETPC() ((void *)((unsigned long)__builtin_return_address(0) - 1))
#endif
#endif /* !defined(__DYNGEN_EXEC_H__) */
