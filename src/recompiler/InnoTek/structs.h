/* $Id$ */
/** @file
 * VBox Recompiler - structure offset tables.
 *
 * Used by op.c and VBoxRecompiler.c to verify they have the 
 * same understanding of the internal structures when using
 * different compilers (GCC 4.x vs. 3.x/ELF).
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
 */


#if REM_STRUCT_OP

/* 
 * we're in op.c 
 */
# define REM_STRUCT_TABLE(strct)        const int g_aiOpStruct_ ## strct []

# define REM_THE_END(strct)             sizeof(strct) | 0x42000000
# define REM_SIZEOF(strct)              sizeof(strct)
# define REM_OFFSETOF(strct, memb)      RT_OFFSETOF(strct, memb)
# define REM_SIZEOFMEMB(strct, memb)    RT_SIZEOFMEMB(strct, memb)

#else /* !REM_STRUCT_OP */
/*
 * We're in VBoxRecompiler.c. 
 */
# define REM_STRUCT_TABLE(strct)  \
    extern const int g_aiOpStruct_ ## strct []; \
    \
    const REMSTRUCTENTRY g_aMyStruct_ ## strct []

# define REM_THE_END(strct)             { sizeof(strct) | 0x42000000, #strct " - the end" }
# define REM_SIZEOF(strct)              { sizeof(strct)             , #strct " - size of" }
# define REM_OFFSETOF(strct, memb)      { RT_OFFSETOF(strct, memb)  , #strct "::" #memb " - offsetof" }
# define REM_SIZEOFMEMB(strct, memb)    { RT_SIZEOFMEMB(strct, memb), #strct "::" #memb " - sizeof" }

