/** @file
 * innotek Portable Runtime - Assembly Functions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

#ifndef ___iprt_asm_h
#define ___iprt_asm_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/assert.h>
/** @todo #include <iprt/param.h> for PAGE_SIZE. */
/** @def RT_INLINE_ASM_USES_INTRIN
 * Defined as 1 if we're using a _MSC_VER 1400.
 * Otherwise defined as 0.
 */

#ifdef _MSC_VER
# if _MSC_VER >= 1400
#  define RT_INLINE_ASM_USES_INTRIN 1
#  include <intrin.h>
   /* Emit the intrinsics at all optimization levels. */
#  pragma intrinsic(_ReadWriteBarrier)
#  pragma intrinsic(__cpuid)
#  pragma intrinsic(_enable)
#  pragma intrinsic(_disable)
#  pragma intrinsic(__rdtsc)
#  pragma intrinsic(__readmsr)
#  pragma intrinsic(__writemsr)
#  pragma intrinsic(__outbyte)
#  pragma intrinsic(__outword)
#  pragma intrinsic(__outdword)
#  pragma intrinsic(__inbyte)
#  pragma intrinsic(__inword)
#  pragma intrinsic(__indword)
#  pragma intrinsic(__invlpg)
#  pragma intrinsic(__stosd)
#  pragma intrinsic(__stosw)
#  pragma intrinsic(__stosb)
#  pragma intrinsic(__readcr0)
#  pragma intrinsic(__readcr2)
#  pragma intrinsic(__readcr3)
#  pragma intrinsic(__readcr4)
#  pragma intrinsic(__writecr0)
#  pragma intrinsic(__writecr3)
#  pragma intrinsic(__writecr4)
#  pragma intrinsic(_BitScanForward)
#  pragma intrinsic(_BitScanReverse)
#  pragma intrinsic(_bittest)
#  pragma intrinsic(_bittestandset)
#  pragma intrinsic(_bittestandreset)
#  pragma intrinsic(_bittestandcomplement)
#  pragma intrinsic(_byteswap_ushort)
#  pragma intrinsic(_byteswap_ulong)
#  pragma intrinsic(_interlockedbittestandset)
#  pragma intrinsic(_interlockedbittestandreset)
#  pragma intrinsic(_InterlockedAnd)
#  pragma intrinsic(_InterlockedOr)
#  pragma intrinsic(_InterlockedIncrement)
#  pragma intrinsic(_InterlockedDecrement)
#  pragma intrinsic(_InterlockedExchange)
#  pragma intrinsic(_InterlockedExchangeAdd)
#  pragma intrinsic(_InterlockedCompareExchange)
#  pragma intrinsic(_InterlockedCompareExchange64)
#  ifdef RT_ARCH_AMD64
#   pragma intrinsic(__stosq)
#   pragma intrinsic(__readcr8)
#   pragma intrinsic(__writecr8)
#   pragma intrinsic(_byteswap_uint64)
#   pragma intrinsic(_InterlockedExchange64)
#  endif
# endif
#endif
#ifndef RT_INLINE_ASM_USES_INTRIN
# define RT_INLINE_ASM_USES_INTRIN 0
#endif



/** @defgroup grp_asm       ASM - Assembly Routines
 * @ingroup grp_rt
 *
 * @remarks The difference between ordered and unordered atomic operations are that
 *          the former will complete outstanding reads and writes before continuing
 *          while the latter doesn't make any promisses about the order. Ordered
 *          operations doesn't, it seems, make any 100% promise wrt to whether
 *          the operation will complete before any subsequent memory access.
 *          (please, correct if wrong.)
 *
 *          ASMAtomicSomething operations are all ordered, while ASMAtomicUoSomething
 *          are unordered (note the Uo).
 *
 * @{
 */

/** @def RT_INLINE_ASM_EXTERNAL
 * Defined as 1 if the compiler does not support inline assembly.
 * The ASM* functions will then be implemented in an external .asm file.
 *
 * @remark  At the present time it's unconfirmed whether or not Microsoft skipped
 *          inline assmebly in their AMD64 compiler.
 */
#if defined(_MSC_VER) && defined(RT_ARCH_AMD64)
# define RT_INLINE_ASM_EXTERNAL 1
#else
# define RT_INLINE_ASM_EXTERNAL 0
#endif

/** @def RT_INLINE_ASM_GNU_STYLE
 * Defined as 1 if the compiler understand GNU style inline assembly.
 */
#if defined(_MSC_VER)
# define RT_INLINE_ASM_GNU_STYLE 0
#else
# define RT_INLINE_ASM_GNU_STYLE 1
#endif


/** @todo find a more proper place for this structure? */
#pragma pack(1)
/** IDTR */
typedef struct RTIDTR
{
    /** Size of the IDT. */
    uint16_t    cbIdt;
    /** Address of the IDT. */
    uintptr_t   pIdt;
} RTIDTR, *PRTIDTR;
#pragma pack()

#pragma pack(1)
/** GDTR */
typedef struct RTGDTR
{
    /** Size of the GDT. */
    uint16_t    cbGdt;
    /** Address of the GDT. */
    uintptr_t   pGdt;
} RTGDTR, *PRTGDTR;
#pragma pack()


/** @def ASMReturnAddress
 * Gets the return address of the current (or calling if you like) function or method.
 */
#ifdef _MSC_VER
# ifdef __cplusplus
extern "C"
# endif
void * _ReturnAddress(void);
# pragma intrinsic(_ReturnAddress)
# define ASMReturnAddress() _ReturnAddress()
#elif defined(__GNUC__) || defined(DOXYGEN_RUNNING)
# define ASMReturnAddress() __builtin_return_address(0)
#else
# error "Unsupported compiler."
#endif


/**
 * Gets the content of the IDTR CPU register.
 * @param   pIdtr   Where to store the IDTR contents.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMGetIDTR(PRTIDTR pIdtr);
#else
DECLINLINE(void) ASMGetIDTR(PRTIDTR pIdtr)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("sidt %0" : "=m" (*pIdtr));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pIdtr]
        sidt    [rax]
#  else
        mov     eax, [pIdtr]
        sidt    [eax]
#  endif
    }
# endif
}
#endif


/**
 * Sets the content of the IDTR CPU register.
 * @param   pIdtr   Where to load the IDTR contents from
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMSetIDTR(const RTIDTR *pIdtr);
#else
DECLINLINE(void) ASMSetIDTR(const RTIDTR *pIdtr)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lidt %0" : : "m" (*pIdtr));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pIdtr]
        lidt    [rax]
#  else
        mov     eax, [pIdtr]
        lidt    [eax]
#  endif
    }
# endif
}
#endif


/**
 * Gets the content of the GDTR CPU register.
 * @param   pGdtr   Where to store the GDTR contents.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMGetGDTR(PRTGDTR pGdtr);
#else
DECLINLINE(void) ASMGetGDTR(PRTGDTR pGdtr)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("sgdt %0" : "=m" (*pGdtr));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pGdtr]
        sgdt    [rax]
#  else
        mov     eax, [pGdtr]
        sgdt    [eax]
#  endif
    }
# endif
}
#endif

/**
 * Get the cs register.
 * @returns cs.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetCS(void);
#else
DECLINLINE(RTSEL) ASMGetCS(void)
{
    RTSEL SelCS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%cs, %0\n\t" : "=r" (SelCS));
# else
    __asm
    {
        mov     ax, cs
        mov     [SelCS], ax
    }
# endif
    return SelCS;
}
#endif


/**
 * Get the DS register.
 * @returns DS.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetDS(void);
#else
DECLINLINE(RTSEL) ASMGetDS(void)
{
    RTSEL SelDS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%ds, %0\n\t" : "=r" (SelDS));
# else
    __asm
    {
        mov     ax, ds
        mov     [SelDS], ax
    }
# endif
    return SelDS;
}
#endif


/**
 * Get the ES register.
 * @returns ES.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetES(void);
#else
DECLINLINE(RTSEL) ASMGetES(void)
{
    RTSEL SelES;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%es, %0\n\t" : "=r" (SelES));
# else
    __asm
    {
        mov     ax, es
        mov     [SelES], ax
    }
# endif
    return SelES;
}
#endif


/**
 * Get the FS register.
 * @returns FS.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetFS(void);
#else
DECLINLINE(RTSEL) ASMGetFS(void)
{
    RTSEL SelFS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%fs, %0\n\t" : "=r" (SelFS));
# else
    __asm
    {
        mov     ax, fs
        mov     [SelFS], ax
    }
# endif
    return SelFS;
}
# endif


/**
 * Get the GS register.
 * @returns GS.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetGS(void);
#else
DECLINLINE(RTSEL) ASMGetGS(void)
{
    RTSEL SelGS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%gs, %0\n\t" : "=r" (SelGS));
# else
    __asm
    {
        mov     ax, gs
        mov     [SelGS], ax
    }
# endif
    return SelGS;
}
#endif


/**
 * Get the SS register.
 * @returns SS.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetSS(void);
#else
DECLINLINE(RTSEL) ASMGetSS(void)
{
    RTSEL SelSS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movw  %%ss, %0\n\t" : "=r" (SelSS));
# else
    __asm
    {
        mov     ax, ss
        mov     [SelSS], ax
    }
# endif
    return SelSS;
}
#endif


/**
 * Get the TR register.
 * @returns TR.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTSEL) ASMGetTR(void);
#else
DECLINLINE(RTSEL) ASMGetTR(void)
{
    RTSEL SelTR;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("str %w0\n\t" : "=r" (SelTR));
# else
    __asm
    {
        str     ax
        mov     [SelTR], ax
    }
# endif
    return SelTR;
}
#endif


/**
 * Get the [RE]FLAGS register.
 * @returns [RE]FLAGS.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTCCUINTREG) ASMGetFlags(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetFlags(void)
{
    RTCCUINTREG uFlags;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("pushfq\n\t"
                         "popq  %0\n\t"
                         : "=g" (uFlags));
#  else
    __asm__ __volatile__("pushfl\n\t"
                         "popl  %0\n\t"
                         : "=g" (uFlags));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        pushfq
        pop  [uFlags]
#  else
        pushfd
        pop  [uFlags]
#  endif
    }
# endif
    return uFlags;
}
#endif


/**
 * Set the [RE]FLAGS register.
 * @param   uFlags      The new [RE]FLAGS value.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMSetFlags(RTCCUINTREG uFlags);
#else
DECLINLINE(void) ASMSetFlags(RTCCUINTREG uFlags)
{
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("pushq %0\n\t"
                         "popfq\n\t"
                         : : "g" (uFlags));
#  else
    __asm__ __volatile__("pushl %0\n\t"
                         "popfl\n\t"
                         : : "g" (uFlags));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        push    [uFlags]
        popfq
#  else
        push    [uFlags]
        popfd
#  endif
    }
# endif
}
#endif


/**
 * Gets the content of the CPU timestamp counter register.
 *
 * @returns TSC.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMReadTSC(void);
#else
DECLINLINE(uint64_t) ASMReadTSC(void)
{
    RTUINT64U u;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("rdtsc\n\t" : "=a" (u.s.Lo), "=d" (u.s.Hi));
# else
#  if RT_INLINE_ASM_USES_INTRIN
    u.u = __rdtsc();
#  else
    __asm
    {
        rdtsc
        mov     [u.s.Lo], eax
        mov     [u.s.Hi], edx
    }
#  endif
# endif
    return u.u;
}
#endif


/**
 * Performs the cpuid instruction returning all registers.
 *
 * @param   uOperator   CPUID operation (eax).
 * @param   pvEAX       Where to store eax.
 * @param   pvEBX       Where to store ebx.
 * @param   pvECX       Where to store ecx.
 * @param   pvEDX       Where to store edx.
 * @remark  We're using void pointers to ease the use of special bitfield structures and such.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMCpuId(uint32_t uOperator, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX);
#else
DECLINLINE(void) ASMCpuId(uint32_t uOperator, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX)
{
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    RTCCUINTREG uRAX, uRBX, uRCX, uRDX;
    __asm__ ("cpuid\n\t"
             : "=a" (uRAX),
               "=b" (uRBX),
               "=c" (uRCX),
               "=d" (uRDX)
             : "0" (uOperator));
    *(uint32_t *)pvEAX = (uint32_t)uRAX;
    *(uint32_t *)pvEBX = (uint32_t)uRBX;
    *(uint32_t *)pvECX = (uint32_t)uRCX;
    *(uint32_t *)pvEDX = (uint32_t)uRDX;
#  else
    __asm__ ("xchgl %%ebx, %1\n\t"
             "cpuid\n\t"
             "xchgl %%ebx, %1\n\t"
             : "=a" (*(uint32_t *)pvEAX),
               "=r" (*(uint32_t *)pvEBX),
               "=c" (*(uint32_t *)pvECX),
               "=d" (*(uint32_t *)pvEDX)
             : "0" (uOperator));
#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    __cpuid(aInfo, uOperator);
    *(uint32_t *)pvEAX = aInfo[0];
    *(uint32_t *)pvEBX = aInfo[1];
    *(uint32_t *)pvECX = aInfo[2];
    *(uint32_t *)pvEDX = aInfo[3];

# else
    uint32_t    uEAX;
    uint32_t    uEBX;
    uint32_t    uECX;
    uint32_t    uEDX;
    __asm
    {
        push    ebx
        mov     eax, [uOperator]
        cpuid
        mov     [uEAX], eax
        mov     [uEBX], ebx
        mov     [uECX], ecx
        mov     [uEDX], edx
        pop     ebx
    }
    *(uint32_t *)pvEAX = uEAX;
    *(uint32_t *)pvEBX = uEBX;
    *(uint32_t *)pvECX = uECX;
    *(uint32_t *)pvEDX = uEDX;
# endif
}
#endif


/**
 * Performs the cpuid instruction returning all registers.
 * Some subfunctions of cpuid take ECX as additional parameter (currently known for EAX=4)
 *
 * @param   uOperator   CPUID operation (eax).
 * @param   uIdxECX     ecx index
 * @param   pvEAX       Where to store eax.
 * @param   pvEBX       Where to store ebx.
 * @param   pvECX       Where to store ecx.
 * @param   pvEDX       Where to store edx.
 * @remark  We're using void pointers to ease the use of special bitfield structures and such.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMCpuId_Idx_ECX(uint32_t uOperator, uint32_t uIdxECX, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX);
#else
DECLINLINE(void) ASMCpuId_Idx_ECX(uint32_t uOperator, uint32_t uIdxECX, void *pvEAX, void *pvEBX, void *pvECX, void *pvEDX)
{
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    RTCCUINTREG uRAX, uRBX, uRCX, uRDX;
    __asm__ ("cpuid\n\t"
             : "=a" (uRAX),
               "=b" (uRBX),
               "=c" (uRCX),
               "=d" (uRDX)
             : "0" (uOperator),
               "2" (uIdxECX));
    *(uint32_t *)pvEAX = (uint32_t)uRAX;
    *(uint32_t *)pvEBX = (uint32_t)uRBX;
    *(uint32_t *)pvECX = (uint32_t)uRCX;
    *(uint32_t *)pvEDX = (uint32_t)uRDX;
#  else
    __asm__ ("xchgl %%ebx, %1\n\t"
             "cpuid\n\t"
             "xchgl %%ebx, %1\n\t"
             : "=a" (*(uint32_t *)pvEAX),
               "=r" (*(uint32_t *)pvEBX),
               "=c" (*(uint32_t *)pvECX),
               "=d" (*(uint32_t *)pvEDX)
             : "0" (uOperator),
               "2" (uIdxECX));
#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    /* ??? another intrinsic ??? */
    __cpuid(aInfo, uOperator);
    *(uint32_t *)pvEAX = aInfo[0];
    *(uint32_t *)pvEBX = aInfo[1];
    *(uint32_t *)pvECX = aInfo[2];
    *(uint32_t *)pvEDX = aInfo[3];

