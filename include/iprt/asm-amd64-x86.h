/** @file
 * IPRT - AMD64 and x86 Specific Assembly Functions.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

#ifndef ___iprt_asm_amd64_x86_h
#define ___iprt_asm_amd64_x86_h

/* We depend on several defines and pragmas that live in iprt/asm.h. */
#include <iprt/asm.h>

#ifdef _MSC_VER
# if _MSC_VER >= 1400 && RT_INLINE_ASM_USES_INTRIN
   /* Emit the intrinsics at all optimization levels. */
#  pragma intrinsic(_ReadWriteBarrier)
#  pragma intrinsic(__cpuid)
#  pragma intrinsic(_enable)
#  pragma intrinsic(_disable)
#  pragma intrinsic(__rdtsc)
#  pragma intrinsic(__readmsr)
#  pragma intrinsic(__writemsr)
#  pragma intrinsic(__outbyte)
#  pragma intrinsic(__outbytestring)
#  pragma intrinsic(__outword)
#  pragma intrinsic(__outwordstring)
#  pragma intrinsic(__outdword)
#  pragma intrinsic(__outdwordstring)
#  pragma intrinsic(__inbyte)
#  pragma intrinsic(__inbytestring)
#  pragma intrinsic(__inword)
#  pragma intrinsic(__inwordstring)
#  pragma intrinsic(__indword)
#  pragma intrinsic(__indwordstring)
#  pragma intrinsic(__invlpg)
#  pragma intrinsic(__wbinvd)
#  pragma intrinsic(__readcr0)
#  pragma intrinsic(__readcr2)
#  pragma intrinsic(__readcr3)
#  pragma intrinsic(__readcr4)
#  pragma intrinsic(__writecr0)
#  pragma intrinsic(__writecr3)
#  pragma intrinsic(__writecr4)
#  pragma intrinsic(__readdr)
#  pragma intrinsic(__writedr)
#  ifdef RT_ARCH_AMD64
#   pragma intrinsic(__readcr8)
#   pragma intrinsic(__writecr8)
#  endif
# endif
#endif



/** @defgroup grp_rt_asm_amd64_x86  AMD64 and x86 Specific ASM Routines
 * @ingroup grp_rt_asm
 * @{
 */

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
    __asm__ __volatile__("sidt %0" : "=m" (*pIdtr));
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
    __asm__ __volatile__("lidt %0" : : "m" (*pIdtr));
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
    __asm__ __volatile__("sgdt %0" : "=m" (*pGdtr));
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
                         : "=r" (uFlags));
#  else
    __asm__ __volatile__("pushfl\n\t"
                         "popl  %0\n\t"
                         : "=r" (uFlags));
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
    __asm__ __volatile__("rdtsc\n\t" : "=a" (u.s.Lo), "=d" (u.s.Hi));
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
#  elif (defined(PIC) || defined(__PIC__)) && defined(__i386__)
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
#  elif (defined(PIC) || defined(__PIC__)) && defined(__i386__)
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
#  elif (defined(PIC) || defined(__PIC__)) && defined(__i386__)
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
 * Tests if it a genuine Intel CPU based on the ASMCpuId(0) output.
 *
 * @returns true/false.
 * @param   uEBX    EBX return from ASMCpuId(0)
 * @param   uECX    ECX return from ASMCpuId(0)
 * @param   uEDX    EDX return from ASMCpuId(0)
 */
DECLINLINE(bool) ASMIsIntelCpuEx(uint32_t uEBX, uint32_t uECX, uint32_t uEDX)
{
    return uEBX == UINT32_C(0x756e6547)
        && uECX == UINT32_C(0x6c65746e)
        && uEDX == UINT32_C(0x49656e69);
}


/**
 * Tests if this is a genuine Intel CPU.
 *
 * @returns true/false.
 * @remarks ASSUMES that cpuid is supported by the CPU.
 */
DECLINLINE(bool) ASMIsIntelCpu(void)
{
    uint32_t uEAX, uEBX, uECX, uEDX;
    ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
    return ASMIsIntelCpuEx(uEBX, uECX, uEDX);
}


/**
 * Tests if it a authentic AMD CPU based on the ASMCpuId(0) output.
 *
 * @returns true/false.
 * @param   uEBX    EBX return from ASMCpuId(0)
 * @param   uECX    ECX return from ASMCpuId(0)
 * @param   uEDX    EDX return from ASMCpuId(0)
 */
