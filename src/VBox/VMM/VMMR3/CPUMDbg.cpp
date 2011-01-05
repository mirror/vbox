/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager, Debugger & Debugging APIs.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DBGF
#include <VBox/vmm/dbgf.h>
#include "DBGFInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>


static DECLCALLBACK(int) dbgfR3RegSet_seg(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_crX(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_crX(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_drX(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_drX(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_msr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_msr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_gdtr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_gdtr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_idtr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_idtr(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_ftw(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_ftw(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegGet_stN(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCCPUMCTX pCtx, PRTUINT128U puValue)
{
    return VERR_NOT_IMPLEMENTED;
}

static DECLCALLBACK(int) dbgfR3RegSet_stN(PVMCPU pVCpu, struct DBGFREGDESC const *pDesc, PCPUMCTX pCtx, RTUINT128U uValue, RTUINT128U fMask)
{
    return VERR_NOT_IMPLEMENTED;
}



/*
 * Set up aliases.
 */
#define DBGFREGALIAS_STD(Name, psz32, psz16, psz8)  \
    static DBGFREGALIAS const g_aDbgfRegAliases_##Name[] = \
    { \
        { psz32, DBGFREGVALTYPE_U32    }, \
        { psz16, DBGFREGVALTYPE_U16    }, \
        { psz8,  DBGFREGVALTYPE_U8     }, \
        { NULL, DBGFREGVALTYPE_INVALID } \
    }
DBGFREGALIAS_STD(rax,  "eax",   "ax",   "al");
DBGFREGALIAS_STD(rcx,  "ecx",   "cx",   "cl");
DBGFREGALIAS_STD(rdx,  "edx",   "dx",   "dl");
DBGFREGALIAS_STD(rbx,  "ebx",   "bx",   "bl");
DBGFREGALIAS_STD(rsp,  "esp",   "sp",   NULL);
DBGFREGALIAS_STD(rbp,  "ebp",   "bp",   NULL);
DBGFREGALIAS_STD(rsi,  "esi",   "si",  "sil");
DBGFREGALIAS_STD(rdi,  "edi",   "di",  "dil");
DBGFREGALIAS_STD(r8,   "r8d",  "r8w",  "r8b");
DBGFREGALIAS_STD(r9,   "r9d",  "r9w",  "r9b");
DBGFREGALIAS_STD(r10, "r10d", "r10w", "r10b");
DBGFREGALIAS_STD(r11, "r11d", "r11w", "r11b");
DBGFREGALIAS_STD(r12, "r12d", "r12w", "r12b");
DBGFREGALIAS_STD(r13, "r13d", "r13w", "r13b");
DBGFREGALIAS_STD(r14, "r14d", "r14w", "r14b");
DBGFREGALIAS_STD(r15, "r15d", "r15w", "r15b");
DBGFREGALIAS_STD(rip, "eip",   "ip",    NULL);
DBGFREGALIAS_STD(rflags, "eflags", "flags", NULL);
#undef DBGFREGALIAS_STD

static DBGFREGALIAS const g_aDbgfRegAliases_fpuip[] =
{
    { "fpuip", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aDbgfRegAliases_fpudp[] =
{
    { "fpudp", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

static DBGFREGALIAS const g_aDbgfRegAliases_cr0[] =
{
    { "msw", DBGFREGVALTYPE_U16  },
    { NULL, DBGFREGVALTYPE_INVALID }
};

/*
 * Sub fields.
 */
/** Sub-fields for the (hidden) segment attribute register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_seg[] =
{
    { "type",   0,   4,  0 },
    { "s",      4,   1,  0 },
    { "dpl",    5,   2,  0 },
    { "p",      7,   1,  0 },
    { "avl",   12,   1,  0 },
    { "l",     13,   1,  0 },
    { "d",     14,   1,  0 },
    { "g",     15,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the flags register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_rflags[] =
{
    { "cf",     0,   1,  0 },
    { "pf",     2,   1,  0 },
    { "af",     4,   1,  0 },
    { "zf",     6,   1,  0 },
    { "sf",     7,   1,  0 },
    { "tf",     8,   1,  0 },
    { "if",     9,   1,  0 },
    { "df",    10,   1,  0 },
    { "of",    11,   1,  0 },
    { "iopl",  12,   2,  0 },
    { "nt",    14,   1,  0 },
    { "rf",    16,   1,  0 },
    { "vm",    17,   1,  0 },
    { "ac",    18,   1,  0 },
    { "vif",   19,   1,  0 },
    { "vip",   20,   1,  0 },
    { "id",    21,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the FPU control word register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_fcw[] =
{
    { "im",     1,   1,  0 },
    { "dm",     2,   1,  0 },
    { "zm",     3,   1,  0 },
    { "om",     4,   1,  0 },
    { "um",     5,   1,  0 },
    { "pm",     6,   1,  0 },
    { "pc",     8,   2,  0 },
    { "rc",    10,   2,  0 },
    { "x",     12,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the FPU status word register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_fsw[] =
{
    { "ie",     0,   1,  0 },
    { "de",     1,   1,  0 },
    { "ze",     2,   1,  0 },
    { "oe",     3,   1,  0 },
    { "ue",     4,   1,  0 },
    { "pe",     5,   1,  0 },
    { "se",     6,   1,  0 },
    { "es",     7,   1,  0 },
    { "c0",     8,   1,  0 },
    { "c1",     9,   1,  0 },
    { "c2",    10,   1,  0 },
    { "top",   11,   3,  0 },
    { "c3",    14,   1,  0 },
    { "b",     15,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_ftw[] =
{
    { "tag0",   0,   2,  0 },
    { "tag1",   2,   2,  0 },
    { "tag2",   4,   2,  0 },
    { "tag3",   6,   2,  0 },
    { "tag4",   8,   2,  0 },
    { "tag5",  10,   2,  0 },
    { "tag6",  12,   2,  0 },
    { "tag7",  14,   2,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the Multimedia Extensions Control and Status Register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_mxcsr[] =
{
    { "ie",     0,   1,  0 },
    { "de",     1,   1,  0 },
    { "ze",     2,   1,  0 },
    { "oe",     3,   1,  0 },
    { "ue",     4,   1,  0 },
    { "pe",     5,   1,  0 },
    { "daz",    6,   1,  0 },
    { "im",     7,   1,  0 },
    { "dm",     8,   1,  0 },
    { "zm",     9,   1,  0 },
    { "om",    10,   1,  0 },
    { "um",    11,   1,  0 },
    { "pm",    12,   1,  0 },
    { "rc",    13,   2,  0 },
    { "fz",    14,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the FPU tag word register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_stN[] =
{
    { "man",    0,  64,  0 },
    { "exp",   64,  15,  0 },
    { "sig",   79,   1,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the MMX registers. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_mmN[] =
{
    { "dw0",    0,  32,  0 },
    { "dw1",   32,  32,  0 },
    { "w0",     0,  16,  0 },
    { "w1",    16,  16,  0 },
    { "w2",    32,  16,  0 },
    { "w3",    48,  16,  0 },
    { "b0",     0,   8,  0 },
    { "b1",     8,   8,  0 },
    { "b2",    16,   8,  0 },
    { "b3",    24,   8,  0 },
    { "b4",    32,   8,  0 },
    { "b5",    40,   8,  0 },
    { "b6",    48,   8,  0 },
    { "b7",    56,   8,  0 },
    { NULL,     0,   0,  0 }
};

/** Sub-fields for the XMM registers. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_xmmN[] =
{
    { "r0",      0,     32,  0 },
    { "r0.man",  0+ 0,  23,  0 },
    { "r0.exp",  0+23,   8,  0 },
    { "r0.sig",  0+31,   1,  0 },
    { "r1",     32,     32,  0 },
    { "r1.man", 32+ 0,  23,  0 },
    { "r1.exp", 32+23,   8,  0 },
    { "r1.sig", 32+31,   1,  0 },
    { "r2",     64,     32,  0 },
    { "r2.man", 64+ 0,  23,  0 },
    { "r2.exp", 64+23,   8,  0 },
    { "r2.sig", 64+31,   1,  0 },
    { "r3",     96,     32,  0 },
    { "r3.man", 96+ 0,  23,  0 },
    { "r3.exp", 96+23,   8,  0 },
    { "r3.sig", 96+31,   1,  0 },
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CR0 register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_cr0[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CR3 register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_cr3[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CR4 register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_cr4[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the DR6 register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_dr6[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the DR7 register. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_dr7[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_apic_base[] =
{
    { "bsp",     8,      1,  0 },
    { "ge",      9,      1,  0 },
    { "base",    12,    20, 12 },
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CR_PAT MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_cr_pat[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the PERF_STATUS MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_perf_status[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the EFER MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_efer[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the STAR MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_star[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the CSTAR MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_cstar[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the LSTAR MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_lstar[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};

/** Sub-fields for the SF_MASK MSR. */
static DBGFREGSUBFIELD const g_aDbgfRegFields_sf_mask[] =
{
    /** @todo  */
    { NULL,      0,      0,  0 }
};



/**
 * The register descriptors.
 */
static DBGFREGDESC const g_aDbgfRegDescs[] =
{
#define DBGFREGDESC_REG(UName, LName) \
    { #LName, DBGFREG_##UName, DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, LName), NULL, NULL, g_aDbgfRegAliases_##LName, NULL }
    DBGFREGDESC_REG(RAX, rax),
    DBGFREGDESC_REG(RCX, rcx),
    DBGFREGDESC_REG(RDX, rdx),
    DBGFREGDESC_REG(RSP, rsp),
    DBGFREGDESC_REG(RBP, rbp),
    DBGFREGDESC_REG(RSI, rsi),
    DBGFREGDESC_REG(RDI, rdi),
    DBGFREGDESC_REG(R8,   r8),
    DBGFREGDESC_REG(R9,   r9),
    DBGFREGDESC_REG(R10, r10),
    DBGFREGDESC_REG(R11, r11),
    DBGFREGDESC_REG(R12, r12),
    DBGFREGDESC_REG(R13, r13),
    DBGFREGDESC_REG(R14, r14),
    DBGFREGDESC_REG(R15, r15),
#define DBGFREGDESC_SEG(UName, LName) \
    { #LName,         DBGFREG_##UName,        DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, LName),               NULL, dbgfR3RegSet_seg, NULL, NULL },  \
    { #LName "_attr", DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, LName##Hid.Attr.u),   NULL, NULL, NULL, g_aDbgfRegFields_seg }, \
    { #LName "_base", DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, LName##Hid.u64Base),  NULL, NULL, NULL, NULL }, \
    { #LName "_lim",  DBGFREG_##UName##_ATTR, DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, LName##Hid.u32Limit), NULL, NULL, NULL, NULL }
    DBGFREGDESC_SEG(CS, cs),
    DBGFREGDESC_SEG(DS, ds),
    DBGFREGDESC_SEG(ES, es),
    DBGFREGDESC_SEG(FS, fs),
    DBGFREGDESC_SEG(GS, gs),
    DBGFREGDESC_SEG(SS, ss),
    DBGFREGDESC_REG(RIP, rip),
    { "rflags",     DBGFREG_RFLAGS,     DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, rflags),           NULL, NULL, g_aDbgfRegAliases_rflags, g_aDbgfRegFields_rflags },
    { "fcw",        DBGFREG_FCW,        DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.FCW),          NULL, NULL, NULL, g_aDbgfRegFields_fcw },
    { "fsw",        DBGFREG_FSW,        DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.FSW),          NULL, NULL, NULL, g_aDbgfRegFields_fsw },
    { "ftw",        DBGFREG_FTW,        DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.FTW), dbgfR3RegGet_ftw, dbgfR3RegSet_ftw, NULL, g_aDbgfRegFields_ftw },
    { "fop",        DBGFREG_FOP,        DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.FOP),          NULL, NULL, NULL, NULL },
    { "fpuip",      DBGFREG_FPUIP,      DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, fpu.FPUIP),        NULL, NULL, g_aDbgfRegAliases_fpuip, NULL },
    { "fpucs",      DBGFREG_FPUCS,      DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.CS),           NULL, NULL, NULL, NULL },
    { "fpudp",      DBGFREG_FPUDP,      DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, fpu.FPUDP),        NULL, NULL, g_aDbgfRegAliases_fpudp, NULL },
    { "fpuds",      DBGFREG_FPUDS,      DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, fpu.DS),           NULL, NULL, NULL, NULL },
    { "mxcsr",      DBGFREG_MXCSR,      DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, fpu.MXCSR),        NULL, NULL, NULL, g_aDbgfRegFields_mxcsr },
    { "mxcsr_mask", DBGFREG_MXCSR_MASK, DBGFREGVALTYPE_U32, RT_OFFSETOF(CPUMCTX, fpu.MXCSR_MASK),   NULL, NULL, NULL, g_aDbgfRegFields_mxcsr },
#define DBGFREGDESC_ST(n) \
    { "st" #n,      DBGFREG_ST##n,      DBGFREGVALTYPE_80, ~(size_t)0, dbgfR3RegGet_stN, dbgfR3RegSet_stN, NULL, g_aDbgfRegFields_stN }
    DBGFREGDESC_ST(0),
    DBGFREGDESC_ST(1),
    DBGFREGDESC_ST(2),
    DBGFREGDESC_ST(3),
    DBGFREGDESC_ST(4),
    DBGFREGDESC_ST(5),
    DBGFREGDESC_ST(6),
    DBGFREGDESC_ST(7),
#define DBGFREGDESC_MM(n) \
    { "mm" #n,      DBGFREG_MM##n,      DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, fpu.aRegs[n].mmx), NULL, NULL, NULL, g_aDbgfRegFields_mmN }
    DBGFREGDESC_MM(0),
    DBGFREGDESC_MM(1),
    DBGFREGDESC_MM(2),
    DBGFREGDESC_MM(3),
    DBGFREGDESC_MM(4),
    DBGFREGDESC_MM(5),
    DBGFREGDESC_MM(6),
    DBGFREGDESC_MM(7),
#define DBGFREGDESC_XMM(n) \
    { "xmm" #n,     DBGFREG_XMM##n,     DBGFREGVALTYPE_U128, RT_OFFSETOF(CPUMCTX, fpu.aXMM[n].xmm), NULL, NULL, NULL, g_aDbgfRegFields_xmmN }
    DBGFREGDESC_XMM(0),
    DBGFREGDESC_XMM(1),
    DBGFREGDESC_XMM(2),
    DBGFREGDESC_XMM(3),
    DBGFREGDESC_XMM(4),
    DBGFREGDESC_XMM(5),
    DBGFREGDESC_XMM(6),
    DBGFREGDESC_XMM(7),
    DBGFREGDESC_XMM(8),
    DBGFREGDESC_XMM(9),
    DBGFREGDESC_XMM(10),
    DBGFREGDESC_XMM(11),
    DBGFREGDESC_XMM(12),
    DBGFREGDESC_XMM(13),
    DBGFREGDESC_XMM(14),
    DBGFREGDESC_XMM(15),
    { "gdtr_base",  DBGFREG_GDTR_BASE,      DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, gdtr.pGdt),           NULL, NULL, NULL, NULL },
    { "gdtr_limit", DBGFREG_GDTR_LIMIT,     DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, gdtr.cbGdt),          NULL, NULL, NULL, NULL },
    { "idtr_base",  DBGFREG_IDTR_BASE,      DBGFREGVALTYPE_U64, RT_OFFSETOF(CPUMCTX, idtr.pIdt),           NULL, NULL, NULL, NULL },
    { "idtr_limit", DBGFREG_IDTR_LIMIT,     DBGFREGVALTYPE_U16, RT_OFFSETOF(CPUMCTX, idtr.cbIdt),          NULL, NULL, NULL, NULL },
    DBGFREGDESC_SEG(LDTR, ldtr),
    DBGFREGDESC_SEG(TR, tr),
    { "cr0",        DBGFREG_CR0,       DBGFREGVALTYPE_U32, 0, dbgfR3RegGet_crX, dbgfR3RegSet_crX, g_aDbgfRegAliases_cr0, g_aDbgfRegFields_cr0 },
    { "cr2",        DBGFREG_CR2,       DBGFREGVALTYPE_U64, 2, dbgfR3RegGet_crX, dbgfR3RegSet_crX, NULL, NULL },
    { "cr3",        DBGFREG_CR3,       DBGFREGVALTYPE_U64, 3, dbgfR3RegGet_crX, dbgfR3RegSet_crX, NULL, g_aDbgfRegFields_cr3 },
    { "cr4",        DBGFREG_CR4,       DBGFREGVALTYPE_U32, 4, dbgfR3RegGet_crX, dbgfR3RegSet_crX, NULL, g_aDbgfRegFields_cr4 },
    { "cr8",        DBGFREG_CR8,       DBGFREGVALTYPE_U32, 8, dbgfR3RegGet_crX, dbgfR3RegSet_crX, NULL, NULL },
    { "dr0",        DBGFREG_DR0,       DBGFREGVALTYPE_U64, 0, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, NULL },
    { "dr1",        DBGFREG_DR1,       DBGFREGVALTYPE_U64, 1, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, NULL },
    { "dr2",        DBGFREG_DR2,       DBGFREGVALTYPE_U64, 2, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, NULL },
    { "dr3",        DBGFREG_DR3,       DBGFREGVALTYPE_U64, 3, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, NULL },
    { "dr6",        DBGFREG_DR6,       DBGFREGVALTYPE_U32, 6, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, g_aDbgfRegFields_dr6 },
    { "dr7",        DBGFREG_DR7,       DBGFREGVALTYPE_U32, 7, dbgfR3RegGet_drX, dbgfR3RegSet_drX, NULL, g_aDbgfRegFields_dr7 },
    { "apic_base",    DBGFREG_MSR_IA32_APICBASE,      DBGFREGVALTYPE_U32, MSR_IA32_APICBASE,      dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_apic_base },
    { "pat",          DBGFREG_MSR_IA32_CR_PAT,        DBGFREGVALTYPE_U64, MSR_IA32_CR_PAT,        dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_cr_pat },
    { "perf_status",  DBGFREG_MSR_IA32_PERF_STATUS,   DBGFREGVALTYPE_U64, MSR_IA32_PERF_STATUS,   dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_perf_status },
    { "sysenter_cs",  DBGFREG_MSR_IA32_SYSENTER_CS,   DBGFREGVALTYPE_U16, MSR_IA32_SYSENTER_CS,   dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "sysenter_eip", DBGFREG_MSR_IA32_SYSENTER_EIP,  DBGFREGVALTYPE_U32, MSR_IA32_SYSENTER_EIP,  dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "sysenter_esp", DBGFREG_MSR_IA32_SYSENTER_ESP,  DBGFREGVALTYPE_U32, MSR_IA32_SYSENTER_ESP,  dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "tsc",          DBGFREG_MSR_IA32_TSC,           DBGFREGVALTYPE_U32, MSR_IA32_TSC,           dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "efer",         DBGFREG_MSR_K6_EFER,            DBGFREGVALTYPE_U32, MSR_K6_EFER,            dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_efer },
    { "star",         DBGFREG_MSR_K6_STAR,            DBGFREGVALTYPE_U64, MSR_K6_STAR,            dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_star },
    { "cstar",        DBGFREG_MSR_K8_CSTAR,           DBGFREGVALTYPE_U64, MSR_K8_CSTAR,           dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_cstar },
    { "msr_fs_base",  DBGFREG_MSR_K8_FS_BASE,         DBGFREGVALTYPE_U64, MSR_K8_FS_BASE,         dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "msr_gs_base",  DBGFREG_MSR_K8_GS_BASE,         DBGFREGVALTYPE_U64, MSR_K8_GS_BASE,         dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "krnl_gs_base", DBGFREG_MSR_K8_KERNEL_GS_BASE,  DBGFREGVALTYPE_U64, MSR_K8_KERNEL_GS_BASE,  dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "lstar",        DBGFREG_MSR_K8_LSTAR,           DBGFREGVALTYPE_U64, MSR_K8_LSTAR,           dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, g_aDbgfRegFields_lstar },
    { "tsc_aux",      DBGFREG_MSR_K8_TSC_AUX,         DBGFREGVALTYPE_U64, MSR_K8_TSC_AUX,         dbgfR3RegGet_msr, dbgfR3RegSet_msr, NULL, NULL },
    { "ah",           DBGFREG_AH,       DBGFREGVALTYPE_U8,  RT_OFFSETOF(CPUMCTX, rax) + 1,  NULL, NULL, NULL, NULL },
    { "ch",           DBGFREG_CH,       DBGFREGVALTYPE_U8,  RT_OFFSETOF(CPUMCTX, rcx) + 1,  NULL, NULL, NULL, NULL },
    { "dh",           DBGFREG_DH,       DBGFREGVALTYPE_U8,  RT_OFFSETOF(CPUMCTX, rdx) + 1,  NULL, NULL, NULL, NULL },
    { "bh",           DBGFREG_BH,       DBGFREGVALTYPE_U8,  RT_OFFSETOF(CPUMCTX, rbx) + 1,  NULL, NULL, NULL, NULL },
    { "gdtr",         DBGFREG_GDTR,     DBGFREGVALTYPE_DTR, ~(size_t)0, dbgfR3RegGet_gdtr, dbgfR3RegSet_gdtr, NULL, NULL },
    { "idtr",         DBGFREG_IDTR,     DBGFREGVALTYPE_DTR, ~(size_t)0, dbgfR3RegGet_idtr, dbgfR3RegSet_idtr, NULL, NULL },
#undef DBGFREGDESC_REG
#undef DBGFREGDESC_SEG
#undef DBGFREGDESC_ST
#undef DBGFREGDESC_MM
#undef DBGFREGDESC_XMM
};