# else
    uint32_t    uEAX;
    uint32_t    uEBX;
    uint32_t    uECX;
    uint32_t    uEDX;
    __asm
    {
        push    ebx
        mov     eax, [uOperator]
        mov     ecx, [uIdxECX]
        cpuid
        mov     [uEAX], eax
        mov     [uEBX], ebx
        mov     [uECX], ecx
        mov     [uEDX], edx
        pop     ebx
    }
    *(uint32_t *)pvEAX = uEAX;
    *(uint32_t *)pvEBX = uEBX;
    *(uint32_t *)pvECX = uECX;
    *(uint32_t *)pvEDX = uEDX;
# endif
}
#endif


/**
 * Performs the cpuid instruction returning ecx and edx.
 *
 * @param   uOperator   CPUID operation (eax).
 * @param   pvECX       Where to store ecx.
 * @param   pvEDX       Where to store edx.
 * @remark  We're using void pointers to ease the use of special bitfield structures and such.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMCpuId_ECX_EDX(uint32_t uOperator, void *pvECX, void *pvEDX);
#else
DECLINLINE(void) ASMCpuId_ECX_EDX(uint32_t uOperator, void *pvECX, void *pvEDX)
{
    uint32_t uEBX;
    ASMCpuId(uOperator, &uOperator, &uEBX, pvECX, pvEDX);
}
#endif


/**
 * Performs the cpuid instruction returning edx.
 *
 * @param   uOperator   CPUID operation (eax).
 * @returns EDX after cpuid operation.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMCpuId_EDX(uint32_t uOperator);
#else
DECLINLINE(uint32_t) ASMCpuId_EDX(uint32_t uOperator)
{
    RTCCUINTREG xDX;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    RTCCUINTREG uSpill;
    __asm__ ("cpuid"
             : "=a" (uSpill),
               "=d" (xDX)
             : "0" (uOperator)
             : "rbx", "rcx");
#  elif (defined(PIC) || defined(RT_OS_DARWIN)) && defined(__i386__) /* darwin: PIC by default. */
    __asm__ ("push  %%ebx\n\t"
             "cpuid\n\t"
             "pop   %%ebx\n\t"
             : "=a" (uOperator),
               "=d" (xDX)
             : "0" (uOperator)
             : "ecx");
#  else
    __asm__ ("cpuid"
             : "=a" (uOperator),
               "=d" (xDX)
             : "0" (uOperator)
             : "ebx", "ecx");
#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    __cpuid(aInfo, uOperator);
    xDX = aInfo[3];

# else
    __asm
    {
        push    ebx
        mov     eax, [uOperator]
        cpuid
        mov     [xDX], edx
        pop     ebx
    }
# endif
    return (uint32_t)xDX;
}
#endif


/**
 * Performs the cpuid instruction returning ecx.
 *
 * @param   uOperator   CPUID operation (eax).
 * @returns ECX after cpuid operation.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMCpuId_ECX(uint32_t uOperator);
#else
DECLINLINE(uint32_t) ASMCpuId_ECX(uint32_t uOperator)
{
    RTCCUINTREG xCX;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    RTCCUINTREG uSpill;
    __asm__ ("cpuid"
             : "=a" (uSpill),
               "=c" (xCX)
             : "0" (uOperator)
             : "rbx", "rdx");
#  elif (defined(PIC) || defined(RT_OS_DARWIN)) && defined(__i386__) /* darwin: 4.0.1 compiler option / bug? */
    __asm__ ("push  %%ebx\n\t"
             "cpuid\n\t"
             "pop   %%ebx\n\t"
             : "=a" (uOperator),
               "=c" (xCX)
             : "0" (uOperator)
             : "edx");
#  else
    __asm__ ("cpuid"
             : "=a" (uOperator),
               "=c" (xCX)
             : "0" (uOperator)
             : "ebx", "edx");

#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    __cpuid(aInfo, uOperator);
    xCX = aInfo[2];

# else
    __asm
    {
        push    ebx
        mov     eax, [uOperator]
        cpuid
        mov     [xCX], ecx
        pop     ebx
    }
# endif
    return (uint32_t)xCX;
}
#endif


/**
 * Checks if the current CPU supports CPUID.
 *
 * @returns true if CPUID is supported.
 */
DECLINLINE(bool) ASMHasCpuId(void)
{
#ifdef RT_ARCH_AMD64
    return true; /* ASSUME that all amd64 compatible CPUs have cpuid. */
#else /* !RT_ARCH_AMD64 */
    bool        fRet = false;
# if RT_INLINE_ASM_GNU_STYLE
    uint32_t    u1;
    uint32_t    u2;
    __asm__ ("pushf\n\t"
             "pop   %1\n\t"
             "mov   %1, %2\n\t"
             "xorl  $0x200000, %1\n\t"
             "push  %1\n\t"
             "popf\n\t"
             "pushf\n\t"
             "pop   %1\n\t"
             "cmpl  %1, %2\n\t"
             "setne %0\n\t"
             "push  %2\n\t"
             "popf\n\t"
             : "=m" (fRet), "=r" (u1), "=r" (u2));
# else
    __asm
    {
        pushfd
        pop     eax
        mov     ebx, eax
        xor     eax, 0200000h
        push    eax
        popfd
        pushfd
        pop     eax
        cmp     eax, ebx
        setne   fRet
        push    ebx
        popfd
    }
# endif
    return fRet;
#endif /* !RT_ARCH_AMD64 */
}


/**
 * Gets the APIC ID of the current CPU.
 *
 * @returns the APIC ID.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint8_t) ASMGetApicId(void);
#else
DECLINLINE(uint8_t) ASMGetApicId(void)
{
    RTCCUINTREG xBX;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    RTCCUINTREG uSpill;
    __asm__ ("cpuid"
             : "=a" (uSpill),
               "=b" (xBX)
             : "0" (1)
             : "rcx", "rdx");
#  elif (defined(PIC) || defined(RT_OS_DARWIN)) && defined(__i386__)
    RTCCUINTREG uSpill;
    __asm__ ("mov   %%ebx,%1\n\t"
             "cpuid\n\t"
             "xchgl %%ebx,%1\n\t"
             : "=a" (uSpill),
               "=r" (xBX)
             : "0" (1)
             : "ecx", "edx");
#  else
    RTCCUINTREG uSpill;
    __asm__ ("cpuid"
             : "=a" (uSpill),
               "=b" (xBX)
             : "0" (1)
             : "ecx", "edx");
#  endif

# elif RT_INLINE_ASM_USES_INTRIN
    int aInfo[4];
    __cpuid(aInfo, 1);
    xBX = aInfo[1];

# else
    __asm
    {
        push    ebx
        mov     eax, 1
        cpuid
        mov     [xBX], ebx
        pop     ebx
    }
# endif
    return (uint8_t)(xBX >> 24);
}
#endif

/**
 * Get cr0.
 * @returns cr0.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetCR0(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetCR0(void)
{
    RTCCUINTREG uCR0;
# if RT_INLINE_ASM_USES_INTRIN
    uCR0 = __readcr0();

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq  %%cr0, %0\t\n" : "=r" (uCR0));
#  else
    __asm__ ("movl  %%cr0, %0\t\n" : "=r" (uCR0));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, cr0
        mov     [uCR0], rax
#  else
        mov     eax, cr0
        mov     [uCR0], eax
#  endif
    }
# endif
    return uCR0;
}
#endif


/**
 * Sets the CR0 register.
 * @param   uCR0 The new CR0 value.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetCR0(RTCCUINTREG uCR0);
#else
DECLINLINE(void) ASMSetCR0(RTCCUINTREG uCR0)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writecr0(uCR0);

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq %0, %%cr0\n\t" :: "r" (uCR0));
#  else
    __asm__ __volatile__("movl %0, %%cr0\n\t" :: "r" (uCR0));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uCR0]
        mov     cr0, rax
#  else
        mov     eax, [uCR0]
        mov     cr0, eax
#  endif
    }
# endif
}
#endif


/**
 * Get cr2.
 * @returns cr2.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetCR2(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetCR2(void)
{
    RTCCUINTREG uCR2;
# if RT_INLINE_ASM_USES_INTRIN
    uCR2 = __readcr2();

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq  %%cr2, %0\t\n" : "=r" (uCR2));
#  else
    __asm__ ("movl  %%cr2, %0\t\n" : "=r" (uCR2));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, cr2
        mov     [uCR2], rax
#  else
        mov     eax, cr2
        mov     [uCR2], eax
#  endif
    }
# endif
    return uCR2;
}
#endif


/**
 * Sets the CR2 register.
 * @param   uCR2 The new CR0 value.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMSetCR2(RTCCUINTREG uCR2);
#else
DECLINLINE(void) ASMSetCR2(RTCCUINTREG uCR2)
{
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq %0, %%cr2\n\t" :: "r" (uCR2));
#  else
    __asm__ __volatile__("movl %0, %%cr2\n\t" :: "r" (uCR2));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uCR2]
        mov     cr2, rax
#  else
        mov     eax, [uCR2]
        mov     cr2, eax
#  endif
    }
# endif
}
#endif


/**
 * Get cr3.
 * @returns cr3.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetCR3(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetCR3(void)
{
    RTCCUINTREG uCR3;
# if RT_INLINE_ASM_USES_INTRIN
    uCR3 = __readcr3();

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq  %%cr3, %0\t\n" : "=r" (uCR3));
#  else
    __asm__ ("movl  %%cr3, %0\t\n" : "=r" (uCR3));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, cr3
        mov     [uCR3], rax
#  else
        mov     eax, cr3
        mov     [uCR3], eax
#  endif
    }
# endif
    return uCR3;
}
#endif


/**
 * Sets the CR3 register.
 *
 * @param   uCR3    New CR3 value.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetCR3(RTCCUINTREG uCR3);
#else
DECLINLINE(void) ASMSetCR3(RTCCUINTREG uCR3)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writecr3(uCR3);

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__ ("movq %0, %%cr3\n\t" : : "r" (uCR3));
#  else
    __asm__ __volatile__ ("movl %0, %%cr3\n\t" : : "r" (uCR3));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uCR3]
        mov     cr3, rax
#  else
        mov     eax, [uCR3]
        mov     cr3, eax
#  endif
    }
# endif
}
#endif


/**
 * Reloads the CR3 register.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMReloadCR3(void);
#else
DECLINLINE(void) ASMReloadCR3(void)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writecr3(__readcr3());

# elif RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG u;
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__ ("movq %%cr3, %0\n\t"
                          "movq %0, %%cr3\n\t"
                          : "=r" (u));
#  else
    __asm__ __volatile__ ("movl %%cr3, %0\n\t"
                          "movl %0, %%cr3\n\t"
                          : "=r" (u));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, cr3
        mov     cr3, rax
#  else
        mov     eax, cr3
        mov     cr3, eax
#  endif
    }
# endif
}
#endif


/**
 * Get cr4.
 * @returns cr4.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetCR4(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetCR4(void)
{
    RTCCUINTREG uCR4;
# if RT_INLINE_ASM_USES_INTRIN
    uCR4 = __readcr4();

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq  %%cr4, %0\t\n" : "=r" (uCR4));
#  else
    __asm__ ("movl  %%cr4, %0\t\n" : "=r" (uCR4));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, cr4
        mov     [uCR4], rax
#  else
        push    eax /* just in case */
        /*mov     eax, cr4*/
        _emit 0x0f
        _emit 0x20
        _emit 0xe0
        mov     [uCR4], eax
        pop     eax
#  endif
    }
# endif
    return uCR4;
}
#endif


/**
 * Sets the CR4 register.
 *
 * @param   uCR4    New CR4 value.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetCR4(RTCCUINTREG uCR4);
#else
DECLINLINE(void) ASMSetCR4(RTCCUINTREG uCR4)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writecr4(uCR4);

# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__ ("movq %0, %%cr4\n\t" : : "r" (uCR4));
#  else
    __asm__ __volatile__ ("movl %0, %%cr4\n\t" : : "r" (uCR4));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uCR4]
        mov     cr4, rax
#  else
        mov     eax, [uCR4]
        _emit   0x0F
        _emit   0x22
        _emit   0xE0        /* mov     cr4, eax */
#  endif
    }
# endif
}
#endif


/**
 * Get cr8.
 * @returns cr8.
 * @remark  The lock prefix hack for access from non-64-bit modes is NOT used and 0 is returned.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetCR8(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetCR8(void)
{
# ifdef RT_ARCH_AMD64
    RTCCUINTREG uCR8;
#  if RT_INLINE_ASM_USES_INTRIN
    uCR8 = __readcr8();

#  elif RT_INLINE_ASM_GNU_STYLE
    __asm__ ("movq  %%cr8, %0\t\n" : "=r" (uCR8));
#  else
    __asm
    {
        mov     rax, cr8
        mov     [uCR8], rax
    }
#  endif
    return uCR8;
# else /* !RT_ARCH_AMD64 */
    return 0;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Enables interrupts (EFLAGS.IF).
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMIntEnable(void);
#else
DECLINLINE(void) ASMIntEnable(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm("sti\n");
# elif RT_INLINE_ASM_USES_INTRIN
    _enable();
# else
    __asm sti
# endif
}
#endif