DECLINLINE(bool) ASMIsAmdCpuEx(uint32_t uEBX, uint32_t uECX, uint32_t uEDX)
{
    return uEBX == UINT32_C(0x68747541)
        && uECX == UINT32_C(0x444d4163)
        && uEDX == UINT32_C(0x69746e65);
}


/**
 * Tests if this is an authentic AMD CPU.
 *
 * @returns true/false.
 * @remarks ASSUMES that cpuid is supported by the CPU.
 */
DECLINLINE(bool) ASMIsAmdCpu(void)
{
    uint32_t uEAX, uEBX, uECX, uEDX;
    ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
    return ASMIsAmdCpuEx(uEBX, uECX, uEDX);
}


/**
 * Extracts the CPU family from ASMCpuId(1) or ASMCpuId(0x80000001)
 *
 * @returns Family.
 * @param   uEAX    EAX return from ASMCpuId(1) or ASMCpuId(0x80000001).
 */
DECLINLINE(uint32_t) ASMGetCpuFamily(uint32_t uEAX)
{
    return ((uEAX >> 8) & 0xf) == 0xf
         ? ((uEAX >> 20) & 0x7f) + 0xf
         : ((uEAX >> 8) & 0xf);
}


/**
 * Extracts the CPU model from ASMCpuId(1) or ASMCpuId(0x80000001), Intel variant.
 *
 * @returns Model.
 * @param   uEAX    EAX from ASMCpuId(1) or ASMCpuId(0x80000001).
 */
DECLINLINE(uint32_t) ASMGetCpuModelIntel(uint32_t uEAX)
{
    return ((uEAX >> 8) & 0xf) == 0xf || (((uEAX >> 8) & 0xf) == 0x6) /* family! */
         ? ((uEAX >> 4) & 0xf) | ((uEAX >> 12) & 0xf0)
         : ((uEAX >> 4) & 0xf);
}


/**
 * Extracts the CPU model from ASMCpuId(1) or ASMCpuId(0x80000001), AMD variant.
 *
 * @returns Model.
 * @param   uEAX    EAX from ASMCpuId(1) or ASMCpuId(0x80000001).
 */
DECLINLINE(uint32_t) ASMGetCpuModelAMD(uint32_t uEAX)
{
    return ((uEAX >> 8) & 0xf) == 0xf
         ? ((uEAX >> 4) & 0xf) | ((uEAX >> 12) & 0xf0)
         : ((uEAX >> 4) & 0xf);
}


/**
 * Extracts the CPU model from ASMCpuId(1) or ASMCpuId(0x80000001)
 *
 * @returns Model.
 * @param   uEAX    EAX from ASMCpuId(1) or ASMCpuId(0x80000001).
 * @param   fIntel  Whether it's an intel CPU. Use ASMIsIntelCpuEx() or ASMIsIntelCpu().
 */
DECLINLINE(uint32_t) ASMGetCpuModel(uint32_t uEAX, bool fIntel)
{
    return ((uEAX >> 8) & 0xf) == 0xf || (((uEAX >> 8) & 0xf) == 0x6 && fIntel) /* family! */
         ? ((uEAX >> 4) & 0xf) | ((uEAX >> 12) & 0xf0)
         : ((uEAX >> 4) & 0xf);
}


/**
 * Extracts the CPU stepping from ASMCpuId(1) or ASMCpuId(0x80000001)
 *
 * @returns Model.
 * @param   uEAX    EAX from ASMCpuId(1) or ASMCpuId(0x80000001).
 */
DECLINLINE(uint32_t) ASMGetCpuStepping(uint32_t uEAX)
{
    return uEAX & 0xf;
}


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
    __asm__ __volatile__("movq  %%cr0, %0\t\n" : "=r" (uCR0));
#  else
    __asm__ __volatile__("movl  %%cr0, %0\t\n" : "=r" (uCR0));
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
    __asm__ __volatile__("movq  %%cr2, %0\t\n" : "=r" (uCR2));
#  else
    __asm__ __volatile__("movl  %%cr2, %0\t\n" : "=r" (uCR2));
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
    __asm__ __volatile__("movq  %%cr3, %0\t\n" : "=r" (uCR3));
#  else
    __asm__ __volatile__("movl  %%cr3, %0\t\n" : "=r" (uCR3));
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
    __asm__ __volatile__("movq %0, %%cr3\n\t" : : "r" (uCR3));