/** Matches the My and Op tables for a strct. */
# define ASSERT_STRUCT_TABLE(strct) \
    for (i = 0; i < RT_ELEMENTS(g_aMyStruct_ ## strct); i++) \
        AssertReleaseMsg(g_aMyStruct_ ## strct [i].iValue == g_aiOpStruct_ ## strct [i], \
                         (#strct "[%d] - %d != %d - %s\n", \
                          i, \
                          g_aMyStruct_ ## strct [i].iValue,\
                          g_aiOpStruct_ ## strct [i], \
                          g_aMyStruct_ ## strct [i].pszExpr))

/** Struct check entry. */
typedef struct REMSTRUCTENTRY
{
    int iValue;
    const char *pszExpr;
} REMSTRUCTENTRY;

#endif /* !REM_STRUCT_OP */


REM_STRUCT_TABLE(Misc) =
{
    REM_SIZEOF(char),
    REM_SIZEOF(short),
    REM_SIZEOF(int),
    REM_SIZEOF(long),
    REM_SIZEOF(float),
    REM_SIZEOF(double),
    REM_SIZEOF(long double),
    REM_SIZEOF(void *),
    REM_SIZEOF(char *),
    REM_SIZEOF(short *),
    REM_SIZEOF(long *),
    REM_SIZEOF(size_t),
    REM_SIZEOF(uint8_t),
    REM_SIZEOF(uint16_t),
    REM_SIZEOF(uint32_t),
    REM_SIZEOF(uint64_t),
    REM_SIZEOF(uintptr_t),
    REM_SIZEOF(RTGCUINTPTR),
    REM_SIZEOF(RTHCUINTPTR),
    REM_SIZEOF(RTR3UINTPTR),
    REM_SIZEOF(RTR0UINTPTR),
    REM_THE_END(char)
};

REM_STRUCT_TABLE(TLB) =
{
    REM_SIZEOF(CPUTLBEntry),
    REM_OFFSETOF(CPUTLBEntry, addr_read),
    REM_OFFSETOF(CPUTLBEntry, addr_write),
    REM_OFFSETOF(CPUTLBEntry, addr_code),
    REM_OFFSETOF(CPUTLBEntry, addend),
    REM_THE_END(CPUTLBEntry)
};

REM_STRUCT_TABLE(SegmentCache) =
{
    REM_SIZEOF(SegmentCache),
    REM_OFFSETOF(SegmentCache, selector),
    REM_OFFSETOF(SegmentCache, base),
    REM_OFFSETOF(SegmentCache, limit),
    REM_OFFSETOF(SegmentCache, flags),
    REM_OFFSETOF(SegmentCache, newselector),
    REM_THE_END(SegmentCache)
};

REM_STRUCT_TABLE(XMMReg) =
{
    REM_SIZEOF(XMMReg),
    REM_OFFSETOF(XMMReg, _b[1]),
    REM_OFFSETOF(XMMReg, _w[1]),
    REM_OFFSETOF(XMMReg, _l[1]),
    REM_OFFSETOF(XMMReg, _q[1]),
    REM_OFFSETOF(XMMReg, _s[1]),
    REM_OFFSETOF(XMMReg, _d[1]),
    REM_THE_END(XMMReg)
};

REM_STRUCT_TABLE(MMXReg) =
{
    REM_SIZEOF(MMXReg),
    REM_OFFSETOF(MMXReg, _b[1]),
    REM_OFFSETOF(MMXReg, _w[1]),
    REM_OFFSETOF(MMXReg, _l[1]),
    REM_OFFSETOF(MMXReg, q),
    REM_THE_END(MMXReg)
};

REM_STRUCT_TABLE(float_status) =
{
    REM_SIZEOF(float_status),
    //REM_OFFSETOF(float_status, float_detect_tininess),
    REM_OFFSETOF(float_status, float_rounding_mode),
    //REM_OFFSETOF(float_status, float_exception_flags),
    REM_OFFSETOF(float_status, floatx80_rounding_precision),
    REM_THE_END(float_status)
};

REM_STRUCT_TABLE(float32u) =
{
    REM_SIZEOF(float32u),
    REM_OFFSETOF(float32u, f),
    REM_SIZEOFMEMB(float32u, f),
    REM_OFFSETOF(float32u, i),
    REM_SIZEOFMEMB(float32u, i),
    REM_THE_END(float32u)
};

REM_STRUCT_TABLE(float64u) =
{
    REM_SIZEOF(float64u),
    REM_OFFSETOF(float64u, f),
    REM_SIZEOFMEMB(float64u, f),
    REM_OFFSETOF(float64u, i),
    REM_SIZEOFMEMB(float64u, i),
    REM_THE_END(float64u)
};

REM_STRUCT_TABLE(floatx80u) =
{
    REM_SIZEOF(floatx80u),
    REM_OFFSETOF(floatx80u, f),
    REM_SIZEOFMEMB(floatx80u, f),
    REM_OFFSETOF(floatx80u, i),
    REM_SIZEOFMEMB(floatx80u, i),
    REM_OFFSETOF(floatx80u, i.low),
    REM_SIZEOFMEMB(floatx80u, i.low),
    REM_OFFSETOF(floatx80u, i.high),
    REM_SIZEOFMEMB(floatx80u, i.high),
    REM_THE_END(floatx80u)
};

REM_STRUCT_TABLE(CPUState) =
{
    REM_SIZEOF(CPUState),
    REM_OFFSETOF(CPUState, regs),
    REM_OFFSETOF(CPUState, regs[R_EAX]),
    REM_OFFSETOF(CPUState, regs[R_EBX]),
    REM_OFFSETOF(CPUState, regs[R_ECX]),
    REM_OFFSETOF(CPUState, regs[R_EDX]),
    REM_OFFSETOF(CPUState, regs[R_ESI]),
    REM_OFFSETOF(CPUState, regs[R_EDI]),
    REM_OFFSETOF(CPUState, regs[R_EBP]),
    REM_OFFSETOF(CPUState, regs[R_ESP]),
    REM_OFFSETOF(CPUState, eip),
    REM_OFFSETOF(CPUState, eflags),
    REM_OFFSETOF(CPUState, cc_src),
    REM_OFFSETOF(CPUState, cc_dst),
    REM_OFFSETOF(CPUState, cc_op),
    REM_OFFSETOF(CPUState, df),
    REM_OFFSETOF(CPUState, hflags),
    REM_OFFSETOF(CPUState, segs),
    REM_OFFSETOF(CPUState, segs[R_SS]),
    REM_OFFSETOF(CPUState, segs[R_CS]),
    REM_OFFSETOF(CPUState, segs[R_DS]),
    REM_OFFSETOF(CPUState, segs[R_ES]),
    REM_OFFSETOF(CPUState, segs[R_FS]),
    REM_OFFSETOF(CPUState, segs[R_GS]),
    REM_OFFSETOF(CPUState, ldt),
    REM_OFFSETOF(CPUState, tr),
    REM_OFFSETOF(CPUState, gdt),
    REM_OFFSETOF(CPUState, idt),
    REM_OFFSETOF(CPUState, cr),
    REM_SIZEOFMEMB(CPUState, cr),
    REM_OFFSETOF(CPUState, cr[1]),
    REM_OFFSETOF(CPUState, a20_mask),
    REM_OFFSETOF(CPUState, fpstt),
    REM_OFFSETOF(CPUState, fpus),
    REM_OFFSETOF(CPUState, fpuc),
    REM_OFFSETOF(CPUState, fptags),
    REM_SIZEOFMEMB(CPUState, fptags),
    REM_OFFSETOF(CPUState, fptags[1]),
    REM_OFFSETOF(CPUState, fpregs),
    REM_SIZEOFMEMB(CPUState, fpregs),
    REM_OFFSETOF(CPUState, fpregs[1]),
    REM_OFFSETOF(CPUState, fp_status),
    REM_OFFSETOF(CPUState, ft0),
    REM_SIZEOFMEMB(CPUState, ft0),
    REM_OFFSETOF(CPUState, fp_convert),
    REM_SIZEOFMEMB(CPUState, fp_convert),
    REM_OFFSETOF(CPUState, sse_status),
    REM_OFFSETOF(CPUState, mxcsr),
    REM_OFFSETOF(CPUState, xmm_regs),
    REM_SIZEOFMEMB(CPUState, xmm_regs),
    REM_OFFSETOF(CPUState, xmm_regs[1]),
    REM_OFFSETOF(CPUState, mmx_t0),
    REM_OFFSETOF(CPUState, sysenter_cs),
    REM_OFFSETOF(CPUState, sysenter_esp),
    REM_OFFSETOF(CPUState, sysenter_eip),
    REM_OFFSETOF(CPUState, efer),
    REM_OFFSETOF(CPUState, star),
    REM_OFFSETOF(CPUState, pat),
    REM_OFFSETOF(CPUState, jmp_env),
    REM_SIZEOFMEMB(CPUState, jmp_env),
    REM_OFFSETOF(CPUState, exception_index),
    REM_OFFSETOF(CPUState, error_code),
    REM_OFFSETOF(CPUState, exception_is_int),
    REM_OFFSETOF(CPUState, exception_next_eip),
    REM_OFFSETOF(CPUState, dr),
    REM_SIZEOFMEMB(CPUState, dr),
    REM_OFFSETOF(CPUState, dr[1]),
    REM_OFFSETOF(CPUState, smbase),
    REM_OFFSETOF(CPUState, interrupt_request),
    REM_OFFSETOF(CPUState, user_mode_only),
    REM_OFFSETOF(CPUState, state),
    REM_OFFSETOF(CPUState, pVM),
    REM_OFFSETOF(CPUState, pvCodeBuffer),
    REM_OFFSETOF(CPUState, cbCodeBuffer),
    REM_OFFSETOF(CPUState, cpuid_features),
    REM_OFFSETOF(CPUState, cpuid_ext_features),
    REM_OFFSETOF(CPUState, cpuid_ext2_features),

    /* cpu-defs.h */
    REM_OFFSETOF(CPUState, current_tb),
    REM_OFFSETOF(CPUState, mem_write_pc),
    REM_OFFSETOF(CPUState, mem_write_vaddr),
    REM_OFFSETOF(CPUState, tlb_table),
    REM_SIZEOFMEMB(CPUState, tlb_table),
    REM_OFFSETOF(CPUState, tb_jmp_cache),
    REM_SIZEOFMEMB(CPUState, tb_jmp_cache),
    REM_OFFSETOF(CPUState, breakpoints),
    REM_SIZEOFMEMB(CPUState, breakpoints),
    REM_OFFSETOF(CPUState, nb_breakpoints),
    REM_OFFSETOF(CPUState, singlestep_enabled),
    REM_OFFSETOF(CPUState, next_cpu),
    REM_OFFSETOF(CPUState, cpu_index),
    REM_OFFSETOF(CPUState, opaque),

    REM_THE_END(CPUState)
};

#undef REM_THE_END
#undef REM_OFFSETOF
#undef REM_SIZEOF
#undef REM_SIZEOFMEMB
#undef REM_STRUCT_TABLE