/**
 * Disables interrupts (!EFLAGS.IF).
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMIntDisable(void);
#else
DECLINLINE(void) ASMIntDisable(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm("cli\n");
# elif RT_INLINE_ASM_USES_INTRIN
    _disable();
# else
    __asm cli
# endif
}
#endif


/**
 * Disables interrupts and returns previous xFLAGS.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMIntDisableFlags(void);
#else
DECLINLINE(RTCCUINTREG) ASMIntDisableFlags(void)
{
    RTCCUINTREG xFlags;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("pushfq\n\t"
                         "cli\n\t"
                         "popq  %0\n\t"
                         : "=rm" (xFlags));
#  else
    __asm__ __volatile__("pushfl\n\t"
                         "cli\n\t"
                         "popl  %0\n\t"
                         : "=rm" (xFlags));
#  endif
# elif RT_INLINE_ASM_USES_INTRIN && !defined(RT_ARCH_X86)
    xFlags = ASMGetFlags();
    _disable();
# else
    __asm {
        pushfd
        cli
        pop  [xFlags]
    }
# endif
    return xFlags;
}
#endif


/**
 * Reads a machine specific register.
 *
 * @returns Register content.
 * @param   uRegister   Register to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMRdMsr(uint32_t uRegister);
#else
DECLINLINE(uint64_t) ASMRdMsr(uint32_t uRegister)
{
    RTUINT64U u;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ ("rdmsr\n\t"
             : "=a" (u.s.Lo),
               "=d" (u.s.Hi)
             : "c" (uRegister));

# elif RT_INLINE_ASM_USES_INTRIN
    u.u = __readmsr(uRegister);

# else
    __asm
    {
        mov     ecx, [uRegister]
        rdmsr
        mov     [u.s.Lo], eax
        mov     [u.s.Hi], edx
    }
# endif

    return u.u;
}
#endif


/**
 * Writes a machine specific register.
 *
 * @returns Register content.
 * @param   uRegister   Register to write to.
 * @param   u64Val      Value to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMWrMsr(uint32_t uRegister, uint64_t u64Val);
#else
DECLINLINE(void) ASMWrMsr(uint32_t uRegister, uint64_t u64Val)
{
    RTUINT64U u;

    u.u = u64Val;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("wrmsr\n\t"
                         ::"a" (u.s.Lo),
                           "d" (u.s.Hi),
                           "c" (uRegister));

# elif RT_INLINE_ASM_USES_INTRIN
    __writemsr(uRegister, u.u);

# else
    __asm
    {
        mov     ecx, [uRegister]
        mov     edx, [u.s.Hi]
        mov     eax, [u.s.Lo]
        wrmsr
    }
# endif
}
#endif


/**
 * Reads low part of a machine specific register.
 *
 * @returns Register content.
 * @param   uRegister   Register to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMRdMsr_Low(uint32_t uRegister);
#else
DECLINLINE(uint32_t) ASMRdMsr_Low(uint32_t uRegister)
{
    uint32_t u32;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ ("rdmsr\n\t"
             : "=a" (u32)
             : "c" (uRegister)
             : "edx");

# elif RT_INLINE_ASM_USES_INTRIN
    u32 = (uint32_t)__readmsr(uRegister);

#else
    __asm
    {
        mov     ecx, [uRegister]
        rdmsr
        mov     [u32], eax
    }
# endif

    return u32;
}
#endif


/**
 * Reads high part of a machine specific register.
 *
 * @returns Register content.
 * @param   uRegister   Register to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMRdMsr_High(uint32_t uRegister);
#else
DECLINLINE(uint32_t) ASMRdMsr_High(uint32_t uRegister)
{
    uint32_t    u32;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ ("rdmsr\n\t"
             : "=d" (u32)
             : "c" (uRegister)
             : "eax");

# elif RT_INLINE_ASM_USES_INTRIN
    u32 = (uint32_t)(__readmsr(uRegister) >> 32);

# else
    __asm
    {
        mov     ecx, [uRegister]
        rdmsr
        mov     [u32], edx
    }
# endif

    return u32;
}
#endif


/**
 * Gets dr7.
 *
 * @returns dr7.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTCCUINTREG) ASMGetDR7(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR7(void)
{
    RTCCUINTREG uDR7;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq   %%dr7, %0\n\t" : "=r" (uDR7));
#  else
    __asm__ ("movl   %%dr7, %0\n\t" : "=r" (uDR7));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr7
        mov     [uDR7], rax
#  else
        mov     eax, dr7
        mov     [uDR7], eax
#  endif
    }
# endif
    return uDR7;
}
#endif


/**
 * Gets dr6.
 *
 * @returns dr6.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTCCUINTREG) ASMGetDR6(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR6(void)
{
    RTCCUINTREG uDR6;
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq   %%dr6, %0\n\t" : "=r" (uDR6));
#  else
    __asm__ ("movl   %%dr6, %0\n\t" : "=r" (uDR6));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr6
        mov     [uDR6], rax
#  else
        mov     eax, dr6
        mov     [uDR6], eax
#  endif
    }
# endif
    return uDR6;
}
#endif


/**
 * Reads and clears DR6.
 *
 * @returns DR6.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(RTCCUINTREG) ASMGetAndClearDR6(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetAndClearDR6(void)
{
    RTCCUINTREG uDR6;
# if RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG uNewValue =  0xffff0ff0;  /* 31-16 and 4-11 are 1's, 12 and 63-31 are zero. */
#  ifdef RT_ARCH_AMD64
    __asm__ ("movq   %%dr6, %0\n\t"
             "movq   %1, %%dr6\n\t"
             : "=r" (uDR6)
             : "r" (uNewValue));
#  else
    __asm__ ("movl   %%dr6, %0\n\t"
             "movl   %1, %%dr6\n\t"
             : "=r" (uDR6)
             : "r" (uNewValue));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr6
        mov     [uDR6], rax
        mov     rcx, rax
        mov     ecx, 0ffff0ff0h;        /* 31-16 and 4-11 are 1's, 12 and 63-31 are zero. */
        mov     dr6, rcx
#  else
        mov     eax, dr6
        mov     [uDR6], eax
        mov     ecx, 0ffff0ff0h;        /* 31-16 and 4-11 are 1's, 12 is zero. */
        mov     dr6, ecx
#  endif
    }
# endif
    return uDR6;
}
#endif


/**
 * Compiler memory barrier.
 *
 * Ensure that the compiler does not use any cached (register/tmp stack) memory
 * values or any outstanding writes when returning from this function.
 *
 * This function must be used if non-volatile data is modified by a
 * device or the VMM. Typical cases are port access, MMIO access,
 * trapping instruction, etc.
 */
#if RT_INLINE_ASM_GNU_STYLE
# define ASMCompilerBarrier()   do { __asm__ __volatile__ ("" : : : "memory"); } while (0)
#elif RT_INLINE_ASM_USES_INTRIN
# define ASMCompilerBarrier()   do { _ReadWriteBarrier(); } while (0)
#else /* 2003 should have _ReadWriteBarrier() but I guess we're at 2002 level then... */
DECLINLINE(void) ASMCompilerBarrier(void)
{
    __asm
    {
    }
}
#endif


/**
 * Writes a 8-bit unsigned integer to an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   u8      8-bit integer to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutU8(RTIOPORT Port, uint8_t u8);
#else
DECLINLINE(void) ASMOutU8(RTIOPORT Port, uint8_t u8)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("outb %b1, %w0\n\t"
                         :: "Nd" (Port),
                            "a" (u8));

# elif RT_INLINE_ASM_USES_INTRIN
    __outbyte(Port, u8);

# else
    __asm
    {
        mov     dx, [Port]
        mov     al, [u8]
        out     dx, al
    }
# endif
}
#endif


/**
 * Gets a 8-bit unsigned integer from an I/O port, ordered.
 *
 * @returns 8-bit integer.
 * @param   Port    I/O port to read from.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint8_t) ASMInU8(RTIOPORT Port);
#else
DECLINLINE(uint8_t) ASMInU8(RTIOPORT Port)
{
    uint8_t u8;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("inb %w1, %b0\n\t"
                         : "=a" (u8)
                         : "Nd" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    u8 = __inbyte(Port);

# else
    __asm
    {
        mov     dx, [Port]
        in      al, dx
        mov     [u8], al
    }
# endif
    return u8;
}
#endif


/**
 * Writes a 16-bit unsigned integer to an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   u16     16-bit integer to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutU16(RTIOPORT Port, uint16_t u16);
#else
DECLINLINE(void) ASMOutU16(RTIOPORT Port, uint16_t u16)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("outw %w1, %w0\n\t"
                         :: "Nd" (Port),
                            "a" (u16));

# elif RT_INLINE_ASM_USES_INTRIN
    __outword(Port, u16);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ax, [u16]
        out     dx, ax
    }
# endif
}
#endif


/**
 * Gets a 16-bit unsigned integer from an I/O port, ordered.
 *
 * @returns 16-bit integer.
 * @param   Port    I/O port to read from.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint16_t) ASMInU16(RTIOPORT Port);
#else
DECLINLINE(uint16_t) ASMInU16(RTIOPORT Port)
{
    uint16_t u16;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("inw %w1, %w0\n\t"
                         : "=a" (u16)
                         : "Nd" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    u16 = __inword(Port);

# else
    __asm
    {
        mov     dx, [Port]
        in      ax, dx
        mov     [u16], ax
    }
# endif
    return u16;
}
#endif


/**
 * Writes a 32-bit unsigned integer to an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   u32     32-bit integer to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutU32(RTIOPORT Port, uint32_t u32);
#else
DECLINLINE(void) ASMOutU32(RTIOPORT Port, uint32_t u32)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("outl %1, %w0\n\t"
                         :: "Nd" (Port),
                            "a" (u32));

# elif RT_INLINE_ASM_USES_INTRIN
    __outdword(Port, u32);

# else
    __asm
    {
        mov     dx, [Port]
        mov     eax, [u32]
        out     dx, eax
    }
# endif
}
#endif


/**
 * Gets a 32-bit unsigned integer from an I/O port, ordered.
 *
 * @returns 32-bit integer.
 * @param   Port    I/O port to read from.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMInU32(RTIOPORT Port);
#else
DECLINLINE(uint32_t) ASMInU32(RTIOPORT Port)
{
    uint32_t u32;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("inl %w1, %0\n\t"
                         : "=a" (u32)
                         : "Nd" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    u32 = __indword(Port);

# else
    __asm
    {
        mov     dx, [Port]
        in      eax, dx
        mov     [u32], eax
    }
# endif
    return u32;
}
#endif

/** @todo string i/o */


/**
 * Atomically Exchange an unsigned 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to update.
 * @param   u8     The 8-bit value to assign to *pu8.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint8_t) ASMAtomicXchgU8(volatile uint8_t *pu8, uint8_t u8);
#else
DECLINLINE(uint8_t) ASMAtomicXchgU8(volatile uint8_t *pu8, uint8_t u8)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgb %0, %1\n\t"
                         : "=m" (*pu8),
                           "=q" (u8) /* =r - busted on g++ (GCC) 3.4.4 20050721 (Red Hat 3.4.4-2) */
                         : "1" (u8));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu8]
        mov     al, [u8]
        xchg    [rdx], al
        mov     [u8], al
#  else
        mov     edx, [pu8]
        mov     al, [u8]
        xchg    [edx], al
        mov     [u8], al
#  endif
    }
# endif
    return u8;
}
#endif


/**
 * Atomically Exchange a signed 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pi8     Pointer to the 8-bit variable to update.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(int8_t) ASMAtomicXchgS8(volatile int8_t *pi8, int8_t i8)
{
    return (int8_t)ASMAtomicXchgU8((volatile uint8_t *)pi8, (uint8_t)i8);
}


/**
 * Atomically Exchange a bool value, ordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the 8-bit variable to update.
 * @param   f       The 8-bit value to assign to *pi8.
 */
DECLINLINE(bool) ASMAtomicXchgBool(volatile bool *pf, bool f)
{
#ifdef _MSC_VER
    return !!ASMAtomicXchgU8((volatile uint8_t *)pf, (uint8_t)f);
#else
    return (bool)ASMAtomicXchgU8((volatile uint8_t *)pf, (uint8_t)f);
#endif
}


/**
 * Atomically Exchange an unsigned 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to update.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint16_t) ASMAtomicXchgU16(volatile uint16_t *pu16, uint16_t u16);
#else
DECLINLINE(uint16_t) ASMAtomicXchgU16(volatile uint16_t *pu16, uint16_t u16)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgw %0, %1\n\t"
                         : "=m" (*pu16),
                           "=r" (u16)
                         : "1" (u16));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu16]
        mov     ax, [u16]
        xchg    [rdx], ax
        mov     [u16], ax
#  else
        mov     edx, [pu16]
        mov     ax, [u16]
        xchg    [edx], ax
        mov     [u16], ax
#  endif
    }
# endif
    return u16;
}
#endif


/**
 * Atomically Exchange a signed 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pi16    Pointer to the 16-bit variable to update.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(int16_t) ASMAtomicXchgS16(volatile int16_t *pi16, int16_t i16)
{
    return (int16_t)ASMAtomicXchgU16((volatile uint16_t *)pi16, (uint16_t)i16);
}


/**
 * Atomically Exchange an unsigned 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to update.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicXchgU32(volatile uint32_t *pu32, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMAtomicXchgU32(volatile uint32_t *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgl %0, %1\n\t"
                         : "=m" (*pu32),
                           "=r" (u32)
                         : "1" (u32));

# elif RT_INLINE_ASM_USES_INTRIN
   u32 = _InterlockedExchange((long *)pu32, u32);

# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        mov     eax, u32
        xchg    [rdx], eax
        mov     [u32], eax
#  else
        mov     edx, [pu32]
        mov     eax, u32
        xchg    [edx], eax
        mov     [u32], eax
#  endif
    }
# endif
    return u32;
}
#endif


/**
 * Atomically Exchange a signed 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pi32    Pointer to the 32-bit variable to update.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(int32_t) ASMAtomicXchgS32(volatile int32_t *pi32, int32_t i32)
{
    return (int32_t)ASMAtomicXchgU32((volatile uint32_t *)pi32, (uint32_t)i32);
}


/**
 * Atomically Exchange an unsigned 64-bit value, ordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint64_t) ASMAtomicXchgU64(volatile uint64_t *pu64, uint64_t u64);
#else
DECLINLINE(uint64_t) ASMAtomicXchgU64(volatile uint64_t *pu64, uint64_t u64)
{
# if defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_USES_INTRIN
   u64 = _InterlockedExchange64((__int64 *)pu64, u64);

#  elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("xchgq %0, %1\n\t"
                         : "=m" (*pu64),
                           "=r" (u64)
                         : "1" (u64));
#  else
    __asm
    {
        mov     rdx, [pu64]
        mov     rax, [u64]
        xchg    [rdx], rax
        mov     [u64], rax
    }
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(RT_OS_DARWIN) /* darwin: 4.0.1 compiler option / bug? */
    uint32_t u32 = (uint32_t)u64;
    __asm__ __volatile__(/*"xchgl %%esi, %5\n\t"*/
                         "xchgl %%ebx, %3\n\t"
                         "1:\n\t"
                         "lock; cmpxchg8b (%5)\n\t"
                         "jnz 1b\n\t"
                         "xchgl %%ebx, %3\n\t"
                         /*"xchgl %%esi, %5\n\t"*/
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (*pu64),
                           "m" ( u32 ),
                           "c" ( (uint32_t)(u64 >> 32) ),
                           "S" (pu64) );