#  else
    __asm__ __volatile__("movl %0, %%cr3\n\t" : : "r" (uCR3));
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
    __asm__ __volatile__("movq %%cr3, %0\n\t"
                         "movq %0, %%cr3\n\t"
                         : "=r" (u));
#  else
    __asm__ __volatile__("movl %%cr3, %0\n\t"
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
    __asm__ __volatile__("movq  %%cr4, %0\t\n" : "=r" (uCR4));
#  else
    __asm__ __volatile__("movl  %%cr4, %0\t\n" : "=r" (uCR4));
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
        _emit   0x0f
        _emit   0x20
        _emit   0xe0
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
    __asm__ __volatile__("movq %0, %%cr4\n\t" : : "r" (uCR4));
#  else
    __asm__ __volatile__("movl %0, %%cr4\n\t" : : "r" (uCR4));
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
    __asm__ __volatile__("movq  %%cr8, %0\t\n" : "=r" (uCR8));
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
                         : "=r" (xFlags));
#  else
    __asm__ __volatile__("pushfl\n\t"
                         "cli\n\t"
                         "popl  %0\n\t"
                         : "=r" (xFlags));
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
 * Are interrupts enabled?
 *
 * @returns true / false.
 */
DECLINLINE(RTCCUINTREG) ASMIntAreEnabled(void)
{
    RTCCUINTREG uFlags = ASMGetFlags();
    return uFlags & 0x200 /* X86_EFL_IF */ ? true : false;
}


/**
 * Halts the CPU until interrupted.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMHalt(void);
#else
DECLINLINE(void) ASMHalt(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("hlt\n\t");
# else
    __asm {
        hlt
    }
# endif
}
#endif


/**
 * The PAUSE variant of NOP for helping hyperthreaded CPUs detecing spin locks.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMNopPause(void);
#else
DECLINLINE(void) ASMNopPause(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__(".byte 0xf3,0x90\n\t");
# else
    __asm {
        _emit 0f3h
        _emit 090h
    }
# endif
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
    __asm__ __volatile__("rdmsr\n\t"
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
    __asm__ __volatile__("rdmsr\n\t"
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
    __asm__ __volatile__("rdmsr\n\t"
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
 * Gets dr0.
 *
 * @returns dr0.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR0(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR0(void)
{
    RTCCUINTREG uDR0;
# if RT_INLINE_ASM_USES_INTRIN
    uDR0 = __readdr(0);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr0, %0\n\t" : "=r" (uDR0));
#  else
    __asm__ __volatile__("movl   %%dr0, %0\n\t" : "=r" (uDR0));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr0
        mov     [uDR0], rax
#  else
        mov     eax, dr0
        mov     [uDR0], eax
#  endif
    }
# endif
    return uDR0;
}
#endif


/**
 * Gets dr1.
 *
 * @returns dr1.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR1(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR1(void)
{
    RTCCUINTREG uDR1;
# if RT_INLINE_ASM_USES_INTRIN
    uDR1 = __readdr(1);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr1, %0\n\t" : "=r" (uDR1));
#  else
    __asm__ __volatile__("movl   %%dr1, %0\n\t" : "=r" (uDR1));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr1
        mov     [uDR1], rax
#  else
        mov     eax, dr1
        mov     [uDR1], eax
#  endif
    }
# endif
    return uDR1;
}
#endif


/**
 * Gets dr2.
 *
 * @returns dr2.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR2(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR2(void)
{
    RTCCUINTREG uDR2;
# if RT_INLINE_ASM_USES_INTRIN
    uDR2 = __readdr(2);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr2, %0\n\t" : "=r" (uDR2));
#  else
    __asm__ __volatile__("movl   %%dr2, %0\n\t" : "=r" (uDR2));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr2
        mov     [uDR2], rax
#  else
        mov     eax, dr2
        mov     [uDR2], eax
#  endif
    }
# endif
    return uDR2;
}
#endif


/**
 * Gets dr3.
 *
 * @returns dr3.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR3(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR3(void)
{
    RTCCUINTREG uDR3;
# if RT_INLINE_ASM_USES_INTRIN
    uDR3 = __readdr(3);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr3, %0\n\t" : "=r" (uDR3));
#  else
    __asm__ __volatile__("movl   %%dr3, %0\n\t" : "=r" (uDR3));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, dr3
        mov     [uDR3], rax
#  else
        mov     eax, dr3
        mov     [uDR3], eax
#  endif
    }
# endif
    return uDR3;
}
#endif


/**
 * Gets dr6.
 *
 * @returns dr6.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR6(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR6(void)
{
    RTCCUINTREG uDR6;
# if RT_INLINE_ASM_USES_INTRIN
    uDR6 = __readdr(6);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr6, %0\n\t" : "=r" (uDR6));
#  else
    __asm__ __volatile__("movl   %%dr6, %0\n\t" : "=r" (uDR6));
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
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetAndClearDR6(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetAndClearDR6(void)
{
    RTCCUINTREG uDR6;
# if RT_INLINE_ASM_USES_INTRIN
    uDR6 = __readdr(6);
    __writedr(6, 0xffff0ff0U);          /* 31-16 and 4-11 are 1's, 12 and 63-31 are zero. */
# elif RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG uNewValue = 0xffff0ff0U;/* 31-16 and 4-11 are 1's, 12 and 63-31 are zero. */
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr6, %0\n\t"
                         "movq   %1, %%dr6\n\t"
                         : "=r" (uDR6)
                         : "r" (uNewValue));
#  else
    __asm__ __volatile__("movl   %%dr6, %0\n\t"
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
 * Gets dr7.
 *
 * @returns dr7.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(RTCCUINTREG) ASMGetDR7(void);
#else
DECLINLINE(RTCCUINTREG) ASMGetDR7(void)
{
    RTCCUINTREG uDR7;
# if RT_INLINE_ASM_USES_INTRIN
    uDR7 = __readdr(7);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %%dr7, %0\n\t" : "=r" (uDR7));
#  else
    __asm__ __volatile__("movl   %%dr7, %0\n\t" : "=r" (uDR7));
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
 * Sets dr0.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR0(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR0(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(0, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr0\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr0\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr0, rax
#  else
        mov     eax, [uDRVal]
        mov     dr0, eax
#  endif
    }
# endif
}
#endif


/**
 * Sets dr1.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR1(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR1(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(1, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr1\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr1\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr1, rax
#  else
        mov     eax, [uDRVal]
        mov     dr1, eax
#  endif
    }
# endif
}
#endif


/**
 * Sets dr2.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR2(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR2(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(2, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr2\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr2\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr2, rax
#  else
        mov     eax, [uDRVal]
        mov     dr2, eax
#  endif
    }
# endif
}
#endif


/**
 * Sets dr3.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR3(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR3(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(3, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr3\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr3\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr3, rax
#  else
        mov     eax, [uDRVal]
        mov     dr3, eax
#  endif
    }
# endif
}
#endif


/**
 * Sets dr6.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR6(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR6(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(6, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr6\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr6\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr6, rax
#  else
        mov     eax, [uDRVal]
        mov     dr6, eax
#  endif
    }
# endif
}
#endif


/**
 * Sets dr7.
 *
 * @param   uDRVal   Debug register value to write
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMSetDR7(RTCCUINTREG uDRVal);
#else
DECLINLINE(void) ASMSetDR7(RTCCUINTREG uDRVal)
{
# if RT_INLINE_ASM_USES_INTRIN
    __writedr(7, uDRVal);
# elif RT_INLINE_ASM_GNU_STYLE
#  ifdef RT_ARCH_AMD64
    __asm__ __volatile__("movq   %0, %%dr7\n\t" : : "r" (uDRVal));
#  else
    __asm__ __volatile__("movl   %0, %%dr7\n\t" : : "r" (uDRVal));
#  endif
# else
    __asm
    {
#  ifdef RT_ARCH_AMD64
        mov     rax, [uDRVal]
        mov     dr7, rax
#  else
        mov     eax, [uDRVal]
        mov     dr7, eax
#  endif
    }
# endif
}
#endif


/**
 * Writes a 8-bit unsigned integer to an I/O port, ordered.
 *
 * @param   Port    I/O port to write to.
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
 * Reads a 8-bit unsigned integer from an I/O port, ordered.
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
 * @param   Port    I/O port to write to.
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
 * Reads a 16-bit unsigned integer from an I/O port, ordered.
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
 * @param   Port    I/O port to write to.
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
 * Reads a 32-bit unsigned integer from an I/O port, ordered.
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


/**
 * Writes a string of 8-bit unsigned integer items to an I/O port, ordered.
 *
 * @param   Port    I/O port to write to.
 * @param   pau8    Pointer to the string buffer.
 * @param   c       The number of items to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutStrU8(RTIOPORT Port, uint8_t const *pau8, size_t c);
#else
DECLINLINE(void) ASMOutStrU8(RTIOPORT Port, uint8_t const *pau8, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; outsb\n\t"
                         : "+S" (pau8),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __outbytestring(Port, (unsigned char *)pau8, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau8]
        xchg    esi, eax
        rep outsb
        xchg    esi, eax
    }
# endif
}
#endif


/**
 * Reads a string of 8-bit unsigned integer items from an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   pau8    Pointer to the string buffer (output).
 * @param   c       The number of items to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMInStrU8(RTIOPORT Port, uint8_t *pau8, size_t c);
#else
DECLINLINE(void) ASMInStrU8(RTIOPORT Port, uint8_t *pau8, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; insb\n\t"
                         : "+D" (pau8),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __inbytestring(Port, pau8, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau8]
        xchg    edi, eax
        rep insb
        xchg    edi, eax
    }
# endif
}
#endif


/**
 * Writes a string of 16-bit unsigned integer items to an I/O port, ordered.
 *
 * @param   Port    I/O port to write to.
 * @param   pau16   Pointer to the string buffer.
 * @param   c       The number of items to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutStrU16(RTIOPORT Port, uint16_t const *pau16, size_t c);
#else
DECLINLINE(void) ASMOutStrU16(RTIOPORT Port, uint16_t const *pau16, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; outsw\n\t"
                         : "+S" (pau16),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __outwordstring(Port, (unsigned short *)pau16, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau16]
        xchg    esi, eax
        rep outsw
        xchg    esi, eax
    }
# endif
}
#endif


/**
 * Reads a string of 16-bit unsigned integer items from an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   pau16   Pointer to the string buffer (output).
 * @param   c       The number of items to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMInStrU16(RTIOPORT Port, uint16_t *pau16, size_t c);
#else
DECLINLINE(void) ASMInStrU16(RTIOPORT Port, uint16_t *pau16, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; insw\n\t"
                         : "+D" (pau16),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __inwordstring(Port, pau16, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau16]
        xchg    edi, eax
        rep insw
        xchg    edi, eax
    }
# endif
}
#endif


/**
 * Writes a string of 32-bit unsigned integer items to an I/O port, ordered.
 *
 * @param   Port    I/O port to write to.
 * @param   pau32   Pointer to the string buffer.
 * @param   c       The number of items to write.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMOutStrU32(RTIOPORT Port, uint32_t const *pau32, size_t c);
#else
DECLINLINE(void) ASMOutStrU32(RTIOPORT Port, uint32_t const *pau32, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; outsl\n\t"
                         : "+S" (pau32),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __outdwordstring(Port, (unsigned long *)pau32, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau32]
        xchg    esi, eax
        rep outsd
        xchg    esi, eax
    }
# endif
}
#endif


/**
 * Reads a string of 32-bit unsigned integer items from an I/O port, ordered.
 *
 * @param   Port    I/O port to read from.
 * @param   pau32   Pointer to the string buffer (output).
 * @param   c       The number of items to read.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMInStrU32(RTIOPORT Port, uint32_t *pau32, size_t c);
#else
DECLINLINE(void) ASMInStrU32(RTIOPORT Port, uint32_t *pau32, size_t c)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("rep; insl\n\t"
                         : "+D" (pau32),
                           "+c" (c)
                         : "d" (Port));

# elif RT_INLINE_ASM_USES_INTRIN
    __indwordstring(Port, (unsigned long *)pau32, (unsigned long)c);

# else
    __asm
    {
        mov     dx, [Port]
        mov     ecx, [c]
        mov     eax, [pau32]
        xchg    edi, eax
        rep insd
        xchg    edi, eax
    }
# endif
}
#endif


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


/**
 * Write back the internal caches and invalidate them.
 */
#if RT_INLINE_ASM_EXTERNAL && !RT_INLINE_ASM_USES_INTRIN
DECLASM(void) ASMWriteBackAndInvalidateCaches(void);
#else
DECLINLINE(void) ASMWriteBackAndInvalidateCaches(void)
{
# if RT_INLINE_ASM_USES_INTRIN
    __wbinvd();

# elif RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("wbinvd");
# else
    __asm
    {
        wbinvd
    }
# endif
}
#endif


/**
 * Invalidate internal and (perhaps) external caches without first
 * flushing dirty cache lines. Use with extreme care.
 */
#if RT_INLINE_ASM_EXTERNAL
DECLASM(void) ASMInvalidateInternalCaches(void);
#else
DECLINLINE(void) ASMInvalidateInternalCaches(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__("invd");
# else
    __asm
    {
        invd
    }
# endif
}
#endif


/**
 * Memory load/store fence, waits for any pending writes and reads to complete.
 * Requires the X86_CPUID_FEATURE_EDX_SSE2 CPUID bit set.
 */
DECLINLINE(void) ASMMemoryFenceSSE2(void)
{
#if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (".byte 0x0f,0xae,0xf0\n\t");
#elif RT_INLINE_ASM_USES_INTRIN
    _mm_mfence();
#else
    __asm
    {
        _emit   0x0f
        _emit   0xae
        _emit   0xf0
    }
#endif
}


/**
 * Memory store fence, waits for any writes to complete.
 * Requires the X86_CPUID_FEATURE_EDX_SSE CPUID bit set.
 */
DECLINLINE(void) ASMWriteFenceSSE(void)
{
#if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (".byte 0x0f,0xae,0xf8\n\t");
#elif RT_INLINE_ASM_USES_INTRIN
    _mm_sfence();
#else
    __asm
    {
        _emit   0x0f
        _emit   0xae
        _emit   0xf8
    }
#endif
}


/**
 * Memory load fence, waits for any pending reads to complete.
 * Requires the X86_CPUID_FEATURE_EDX_SSE2 CPUID bit set.
 */
DECLINLINE(void) ASMReadFenceSSE2(void)
{
#if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (".byte 0x0f,0xae,0xe8\n\t");
#elif RT_INLINE_ASM_USES_INTRIN
    _mm_lfence();
#else
    __asm
    {
        _emit   0x0f
        _emit   0xae
        _emit   0xe8
    }
#endif
}


/** @name Interger Math Optimizations
 * @{ */

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
 * Divides a 64-bit unsigned by a 32-bit unsigned returning an unsigned 32-bit result.
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
    RTCCUINTREG uDummy;
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
 * Divides a 64-bit signed by a 32-bit signed returning a signed 32-bit result.
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
    RTCCUINTREG iDummy;
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
 * Performs 64-bit unsigned by a 32-bit unsigned division with a 32-bit unsigned result,
 * returning the rest.
 *
 * @returns u64 % u32.
 *
 * @remarks It is important that the result is <= UINT32_MAX or we'll overflow and crash.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(uint32_t) ASMModU64ByU32RetU32(uint64_t u64, uint32_t u32);
#else
DECLINLINE(uint32_t) ASMModU64ByU32RetU32(uint64_t u64, uint32_t u32)
{
# ifdef RT_ARCH_AMD64
    return (uint32_t)(u64 % u32);
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG uDummy;
    __asm__ __volatile__("divl %3"
                         : "=a" (uDummy), "=d"(u32)
                         : "A" (u64), "r" (u32));
#  else
    __asm
    {
        mov     eax, dword ptr [u64]
        mov     edx, dword ptr [u64 + 4]
        mov     ecx, [u32]
        div     ecx
        mov     [u32], edx
    }
#  endif
    return u32;
# endif /* !RT_ARCH_AMD64 */
}
#endif


/**
 * Performs 64-bit signed by a 32-bit signed division with a 32-bit signed result,
 * returning the rest.
 *
 * @returns u64 % u32.
 *
 * @remarks It is important that the result is <= UINT32_MAX or we'll overflow and crash.
 */
#if RT_INLINE_ASM_EXTERNAL && !defined(RT_ARCH_AMD64)
DECLASM(int32_t) ASMModS64ByS32RetS32(int64_t i64, int32_t i32);
#else
DECLINLINE(int32_t) ASMModS64ByS32RetS32(int64_t i64, int32_t i32)
{
# ifdef RT_ARCH_AMD64
    return (int32_t)(i64 % i32);
# else /* !RT_ARCH_AMD64 */
#  if RT_INLINE_ASM_GNU_STYLE
    RTCCUINTREG iDummy;
    __asm__ __volatile__("idivl %3"
                         : "=a" (iDummy), "=d"(i32)
                         : "A" (i64), "r" (i32));
#  else
    __asm
    {
        mov     eax, dword ptr [i64]
        mov     edx, dword ptr [i64 + 4]
        mov     ecx, [i32]
        idiv    ecx
        mov     [i32], edx
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

/** @} */

/** @} */
#endif