#   else /* !PIC */
    __asm__ __volatile__("1:\n\t"
                         "lock; cmpxchg8b %1\n\t"
                         "jnz 1b\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (*pu64),
                           "b" ( (uint32_t)u64 ),
                           "c" ( (uint32_t)(u64 >> 32) ));
#   endif
#  else
    __asm
    {
        mov     ebx, dword ptr [u64]
        mov     ecx, dword ptr [u64 + 4]
        mov     edi, pu64
        mov     eax, dword ptr [edi]
        mov     edx, dword ptr [edi + 4]
    retry:
        lock cmpxchg8b [edi]
        jnz retry
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically Exchange an signed 64-bit value, ordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(int64_t) ASMAtomicXchgS64(volatile int64_t *pi64, int64_t i64)
{
    return (int64_t)ASMAtomicXchgU64((volatile uint64_t *)pi64, (uint64_t)i64);
}


#ifdef RT_ARCH_AMD64
/**
 * Atomically Exchange an unsigned 128-bit value, ordered.
 *
 * @returns Current *pu128.
 * @param   pu128   Pointer to the 128-bit variable to update.
 * @param   u128    The 128-bit value to assign to *pu128.
 *
 * @remark  We cannot really assume that any hardware supports this. Nor do I have
 *          GAS support for it. So, for the time being we'll BREAK the atomic
 *          bit of this function and use two 64-bit exchanges instead.
 */
# if 0 /* see remark RT_INLINE_ASM_EXTERNAL */
DECLASM(uint128_t) ASMAtomicXchgU128(volatile uint128_t *pu128, uint128_t u128);
# else
DECLINLINE(uint128_t) ASMAtomicXchgU128(volatile uint128_t *pu128, uint128_t u128)
{
   if (true)/*ASMCpuId_ECX(1) & RT_BIT(13))*/
   {
       /** @todo this is clumsy code */
       RTUINT128U u128Ret;
       u128Ret.u = u128;
       u128Ret.s.Lo = ASMAtomicXchgU64(&((PRTUINT128U)(uintptr_t)pu128)->s.Lo, u128Ret.s.Lo);
       u128Ret.s.Hi = ASMAtomicXchgU64(&((PRTUINT128U)(uintptr_t)pu128)->s.Hi, u128Ret.s.Hi);
       return u128Ret.u;
   }
#if 0  /* later? */
   else
   {
#  if RT_INLINE_ASM_GNU_STYLE
        __asm__ __volatile__("1:\n\t"
                             "lock; cmpxchg8b %1\n\t"
                             "jnz 1b\n\t"
                             : "=A" (u128),
                               "=m" (*pu128)
                             : "0" (*pu128),
                               "b" ( (uint64_t)u128 ),
                               "c" ( (uint64_t)(u128 >> 64) ));
#  else
        __asm
        {
            mov     rbx, dword ptr [u128]
            mov     rcx, dword ptr [u128 + 8]
            mov     rdi, pu128
            mov     rax, dword ptr [rdi]
            mov     rdx, dword ptr [rdi + 8]
        retry:
            lock cmpxchg16b [rdi]
            jnz retry
            mov     dword ptr [u128], rax
            mov     dword ptr [u128 + 8], rdx
        }
#  endif
    }
    return u128;
#endif
}
# endif
#endif /* RT_ARCH_AMD64 */


/**
 * Atomically Exchange a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 */
#define ASMAtomicXchgSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicXchgU8((volatile uint8_t *)(void *)(pu), (uint8_t)(uNew)); break; \
            case 2: ASMAtomicXchgU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicXchgU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicXchgU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)


/**
 * Atomically Exchange a pointer value, ordered.
 *
 * @returns Current *ppv value
 * @param   ppv    Pointer to the pointer variable to update.
 * @param   pv     The pointer value to assign to *ppv.
 */
DECLINLINE(void *) ASMAtomicXchgPtr(void * volatile *ppv, void *pv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicXchgU32((volatile uint32_t *)(void *)ppv, (uint32_t)pv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicXchgU64((volatile uint64_t *)(void *)ppv, (uint64_t)pv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically Compare and Exchange an unsigned 32-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu32        Pointer to the value to update.
 * @param   u32New      The new value to assigned to *pu32.
 * @param   u32Old      The old value to *pu32 compare with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old)
{
# if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgl %2, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu32),
                           "=qm" (u8Ret)
                         : "r" (u32New),
                           "a" (u32Old));
    return (bool)u8Ret;

# elif RT_INLINE_ASM_USES_INTRIN
    return _InterlockedCompareExchange((long *)pu32, u32New, u32Old) == u32Old;

# else
    uint32_t u32Ret;
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
#  else
        mov     edx, [pu32]
#  endif
        mov     eax, [u32Old]
        mov     ecx, [u32New]
#  ifdef RT_ARCH_AMD64
        lock cmpxchg [rdx], ecx
#  else
        lock cmpxchg [edx], ecx
#  endif
        setz    al
        movzx   eax, al
        mov     [u32Ret], eax
    }
    return !!u32Ret;
# endif
}
#endif


/**
 * Atomically Compare and Exchange a signed 32-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi32        Pointer to the value to update.
 * @param   i32New      The new value to assigned to *pi32.
 * @param   i32Old      The old value to *pi32 compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgS32(volatile int32_t *pi32, const int32_t i32New, const int32_t i32Old)
{
    return ASMAtomicCmpXchgU32((volatile uint32_t *)pi32, (uint32_t)i32New, (uint32_t)i32Old);
}


/**
 * Atomically Compare and exchange an unsigned 64-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64New  The 64-bit value to assign to *pu64.
 * @param   u64Old  The value to compare with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old)
{
# if RT_INLINE_ASM_USES_INTRIN
   return _InterlockedCompareExchange64((__int64 *)pu64, u64New, u64Old) == u64Old;

# elif defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgq %2, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu64),
                           "=qm" (u8Ret)
                         : "r" (u64New),
                           "a" (u64Old));
    return (bool)u8Ret;
#  else
    bool fRet;
    __asm
    {
        mov     rdx, [pu32]
        mov     rax, [u64Old]
        mov     rcx, [u64New]
        lock cmpxchg [rdx], rcx
        setz    al
        mov     [fRet], al
    }
    return fRet;
#  endif
# else /* !RT_ARCH_AMD64 */
    uint32_t u32Ret;
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(RT_OS_DARWIN) /* darwin: 4.0.1 compiler option / bug? */
    uint32_t u32 = (uint32_t)u64New;
    uint32_t u32Spill;
    __asm__ __volatile__("xchgl %%ebx, %4\n\t"
                         "lock; cmpxchg8b (%6)\n\t"
                         "setz  %%al\n\t"
                         "xchgl %%ebx, %4\n\t"
                         "movzbl %%al, %%eax\n\t"
                         : "=a" (u32Ret),
                           "=d" (u32Spill),
                           "=m" (*pu64)
                         : "A" (u64Old),
                           "m" ( u32 ),
                           "c" ( (uint32_t)(u64New >> 32) ),
                           "S" (pu64) );
#   else /* !PIC */
    uint32_t u32Spill;
    __asm__ __volatile__("lock; cmpxchg8b %2\n\t"
                         "setz  %%al\n\t"
                         "movzbl %%al, %%eax\n\t"
                         : "=a" (u32Ret),
                           "=d" (u32Spill),
                           "=m" (*pu64)
                         : "A" (u64Old),
                           "b" ( (uint32_t)u64New ),
                           "c" ( (uint32_t)(u64New >> 32) ));
#   endif
    return (bool)u32Ret;
#  else
    __asm
    {
        mov     ebx, dword ptr [u64New]
        mov     ecx, dword ptr [u64New + 4]
        mov     edi, [pu64]
        mov     eax, dword ptr [u64Old]
        mov     edx, dword ptr [u64Old + 4]
        lock cmpxchg8b [edi]
        setz    al
        movzx   eax, al
        mov     dword ptr [u32Ret], eax
    }
    return !!u32Ret;
#  endif
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Atomically Compare and exchange a signed 64-bit value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pu64.
 * @param   i64Old  The value to compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgS64(volatile int64_t *pi64, const int64_t i64, const int64_t i64Old)
{
    return ASMAtomicCmpXchgU64((volatile uint64_t *)pi64, (uint64_t)i64, (uint64_t)i64Old);
}


/** @def ASMAtomicCmpXchgSize
 * Atomically Compare and Exchange a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu          Pointer to the value to update.
 * @param   uNew        The new value to assigned to *pu.
 * @param   uOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 */
#define ASMAtomicCmpXchgSize(pu, uNew, uOld, fRc) \
    do { \
        switch (sizeof(*(pu))) { \
            case 4: (fRc) = ASMAtomicCmpXchgU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew), (uint32_t)(uOld)); \
                break; \
            case 8: (fRc) = ASMAtomicCmpXchgU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew), (uint64_t)(uOld)); \
                break; \
            default: AssertMsgFailed(("ASMAtomicCmpXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
                (fRc) = false; \
                break; \
        } \
    } while (0)


/**
 * Atomically Compare and Exchange a pointer value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 */
DECLINLINE(bool) ASMAtomicCmpXchgPtr(void * volatile *ppv, void *pvNew, void *pvOld)
{
#if ARCH_BITS == 32
    return ASMAtomicCmpXchgU32((volatile uint32_t *)(void *)ppv, (uint32_t)pvNew, (uint32_t)pvOld);
#elif ARCH_BITS == 64
    return ASMAtomicCmpXchgU64((volatile uint64_t *)(void *)ppv, (uint64_t)pvNew, (uint64_t)pvOld);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically Compare and Exchange an unsigned 32-bit value, additionally
 * passes back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu32        Pointer to the value to update.
 * @param   u32New      The new value to assigned to *pu32.
 * @param   u32Old      The old value to *pu32 compare with.
 * @param   pu32Old     Pointer store the old value at.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgExU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old, uint32_t *pu32Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgExU32(volatile uint32_t *pu32, const uint32_t u32New, const uint32_t u32Old, uint32_t *pu32Old)
{
# if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgl %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu32),
                           "=qm" (u8Ret),
                           "=a" (*pu32Old)
                         : "r" (u32New),
                           "a" (u32Old));
    return (bool)u8Ret;

# elif RT_INLINE_ASM_USES_INTRIN
    return (*pu32Old =_InterlockedCompareExchange((long *)pu32, u32New, u32Old)) == u32Old;

# else
    uint32_t u32Ret;
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
#  else
        mov     edx, [pu32]
#  endif
        mov     eax, [u32Old]
        mov     ecx, [u32New]
#  ifdef RT_ARCH_AMD64
        lock cmpxchg [rdx], ecx
        mov     rdx, [pu32Old]
        mov     [rdx], eax
#  else
        lock cmpxchg [edx], ecx
        mov     edx, [pu32Old]
        mov     [edx], eax
#  endif
        setz    al
        movzx   eax, al
        mov     [u32Ret], eax
    }
    return !!u32Ret;
# endif
}
#endif


/**
 * Atomically Compare and Exchange a signed 32-bit value, additionally
 * passes back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi32        Pointer to the value to update.
 * @param   i32New      The new value to assigned to *pi32.
 * @param   i32Old      The old value to *pi32 compare with.
 * @param   pi32Old     Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExS32(volatile int32_t *pi32, const int32_t i32New, const int32_t i32Old, int32_t *pi32Old)
{
    return ASMAtomicCmpXchgExU32((volatile uint32_t *)pi32, (uint32_t)i32New, (uint32_t)i32Old, (uint32_t *)pi32Old);
}


/**
 * Atomically Compare and exchange an unsigned 64-bit value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pu64    Pointer to the 64-bit variable to update.
 * @param   u64New  The 64-bit value to assign to *pu64.
 * @param   u64Old  The value to compare with.
 * @param   pu64Old     Pointer store the old value at.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicCmpXchgExU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old, uint64_t *pu64Old);
#else
DECLINLINE(bool) ASMAtomicCmpXchgExU64(volatile uint64_t *pu64, const uint64_t u64New, const uint64_t u64Old, uint64_t *pu64Old)
{
# if RT_INLINE_ASM_USES_INTRIN
   return (*pu64Old =_InterlockedCompareExchange64((__int64 *)pu64, u64New, u64Old)) == u64Old;

# elif defined(RT_ARCH_AMD64)
#  if RT_INLINE_ASM_GNU_STYLE
    uint8_t u8Ret;
    __asm__ __volatile__("lock; cmpxchgq %3, %0\n\t"
                         "setz  %1\n\t"
                         : "=m" (*pu64),
                           "=qm" (u8Ret),
                           "=a" (*pu64Old)
                         : "r" (u64New),
                           "a" (u64Old));
    return (bool)u8Ret;
#  else
    bool fRet;
    __asm
    {
        mov     rdx, [pu32]
        mov     rax, [u64Old]
        mov     rcx, [u64New]
        lock cmpxchg [rdx], rcx
        mov     rdx, [pu64Old]
        mov     [rdx], rax
        setz    al
        mov     [fRet], al
    }
    return fRet;
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    uint64_t u64Ret;
#   if defined(PIC) || defined(RT_OS_DARWIN) /* darwin: 4.0.1 compiler option / bug? */
    /* NB: this code uses a memory clobber description, because the clean
     * solution with an output value for *pu64 makes gcc run out of registers.
     * This will cause suboptimal code, and anyone with a better solution is
     * welcome to improve this. */
    __asm__ __volatile__("xchgl %%ebx, %1\n\t"
                         "lock; cmpxchg8b %3\n\t"
                         "xchgl %%ebx, %1\n\t"
                         : "=A" (u64Ret)
                         : "DS" ((uint32_t)u64New),
                           "c" ((uint32_t)(u64New >> 32)),
                           "m" (*pu64),
                           "0" (u64Old)
                         : "memory" );
#   else /* !PIC */
    __asm__ __volatile__("lock; cmpxchg8b %4\n\t"
                         : "=A" (u64Ret),
                           "=m" (*pu64)
                         : "b" ((uint32_t)u64New),
                           "c" ((uint32_t)(u64New >> 32)),
                           "m" (*pu64),
                           "0" (u64Old));
#   endif
    *pu64Old = u64Ret;
    return u64Ret == u64Old;
#  else
    uint32_t u32Ret;
    __asm
    {
        mov     ebx, dword ptr [u64New]
        mov     ecx, dword ptr [u64New + 4]
        mov     edi, [pu64]
        mov     eax, dword ptr [u64Old]
        mov     edx, dword ptr [u64Old + 4]
        lock cmpxchg8b [edi]
        mov     ebx, [pu64Old]
        mov     [ebx], eax
        setz    al
        movzx   eax, al
        add     ebx, 4
        mov     [ebx], edx
        mov     dword ptr [u32Ret], eax
    }
    return !!u32Ret;
#  endif
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Atomically Compare and exchange a signed 64-bit value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   pi64    Pointer to the 64-bit variable to update.
 * @param   i64     The 64-bit value to assign to *pu64.
 * @param   i64Old  The value to compare with.
 * @param   pi64Old Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExS64(volatile int64_t *pi64, const int64_t i64, const int64_t i64Old, int64_t *pi64Old)
{
    return ASMAtomicCmpXchgExU64((volatile uint64_t *)pi64, (uint64_t)i64, (uint64_t)i64Old, (uint64_t *)pi64Old);
}


/** @def ASMAtomicCmpXchgExSize
 * Atomically Compare and Exchange a value which size might differ
 * between platforms or compilers. Additionally passes back old value.
 *
 * @param   pu          Pointer to the value to update.
 * @param   uNew        The new value to assigned to *pu.
 * @param   uOld        The old value to *pu compare with.
 * @param   fRc         Where to store the result.
 * @param   uOldVal     Where to store the old value.
 */
#define ASMAtomicCmpXchgExSize(pu, uNew, uOld, fRc, uOldVal) \
    do { \
        switch (sizeof(*(pu))) { \
            case 4: (fRc) = ASMAtomicCmpXchgExU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew), (uint32_t)(uOld), (uint32_t *)&(uOldVal)); \
                break; \
            case 8: (fRc) = ASMAtomicCmpXchgExU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew), (uint64_t)(uOld), (uint64_t *)&(uOldVal)); \
                break; \
            default: AssertMsgFailed(("ASMAtomicCmpXchgSize: size %d is not supported\n", sizeof(*(pu)))); \
                (fRc) = false; \
                (uOldVal) = 0; \
                break; \
        } \
    } while (0)


/**
 * Atomically Compare and Exchange a pointer value, additionally
 * passing back old value, ordered.
 *
 * @returns true if xchg was done.
 * @returns false if xchg wasn't done.
 *
 * @param   ppv         Pointer to the value to update.
 * @param   pvNew       The new value to assigned to *ppv.
 * @param   pvOld       The old value to *ppv compare with.
 * @param   ppvOld      Pointer store the old value at.
 */
DECLINLINE(bool) ASMAtomicCmpXchgExPtr(void * volatile *ppv, void *pvNew, void *pvOld, void **ppvOld)
{
#if ARCH_BITS == 32
    return ASMAtomicCmpXchgExU32((volatile uint32_t *)(void *)ppv, (uint32_t)pvNew, (uint32_t)pvOld, (uint32_t *)ppvOld);
#elif ARCH_BITS == 64
    return ASMAtomicCmpXchgExU64((volatile uint64_t *)(void *)ppv, (uint64_t)pvNew, (uint64_t)pvOld, (uint64_t *)ppvOld);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically exchanges and adds to a 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pu32        Pointer to the value.
 * @param   u32         Number to add.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicAddU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMAtomicAddU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedExchangeAdd((long *)pu32, u32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (u32)
                         : "memory");
    return u32;
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     [u32], eax
    }
    return u32;
# endif
}
#endif


/**
 * Atomically exchanges and adds to a signed 32-bit value, ordered.
 *
 * @returns The old value.
 * @param   pi32        Pointer to the value.
 * @param   i32         Number to add.
 */
DECLINLINE(int32_t) ASMAtomicAddS32(int32_t volatile *pi32, int32_t i32)
{
    return (int32_t)ASMAtomicAddU32((uint32_t volatile *)pi32, (uint32_t)i32);
}


/**
 * Atomically increment a 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu32        Pointer to the value to increment.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicIncU32(uint32_t volatile *pu32);
#else
DECLINLINE(uint32_t) ASMAtomicIncU32(uint32_t volatile *pu32)
{
    uint32_t u32;
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedIncrement((long *)pu32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (1)
                         : "memory");
    return u32+1;
# else
    __asm
    {
        mov     eax, 1
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     u32, eax
    }
    return u32+1;
# endif
}
#endif


/**
 * Atomically increment a signed 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi32        Pointer to the value to increment.
 */
DECLINLINE(int32_t) ASMAtomicIncS32(int32_t volatile *pi32)
{
    return (int32_t)ASMAtomicIncU32((uint32_t volatile *)pi32);
}


/**
 * Atomically decrement an unsigned 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pu32        Pointer to the value to decrement.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(uint32_t) ASMAtomicDecU32(uint32_t volatile *pu32);
#else
DECLINLINE(uint32_t) ASMAtomicDecU32(uint32_t volatile *pu32)
{
    uint32_t u32;
# if RT_INLINE_ASM_USES_INTRIN
    u32 = _InterlockedDecrement((long *)pu32);
    return u32;

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; xaddl %0, %1\n\t"
                         : "=r" (u32),
                           "=m" (*pu32)
                         : "0" (-1)
                         : "memory");
    return u32-1;
# else
    __asm
    {
        mov     eax, -1
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock xadd [rdx], eax
#  else
        mov     edx, [pu32]
        lock xadd [edx], eax
#  endif
        mov     u32, eax
    }
    return u32-1;
# endif
}
#endif


/**
 * Atomically decrement a signed 32-bit value, ordered.
 *
 * @returns The new value.
 * @param   pi32        Pointer to the value to decrement.
 */
DECLINLINE(int32_t) ASMAtomicDecS32(int32_t volatile *pi32)
{
    return (int32_t)ASMAtomicDecU32((uint32_t volatile *)pi32);
}


/**
 * Atomically Or an unsigned 32-bit value, ordered.
 *
 * @param   pu32   Pointer to the pointer variable to OR u32 with.
 * @param   u32    The value to OR *pu32 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicOrU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(void) ASMAtomicOrU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    _InterlockedOr((long volatile *)pu32, (long)u32);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; orl %1, %0\n\t"
                         : "=m" (*pu32)
                         : "ir" (u32));
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock    or [rdx], eax
#  else
        mov     edx, [pu32]
        lock    or [edx], eax
#  endif
    }
# endif
}
#endif


/**
 * Atomically Or a signed 32-bit value, ordered.
 *
 * @param   pi32   Pointer to the pointer variable to OR u32 with.
 * @param   i32    The value to OR *pu32 with.
 */
DECLINLINE(void) ASMAtomicOrS32(int32_t volatile *pi32, int32_t i32)
{
    ASMAtomicOrU32((uint32_t volatile *)pi32, i32);
}


/**
 * Atomically And an unsigned 32-bit value, ordered.
 *
 * @param   pu32   Pointer to the pointer variable to AND u32 with.
 * @param   u32    The value to AND *pu32 with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicAndU32(uint32_t volatile *pu32, uint32_t u32);
#else
DECLINLINE(void) ASMAtomicAndU32(uint32_t volatile *pu32, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    _InterlockedAnd((long volatile *)pu32, u32);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("lock; andl %1, %0\n\t"
                         : "=m" (*pu32)
                         : "ir" (u32));
# else
    __asm
    {
        mov     eax, [u32]
#  ifdef RT_ARCH_AMD64
        mov     rdx, [pu32]
        lock and [rdx], eax
#  else
        mov     edx, [pu32]
        lock and [edx], eax
#  endif
    }
# endif
}
#endif


/**
 * Atomically And a signed 32-bit value, ordered.
 *
 * @param   pi32   Pointer to the pointer variable to AND i32 with.
 * @param   i32    The value to AND *pi32 with.
 */
DECLINLINE(void) ASMAtomicAndS32(int32_t volatile *pi32, int32_t i32)
{
    ASMAtomicAndU32((uint32_t volatile *)pi32, (uint32_t)i32);
}


/**
 * Memory fence, waits for any pending writes and reads to complete.
 */
DECLINLINE(void) ASMMemoryFence(void)
{
    /** @todo use mfence? check if all cpus we care for support it. */
    uint32_t volatile u32;
    ASMAtomicXchgU32(&u32, 0);
}


/**
 * Write fence, waits for any pending writes to complete.
 */
DECLINLINE(void) ASMWriteFence(void)
{
    /** @todo use sfence? check if all cpus we care for support it. */
    ASMMemoryFence();
}


/**
 * Read fence, waits for any pending reads to complete.
 */
DECLINLINE(void) ASMReadFence(void)
{
    /** @todo use lfence? check if all cpus we care for support it. */
    ASMMemoryFence();
}


/**
 * Atomically reads an unsigned 8-bit value, ordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(uint8_t) ASMAtomicReadU8(volatile uint8_t *pu8)
{
    ASMMemoryFence();
    return *pu8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads an unsigned 8-bit value, unordered.
 *
 * @returns Current *pu8 value
 * @param   pu8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(uint8_t) ASMAtomicUoReadU8(volatile uint8_t *pu8)
{
    return *pu8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a signed 8-bit value, ordered.
 *
 * @returns Current *pi8 value
 * @param   pi8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(int8_t) ASMAtomicReadS8(volatile int8_t *pi8)
{
    ASMMemoryFence();
    return *pi8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a signed 8-bit value, unordered.
 *
 * @returns Current *pi8 value
 * @param   pi8    Pointer to the 8-bit variable to read.
 */
DECLINLINE(int8_t) ASMAtomicUoReadS8(volatile int8_t *pi8)
{
    return *pi8;    /* byte reads are atomic on x86 */
}


/**
 * Atomically reads an unsigned 16-bit value, ordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(uint16_t) ASMAtomicReadU16(volatile uint16_t *pu16)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pu16 & 1));
    return *pu16;
}


/**
 * Atomically reads an unsigned 16-bit value, unordered.
 *
 * @returns Current *pu16 value
 * @param   pu16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(uint16_t) ASMAtomicUoReadU16(volatile uint16_t *pu16)
{
    Assert(!((uintptr_t)pu16 & 1));
    return *pu16;
}


/**
 * Atomically reads a signed 16-bit value, ordered.
 *
 * @returns Current *pi16 value
 * @param   pi16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(int16_t) ASMAtomicReadS16(volatile int16_t *pi16)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pi16 & 1));
    return *pi16;
}


/**
 * Atomically reads a signed 16-bit value, unordered.
 *
 * @returns Current *pi16 value
 * @param   pi16    Pointer to the 16-bit variable to read.
 */
DECLINLINE(int16_t) ASMAtomicUoReadS16(volatile int16_t *pi16)
{
    Assert(!((uintptr_t)pi16 & 1));
    return *pi16;
}


/**
 * Atomically reads an unsigned 32-bit value, ordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(uint32_t) ASMAtomicReadU32(volatile uint32_t *pu32)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pu32 & 3));
    return *pu32;
}


/**
 * Atomically reads an unsigned 32-bit value, unordered.
 *
 * @returns Current *pu32 value
 * @param   pu32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(uint32_t) ASMAtomicUoReadU32(volatile uint32_t *pu32)
{
    Assert(!((uintptr_t)pu32 & 3));
    return *pu32;
}


/**
 * Atomically reads a signed 32-bit value, ordered.
 *
 * @returns Current *pi32 value
 * @param   pi32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(int32_t) ASMAtomicReadS32(volatile int32_t *pi32)
{
    ASMMemoryFence();
    Assert(!((uintptr_t)pi32 & 3));
    return *pi32;
}


/**
 * Atomically reads a signed 32-bit value, unordered.
 *
 * @returns Current *pi32 value
 * @param   pi32    Pointer to the 32-bit variable to read.
 */
DECLINLINE(int32_t) ASMAtomicUoReadS32(volatile int32_t *pi32)
{
    Assert(!((uintptr_t)pi32 & 3));
    return *pi32;
}


/**
 * Atomically reads an unsigned 64-bit value, ordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint64_t) ASMAtomicReadU64(volatile uint64_t *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicReadU64(volatile uint64_t *pu64)
{
    uint64_t u64;
# ifdef RT_ARCH_AMD64
#  if RT_INLINE_ASM_GNU_STYLE
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__(  "mfence\n\t"
                           "movq %1, %0\n\t"
                         : "=r" (u64)
                         : "m" (*pu64));
#  else
    __asm
    {
        mfence
        mov     rdx, [pu64]
        mov     rax, [rdx]
        mov     [u64], rax
    }
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(RT_OS_DARWIN) /* darwin: 4.0.1 compiler option / bug? */
    uint32_t u32EBX = 0;
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("xchgl %%ebx, %3\n\t"
                         "lock; cmpxchg8b (%5)\n\t"
                         "xchgl %%ebx, %3\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (0),
                           "m" (u32EBX),
                           "c" (0),
                           "S" (pu64));
#   else /* !PIC */
    __asm__ __volatile__("lock; cmpxchg8b %1\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (0),
                           "b" (0),
                           "c" (0));
#   endif
#  else
    Assert(!((uintptr_t)pu64 & 7));
    __asm
    {
        xor     eax, eax
        xor     edx, edx
        mov     edi, pu64
        xor     ecx, ecx
        xor     ebx, ebx
        lock cmpxchg8b [edi]
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically reads an unsigned 64-bit value, unordered.
 *
 * @returns Current *pu64 value
 * @param   pu64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint64_t) ASMAtomicUoReadU64(volatile uint64_t *pu64);
#else
DECLINLINE(uint64_t) ASMAtomicUoReadU64(volatile uint64_t *pu64)
{
    uint64_t u64;
# ifdef RT_ARCH_AMD64
#  if RT_INLINE_ASM_GNU_STYLE
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("movq %1, %0\n\t"
                         : "=r" (u64)
                         : "m" (*pu64));
#  else
    __asm
    {
        mov     rdx, [pu64]
        mov     rax, [rdx]
        mov     [u64], rax
    }
#  endif
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
#   if defined(PIC) || defined(RT_OS_DARWIN) /* darwin: 4.0.1 compiler option / bug? */
    uint32_t u32EBX = 0;
    Assert(!((uintptr_t)pu64 & 7));
    __asm__ __volatile__("xchgl %%ebx, %3\n\t"
                         "lock; cmpxchg8b (%5)\n\t"
                         "xchgl %%ebx, %3\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (0),
                           "m" (u32EBX),
                           "c" (0),
                           "S" (pu64));
#   else /* !PIC */
    __asm__ __volatile__("cmpxchg8b %1\n\t"
                         : "=A" (u64),
                           "=m" (*pu64)
                         : "0" (0),
                           "b" (0),
                           "c" (0));
#   endif
#  else
    Assert(!((uintptr_t)pu64 & 7));
    __asm
    {
        xor     eax, eax
        xor     edx, edx
        mov     edi, pu64
        xor     ecx, ecx
        xor     ebx, ebx
        lock cmpxchg8b [edi]
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
# endif /* !RT_ARCH_AMD64 */
    return u64;
}
#endif


/**
 * Atomically reads a signed 64-bit value, ordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
DECLINLINE(int64_t) ASMAtomicReadS64(volatile int64_t *pi64)
{
    return (int64_t)ASMAtomicReadU64((volatile uint64_t *)pi64);
}


/**
 * Atomically reads a signed 64-bit value, unordered.
 *
 * @returns Current *pi64 value
 * @param   pi64    Pointer to the 64-bit variable to read.
 *                  The memory pointed to must be writable.
 * @remark  This will fault if the memory is read-only!
 */
DECLINLINE(int64_t) ASMAtomicUoReadS64(volatile int64_t *pi64)
{
    return (int64_t)ASMAtomicUoReadU64((volatile uint64_t *)pi64);
}


/**
 * Atomically reads a pointer value, ordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 */
DECLINLINE(void *) ASMAtomicReadPtr(void * volatile *ppv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicReadU32((volatile uint32_t *)(void *)ppv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicReadU64((volatile uint64_t *)(void *)ppv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically reads a pointer value, unordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable to read.
 */
DECLINLINE(void *) ASMAtomicUoReadPtr(void * volatile *ppv)
{
#if ARCH_BITS == 32
    return (void *)ASMAtomicUoReadU32((volatile uint32_t *)(void *)ppv);
#elif ARCH_BITS == 64
    return (void *)ASMAtomicUoReadU64((volatile uint64_t *)(void *)ppv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically reads a boolean value, ordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the boolean variable to read.
 */
DECLINLINE(bool) ASMAtomicReadBool(volatile bool *pf)
{
    ASMMemoryFence();
    return *pf;     /* byte reads are atomic on x86 */
}


/**
 * Atomically reads a boolean value, unordered.
 *
 * @returns Current *pf value
 * @param   pf      Pointer to the boolean variable to read.
 */
DECLINLINE(bool) ASMAtomicUoReadBool(volatile bool *pf)
{
    return *pf;     /* byte reads are atomic on x86 */
}


/**
 * Atomically read a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   puRes   Where to store the result.
 */
#define ASMAtomicReadSize(pu, puRes) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: *(uint8_t  *)(puRes) = ASMAtomicReadU8( (volatile uint8_t  *)(void *)(pu)); break; \
            case 2: *(uint16_t *)(puRes) = ASMAtomicReadU16((volatile uint16_t *)(void *)(pu)); break; \
            case 4: *(uint32_t *)(puRes) = ASMAtomicReadU32((volatile uint32_t *)(void *)(pu)); break; \
            case 8: *(uint64_t *)(puRes) = ASMAtomicReadU64((volatile uint64_t *)(void *)(pu)); break; \
            default: AssertMsgFailed(("ASMAtomicReadSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)


/**
 * Atomically read a value which size might differ
 * between platforms or compilers, unordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   puRes   Where to store the result.
 */
#define ASMAtomicUoReadSize(pu, puRes) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: *(uint8_t  *)(puRes) = ASMAtomicUoReadU8( (volatile uint8_t  *)(void *)(pu)); break; \
            case 2: *(uint16_t *)(puRes) = ASMAtomicUoReadU16((volatile uint16_t *)(void *)(pu)); break; \
            case 4: *(uint32_t *)(puRes) = ASMAtomicUoReadU32((volatile uint32_t *)(void *)(pu)); break; \
            case 8: *(uint64_t *)(puRes) = ASMAtomicUoReadU64((volatile uint64_t *)(void *)(pu)); break; \
            default: AssertMsgFailed(("ASMAtomicReadSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)


/**
 * Atomically writes an unsigned 8-bit value, ordered.
 *
 * @param   pu8     Pointer to the 8-bit variable.
 * @param   u8      The 8-bit value to assign to *pu8.
 */
DECLINLINE(void) ASMAtomicWriteU8(volatile uint8_t *pu8, uint8_t u8)
{
    ASMAtomicXchgU8(pu8, u8);
}


/**
 * Atomically writes an unsigned 8-bit value, unordered.
 *
 * @param   pu8     Pointer to the 8-bit variable.
 * @param   u8      The 8-bit value to assign to *pu8.
 */
DECLINLINE(void) ASMAtomicUoWriteU8(volatile uint8_t *pu8, uint8_t u8)
{
    *pu8 = u8;      /* byte writes are atomic on x86 */
}


/**
 * Atomically writes a signed 8-bit value, ordered.
 *
 * @param   pi8     Pointer to the 8-bit variable to read.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(void) ASMAtomicWriteS8(volatile int8_t *pi8, int8_t i8)
{
    ASMAtomicXchgS8(pi8, i8);
}


/**
 * Atomically writes a signed 8-bit value, unordered.
 *
 * @param   pi8     Pointer to the 8-bit variable to read.
 * @param   i8      The 8-bit value to assign to *pi8.
 */
DECLINLINE(void) ASMAtomicUoWriteS8(volatile int8_t *pi8, int8_t i8)
{
    *pi8 = i8;      /* byte writes are atomic on x86 */
}


/**
 * Atomically writes an unsigned 16-bit value, ordered.
 *
 * @param   pu16    Pointer to the 16-bit variable.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
DECLINLINE(void) ASMAtomicWriteU16(volatile uint16_t *pu16, uint16_t u16)
{
    ASMAtomicXchgU16(pu16, u16);
}


/**
 * Atomically writes an unsigned 16-bit value, unordered.
 *
 * @param   pu16    Pointer to the 16-bit variable.
 * @param   u16     The 16-bit value to assign to *pu16.
 */
DECLINLINE(void) ASMAtomicUoWriteU16(volatile uint16_t *pu16, uint16_t u16)
{
    Assert(!((uintptr_t)pu16 & 1));
    *pu16 = u16;
}


/**
 * Atomically writes a signed 16-bit value, ordered.
 *
 * @param   pi16    Pointer to the 16-bit variable to read.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(void) ASMAtomicWriteS16(volatile int16_t *pi16, int16_t i16)
{
    ASMAtomicXchgS16(pi16, i16);
}


/**
 * Atomically writes a signed 16-bit value, unordered.
 *
 * @param   pi16    Pointer to the 16-bit variable to read.
 * @param   i16     The 16-bit value to assign to *pi16.
 */
DECLINLINE(void) ASMAtomicUoWriteS16(volatile int16_t *pi16, int16_t i16)
{
    Assert(!((uintptr_t)pi16 & 1));
    *pi16 = i16;
}


/**
 * Atomically writes an unsigned 32-bit value, ordered.
 *
 * @param   pu32    Pointer to the 32-bit variable.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
DECLINLINE(void) ASMAtomicWriteU32(volatile uint32_t *pu32, uint32_t u32)
{
    ASMAtomicXchgU32(pu32, u32);
}


/**
 * Atomically writes an unsigned 32-bit value, unordered.
 *
 * @param   pu32    Pointer to the 32-bit variable.
 * @param   u32     The 32-bit value to assign to *pu32.
 */
DECLINLINE(void) ASMAtomicUoWriteU32(volatile uint32_t *pu32, uint32_t u32)
{
    Assert(!((uintptr_t)pu32 & 3));
    *pu32 = u32;
}


/**
 * Atomically writes a signed 32-bit value, ordered.
 *
 * @param   pi32    Pointer to the 32-bit variable to read.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(void) ASMAtomicWriteS32(volatile int32_t *pi32, int32_t i32)
{
    ASMAtomicXchgS32(pi32, i32);
}


/**
 * Atomically writes a signed 32-bit value, unordered.
 *
 * @param   pi32    Pointer to the 32-bit variable to read.
 * @param   i32     The 32-bit value to assign to *pi32.
 */
DECLINLINE(void) ASMAtomicUoWriteS32(volatile int32_t *pi32, int32_t i32)
{
    Assert(!((uintptr_t)pi32 & 3));
    *pi32 = i32;
}


/**
 * Atomically writes an unsigned 64-bit value, ordered.
 *
 * @param   pu64    Pointer to the 64-bit variable.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
DECLINLINE(void) ASMAtomicWriteU64(volatile uint64_t *pu64, uint64_t u64)
{
    ASMAtomicXchgU64(pu64, u64);
}


/**
 * Atomically writes an unsigned 64-bit value, unordered.
 *
 * @param   pu64    Pointer to the 64-bit variable.
 * @param   u64     The 64-bit value to assign to *pu64.
 */
DECLINLINE(void) ASMAtomicUoWriteU64(volatile uint64_t *pu64, uint64_t u64)
{
    Assert(!((uintptr_t)pu64 & 7));
#if ARCH_BITS == 64
    *pu64 = u64;
#else
    ASMAtomicXchgU64(pu64, u64);
#endif
}


/**
 * Atomically writes a signed 64-bit value, ordered.
 *
 * @param   pi64    Pointer to the 64-bit variable.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(void) ASMAtomicWriteS64(volatile int64_t *pi64, int64_t i64)
{
    ASMAtomicXchgS64(pi64, i64);
}


/**
 * Atomically writes a signed 64-bit value, unordered.
 *
 * @param   pi64    Pointer to the 64-bit variable.
 * @param   i64     The 64-bit value to assign to *pi64.
 */
DECLINLINE(void) ASMAtomicUoWriteS64(volatile int64_t *pi64, int64_t i64)
{
    Assert(!((uintptr_t)pi64 & 7));
#if ARCH_BITS == 64
    *pi64 = i64;
#else
    ASMAtomicXchgS64(pi64, i64);
#endif
}


/**
 * Atomically writes a boolean value, unordered.
 *
 * @param   pf      Pointer to the boolean variable.
 * @param   f       The boolean value to assign to *pf.
 */
DECLINLINE(void) ASMAtomicWriteBool(volatile bool *pf, bool f)
{
    ASMAtomicWriteU8((uint8_t volatile *)pf, f);
}


/**
 * Atomically writes a boolean value, unordered.
 *
 * @param   pf      Pointer to the boolean variable.
 * @param   f       The boolean value to assign to *pf.
 */
DECLINLINE(void) ASMAtomicUoWriteBool(volatile bool *pf, bool f)
{
    *pf = f;    /* byte writes are atomic on x86 */
}


/**
 * Atomically writes a pointer value, ordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable.
 * @param   pv      The pointer value to assigne to *ppv.
 */
DECLINLINE(void) ASMAtomicWritePtr(void * volatile *ppv, void *pv)
{
#if ARCH_BITS == 32
    ASMAtomicWriteU32((volatile uint32_t *)(void *)ppv, (uint32_t)pv);
#elif ARCH_BITS == 64
    ASMAtomicWriteU64((volatile uint64_t *)(void *)ppv, (uint64_t)pv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically writes a pointer value, unordered.
 *
 * @returns Current *pv value
 * @param   ppv     Pointer to the pointer variable.
 * @param   pv      The pointer value to assigne to *ppv.
 */
DECLINLINE(void) ASMAtomicUoWritePtr(void * volatile *ppv, void *pv)
{
#if ARCH_BITS == 32
    ASMAtomicUoWriteU32((volatile uint32_t *)(void *)ppv, (uint32_t)pv);
#elif ARCH_BITS == 64
    ASMAtomicUoWriteU64((volatile uint64_t *)(void *)ppv, (uint64_t)pv);
#else
# error "ARCH_BITS is bogus"
#endif
}


/**
 * Atomically write a value which size might differ
 * between platforms or compilers, ordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 */
#define ASMAtomicWriteSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicWriteU8( (volatile uint8_t  *)(void *)(pu), (uint8_t )(uNew)); break; \
            case 2: ASMAtomicWriteU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicWriteU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicWriteU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicWriteSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)

/**
 * Atomically write a value which size might differ
 * between platforms or compilers, unordered.
 *
 * @param   pu      Pointer to the variable to update.
 * @param   uNew    The value to assign to *pu.
 */
#define ASMAtomicUoWriteSize(pu, uNew) \
    do { \
        switch (sizeof(*(pu))) { \
            case 1: ASMAtomicUoWriteU8( (volatile uint8_t  *)(void *)(pu), (uint8_t )(uNew)); break; \
            case 2: ASMAtomicUoWriteU16((volatile uint16_t *)(void *)(pu), (uint16_t)(uNew)); break; \
            case 4: ASMAtomicUoWriteU32((volatile uint32_t *)(void *)(pu), (uint32_t)(uNew)); break; \
            case 8: ASMAtomicUoWriteU64((volatile uint64_t *)(void *)(pu), (uint64_t)(uNew)); break; \
            default: AssertMsgFailed(("ASMAtomicWriteSize: size %d is not supported\n", sizeof(*(pu)))); \
        } \
    } while (0)




/**
 * Invalidate page.
 *
 * @param   pv      Address of the page to invalidate.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMInvalidatePage(void *pv);
#else
DECLINLINE(void) ASMInvalidatePage(void *pv)
{
# if RT_INLINE_ASM_USES_INTRIN
    __invlpg(pv);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("invlpg %0\n\t"
                         : : "m" (*(uint8_t *)pv));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pv]
        invlpg  [rax]
#  else
        mov     eax, [pv]
        invlpg  [eax]
#  endif
    }
# endif
}
#endif


#if defined(PAGE_SIZE) && !defined(NT_INCLUDED)
# if PAGE_SIZE != 0x1000
#  error "PAGE_SIZE is not 0x1000!"
# endif
#endif

/**
 * Zeros a 4K memory page.
 *
 * @param   pv  Pointer to the memory block. This must be page aligned.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemZeroPage(volatile void *pv);
# else
DECLINLINE(void) ASMMemZeroPage(volatile void *pv)
{
#  if RT_INLINE_ASM_USES_INTRIN
#   ifdef RT_ARCH_AMD64
    __stosq((unsigned __int64 *)pv, 0, /*PAGE_SIZE*/0x1000 / 8);
#   else
    __stosd((unsigned long *)pv, 0, /*PAGE_SIZE*/0x1000 / 4);
#   endif

#  elif RT_INLINE_ASM_GNU_STYLE
    RTUINTREG uDummy;
#   ifdef RT_ARCH_AMD64
    __asm__ __volatile__ ("rep stosq"
                          : "=D" (pv),
                            "=c" (uDummy)
                          : "0" (pv),
                            "c" (0x1000 >> 3),
                            "a" (0)
                          : "memory");
#   else
    __asm__ __volatile__ ("rep stosl"
                          : "=D" (pv),
                            "=c" (uDummy)
                          : "0" (pv),
                            "c" (0x1000 >> 2),
                            "a" (0)
                          : "memory");
#   endif
#  else
    __asm
    {
#   ifdef RT_ARCH_AMD64
        xor     rax, rax
        mov     ecx, 0200h
        mov     rdi, [pv]
        rep     stosq
#   else
        xor     eax, eax
        mov     ecx, 0400h
        mov     edi, [pv]
        rep     stosd
#   endif
    }
#  endif
}
# endif


/**
 * Zeros a memory block with a 32-bit aligned size.
 *
 * @param   pv  Pointer to the memory block.
 * @param   cb  Number of bytes in the block. This MUST be aligned on 32-bit!
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemZero32(volatile void *pv, size_t cb);
#else
DECLINLINE(void) ASMMemZero32(volatile void *pv, size_t cb)
{
# if RT_INLINE_ASM_USES_INTRIN
    __stosd((unsigned long *)pv, 0, cb >> 2);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("rep stosl"
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
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMMemFill32(volatile void *pv, size_t cb, uint32_t u32);
#else
DECLINLINE(void) ASMMemFill32(volatile void *pv, size_t cb, uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    __stosd((unsigned long *)pv, 0, cb >> 2);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("rep stosl"
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
 * Checks if a memory block is filled with the specified byte.
 *
 * This is a sort of inverted memchr.
 *
 * @returns Pointer to the byte which doesn't equal u8.
 * @returns NULL if all equal to u8.
 *
 * @param   pv      Pointer to the memory block.
 * @param   cb      Number of bytes in the block. This MUST be aligned on 32-bit!
 * @param   u8      The value it's supposed to be filled with.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void *) ASMMemIsAll8(void const *pv, size_t cb, uint8_t u8);
#else
DECLINLINE(void *) ASMMemIsAll8(void const *pv, size_t cb, uint8_t u8)
{
/** @todo rewrite this in inline assembly. */
    uint8_t const *pb = (uint8_t const *)pv;
    for (; cb; cb--, pb++)
        if (RT_UNLIKELY(*pb != u8))
            return (void *)pb;
    return NULL;
}
#endif



/**
 * Multiplies two unsigned 32-bit values returning an unsigned 64-bit result.
 *
 * @returns u32F1 * u32F2.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(uint64_t) ASMMult2xU32RetU64(uint32_t u32F1, uint32_t u32F2);
#else
DECLINLINE(uint64_t) ASMMult2xU32RetU64(uint32_t u32F1, uint32_t u32F2)
{
# ifdef RT_ARCH_AMD64
    return (uint64_t)u32F1 * u32F2;
# else /* !RT_ARCH_AMD64 */
    uint64_t u64;
#  if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("mull %%edx"
                         : "=A" (u64)
                         : "a" (u32F2), "d" (u32F1));
#  else
    __asm
    {
        mov     edx, [u32F1]
        mov     eax, [u32F2]
        mul     edx
        mov     dword ptr [u64], eax
        mov     dword ptr [u64 + 4], edx
    }
#  endif
    return u64;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Multiplies two signed 32-bit values returning a signed 64-bit result.
 *
 * @returns u32F1 * u32F2.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(int64_t) ASMMult2xS32RetS64(int32_t i32F1, int32_t i32F2);
#else
DECLINLINE(int64_t) ASMMult2xS32RetS64(int32_t i32F1, int32_t i32F2)
{
# ifdef RT_ARCH_AMD64
    return (int64_t)i32F1 * i32F2;
# else /* !RT_ARCH_AMD64 */
    int64_t i64;
#  if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("imull %%edx"
                         : "=A" (i64)
                         : "a" (i32F2), "d" (i32F1));
#  else
    __asm
    {
        mov     edx, [i32F1]
        mov     eax, [i32F2]
        imul    edx
        mov     dword ptr [i64], eax
        mov     dword ptr [i64 + 4], edx
    }
#  endif
    return i64;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Devides a 64-bit unsigned by a 32-bit unsigned returning an unsigned 32-bit result.
 *
 * @returns u64 / u32.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(uint32_t) ASMDivU64ByU32RetU32(uint64_t u64, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMDivU64ByU32RetU32(uint64_t u64, uint32_t u32)
{
# ifdef RT_ARCH_AMD64
    return (uint32_t)(u64 / u32);
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    RTUINTREG uDummy;
    __asm__ __volatile__("divl %3"
                         : "=a" (u32), "=d"(uDummy)
                         : "A" (u64), "r" (u32));
#  else
    __asm
    {
        mov     eax, dword ptr [u64]
        mov     edx, dword ptr [u64 + 4]
        mov     ecx, [u32]
        div     ecx
        mov     [u32], eax
    }
#  endif
    return u32;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Devides a 64-bit signed by a 32-bit signed returning a signed 32-bit result.
 *
 * @returns u64 / u32.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(int32_t) ASMDivS64ByS32RetS32(int64_t i64, int32_t i32);
#else
DECLINLINE(int32_t) ASMDivS64ByS32RetS32(int64_t i64, int32_t i32)
{
# ifdef RT_ARCH_AMD64
    return (int32_t)(i64 / i32);
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    RTUINTREG iDummy;
    __asm__ __volatile__("idivl %3"
                         : "=a" (i32), "=d"(iDummy)
                         : "A" (i64), "r" (i32));
#  else
    __asm
    {
        mov     eax, dword ptr [i64]
        mov     edx, dword ptr [i64 + 4]
        mov     ecx, [i32]
        idiv    ecx
        mov     [i32], eax
    }
#  endif
    return i32;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Multiple a 64-bit by a 32-bit integer and divide the result by a 32-bit integer
 * using a 96 bit intermediate result.
 * @note    Don't use 64-bit C arithmetic here since some gcc compilers generate references to
 *          __udivdi3 and __umoddi3 even if this inline function is not used.
 *
 * @returns (u64A * u32B) / u32C.
 * @param   u64A    The 64-bit value.
 * @param   u32B    The 32-bit value to multiple by A.
 * @param   u32C    The 32-bit value to divide A*B by.
 */
#if RT_INLINE_ASM_EXTERNAL || !defined(__GNUC__)
DECLASM(uint64_t) ASMMultU64ByU32DivByU32(uint64_t u64A, uint32_t u32B, uint32_t u32C);
#else
DECLINLINE(uint64_t) ASMMultU64ByU32DivByU32(uint64_t u64A, uint32_t u32B, uint32_t u32C)
{
# if RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    uint64_t u64Result, u64Spill;
    __asm__ __volatile__("mulq %2\n\t"
                         "divq %3\n\t"
                         : "=a" (u64Result),
                           "=d" (u64Spill)
                         : "r" ((uint64_t)u32B),
                           "r" ((uint64_t)u32C),
                           "0" (u64A),
                           "1" (0));
    return u64Result;
#  else
    uint32_t u32Dummy;
    uint64_t u64Result;
    __asm__ __volatile__("mull %%ecx       \n\t" /* eax = u64Lo.lo = (u64A.lo * u32B).lo
                                                    edx = u64Lo.hi = (u64A.lo * u32B).hi */
                         "xchg %%eax,%%esi \n\t" /* esi = u64Lo.lo
                                                    eax = u64A.hi */
                         "xchg %%edx,%%edi \n\t" /* edi = u64Low.hi
                                                    edx = u32C */
                         "xchg %%edx,%%ecx \n\t" /* ecx = u32C
                                                    edx = u32B */
                         "mull %%edx       \n\t" /* eax = u64Hi.lo = (u64A.hi * u32B).lo
                                                    edx = u64Hi.hi = (u64A.hi * u32B).hi */
                         "addl %%edi,%%eax \n\t" /* u64Hi.lo += u64Lo.hi */
                         "adcl $0,%%edx    \n\t" /* u64Hi.hi += carry */
                         "divl %%ecx       \n\t" /* eax = u64Hi / u32C
                                                    edx = u64Hi % u32C */
                         "movl %%eax,%%edi \n\t" /* edi = u64Result.hi = u64Hi / u32C */
                         "movl %%esi,%%eax \n\t" /* eax = u64Lo.lo */
                         "divl %%ecx       \n\t" /* u64Result.lo */
                         "movl %%edi,%%edx \n\t" /* u64Result.hi */
                         : "=A"(u64Result), "=c"(u32Dummy),
                           "=S"(u32Dummy), "=D"(u32Dummy)
                         : "a"((uint32_t)u64A),
                           "S"((uint32_t)(u64A >> 32)),
                           "c"(u32B),
                           "D"(u32C));
    return u64Result;
#  endif
# else
    RTUINT64U   u;
    uint64_t    u64Lo = (uint64_t)(u64A & 0xffffffff) * u32B;
    uint64_t    u64Hi = (uint64_t)(u64A >> 32)        * u32B;
    u64Hi  += (u64Lo >> 32);
    u.s.Hi = (uint32_t)(u64Hi / u32C);
    u.s.Lo = (uint32_t)((((u64Hi % u32C) << 32) + (u64Lo & 0xffffffff)) / u32C);
    return u.u;
# endif
}
#endif


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
#if RT_INLINE_ASM_EXTERNAL
DECLASM(uint8_t) ASMProbeReadByte(const void *pvByte);
#else
DECLINLINE(uint8_t) ASMProbeReadByte(const void *pvByte)
{
    /** @todo verify that the compiler actually doesn't optimize this away. (intel & gcc) */
    uint8_t u8;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("movb (%1), %0\n\t"
                         : "=r" (u8)
                         : "r" (pvByte));
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvByte]
        mov     al, [rax]
#  else
        mov     eax, [pvByte]
        mov     al, [eax]
#  endif
        mov     [u8], al
    }
# endif
    return u8;
}
#endif

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
DECLINLINE(void) ASMProbeReadBuffer(const void *pvBuf, size_t cbBuf)
{
    /** @todo verify that the compiler actually doesn't optimize this away. (intel & gcc) */
    /* the first byte */
    const uint8_t *pu8 = (const uint8_t *)pvBuf;
    ASMProbeReadByte(pu8);

    /* the pages in between pages. */
    while (cbBuf > /*PAGE_SIZE*/0x1000)
    {
        ASMProbeReadByte(pu8);
        cbBuf -= /*PAGE_SIZE*/0x1000;
        pu8   += /*PAGE_SIZE*/0x1000;
    }

    /* the last byte */
    ASMProbeReadByte(pu8 + cbBuf - 1);
}


/** @def ASMBreakpoint
 * Debugger Breakpoint.
 * @remark  In the gnu world we add a nop instruction after the int3 to
 *          force gdb to remain at the int3 source line.
 * @remark  The L4 kernel will try make sense of the breakpoint, thus the jmp.
 * @internal
 */
#if RT_INLINE_ASM_GNU_STYLE
# ifndef __L4ENV__
#  define ASMBreakpoint()       do { __asm__ __volatile__ ("int3\n\tnop"); } while (0)
# else
#  define ASMBreakpoint()       do { __asm__ __volatile__ ("int3; jmp 1f; 1:"); } while (0)
# endif
#else
# define ASMBreakpoint()        __debugbreak()
#endif



/** @defgroup grp_inline_bits   Bit Operations
 * @{
 */


/**
 * Sets a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitSet(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btsl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        bts     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        bts     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically sets a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMAtomicBitSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitSet(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _interlockedbittestandset((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btsl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock bts [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock bts [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Clears a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to clear.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitClear(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btrl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        btr     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        btr     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically clears a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to toggle set.
 * @remark  No memory barrier, take care on smp.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMAtomicBitClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitClear(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btrl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock btr [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock btr [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Toggles a bit in a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to toggle.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMBitToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMBitToggle(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_USES_INTRIN
    _bittestandcomplement((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btcl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        btc     [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        btc     [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Atomically toggles a bit in a bitmap, ordered.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and set.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMAtomicBitToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(void) ASMAtomicBitToggle(volatile void *pvBitmap, int32_t iBit)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btcl %1, %0"
                          : "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        mov     edx, [iBit]
        lock btc [rax], edx
#  else
        mov     eax, [pvBitmap]
        mov     edx, [iBit]
        lock btc [eax], edx
#  endif
    }
# endif
}
#endif


/**
 * Tests and sets a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTestAndSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btsl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        bts     [rax], edx
#  else
        mov     eax, [pvBitmap]
        bts     [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and sets a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to set.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicBitTestAndSet(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndSet(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _interlockedbittestandset((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btsl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock bts [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock bts [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests and clears a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and clear.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTestAndClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btrl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        btr     [rax], edx
#  else
        mov     eax, [pvBitmap]
        btr     [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and clears a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and clear.
 * @remark  No memory barrier, take care on smp.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMAtomicBitTestAndClear(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndClear(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _interlockedbittestandreset((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btrl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock btr [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock btr [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests and toggles a bit in a bitmap.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and toggle.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLINLINE(bool) ASMBitTestAndToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u8 = _bittestandcomplement((long *)pvBitmap, iBit);

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("btcl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov   edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov   rax, [pvBitmap]
        btc   [rax], edx
#  else
        mov   eax, [pvBitmap]
        btc   [eax], edx
#  endif
        setc  al
        and   eax, 1
        mov   [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Atomically tests and toggles a bit in a bitmap, ordered.
 *
 * @returns true if the bit was set.
 * @returns false if the bit was clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test and toggle.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(bool) ASMAtomicBitTestAndToggle(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMAtomicBitTestAndToggle(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ ("lock; btcl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov     edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov     rax, [pvBitmap]
        lock btc [rax], edx
#  else
        mov     eax, [pvBitmap]
        lock btc [eax], edx
#  endif
        setc    al
        and     eax, 1
        mov     [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Tests if a bit in a bitmap is set.
 *
 * @returns true if the bit is set.
 * @returns false if the bit is clear.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBit        The bit to test.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(bool) ASMBitTest(volatile void *pvBitmap, int32_t iBit);
#else
DECLINLINE(bool) ASMBitTest(volatile void *pvBitmap, int32_t iBit)
{
    union { bool f; uint32_t u32; uint8_t u8; } rc;
# if RT_INLINE_ASM_USES_INTRIN
    rc.u32 = _bittest((long *)pvBitmap, iBit);
# elif RT_INLINE_ASM_GNU_STYLE

    __asm__ __volatile__ ("btl %2, %1\n\t"
                          "setc %b0\n\t"
                          "andl $1, %0\n\t"
                          : "=q" (rc.u32),
                            "=m" (*(volatile long *)pvBitmap)
                          : "Ir" (iBit)
                          : "memory");
# else
    __asm
    {
        mov   edx, [iBit]
#  ifdef RT_ARCH_AMD64
        mov   rax, [pvBitmap]
        bt    [rax], edx
#  else
        mov   eax, [pvBitmap]
        bt    [eax], edx
#  endif
        setc  al
        and   eax, 1
        mov   [rc.u32], eax
    }
# endif
    return rc.f;
}
#endif


/**
 * Clears a bit range within a bitmap.
 *
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   iBitStart   The First bit to clear.
 * @param   iBitEnd     The first bit not to clear.
 */
DECLINLINE(void) ASMBitClearRange(volatile void *pvBitmap, int32_t iBitStart, int32_t iBitEnd)
{
    if (iBitStart < iBitEnd)
    {
        volatile uint32_t *pu32 = (volatile uint32_t *)pvBitmap + (iBitStart >> 5);
        int iStart = iBitStart & ~31;
        int iEnd   = iBitEnd & ~31;
        if (iStart == iEnd)
            *pu32 &= ((1 << (iBitStart & 31)) - 1) | ~((1 << (iBitEnd & 31)) - 1);
        else
        {
            /* bits in first dword. */
            if (iBitStart & 31)
            {
                *pu32 &= (1 << (iBitStart & 31)) - 1;
                pu32++;
                iBitStart = iStart + 32;
            }

            /* whole dword. */
            if (iBitStart != iEnd)
                ASMMemZero32(pu32, (iEnd - iBitStart) >> 3);

            /* bits in last dword. */
            if (iBitEnd & 31)
            {
                pu32 = (volatile uint32_t *)pvBitmap + (iBitEnd >> 5);
                *pu32 &= ~((1 << (iBitEnd & 31)) - 1);
            }
        }
    }
}


/**
 * Finds the first clear bit in a bitmap.
 *
 * @returns Index of the first zero bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(int) ASMBitFirstClear(volatile void *pvBitmap, uint32_t cBits);
#else
DECLINLINE(int) ASMBitFirstClear(volatile void *pvBitmap, uint32_t cBits)
{
    if (cBits)
    {
        int32_t iBit;
# if RT_INLINE_ASM_GNU_STYLE
        RTCCUINTREG uEAX, uECX, uEDI;
        cBits = RT_ALIGN_32(cBits, 32);
        __asm__ __volatile__("repe; scasl\n\t"
                             "je    1f\n\t"
#  ifdef RT_ARCH_AMD64
                             "lea   -4(%%rdi), %%rdi\n\t"
                             "xorl  (%%rdi), %%eax\n\t"
                             "subq  %5, %%rdi\n\t"
#  else
                             "lea   -4(%%edi), %%edi\n\t"
                             "xorl  (%%edi), %%eax\n\t"
                             "subl  %5, %%edi\n\t"
#  endif
                             "shll  $3, %%edi\n\t"
                             "bsfl  %%eax, %%edx\n\t"
                             "addl  %%edi, %%edx\n\t"
                             "1:\t\n"
                             : "=d" (iBit),
                               "=&c" (uECX),
                               "=&D" (uEDI),
                               "=&a" (uEAX)
                             : "0" (0xffffffff),
                               "mr" (pvBitmap),
                               "1" (cBits >> 5),
                               "2" (pvBitmap),
                               "3" (0xffffffff));
# else
        cBits = RT_ALIGN_32(cBits, 32);
        __asm
        {
#  ifdef RT_ARCH_AMD64
            mov     rdi, [pvBitmap]
            mov     rbx, rdi
#  else
            mov     edi, [pvBitmap]
            mov     ebx, edi
#  endif
            mov     edx, 0ffffffffh
            mov     eax, edx
            mov     ecx, [cBits]
            shr     ecx, 5
            repe    scasd
            je      done

#  ifdef RT_ARCH_AMD64
            lea     rdi, [rdi - 4]
            xor     eax, [rdi]
            sub     rdi, rbx
#  else
            lea     edi, [edi - 4]
            xor     eax, [edi]
            sub     edi, ebx
#  endif
            shl     edi, 3
            bsf     edx, eax
            add     edx, edi
        done:
            mov     [iBit], edx
        }
# endif
        return iBit;
    }
    return -1;
}
#endif


/**
 * Finds the next clear bit in a bitmap.
 *
 * @returns Index of the first zero bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 * @param   iBitPrev    The bit returned from the last search.
 *                      The search will start at iBitPrev + 1.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(int) ASMBitNextClear(volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev);
#else
DECLINLINE(int) ASMBitNextClear(volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    int iBit = ++iBitPrev & 31;
    pvBitmap = (volatile char *)pvBitmap + ((iBitPrev >> 5) << 2);
    cBits   -= iBitPrev & ~31;
    if (iBit)
    {
        /* inspect the first dword. */
        uint32_t u32 = (~*(volatile uint32_t *)pvBitmap) >> iBit;
# if RT_INLINE_ASM_USES_INTRIN
        unsigned long ulBit = 0;
        if (_BitScanForward(&ulBit, u32))
            return ulBit + iBitPrev;
        iBit = -1;
# else
#  if RT_INLINE_ASM_GNU_STYLE
        __asm__ __volatile__("bsf %1, %0\n\t"
                             "jnz 1f\n\t"
                             "movl $-1, %0\n\t"
                             "1:\n\t"
                             : "=r" (iBit)
                             : "r" (u32));
#  else
        __asm
        {
            mov     edx, [u32]
            bsf     eax, edx
            jnz     done
            mov     eax, 0ffffffffh
        done:
            mov     [iBit], eax
        }
#  endif
        if (iBit >= 0)
            return iBit + iBitPrev;
# endif
        /* Search the rest of the bitmap, if there is anything. */
        if (cBits > 32)
        {
            iBit = ASMBitFirstClear((volatile char *)pvBitmap + sizeof(uint32_t), cBits - 32);
            if (iBit >= 0)
                return iBit + (iBitPrev & ~31) + 32;
        }
    }
    else
    {
        /* Search the rest of the bitmap. */
        iBit = ASMBitFirstClear(pvBitmap, cBits);
        if (iBit >= 0)
            return iBit + (iBitPrev & ~31);
    }
    return iBit;
}
#endif


/**
 * Finds the first set bit in a bitmap.
 *
 * @returns Index of the first set bit.
 * @returns -1 if no clear bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(int) ASMBitFirstSet(volatile void *pvBitmap, uint32_t cBits);
#else
DECLINLINE(int) ASMBitFirstSet(volatile void *pvBitmap, uint32_t cBits)
{
    if (cBits)
    {
        int32_t iBit;
# if RT_INLINE_ASM_GNU_STYLE
        RTCCUINTREG uEAX, uECX, uEDI;
        cBits = RT_ALIGN_32(cBits, 32);
        __asm__ __volatile__("repe; scasl\n\t"
                             "je    1f\n\t"
#  ifdef RT_ARCH_AMD64
                             "lea   -4(%%rdi), %%rdi\n\t"
                             "movl  (%%rdi), %%eax\n\t"
                             "subq  %5, %%rdi\n\t"
#  else
                             "lea   -4(%%edi), %%edi\n\t"
                             "movl  (%%edi), %%eax\n\t"
                             "subl  %5, %%edi\n\t"
#  endif
                             "shll  $3, %%edi\n\t"
                             "bsfl  %%eax, %%edx\n\t"
                             "addl  %%edi, %%edx\n\t"
                             "1:\t\n"
                             : "=d" (iBit),
                               "=&c" (uECX),
                               "=&D" (uEDI),
                               "=&a" (uEAX)
                             : "0" (0xffffffff),
                               "mr" (pvBitmap),
                               "1" (cBits >> 5),
                               "2" (pvBitmap),
                               "3" (0));
# else
        cBits = RT_ALIGN_32(cBits, 32);
        __asm
        {
#  ifdef RT_ARCH_AMD64
            mov     rdi, [pvBitmap]
            mov     rbx, rdi
#  else
            mov     edi, [pvBitmap]
            mov     ebx, edi
#  endif
            mov     edx, 0ffffffffh
            xor     eax, eax
            mov     ecx, [cBits]
            shr     ecx, 5
            repe    scasd
            je      done
#  ifdef RT_ARCH_AMD64
            lea     rdi, [rdi - 4]
            mov     eax, [rdi]
            sub     rdi, rbx
#  else
            lea     edi, [edi - 4]
            mov     eax, [edi]
            sub     edi, ebx
#  endif
            shl     edi, 3
            bsf     edx, eax
            add     edx, edi
        done:
            mov   [iBit], edx
        }
# endif
        return iBit;
    }
    return -1;
}
#endif


/**
 * Finds the next set bit in a bitmap.
 *
 * @returns Index of the next set bit.
 * @returns -1 if no set bit was found.
 * @param   pvBitmap    Pointer to the bitmap.
 * @param   cBits       The number of bits in the bitmap. Multiple of 32.
 * @param   iBitPrev    The bit returned from the last search.
 *                      The search will start at iBitPrev + 1.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(int) ASMBitNextSet(volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev);
#else
DECLINLINE(int) ASMBitNextSet(volatile void *pvBitmap, uint32_t cBits, uint32_t iBitPrev)
{
    int iBit = ++iBitPrev & 31;
    pvBitmap = (volatile char *)pvBitmap + ((iBitPrev >> 5) << 2);
    cBits   -= iBitPrev & ~31;
    if (iBit)
    {
        /* inspect the first dword. */
        uint32_t u32 = *(volatile uint32_t *)pvBitmap >> iBit;
# if RT_INLINE_ASM_USES_INTRIN
        unsigned long ulBit = 0;
        if (_BitScanForward(&ulBit, u32))
            return ulBit + iBitPrev;
        iBit = -1;
# else
#  if RT_INLINE_ASM_GNU_STYLE
        __asm__ __volatile__("bsf %1, %0\n\t"
                             "jnz 1f\n\t"
                             "movl $-1, %0\n\t"
                             "1:\n\t"
                             : "=r" (iBit)
                             : "r" (u32));
#  else
        __asm
        {
            mov     edx, u32
            bsf     eax, edx
            jnz     done
            mov     eax, 0ffffffffh
        done:
            mov     [iBit], eax
        }
#  endif
        if (iBit >= 0)
            return iBit + iBitPrev;
# endif
        /* Search the rest of the bitmap, if there is anything. */
        if (cBits > 32)
        {
            iBit = ASMBitFirstSet((volatile char *)pvBitmap + sizeof(uint32_t), cBits - 32);
            if (iBit >= 0)
                return iBit + (iBitPrev & ~31) + 32;
        }

    }
    else
    {
        /* Search the rest of the bitmap. */
        iBit = ASMBitFirstSet(pvBitmap, cBits);
        if (iBit >= 0)
            return iBit + (iBitPrev & ~31);
    }
    return iBit;
}
#endif


/**
 * Finds the first bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the first set bit.
 * @returns 0 if all bits are cleared.
 * @param   u32     Integer to search for set bits.
 * @remark  Similar to ffs() in BSD.
 */
DECLINLINE(unsigned) ASMBitFirstSetU32(uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    unsigned long iBit;
    if (_BitScanForward(&iBit, u32))
        iBit++;
    else
        iBit = 0;
# elif RT_INLINE_ASM_GNU_STYLE
    uint32_t iBit;
    __asm__ __volatile__("bsf  %1, %0\n\t"
                         "jnz  1f\n\t"
                         "xorl %0, %0\n\t"
                         "jmp  2f\n"
                         "1:\n\t"
                         "incl %0\n"
                         "2:\n\t"
                         : "=r" (iBit)
                         : "rm" (u32));
# else
    uint32_t iBit;
    _asm
    {
        bsf     eax, [u32]
        jnz     found
        xor     eax, eax
        jmp     done
    found:
        inc     eax
    done:
        mov     [iBit], eax
    }
# endif
    return iBit;
}


/**
 * Finds the first bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the first set bit.
 * @returns 0 if all bits are cleared.
 * @param   i32     Integer to search for set bits.
 * @remark  Similar to ffs() in BSD.
 */
DECLINLINE(unsigned) ASMBitFirstSetS32(int32_t i32)
{
    return ASMBitFirstSetU32((uint32_t)i32);
}


/**
 * Finds the last bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the last set bit.
 * @returns 0 if all bits are cleared.
 * @param   u32     Integer to search for set bits.
 * @remark  Similar to fls() in BSD.
 */
DECLINLINE(unsigned) ASMBitLastSetU32(uint32_t u32)
{
# if RT_INLINE_ASM_USES_INTRIN
    unsigned long iBit;
    if (_BitScanReverse(&iBit, u32))
        iBit++;
    else
        iBit = 0;
# elif RT_INLINE_ASM_GNU_STYLE
    uint32_t iBit;
    __asm__ __volatile__("bsrl %1, %0\n\t"
                         "jnz   1f\n\t"
                         "xorl %0, %0\n\t"
                         "jmp  2f\n"
                         "1:\n\t"
                         "incl %0\n"
                         "2:\n\t"
                         : "=r" (iBit)
                         : "rm" (u32));
# else
    uint32_t iBit;
    _asm
    {
        bsr     eax, [u32]
        jnz     found
        xor     eax, eax
        jmp     done
    found:
        inc     eax
    done:
        mov     [iBit], eax
    }
# endif
    return iBit;
}


/**
 * Finds the last bit which is set in the given 32-bit integer.
 * Bits are numbered from 1 (least significant) to 32.
 *
 * @returns index [1..32] of the last set bit.
 * @returns 0 if all bits are cleared.
 * @param   i32     Integer to search for set bits.
 * @remark  Similar to fls() in BSD.
 */
DECLINLINE(unsigned) ASMBitLastSetS32(int32_t i32)
{
    return ASMBitLastSetS32((uint32_t)i32);
}


/**
 * Reverse the byte order of the given 32-bit integer.
 * @param  u32      Integer
 */
DECLINLINE(uint32_t) ASMByteSwapU32(uint32_t u32)
{
#if RT_INLINE_ASM_USES_INTRIN
    u32 = _byteswap_ulong(u32);
#elif RT_INLINE_ASM_GNU_STYLE
    __asm__ ("bswapl %0" : "=r" (u32) : "0" (u32));
#else
    _asm
    {
        mov     eax, [u32]
        bswap   eax
        mov     [u32], eax
    }
#endif
    return u32;
}

/** @} */


/** @} */
#endif

